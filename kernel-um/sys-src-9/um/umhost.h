/*
 * UM-kernel host link definitions
 */

/*
 * Stuff substituted by the host.
 */

void umscreeninit(void);
void umkbdinit(void);
void umtimerinit(void);
void umcpuidentify(void);
uvlong umfastclock(uvlong *);
void umtimerset(uvlong);