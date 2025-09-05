#include <windows.h>
#include <stdio.h>
#include <intrin.h> // for __rdtsc()

int main() {
    for(;;) {   
        unsigned __int64 start = __rdtsc();  // 1st timestamp
        Sleep(1000); // Sleep
        //for (volatile int i = 0; i < 512000000; ++i); // Buzy
        unsigned __int64 end = __rdtsc();  // 2nd timestamp
        printf("Time-Stamp Counter increments during 1 second: %lld\n", end - start);
    }
    return 0;
}
