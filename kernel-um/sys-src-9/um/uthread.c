/*
 * UM-kernel user thread control.
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"umhost.h"
#include	"uthread.h"

/*
 * User thread descriptors. The kernel thread is the parent to all
 * user threads, so it maintains their host PIDs here.
 */

static Uthread uthreads[MAXUTHREAD];

/*
 * Actual number of threads. If given in the configuration options,
 * will be set as specified. Otherwise set to MINUTHREAD.
 */

static int actthreads;

/*
 * Per-thread area. Each user thread gets a copy of it after
 * fork. This area contains a host file descriptor number
 * for the memory file, and an array of current mappings (initially empty).
 * Each mapping is packed into an uvlong (64bit), and contains
 * physical address (offset in the memory file), virtual address
 * (where to map into the user thread address space), mapping
 * length (in page units, 4096 pages max), and read-write-exec
 * mode bits.
 * Theoretically user code may have write access to this area.
 * It is however harmless: altering the memory file descriptor
 * may disrupt the current thread work, and cause abort of the
 * user process; host may re-spawn a thread in such case. Altering
 * the current mappings will only cause the overhead when returning
 * into the context of the thread by means of extra remappings.
 * Also a special stack is allocated for each user thread to be used
 * when remapping process memory: at some moment user stack may be
 * completely unmapped, so to keep the code running, a temporary
 * stack is needed.
 */

static int memfd;

static uvlong *curmap;
static int curmlen;
static uchar upstk[UPSTKSZ]; /* stack to use when remapping memory */

/*
 * User thread start up.
 */

void 
uthread_main(void)
{
	curmap = nil;
	curmlen = 0;
	print("memfd: %d\n", memfd);
	host_traceme();
	print("resumed\n");
	host_abort(0);
}

/*
 * Initialize user threads. This has to be done early,
 * so forked host processes do not get the copy of kernel pool.
 */

void 
uthreadinit(void)
{
	int i;
	memfd = host_memfd();
	actthreads = MINUTHREAD;	/* has to come from the config */
	for(i = 0; i < actthreads; i++) {
		uthreads[i].uthridx = i;
		uthreads[i].upid = host_uthread();
		if(!uthreads[i].upid)
			uthread_main();
		else {
			int rc, status;
			print("forked user thread #%d, pid=%d\n",
					uthreads[i].uthridx, uthreads[i].upid);
			rc = host_waitpid(uthreads[i].upid, &status);
			print("wait for child stop: %d, %08x\n", rc, status);
			uthreads[i].hsyscalls = 0;
		}
	}
}

/*
 * The up->newtlb flag (set by flushmmu) tells us that address space
 * of a kernel thread has to be remapped. This also happens when a thread
 * is rebound with process other than it was bound with before.
 */

/*
 * Debug: print a segment info.
 */

void
printseg(Segment *s)
{
	int i;
	if(!s) {
		print("<nil>\n");
		return;
	}
	print("\ttype: %ud, base: %08lx, top %08lx, size: %uld"
			", fstart: %08lx, flen: %08lx\n",
			s->type, s->base, s->top, s->size, s->fstart, s->flen);
	print("\tflushme: %d, mapsize: %d\n", s->flushme, s->mapsize);
	for(i = 0; i < s->mapsize; i++) {
		Pte *pte = s->map[i];
		if(!pte) continue;
		print("\tmap[%d]\n", i);
		int j;
		for(j = 0; j < PTEPERTAB; j++) {
			if(!pte->pages[j]) continue;
			print("\t\tpages[%d] pa: %08lx, va: %08lx\n", j, pte->pages[j]->pa,
						pte->pages[j]->va);
		}
	}
}

/*
 * Debug: print process segments.
 */

void
procsegs(Proc *p)
{
	int i;
	if(!p) {
		print("procsegs: nil\n");
		return;
	}
	print("Segments for process %uld:\n", p->pid);
	for(i = 0; i < NSEG; i++) {
		print("%d: ", i);
		printseg(p->seg[i]);
    }
}

/*
 * Replacement of touser: get here to start the user part of the first
 * process. Thread #0 is always bound with the first process. Set up the
 * registers first (eip and esp), then conditionally force memory
 * remapping.
 */

void 
touser(void *sp)
{
	print("touser pid=%uld sp=%08p\n", up?up->pid:-1, sp);
	procsegs(up);
	up->PMMU.uthr = &uthreads[0];
	uthreads[0].proc = up;
	poke_eax(uthreads[0].upid, 0);
	poke_eip(uthreads[0].upid, UTZERO + 32);	/* just like in plan9l.s */
	poke_esp(uthreads[0].upid, (ulong)sp);
	ptrace_cont(uthreads[0].upid);
{
int rc, status=0;
ulong eip;
rc = host_waitpid(uthreads[0].upid, &status);
eip=peek_eip(uthreads[0].upid);
print("touser: rc=%d, status=%08x, eip=%08lx\n", rc, status, eip);
}
	host_abort(0);
}
