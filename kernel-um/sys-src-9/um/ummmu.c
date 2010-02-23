/*
 * The UM-kernel has completely different method of memory management,
 * hence this module is to accomodate the need for that.
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"

#include	"umhost.h"

/*
 * Some stuff comes from the original mmu.c
 */

/*
 * These could go back to being macros once the kernel is debugged,
 * but the extra checking is nice to have.
 */
void*
kaddr(ulong pa)
{
print("kaddr %08lx\n", pa);
	if(pa > (ulong)-KZERO)
		panic("kaddr: pa=%#.8lux", pa);
	return (void*)pa;
//	return (void*)(pa+KZERO);
}

ulong
paddr(void *v)
{
	ulong va;
	
	va = (ulong)v;
//	if(va < KZERO)
//		panic("paddr: va=%#.8lux pc=%#p", va, getcallerpc(&v));
	return va;
//	return va-KZERO;
}

static uvlong globmmid;

void
mmuinit(void)
{
	globmmid = 0ll;
}

/*
 * Unique mmid generator.
 */

uvlong
newmmid(void)
{
	globmmid++;
	return globmmid;
}

/*
 * Initialize memory control structures.
 * Fill in the conf.mem elements (first two). The first describes
 * the chunk of memory mapped at zero till the physical memory limit
 * for processes. The second describes the kernel pool.
 */

void
meminit(void)
{
	host_confmem(&conf.npage, &conf.upages);
	conf.ustktop = host_ustktop();
	host_memsize(&palloc.mem[0].base, &palloc.mem[0].npage);
}

/*
 * Formerly in xalloc.c
 */

void
xsummary(void)
{
	print("xsummary cannot be provided in UM\n");
}

void*
xalloc(ulong size)
{
	return xallocz(size, 1);
}

void*
xallocz(ulong size, int zero)
{
	void *ret;
	print("xallocz size %uld\n", size);
//	mallocVA(size, &ret);
	ret = host_alloc(size);
print("xallocz alloc at %08p\n", ret);
	if(zero)
		memset(ret, 0, size);
	return ret;
}

void
xfree(void *p)
{
	host_free(p);
}

int
xmerge(void *, void *)
{
	return 0;
}

KMap*
kmap(Page *page)
{
print("kmap pa: %08lx va: %08lx\n", page->pa, page->va);
	return nil;
}

void
mmurelease(Proc* proc)
{
print("mmurelease pid:%uld\n", proc->pid);
	return;
}

void
mmuswitch(Proc* proc)
{
print("mmuswitch pid:%uld\n", proc->pid);
	return;
}

ulong*
mmuwalk(ulong* pdb, ulong va, int level, int create)
{
print("mmuwalk pdb=%08p, va=%08lux, level=%d, create=%d\n", pdb, va, level, create);
	return 0;
}

void
putmmu(ulong va, ulong pa, Page*)
{
print("putmmu pa: %08lx va: %08lx\n", pa, va);
	return;
}

void
kunmap(KMap *k)
{
print("kunmap: %08p\n", k);
	return;
}

/*
 * In UM kernel, if a page is created, it is already mapped within the
 * "physical" memory at its "physical" address. Just return that address.
 */

void*
tmpmap(Page *p)
{
print("tmpmap pa: %08lux va: %08lux\n", p->pa, p->va);
	return (void *)p->pa;
}

/*
 * Nothing to do here: the page does not go away.
 */

void
tmpunmap(void *v)
{
print("tmpunmap: %08p\n", v);
	return;
}

void*
vmap(ulong pa, int size)
{
print("vmap: pa:%08lx, size:%d\n", pa, size);
	return nil;
}

void
vunmap(void *v, int size)
{
print("vmap: v:%08p, size:%d\n", v, size);
	return;
}

void
checkfault(ulong, ulong)
{
}

void
checkmmu(ulong, ulong)
{
}

void
countpagerefs(ulong *ref, int prt)
{
print("countpagerefs: ref:%08p, print:%d\n", ref, prt);
}

int
vmapsync(ulong va)
{
print("vmapsync: %08lx\n", va);
	return 1;
}

void
flushmmu(void)
{
}


ulong
upaalloc(int size, int align)
{
print("upaalloc size:%d, align:%d\n", size, align);
	return 0;
}

void
upareserve(ulong pa, int size)
{
print("upareserve pa:%08lx, size:%d\n", pa, size);
	return;
}
