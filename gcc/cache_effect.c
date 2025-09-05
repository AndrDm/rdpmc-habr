#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define ARRAY_SIZE (1024 * 1024 * 1024) // 1GB
#define MAX_STRIDE (1024 * 1024 * 1024)

static inline uint64_t rdpmc(uint32_t counter)
{
	uint32_t lo, hi;
	__asm__ volatile ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(counter));
	return ((uint64_t)hi << 32) | lo;
}

int main() {
	uint8_t *array = (uint8_t *)malloc(ARRAY_SIZE);
	if (!array) {
		fprintf(stderr, "Could not allocate memory\n");
		return 1;
	}
	SetProcessAffinityMask(GetCurrentProcess(), 1);
	// Initialize array to avoid page faults and lazy allocation effects
	for (size_t i = 0; i < ARRAY_SIZE; i += 4096) array[i] = (uint8_t)(i & 0xFF);

	// Iterate over a set of stride values, doubling each time
	printf("Stride\t\t\tCache\n(bytes)\tGB/seconds\tHits\t\tMiss\n");

	for (size_t stride = 1; stride <= MAX_STRIDE; stride *= 2) {
		clock_t start = clock();
		volatile uint8_t tmp;
		size_t accesses = ARRAY_SIZE / stride;
       
		uint64_t gp0_before = rdpmc(0); // GP0
		uint64_t gp1_before = rdpmc(1); // GP1
		for (size_t rep = 0; rep < stride; ++rep)
			for (size_t i = 0; i < accesses; ++i) tmp = array[i * stride];
		uint64_t gp0_after = rdpmc(0); // GP0
		uint64_t gp1_after = rdpmc(1); // GP1

        double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
        printf("%llu \t%f \t%llu \t%llu\n", (unsigned long long)stride, 1.0/seconds,
        	(unsigned long long)(gp0_after - gp0_before),
        	(unsigned long long)(gp1_after - gp1_before));
    }
    free(array);
    return 0;
}
