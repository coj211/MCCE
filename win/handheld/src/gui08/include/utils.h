#ifndef UTILS_H
#define UTILS_H
#include <_types.h>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <stdio.h>
#define vcvts_n_f32_s32(a2, a3) (float)((float)(a2) / (1 << (uint32_t)(a3)))

extern time_t startedAtSec;
#ifdef __cplusplus
extern "C" {
	int32_t getRemainingFileSize(FILE* file);
}
#endif

#endif // UTILS_H
