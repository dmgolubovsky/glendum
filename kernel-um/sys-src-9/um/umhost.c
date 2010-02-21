/*
 * UM-kernel host link facility.
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"umhost.h"

/*
 * This is the entry point of the UM-kernel. It takes a parameter which 
 * defines the host call interface, and stores it locally. It also exports 
 * a number of functions that the kernel may use to communicate 
 * with the host.
 */

typedef long (*hostcall) (int, Hostparm *);

static hostcall _hcall;

void main(void);

/*
 * Data structures needed to simulate the pre-kernel loader.
 */

/* 
 * replaces CPU0MACH that would be used by the loader 
 */

static Mach bootmach;	


void 
um_main(hostcall hcall) 
{
	_hcall = hcall;
	kprintA("glendum...\n");
	m = &bootmach;
	m->machno = 1;
	memset(edata, 0, end - edata); /* clear BSS like l.s does */
	main();
	return;
}

/*
 * Emulation of spl* functions. These functions originally used sti/cli
 * instructions to control interrupts. The UM-kernel is never interrupted
 * in the same manner as a regular kernel would be, also sti/cli are not
 * allowed in user mode. A special field, iprio is added to the Mach
 * structure which holds the value set/reset by splhi/spllo. This field
 * can be either 0x200 (for low priority) or 0 (for high priority). 
 * These emulation functions do not set m->splpc.
 */

int 
splhi(void)
{
	int r = m->iprio;
	m->iprio = 0;
	return r;    
}

int
spllo(void)
{
	int r = m->iprio;
	m->iprio = 0x200;
	return r;
}

void
splx(int p)
{
	if (p & 0x200)
		spllo();
	else
		splhi();
}

void
spldone(void)
{
}

int
islo(void)
{
	return m->iprio & 0x200;
}

/*
 * Come here when cr3 is about to be loaded. Host has to make arrangements.
 */

void 
putcr3(ulong pdb)
{
	kprintf("putcr3: %08x\n", pdb);
}

/*
 * Host screen, kbd, other hardware initialization. 
 * Does nothing at the moment.
 */

void
umscreeninit(void)
{
}

void
umkbdinit(void)
{
}

void
umtimerinit(void)
{
}

void umcpuidentify(void)
{
}

uvlong 
umfastclock(uvlong *phz)
{
	uvlong res;
	hosttimeAA(phz, &res);
	return res;
}

void
umtimerset(uvlong tm)
{
kprintf("umtimerset %08uld\n", tm);
	return;
}

int
uartgetc(void)
{
	return '@';
}

/*
 * Host functions.
 */

/*
 * Jump here for non-patched host call. Not much can be done,
 * just segv.
 */

void 
_failcall(void)
{
	*((int *)nil) = 1;
}

/* kprintA #1 */

void 
kprintA(char *s)
{
	Hostparm p = {.addr = s},
			ps[1] = {p};

	(*_hcall)(1, ps);
	return;
}

/* getconfAU #2 */

void
getconfAU(char *s, unsigned long l)
{
	Hostparm b = {.addr = s},
			 c = {.uval = l},
			ps[2] = {b, c};
	(*_hcall)(2, ps);
	return;
}

/* kprintf #3 */

void 
kprintf(char *fmt, ...)
{
	(*_hcall)(3, (Hostparm *)(&fmt));
	return;
}

/* kwriteAU #4 */

void
kwriteAU(char *buf, int length)
{
	host_write(buf, length);
}

/* confmemAA #5 */

void
confmemAA(ulong *pnpage, ulong *pupages)
{
	Hostparm b = {.addr = pnpage},
			 c = {.addr = pupages},
			ps[2] = {b, c};
	(*_hcall)(5, ps);
	return;
}

/* hostmemAA #6 */

void
hostmemAA(ulong *pbase, ulong *pnpage)
{
	Hostparm b = {.addr = pbase},
			 c = {.addr = pnpage},
			ps[2] = {b, c};
	(*_hcall)(6, ps);
	return;
}

/* mallocVA #7 */

void
mallocVA(ulong size, void **pmem)
{
	Hostparm b = {.uval = size},
			 c = {.addr = pmem},
			ps[2] = {b, c};
	(*_hcall)(7, ps);
	return;
}

/* freeA #8 */

void 
freeA(void *s)
{
	Hostparm p = {.addr = s},
			ps[1] = {p};

	(*_hcall)(8, ps);
	return;
}

/* cpufreqA #9 */

void 
cpufreqA(int *s)
{
	Hostparm p = {.addr = s},
			ps[1] = {p};

	(*_hcall)(9, ps);
	return;
}

/* hosttimeAA #10 */

void
hosttimeAA(uvlong *phz, uvlong *pns)
{
	Hostparm b = {.addr = phz},
			 c = {.addr = pns},
			ps[2] = {b, c};
	(*_hcall)(10, ps);
	return;
}

/* maxpoolA #11 */

void 
maxpoolA(uvlong *s)
{
	Hostparm p = {.addr = s},
			 e = {.addr = end},
			ps[2] = {p, e};

	(*_hcall)(11, ps);
	return;
}

/* ustktopA #12 */

void 
ustktopA(ulong *s)
{
	Hostparm p = {.addr = s},
			ps[1] = {p};

	(*_hcall)(12, ps);
	return;
}
