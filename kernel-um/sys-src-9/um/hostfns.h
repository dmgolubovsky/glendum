/*
 * This file will be automatically processed, and code
 * generated to call host functions and host OS syscalls.
 *
 * Rules:
 *
 * For host function calls, place HOSTLINK right before the
 * function name, and HOSTARGS right after. 
 * The whole prototype has to be on a single line
 * (or at least HOSTLINK and function name and HOSTARGS).
 */

void * HOSTLINK host_alloc HOSTARGS (ulong size);

int HOSTLINK host_write HOSTARGS (char *buf, int length);

void HOSTLINK host_free HOSTARGS (void *p);

int HOSTLINK host_cpufreq HOSTARGS (void);

