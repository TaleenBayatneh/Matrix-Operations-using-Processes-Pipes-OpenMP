#define _POSIX_C_SOURCE 200809L  // Enable POSIX.1-2008 features for clock_gettime

#include <time.h>
#include <stdint.h>

// Returns the current time in milliseconds as a 64-bit unsigned integer. since an arbitrary point (monotonic clock).
// Uses CLOCK_MONOTONIC to ensure the time is not affected by system clock changes.
uint64_t now_millis(void) {

    struct timespec ts; // Structure to hold seconds and nanoseconds
    // Get the current monotonic time
    clock_gettime(CLOCK_MONOTONIC, &ts);
    // Convert seconds to milliseconds, add nanoseconds converted to milliseconds
    return (uint64_t)ts.tv_sec * 1000ull+ (uint64_t)ts.tv_nsec / 1000000ull;
}
