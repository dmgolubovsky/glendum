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
 * Per-thread area. Each user thread gets a copy of it after
 * fork. This area contains a host file descriptor number
 * for the memory file, and an array of current mappings.
 * Each mapping is packed into an uvlong (64bit), and contains
 * physical address (offset in the memory file), virtual address
 * (where to map into the user thread address space), mapping
 * length (in page units, 4096 pages max), and read-write-exec
 * mode bits. Set of mappings is kept for each segment (NSEG total).
 * Theoretically user code may have write access to this area.
 * It is however harmless: altering the memory file descriptor
 * may disrupt the current thread work, and cause abort of the
 * user process; host may re-spawn a thread in such case. Altering
 * the current mappings will only cause the overhead when returning
 * into the context of the thread by means of extra remappings.
 */

static int memfd;

static uvlong curmaps[NSEG];

/*
 * User thread start up.
 */

void 
uthread_main(void)
{
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
		}
	}
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
