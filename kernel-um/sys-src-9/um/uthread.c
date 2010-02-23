/*
 * UM-kernel user thread control.
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
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
