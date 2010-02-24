/*
 * UM-kernel user thread control.
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"umhost.h"

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
 * Initialize user threads. This has to be done early,
 * so forked host processes do not get the copy of kernel pool.
 */

void 
uthreadinit(void)
{
}


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
 * process.
 */

void 
touser(void *sp)
{
	print("touser pid=%uld sp=%08p\n", up?up->pid:-1, sp);
	procsegs(up);
	host_abort(0);
}
