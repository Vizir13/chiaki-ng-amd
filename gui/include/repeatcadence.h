#ifndef CHIAKI_GUI_REPEATCADENCE_H
#define CHIAKI_GUI_REPEATCADENCE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

// Tells new game pictures from repeats in the stream, and how many stream
// frames a game picture lasts.
//
// A 30 fps game in a 60 fps Remote Play stream sends every picture twice; a
// 40 fps game alternates one and two frames, and a late picture is shown for
// three frames and the next one for a single frame. Frame generation must
// interpolate between real pictures only: a repeat taken for a new picture
// makes it interpolate between identical pictures and then jump, and a new
// picture taken for a repeat makes it interpolate across two pictures (twice
// the motion, which shows as artifacts while the camera turns).
//
// The decision is made on the picture itself: a small thumbnail (rendered on
// the GPU) is compared with the thumbnail of the last new picture. A repeat
// only differs by encoder refinement, which the downscale averages away; a
// new picture in motion changes a large part of the thumbnail. The two can
// only be confused when nearly nothing moves, where interpolating across two
// pictures does not show. The compressed size of the frame (a repeat is a
// much smaller P-frame) only decides the few in-between cases.
//
// The cadence is the average distance between new pictures over the last
// pictures, in half stream frames: a lone pacing hiccup (3 then 1) does not
// move it, a real change of the game's frame rate does within ~0.2 s.
//
// Pure logic without Qt or Vulkan so it can be unit tested.
class RepeatDetector
{
public:
    static constexpr int kWidth = 256;
    static constexpr int kHeight = 144;

    enum class Kind {
        New,
        Repeat,
    };

    struct Result
    {
        Kind kind = Kind::New;
        // Share of the thumbnail that changed since the last new picture.
        double changed = 1.0;
        // Decided by the compressed size (the picture alone was not clear).
        bool by_size = false;
        // Taken as new only because it would have been a third repeat in a row.
        bool forced = false;
    };

    // Classifies a frame from its RGBA8 thumbnail (kWidth x kHeight, rows of
    // `row_pitch` bytes), its compressed size (0 = unknown) and its position
    // in the stream (counting every stream frame, lost ones included).
    Result classify(const uint8_t *rgba, size_t row_pitch, size_t packet_size, uint64_t seq)
    {
        Result r;
        luma.resize(static_cast<size_t>(kWidth) * kHeight);
        for (int y = 0; y < kHeight; y++) {
            const uint8_t *row = rgba + static_cast<size_t>(y) * row_pitch;
            uint8_t *out = luma.data() + static_cast<size_t>(y) * kWidth;
            for (int x = 0; x < kWidth; x++) {
                const uint8_t *p = row + x * 4;
                out[x] = static_cast<uint8_t>((54u * p[0] + 183u * p[1] + 19u * p[2] + 128u) >> 8);
            }
        }
        if (reference.size() == luma.size()) {
            size_t changed = 0;
            for (size_t i = 0; i < luma.size(); i++)
                changed += std::abs(static_cast<int>(luma[i]) - static_cast<int>(reference[i])) > kPixelDelta ? 1u : 0u;
            r.changed = static_cast<double>(changed) / static_cast<double>(luma.size());
            if (r.changed <= kRepeatShare) {
                r.kind = Kind::Repeat;
            } else if (r.changed < kNewShare) {
                r.by_size = true;
                const bool small = packet_size > 0 && new_size > 0.0 &&
                    static_cast<double>(packet_size) < new_size * kSmallSizeRatio;
                r.kind = small ? Kind::Repeat : Kind::New;
            }
        }
        // No game shown at 20 fps or more repeats a picture for three stream
        // frames: a third "repeat" in a row is a nearly still scene (a still
        // camera at night, where noise keeps the change between the two
        // thresholds and the new pictures are as small as repeats). Show it,
        // so the output never stalls, and let it reset the size reference;
        // its gap says nothing about the game's frame rate.
        if (r.kind == Kind::Repeat && have_seq && seq >= last_seq + kMaxCadence) {
            r.kind = Kind::New;
            r.forced = true;
        }
        if (r.kind == Kind::New) {
            reference.swap(luma);
            // The size reference follows the scene quickly: a reference left
            // from a busy scene would make every picture of a calm one look
            // like a repeat.
            if (packet_size > 0)
                new_size = new_size > 0.0 ? new_size + (static_cast<double>(packet_size) - new_size) / 4.0
                                          : static_cast<double>(packet_size);
            newPicture(seq, !r.forced);
        }
        return r;
    }

    // A frame that could not be compared (no thumbnail): taken as a new
    // picture, and the next one is compared with nothing.
    void unknown(uint64_t seq)
    {
        reference.clear();
        newPicture(seq);
    }

    // Stream frames per game picture, in halves: 2 = every frame is a new
    // picture, 3 = 40 fps in a 60 fps stream, 4 = 30 fps, 6 = 20 fps.
    int cadence2() const { return cadence_half; }

    void reset()
    {
        reference.clear();
        new_size = 0.0;
        have_seq = false;
        gap_unreliable = false;
        last_seq = 0;
        gaps_count = 0;
        gaps_cursor = 0;
        cadence_half = 2;
    }

private:
    // `measure`: the gap to this picture counts for the cadence. The gap after
    // a forced picture is not measured either (it started at a guess).
    void newPicture(uint64_t seq, bool measure = true)
    {
        const bool measured = measure && !gap_unreliable;
        gap_unreliable = !measure;
        if (measured && have_seq && seq > last_seq) {
            const uint64_t gap = seq - last_seq;
            // Longer gaps are stills or stalls, not the game's frame rate.
            if (gap <= kMaxGap) {
                gaps[gaps_cursor] = static_cast<uint8_t>(gap);
                gaps_cursor = (gaps_cursor + 1) % gaps.size();
                if (gaps_count < gaps.size())
                    gaps_count++;
                updateCadence();
            }
        }
        have_seq = true;
        last_seq = seq;
    }

    void updateCadence()
    {
        if (gaps_count < kMinGaps)
            return;
        int sum = 0;
        for (size_t i = 0; i < gaps_count; i++)
            sum += gaps[i];
        const double target = 2.0 * sum / static_cast<double>(gaps_count);
        // Hysteresis: the mean has to move by more than 0.3 stream frames.
        if (std::fabs(target - cadence_half) > 0.6)
            cadence_half = std::clamp(static_cast<int>(std::lround(target)), 2, 2 * static_cast<int>(kMaxCadence));
    }

    // A thumbnail pixel (an average of ~7x7 stream pixels) counts as changed
    // beyond this many 8-bit luma levels.
    static constexpr int kPixelDelta = 8;
    // At most this share changed: the same picture. At least kNewShare: a new
    // one. In between the compressed size decides.
    static constexpr double kRepeatShare = 0.001;
    static constexpr double kNewShare = 0.01;
    // A repeat is a much smaller P-frame than a new picture (PS5, GTA: 9-14 KB
    // against 45-70 KB, still scenes ~1.3 KB against ~3.4 KB).
    static constexpr double kSmallSizeRatio = 0.5;
    static constexpr uint64_t kMaxGap = 6;
    static constexpr uint64_t kMaxCadence = 3;
    static constexpr size_t kMinGaps = 6;

    std::vector<uint8_t> luma;
    std::vector<uint8_t> reference;
    double new_size = 0.0;
    bool have_seq = false;
    bool gap_unreliable = false;
    uint64_t last_seq = 0;
    std::array<uint8_t, 12> gaps = {};
    size_t gaps_count = 0;
    size_t gaps_cursor = 0;
    int cadence_half = 2;
};

#endif // CHIAKI_GUI_REPEATCADENCE_H
