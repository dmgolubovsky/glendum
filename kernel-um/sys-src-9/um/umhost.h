/*
 * UM-kernel host link definitions
 */

/*
 * A type to represent an argument of a host call. 
 * To avoid unnecessary type coercions,
 * it is defined as an union of possible types.
 */

typedef union {
	void *addr;			/* to represent an address */
	unsigned long uval;		/* to represent an unsigned value */
	long val;				/* to represent a signed value */
} Hostparm;

/*
 * For convenience, names proxy functions for host calls are postfixed 
 * with their parameter types (A, U, V). So, a funciton to print a line 
 * on the host console is named `kprintA' since it only takes one argument 
 * which is an address of a null-terminated string.
 */

/*
 * Print a null-terminated string on the host console.
 */

void kprintA(char *s);

/*
 * Fetch Plan9 configuration from the host as if it came from the loader.
 */

void getconfAU(char *s, unsigned long l);

/*
 * Formatted print (complete type madness...).
 */

void kprintf(char *fmt, ...);

/*
 * Write to the console (buffer with known length).
 */

void kwriteAU(char *buf, int length);

/*
 * Retrieve the total number of pages provided by the host.
 */

void confmemAA(ulong *pnpage, ulong *pupages);

/*
 * Retrieve the base and size of the lower portion of host memory.
 */

void hostmemAA(ulong *pnpage, ulong *pupages);

/*
 * Allocate memory on the host.
 */

void mallocVA(ulong, void **);

/*
 * Free memory on the host.
 */

void freeA(void *);

/*
 * Get host CPU frequency (MHz).
 */

void cpufreqA(int *);

/*
 * Get host timer frequency and current time in ns.
 */

void hosttimeAA(uvlong *, uvlong *);

/*
 * Get approximate value of kernel memory size (malloc-able host memory).
 */

void maxpoolA(uvlong *); 

/*
 * Get virtual address of user stack top.
 */

void ustktopA(ulong *);

/*
 * Other stuff substituted by the host.
 */

void umscreeninit(void);
void umkbdinit(void);
void umtimerinit(void);
void umcpuidentify(void);
uvlong umfastclock(uvlong *);
void umtimerset(uvlong);