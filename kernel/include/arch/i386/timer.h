#pragma once
#include <stdint.h>

#define TIMER_HZ 100

void     timer_init(uint32_t hz);
uint32_t timer_ticks(void);          // ticks since boot (TIMER_HZ per second)
uint64_t timer_ms(void);             // milliseconds since boot (10 ms resolution)
uint32_t wall_time(void);            // unix seconds (utc): rtc at boot + uptime
void     rtc_init(void);             // read the cmos clock; call once at boot
uint32_t rtc_boot_epoch(void);
