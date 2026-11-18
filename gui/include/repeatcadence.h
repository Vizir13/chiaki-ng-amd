#ifndef CHIAKI_GUI_REPEATCADENCE_H
#define CHIAKI_GUI_REPEATCADENCE_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// Recognises a stream that sends every picture twice, e.g. a 30 fps game in a
// 60 fps Remote Play stream. The console encodes the repeat as a much smaller
// P-frame (measured on a PS5 with GTA: 9-14 KB against 45-70 KB for a new
// picture; in a still scene ~80 B against ~1.3 KB), so it shows up as a small
// packet between normal ones. Frame generation fed with those repeats
// interpolates between identical pictures and then jumps, which looks wobbly;
// dropping them restores an even 30 fps source.
//
// "Small" is judged against the typical size of a new picture: the median of
// the larger half of the last 16 frames. A single huge key frame (sent after
// packet loss) therefore does not make every following frame look small, and
// a run of quiet pictures (static scenes) lowers the reference with them.
//
// A 30 fps game does not alternate perfectly: a late picture is shown for
// three stream frames and the next one for a single frame (new, repeat,
// repeat, new, new, repeat...). Once locked the detector therefore does not
// follow a fixed new/repeat slot pattern: every small frame is a repeat and
// is dropped, every normal frame is kept. Only clear evidence that the game
// now renders at the stream rate (several pairs of consecutive substantial new
// pictures) unlocks it; every unlock changes the presentation clock, so
// flapping costs a visible hitch each time.
class RepeatCadenceDetector
{
public:
    // Feeds the compressed size of the next frame (0 = unknown, ignored).
    // Returns true if that frame repeats the previous picture and can be
    // dropped.
    bool feed(size_t packet_size)
    {
        if (!packet_size)
            return false;
        last_size = packet_size;
        const size_t reference = typicalNewSize();
        const bool small = reference > 0 && static_cast<double>(packet_size) < static_cast<double>(reference) * kSmallRatio;
        sizes[cursor] = packet_size;
        cursor = (cursor + 1) % sizes.size();
        if (filled < sizes.size())
            filled++;
        if (!small)
            last_new = packet_size;

        history = (history << 1) | (small ? 1u : 0u);
        if (seen < 32)
            seen++;

        if (!locked) {
            // 12 frames (200 ms at 60 fps) strictly alternating.
            const uint32_t last12 = history & 0xfffu;
            if (seen >= 12 && (last12 == 0xaaau || last12 == 0x555u)) {
                locked = true;
                consecutive_drops = 0;
                substantial = 0;
                long_new = static_cast<double>(last_new);
                if (small) {
                    consecutive_drops = 1;
                    return true;
                }
            }
            return false;
        }

        if (small) {
            substantial = substantial << 1;
            // The picture must move on: after two repeats in a row the next
            // small frame is kept (a stalled game, or a barely changing
            // picture judged small).
            if (consecutive_drops >= 2) {
                consecutive_drops = 0;
                return false;
            }
            consecutive_drops++;
            return true;
        }
        consecutive_drops = 0;

        // Long-term size of a new picture in motion, to tell a real new picture
        // from the near-identical frames of a still scene. Still pictures only
        // let it sink slowly (half in ~10 s), so the repeats right after a
        // still scene are not taken for new pictures.
        const bool is_substantial = static_cast<double>(packet_size) >= long_new * kSubstantialRatio;
        if (is_substantial)
            long_new += (static_cast<double>(packet_size) - long_new) / 64.0;
        else
            long_new *= 0.998;
        substantial = (substantial << 1) | (is_substantial ? 1u : 0u);
        // Pairs of consecutive substantial new pictures in the last 16 frames.
        const uint32_t pairs = substantial & (substantial >> 1) & 0x7fffu;
        if (popcount(pairs) >= kUnlockPairs)
            unlock();
        return false;
    }

    // Stream frames per picture: 2 while every picture is repeated, else 1.
    int cadence() const { return locked ? 2 : 1; }
    size_t lastSize() const { return last_size; }
    // Size of the last frame judged a new picture.
    size_t newPictureSize() const { return last_new; }

    void reset() { *this = RepeatCadenceDetector(); }

private:
    // Median of the larger half of the recent frame sizes.
    size_t typicalNewSize() const
    {
        if (filled < 4)
            return last_new;
        std::array<size_t, 16> tmp = {};
        std::copy(sizes.begin(), sizes.begin() + static_cast<std::ptrdiff_t>(filled), tmp.begin());
        std::sort(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(filled));
        const size_t half = filled / 2;
        return tmp[half + (filled - half) / 2];
    }

    static int popcount(uint32_t v)
    {
        int n = 0;
        for (; v; v &= v - 1)
            n++;
        return n;
    }

    void unlock()
    {
        locked = false;
        history = 0;
        seen = 0;
        consecutive_drops = 0;
        substantial = 0;
    }

    // Repeats measured up to ~0.3 of a new picture; new pictures rarely fall
    // below ~0.6 of the typical size.
    static constexpr double kSmallRatio = 0.42;
    // A new picture smaller than this share of the long-term size belongs to
    // a still scene and proves nothing about the frame rate.
    static constexpr double kSubstantialRatio = 0.5;
    // A game at the stream rate gives ~15 pairs in 16 frames; a 30 fps game
    // with pacing hiccups gives one now and then.
    static constexpr int kUnlockPairs = 4;
    std::array<size_t, 16> sizes = {};
    size_t cursor = 0;
    size_t filled = 0;
    size_t last_new = 0;
    size_t last_size = 0;
    uint32_t history = 0;
    uint32_t substantial = 0;
    double long_new = 0.0;
    int seen = 0;
    int consecutive_drops = 0;
    bool locked = false;
};

#endif // CHIAKI_GUI_REPEATCADENCE_H
