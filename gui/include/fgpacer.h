#ifndef CHIAKI_GUI_FGPACER_H
#define CHIAKI_GUI_FGPACER_H

#include <algorithm>
#include <cstdint>

// Presentation schedule for frame generation.
//
// A batch is the set of frames made from one new stream picture: the
// generated in-between frames followed by the real picture. Pictures reach
// the client with network jitter (several ms over Wi-Fi), so presenting each
// batch "as soon as it is ready" on a fixed clock makes the in-between frames
// bunch up or get dropped whenever a picture is early or late.
//
// The pacer instead runs a smoothed presentation clock:
//  - each batch starts one picture period after the previous one, a small
//    jitter buffer behind the arrival of the picture, and its frames are spread
//    evenly over that period;
//  - the period follows the measured picture rate (console and PC clocks
//    drift apart slowly), bounded to +-1% of the nominal rate;
//  - a late picture shifts the clock (one visible hitch) and widens the
//    buffer; long stable stretches shrink it back, so the added latency stays
//    as small as the connection allows;
//  - if pictures keep arriving well before their slot the clock is pulled in
//    gradually, so latency never builds up.
//
// Pure logic without Qt or Vulkan so it can be unit tested.
class FgPacer
{
public:
    struct Config
    {
        // Every ms here is added to the input latency of every frame: a large
        // buffer only hides rare Wi-Fi spikes, which then cost one hitch.
        int64_t min_buffer_us = 2000;
        int64_t start_buffer_us = 4000;
        int64_t max_buffer_us = 12000;
        // Buffer growth after a late picture and shrink after a stable stretch.
        int64_t grow_us = 2000;
        int64_t shrink_us = 500;
        int stable_batches_to_shrink = 150; // ~5 s at 30 fps
        // Two late pictures within this many batches widen the buffer.
        uint64_t late_window_batches = 60;
    };

    FgPacer() { reset(); }
    explicit FgPacer(const Config &c) : cfg(c) { reset(); }

    void reset()
    {
        next_start_us = 0;
        last_arrival_us = 0;
        period_est_us = 0.0;
        nominal_period_us = 0;
        buffer_us = cfg.start_buffer_us;
        stable_batches = 0;
        late_batches = 0;
        batches = 0;
        last_late_batch = ~uint64_t(0) / 2;
        excess_avg_us = 0.0;
    }

    // A batch of `count` frames for a picture that lasts `nominal_period_us`
    // (stream frame interval times the repeat cadence) is ready at `now_us`.
    // `extra_lead_us` is added to the buffer (e.g. the GPU time still needed
    // for the generated frames). Writes the presentation time of each frame
    // into `targets_us` (count entries) and returns the first one.
    int64_t schedule(int64_t now_us, int64_t nominal_period, int count, int64_t extra_lead_us, int64_t *targets_us)
    {
        count = (std::max)(count, 1);
        extra_lead_us = (std::max)(extra_lead_us, int64_t(0));
        if (nominal_period != nominal_period_us) {
            nominal_period_us = nominal_period;
            period_est_us = static_cast<double>(nominal_period_us);
            next_start_us = 0;
            last_arrival_us = 0;
        }
        const double nominal = static_cast<double>((std::max)(nominal_period_us, int64_t(1)));

        // Track the real picture rate from arrival intervals; gaps (lost
        // pictures, stalls) and bursts do not belong to the rate.
        if (last_arrival_us > 0) {
            const double interval = static_cast<double>(now_us - last_arrival_us);
            if (interval > 0.5 * nominal && interval < 1.5 * nominal) {
                period_est_us += (interval - period_est_us) / 32.0;
                period_est_us = std::clamp(period_est_us, nominal * 0.99, nominal * 1.01);
            }
        }
        last_arrival_us = now_us;
        const int64_t period = static_cast<int64_t>(period_est_us + 0.5);
        const int64_t lead_wanted = buffer_us + extra_lead_us;

        int64_t start;
        if (!next_start_us || next_start_us - now_us > period + cfg.max_buffer_us + extra_lead_us ||
            now_us - next_start_us > period) {
            // First batch, or the clock is far off (stall, long pause): resync.
            start = now_us + lead_wanted;
            stable_batches = 0;
            excess_avg_us = 0.0;
        } else if (next_start_us < now_us + extra_lead_us) {
            // Late: this picture missed its slot. Show it as soon as it can be.
            // A lone spike (Wi-Fi retry) is one hitch; only repeated late
            // pictures widen the buffer, which costs latency on every frame.
            // Half the buffer is rebuilt at once so the next picture is not
            // late again; the steering below restores the rest.
            // Only a picture a little late (jitter just above the buffer)
            // widens it: a large Wi-Fi spike would not be covered by any
            // affordable buffer, so growing for it only adds latency.
            const int64_t late_by_us = now_us + extra_lead_us - next_start_us;
            start = now_us + extra_lead_us + buffer_us / 2;
            late_batches++;
            stable_batches = 0;
            excess_avg_us = 0.0;
            if (late_by_us <= cfg.grow_us && batches - last_late_batch <= cfg.late_window_batches)
                buffer_us = (std::min)(buffer_us + cfg.grow_us, cfg.max_buffer_us);
            last_late_batch = batches;
        } else {
            // Steer the margin between arrival and slot towards the buffer by
            // at most 2% of a period per batch (invisible): pictures arriving
            // well ahead would only add latency, pictures eating into the
            // margin (clock drift) would soon be late.
            // The margin is averaged so arrival jitter does not move the clock.
            const double excess = static_cast<double>((next_start_us - now_us) - lead_wanted);
            excess_avg_us += (excess - excess_avg_us) / 8.0;
            const double max_step = static_cast<double>(period) / 50.0;
            // Pulling in never goes past this picture's own margin: the average
            // lags behind, and overshooting made the next picture late.
            const double step = excess_avg_us > 0.0 ? (std::min)(excess_avg_us, (std::max)(excess, 0.0)) : excess_avg_us;
            start = next_start_us - static_cast<int64_t>(std::clamp(step, -max_step, max_step));
            if (++stable_batches >= cfg.stable_batches_to_shrink) {
                stable_batches = 0;
                buffer_us = (std::max)(buffer_us - cfg.shrink_us, cfg.min_buffer_us);
            }
        }

        for (int k = 0; k < count; k++)
            targets_us[k] = start + (period * k) / count;
        next_start_us = start + period;
        batches++;
        return start;
    }

    int64_t bufferUs() const { return buffer_us; }
    int64_t periodUs() const { return static_cast<int64_t>(period_est_us + 0.5); }
    uint64_t lateBatches() const { return late_batches; }
    uint64_t batchCount() const { return batches; }

private:
    Config cfg;
    int64_t next_start_us = 0;
    int64_t last_arrival_us = 0;
    double period_est_us = 0.0;
    int64_t nominal_period_us = 0;
    int64_t buffer_us = 0;
    int stable_batches = 0;
    uint64_t late_batches = 0;
    uint64_t batches = 0;
    uint64_t last_late_batch = 0;
    double excess_avg_us = 0.0;
};

#endif // CHIAKI_GUI_FGPACER_H
