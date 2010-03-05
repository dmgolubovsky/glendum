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

static ulong *curmap;
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
			uthreads[i].state = UIdle;
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
 * Find an user thread descriptor by the host PID. Given small number
 * of items searched over, linear search is fine. Return nil if not found.
 */

Uthread *
find_uthread(int pid)
{
	int i;
	for(i = 0; i < actthreads; i++)
		if(uthreads[i].upid == pid)
			return uthreads + i;
	return nil;
}

/*
 * Handle syscalls while in the remapping state. If the syscall
 * corresponds to exit (__NR_exit, __NR_exit_group), restore the
 * saved registers, disable host syscalls, set state to running.
 * Otherwise just allow the syscall to proceed.
 */

void
handle_remap(Uthread *uthr)
{
	ulong sysno = peek_sys(uthr->upid);
	print("handle_remap: syscall %uld\n", sysno);
	if(is_exit(sysno)) {
		poke_sys(uthr->upid, 0xFFFFFFFF);
		poke_eax(uthr->upid, uthr->mmeax);
		poke_eip(uthr->upid, uthr->mmeip);
		poke_esp(uthr->upid, uthr->mmesp);
		uthr->state = URunning;
		uthr->hsyscalls = 0;
	}
	ptrace_cont(uthr->upid, uthr->hsyscalls);
}

/*
 * Infinite loop of ptracing user threads.
 */

void
kthread_loop(void)
{
	int rc, status;
	Uthread *uthr;
	while(1) {
		rc = host_waitpid(0, &status);
		uthr = find_uthread(rc);
		print("host process %d, Plan9 process %ld, status %08x\n",
			rc, uthr?uthr->proc->pid:-1, status);
		if(!uthr) 
			continue;
		/*
		 * User thread invokes a syscall, and syscalls are allowed.
		 */
		if(is_sysc(status) && uthr->hsyscalls) {
			if(uthr->state == URemapping)
				handle_remap(uthr);
			else
				print("syscall while not in remapping state\n");
		} else if(is_segv(status) && (uthr->state == URunning)) {
			ulong eip = peek_eip(uthr->upid);
			ulong instr = peek_user(uthr->upid, (void *)eip);
			ulong sysno = peek_eax(uthr->upid) & 0x00FF;
			if((instr & 0x0000FFFF) == 0x40CD)
				print("plan9 syscall %uld at %08lx\n", sysno, eip);
		}
	}
}

/*
 * This function runs in the context of user thread. It gets
 * number of mappings from its first parameter, and the rest
 * is the number of mappings specified (each mapping is described
 * by two ulongs). First, existing mappings are removed. Next, new mappings
 * are taken into action, and stored instead of previously existing.
 * For better optimizing, it makes sense just to compare old and new
 * mappings, and remap only the difference 
 * (this is for the future improvement). Finally, the funciton calls
 * host_abort to terminate itself, and finally switch to the user code.
 */

void
do_maps(ulong nmaps, ulong map0, ...)
{
	int i;
	ulong *mapping;
	print("do_maps: nmaps=%lud\n", nmaps);
	if(curmap && curmlen) {
		for(i = 0, mapping = curmap; i < curmlen; i++, mapping+=2) {
			ulong pa = mapping[0] & (~0xFFF);
			ulong va = mapping[1] & (~0xFFF);
			ulong length = (mapping[1] & 0xFFF) << 12;
			host_mmap(memfd, pa, va, length, 0);
		}
		host_free(curmap);
		curmlen = 0;
	}
	for(i = 0, mapping = &map0; i < nmaps; i++, mapping+=2) {
		ulong pa = mapping[0] & (~0xFFF);
		ulong va = mapping[1] & (~0xFFF);
		int prot = mapping[0] & 0x07;
		ulong length = (mapping[1] & 0xFFF) << 12;
		host_mmap(memfd, pa, va, length, prot);
	}
	curmap = host_alloc(nmaps * 2 * sizeof(ulong));
	for(i = 0; i < nmaps * 2; i++)
		curmap[i] = (&map0)[i];
	curmlen = nmaps;
	host_abort(0);
}

/*
 * Remap user thread memory if needed (up->newtlb is set). Otherwise
 * just return. To remap, place the remapping information onto the
 * special stack (upstk) in the user thread address space, then
 * invoke the code in the user thread which will perform the actual
 * remapping. Remapping information is placed as a pair of ulongs
 * (since ptrace does not allow copying larger chunks of information
 * than an ulong). The ulong at index 0 contains "physical" address
 * of the mapping in bits 12 - 31, and R-W-X mask in bits 0 - 2.
 * The ulong at index 1 contains "virtual" address of the mapping in
 * bits 12 - 31, and number of pages to map in bits 0 - 11. 
 * Number of mappings is placed on the stack below the mapping information.
 */

#define M_R 1
#define M_W 2
#define M_X 4

void
umem_remap(Uthread *uthr)
{
	int i, j, k, nmap;
	uchar *usptr = upstk + UPSTKSZ - sizeof(ulong);
	if(!up->newtlb)
		return;
	uthr->state = URemapping;
	uthr->mmeax = peek_eax(uthr->upid);	/* save user's registers */
	uthr->mmeip = peek_eip(uthr->upid);	/* while remapping: they will be */
	uthr->mmesp = peek_esp(uthr->upid);	/* restored when remapping is done */

	/*
	 * Go over process' segments and find all existing mappings
	 * to be done. (NB adjacent pages have to be collapsed, won't do this
	 * now.
	 */

	for(nmap = 0, i = 0; i < NSEG; i++) {
		Segment *s = uthr->proc->seg[i];
		if(!s) 
			continue;
		for(j = 0; j < s->mapsize; j++) {
			Pte *p = s->map[j];
			ulong segperm = 0;
			if(!p)
				continue;
			switch(s->type & SG_TYPE) {
				case SG_TEXT:
					segperm = M_R | M_X;
					break;
				case SG_DATA:
				case SG_BSS:
				case SG_STACK:
					segperm = M_R | M_W;	/* also M_X? */
					break;
				default:
					print("pid %uld segment %d %d type %d\n",
						uthr->proc->pid, i, j, s->type);
			}
			for(k = 0; k < PTEPERTAB; k++) {
				Page *pg = p->pages[k];
				ulong mapping[2];
				if(!pg)
					continue;
				mapping[0] = (pg->pa & (~0xFFF)) | segperm;
				mapping[1] = (pg->va & (~0xFFF)) | 1;
				print("mapping[0]: %08lx, mapping[1]: %08lx\n",
					mapping[0], mapping[1]);
				nmap++;
				poke_user(uthr->upid, usptr, mapping[1]);
				usptr-=sizeof(ulong);
				poke_user(uthr->upid, usptr, mapping[0]);
				usptr-=sizeof(ulong);
			}
		}
	}
	poke_user(uthr->upid, usptr, nmap);
	usptr-=sizeof(ulong);
	print("total mappings %d\n", nmap);
	poke_user(uthr->upid, usptr, 0);
	/*
	 * At this point, the user stack contains a fake zero return address,
	 * number of mappings, and mappings themselves. The function that does
     * the mappings is variadic: void do_maps(ulong nmaps, ulong map0, ...).
	 * The function should not return: instead is just calls host_abort
	 * which corresponds to exit. This tells the kernel thread loop to
	 * disable host syscalls on the user thread, restore the saved
	 * registers, and resume execution.
	 */
	poke_eax(uthr->upid, 0);
	poke_esp(uthr->upid, (ulong)usptr);
	poke_eip(uthr->upid, (ulong)do_maps);
	uthr->hsyscalls = 1;
	uthr->state = URemapping;
	return;
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
	uthreads[0].state = URunning;
	umem_remap(uthreads);						/* remap memory if needed */
	ptrace_cont(uthreads[0].upid, uthreads[0].hsyscalls);
	kthread_loop();								/* go to the infinite kernel loop */
}
