#include <stdint.h>

#ifdef _WIN32
#include <Windows.h>

typedef struct timespec {
    long tv_sec;
    long tv_nsec;
} timespec;

#define exp7 10000000i64
#define exp9 1000000000i64
#define w2ux 116444736000000000i64

#define CLOCK_MONOTONIC_RAW 1

void unix_time(struct timespec * spec) {
    __int64 wintime;
    GetSystemTimeAsFileTime((FILETIME * ) & wintime);
    wintime -= w2ux;
    spec->tv_sec = wintime / exp7;
    spec->tv_nsec = wintime % exp7 * 100;
}

int clock_gettime(int ignored, timespec * spec) {
    static struct timespec startspec;
    static double ticks2nano;
    static __int64 startticks, tps = 0;
    __int64 tmp, curticks;
    QueryPerformanceFrequency((LARGE_INTEGER * ) & tmp); //some strange system can
    if (tps != tmp) {
        tps = tmp;
        QueryPerformanceCounter((LARGE_INTEGER * ) & startticks);
        unix_time( & startspec);
        ticks2nano = (double) exp9 / tps;
    }

    QueryPerformanceCounter((LARGE_INTEGER * ) & curticks);
    curticks -= startticks;
    spec->tv_sec = startspec.tv_sec + (curticks / tps);
    spec->tv_nsec = startspec.tv_nsec + (double)(curticks % tps) * ticks2nano;

    if (!(spec-> tv_nsec < exp9)) {
        spec->tv_sec++;
        spec->tv_nsec -= exp9;
    }
    return 0;
}
#endif

struct timespec now, last;

uint64_t time_diff() {
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    uint64_t delta_us = (now.tv_sec - last.tv_sec) * 1000000 + (now.tv_nsec - last.tv_nsec) / 1000;
    last = now;
    return delta_us;
}
