#include "fgwait.h"

#include "chiaki/time.h"

#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <time.h>
#endif

namespace {

// Below this much remaining time the wait yields instead of sleeping.
constexpr int64_t kSpinUs = 600;

#ifdef _WIN32
HANDLE threadTimer()
{
    struct Timer
    {
        HANDLE handle = nullptr;
        Timer()
        {
            handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            // Older Windows without high resolution timers: a normal one,
            // still precise with the 1 ms timer resolution set while streaming.
            if (!handle)
                handle = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        }
        ~Timer()
        {
            if (handle)
                CloseHandle(handle);
        }
    };
    thread_local Timer timer;
    return timer.handle;
}
#endif

void sleepUs(int64_t us)
{
#ifdef _WIN32
    HANDLE timer = threadTimer();
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(us) * 10; // relative, 100 ns units
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
    Sleep(static_cast<DWORD>(us / 1000));
#else
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(us / 1000000);
    ts.tv_nsec = static_cast<long>((us % 1000000) * 1000);
    nanosleep(&ts, nullptr);
#endif
}

} // namespace

void fgWaitUntilUs(int64_t target_us, const std::function<bool()> &should_abort, int64_t max_wait_us)
{
    int64_t now = static_cast<int64_t>(chiaki_time_now_monotonic_us());
    if (target_us - now > max_wait_us)
        return;
    for (;;) {
        if (should_abort && should_abort())
            return;
        now = static_cast<int64_t>(chiaki_time_now_monotonic_us());
        const int64_t remaining = target_us - now;
        if (remaining <= 0)
            return;
        if (remaining > kSpinUs) {
            // Sleep in slices of at most 2 ms so an abort is noticed quickly.
            const int64_t slice = remaining - kSpinUs;
            sleepUs(slice > 2000 ? 2000 : slice);
        } else {
            std::this_thread::yield();
        }
    }
}
