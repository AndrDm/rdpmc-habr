#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

static inline uint64_t rdpmc(uint32_t counter)
{
	uint32_t lo, hi;
	__asm__ volatile ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(counter));
	return ((uint64_t)hi << 32) | lo;
}


int main() {
	SetProcessAffinityMask(GetCurrentProcess(), 1);
    for(;;) {   
        unsigned __int64 start0 = rdpmc(0x40000000);
        unsigned __int64 start1 = rdpmc(0x40000001);
        unsigned __int64 start2 = rdpmc(0x40000002);
        Sleep(1000); // Sleep
        //for (volatile int i = 0; i < 512000000; ++i); // Buzy
        unsigned __int64 end0 = rdpmc(0x40000000);
        unsigned __int64 end1 = rdpmc(0x40000001);
        unsigned __int64 end2 = rdpmc(0x40000002);
        printf("Instructions =  %lld\n", end0 - start0);
        printf("Core Cycles  =  %lld\n", end1 - start1);
        printf("Reference Cycles  =  %lld\n", end2 - start2);
    }
    return 0;
}
