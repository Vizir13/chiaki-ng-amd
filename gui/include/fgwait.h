#ifndef CHIAKI_GUI_FGWAIT_H
#define CHIAKI_GUI_FGWAIT_H

#include <cstdint>
#include <functional>

// Blocks the calling thread until the monotonic clock (chiaki_time_now_
// monotonic_us) reaches `target_us`, to within ~0.1 ms: a high resolution
// waitable timer on Windows (plain sleeps there round up to the 1-15.6 ms
// scheduler tick), nanosleep elsewhere, then a short yield loop. Returns early
// when `should_abort` returns true, and does not wait at all for targets
// more than `max_wait_us` away (a stale or bogus target).
void fgWaitUntilUs(int64_t target_us, const std::function<bool()> &should_abort = {}, int64_t max_wait_us = 100000);

#endif // CHIAKI_GUI_FGWAIT_H
