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

void main(void);

/*
 * Data structures needed to simulate the pre-kernel loader.
 */

/* 
 * replaces CPU0MACH that would be used by the loader 
 */

static Mach bootmach;	


void 
um_main(void) 
{
	print("glendum...\n");
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
	print("putcr3: %08lx\n", pdb);
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
	host_timeres(phz, &res);
	return res;
}

void
umtimerset(uvlong tm)
{
print("umtimerset %lld\n", tm);
	return;
}

int
uartgetc(void)
{
	return '@';
}

/*
 * Jump here for non-patched host call. Not much can be done,
 * just segv.
 */

void 
_failcall(void)
{
	*((int *)nil) = 1;
}

