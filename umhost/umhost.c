/* Plan9 a.out kernel image loader */

#include <sys/types.h>
#include <asm/byteorder.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <asm/unistd.h>

typedef long long vlong;
typedef unsigned long long uvlong;

#include "a.out.h"
#include "multiboot.h"

extern char phys, end; /*top of bss, defined by linker */

typedef void (*kstart)();

char *plan9ini=
	"debug=1\r\n"
	"bootfile=sdC0!9fat!9pcdisk\r\n"
	"cfs=#S/sdC0/cache\r\n"
	"*nomp=1\r\n"
	"*maxmem=33554432\r\n"
	"foo=bar\r\n"
	"distname=plan9\r\n";


#define PHYSMEM 256*1024*1024
#define PHYSBASE 0x1000

#define NPAGE (PHYSMEM / 4096)

typedef struct {
	char *symname;
	void *funaddr;
} Patch;

int hwrite(char *buf, size_t size) {
	return write(1, buf, size);
}

static int memfd = -1;

int mkmemfile(void) {
	int fd = open("/tmp/physmem", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
	if(fd == -1) {
		perror("create phys mem file");
		exit(-25);
	}
	memfd = fd;
	printf("memfd: %d\n", fd);
	return fd;
}

void *physmem(ulong size) {
	int buf = 0, fd, rc;
	void *ret;
	if(memfd == -1) {
		fd = mkmemfile();
	} else {
		fd = memfd;
	}
	rc = lseek(fd, PHYSMEM, SEEK_CUR);
	if(rc == -1) {
		perror("setting phys mem size");
		close(fd);
		exit(-26);
	}
	rc = write(fd, &buf, 1);
	if(rc == -1) {
		perror("writing phys mem end");
		close(fd);
		exit(-26);
	}
	ret = mmap((void *)PHYSBASE, PHYSMEM, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mapping phys memory");
		close(fd);
		exit(-27);
	}
	memfd = fd;
	return ret;

}

int getmhz(void) {
	double mhz = 0;
	char buf[100];
	FILE *f = popen("cat /proc/cpuinfo | grep -i 'CPU MHZ' | head -n 1 | cut -d ':' -f2", "r");
	if (f == NULL) {
		perror("getmhz - popen");
		exit(-30);
	}
	memset(buf, 0, 100);
	fread(buf, 99, 1, f);
	mhz = atof(buf);
	pclose(f);
	return (int)mhz;
}

void getconf(char *buf, int size) {
	strncpy(buf, plan9ini, size - 1);
}

void hostmem(void **pmemb, ulong *pnpage) {
	void *phmem = physmem(PHYSMEM);
	*pmemb = phmem;
	*pnpage = NPAGE;
}

void confmem(ulong *pnpage, ulong *pupages) {
	*pnpage = NPAGE;
	*pupages = NPAGE;
}

ulong ustktop(void) {
	return (ulong)&phys - 0x1000;
}

void timeres(uvlong *phz, uvlong *pres) {
	struct timeval ts;
	struct timezone tz;
	uvlong hosttm;
	int rc = gettimeofday(&ts, &tz);
	if(rc == -1) {
		perror("gettimeofday");
		exit (-32);
	}
	hosttm = (uvlong)ts.tv_sec * 1000000000ull + 
		(uvlong)ts.tv_usec * 1000ull;
	if(phz != NULL)
		*phz = 50;
	*pres = hosttm;
}

void kpool(ulong *pkpl, char *kmax) {
	*pkpl = ((ulong)&pkpl - (ulong)kmax)/2;
}

int dblen(char *s) {
	int i;
	for(i = 0; ;i++) {
		if(s[i] == 0 && s [i + 1] == 0)
			return i;
	}
}

/*
 * Set up tracing by the parent process. Stop itself.
 */

void traceme(void) {
	if(ptrace(PTRACE_TRACEME) < 0) {
		perror("traceme");
		exit(-60);
	}
	kill(getpid(), SIGSTOP);
}

/*
 * Wait for a PID with WUNTRACED flag.
 */

pid_t wpid(pid_t pid, int *status) {
	return waitpid(pid, status, WUNTRACED);
}

/*
 * Table of patches. If a symbol is found with given name (.symname),
 * proper function address will be written at offset 2 from the symbol
 * to patch the code segment.
 */

Patch ptt[] = {
	(Patch){.symname = "host_alloc", .funaddr = malloc},
	(Patch){.symname = "host_free", .funaddr = free},
	(Patch){.symname = "host_write", .funaddr = hwrite},
	(Patch){.symname = "host_cpufreq", .funaddr = getmhz},
	(Patch){.symname = "kprintf", .funaddr = vprintf},
	(Patch){.symname = "host_getconf", .funaddr = getconf},
	(Patch){.symname = "host_memsize", .funaddr = hostmem},
	(Patch){.symname = "host_confmem", .funaddr = confmem},
	(Patch){.symname = "host_ustktop", .funaddr = ustktop},
	(Patch){.symname = "host_timeres", .funaddr = timeres},
	(Patch){.symname = "host_kpool", .funaddr = kpool},
	(Patch){.symname = "host_abort", .funaddr = exit},
	(Patch){.symname = "host_memfd", .funaddr = mkmemfile},
	(Patch){.symname = "host_uthread", .funaddr = fork},
	(Patch){.symname = "host_traceme", .funaddr = traceme},
	(Patch){.symname = "host_waitpid", .funaddr = wpid},
	(Patch){NULL}
};

/*
 * Check if the memory at the given address contains 0xFF, 0x25 in its first
 * two bytes. If yes, copy the function address at the offset 6.
 */

void patchcode(void *addr, void *funaddr) {
	unsigned char *caddr = addr;
	if((caddr[0] == 0xFF) && (caddr[1] == 0x25)) {
		memcpy(caddr + 6, &funaddr, sizeof(funaddr));
	}
}

/*
 * Read the symbol table from the file, patch the code segment.
 * To speed up patching, host link symbols are between __hostlink_begin and
 * __hostlink_end.
 */

void hostlink(int fd, unsigned long offsyms, unsigned long ssyms) {
	off_t savepos = lseek(fd, 0, SEEK_CUR);
	int rc, symsize, active;
	char *symname;
	char stype;
	unsigned char *symp, *symt = alloca(ssyms);
	unsigned long *addrp, addr;
	rc = lseek(fd, offsyms, SEEK_SET);
	if(rc == -1) {
		perror("seek to the symbol table");
		exit(-40);
	}
	rc = read(fd, symt, ssyms);
	if(rc != ssyms) {
		fprintf(stderr, "incomplete read of symbol table, %ld vs. %ld\n", 
				(unsigned long)rc, ssyms);
	}
	symp = symt;
	active = 0;
	while(1) {
		addrp = (unsigned long *)symp;
		addr = __be32_to_cpu(*addrp);
		stype = symp[4] - 0x80;
		symname = (char *)symp + 5;
		switch(stype) {
			case 'Z':
			case 'z':
				symsize = 5 + dblen(symname + 1) + 3;
				break;
			default:
				symsize = 5 + strlen(symname) + 1;

		}
		if((stype == 'T') && (!strcmp(symname, "__hostlink_begin")))
			active = 1;
		if(active) {
			if((stype == 'T') && (!strcmp(symname, "__hostlink_end")))
				break;
			if(stype == 'T') {
				Patch *p;
				for(p = ptt; p->funaddr != NULL; p++) {
					if(!strcmp(p->symname, symname)) {
						patchcode((void *)addr, p->funaddr);
					}
				}
			}
		}
		symp += symsize;
		if(symp - symt >= ssyms) break;
	}
	lseek(fd, savepos, SEEK_SET);
}

int main(int argc, char **argv) {
	int i, fd, rc;
	unsigned long magbuf;
       	void *ptext, *pdata, *pload;
	long mbpos;
	multiboot_header_t mbhdr;
	unsigned long magic, entry, stext, ssyms,
		      offsyms, stextp, sidata, sdata, sdatap;
	struct Exec hdr;
	if (argc < 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		exit(-1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(-2);
	}
	if(read(fd, &hdr, sizeof(hdr)) < sizeof(hdr)) {
		perror("read file header");
		close(fd);
		exit(-3);
	}
	if((magic = __be32_to_cpu(hdr.magic)) != I_MAGIC) {
		fprintf(stderr, "expected magic=%d, got %ld\n", I_MAGIC, magic);
		close(fd);
		exit(-4);
	}
	entry = __be32_to_cpu(hdr.entry);
	stext = __be32_to_cpu(hdr.text);
	sidata = __be32_to_cpu(hdr.data);
	ssyms = __be32_to_cpu(hdr.syms);
	sdata = __be32_to_cpu(hdr.data) + __be32_to_cpu(hdr.bss);
	printf("entry point at %08lx\n", entry);
	stextp = stext + (4096 - stext) % 4096;
	sdatap = sdata + (4096 - sdata) % 4096;
	magbuf = 0;
	for(i = 0; i<256; i++) {
		rc = read(fd, &magbuf, sizeof(magbuf));
		if (rc < 0) {
			perror("looking for multiboot header");
			close(fd);
			exit(-10);
		}
		if (magbuf == MULTIBOOT_HEADER_MAGIC) {
			mbpos = lseek(fd, -sizeof(magbuf), SEEK_CUR);
			printf("a.out header size %08x\nfound multiboot header at %08lx\n", 
					sizeof(hdr), mbpos);
			break;
		}
	}
	if (magbuf != MULTIBOOT_HEADER_MAGIC) {
		fprintf(stderr, "could not find multiboot header\n");
		close(fd);
		exit(-11);
	}
	if(lseek(fd, mbpos, SEEK_SET) != mbpos) {
		perror("seek to the multiboot header");
		close(fd);
		exit(-12);
	}
	if(read(fd, &mbhdr, sizeof(mbhdr)) < sizeof(mbhdr)) {
		perror("read multiboot header");
		close(fd);
		exit(-13);
	}
	if(mbhdr.magic + mbhdr.flags + mbhdr.checksum != 0) {
		fprintf(stderr, "incorrect checksum in multiboot header\n");
		close(fd);
		exit(-14);
	}
	if(!(mbhdr.flags & 0x10000)) {
		fprintf(stderr, "multiboot header does not contain address information\n");
		close(fd);
		exit(-15);
	}
	printf("end at %08lx\n", (unsigned long)&end);
	if ((unsigned long)&end > mbhdr.load_addr) {
		fprintf(stderr, "host .bss end too high, cannot load\n");
		close(fd);
		exit(-16);
	}
	printf("header_addr:   %08lx\n"
		"load_addr:     %08lx\n"
		"load_end_addr: %08lx\n"
		"bss_end_addr:  %08lx\n"
		"entry_addr:    %08lx\n",
		mbhdr.header_addr, mbhdr.load_addr, mbhdr.load_end_addr, 
		mbhdr.bss_end_addr, mbhdr.entry_addr);
	pload = mmap((void *)(mbhdr.load_addr - 0x100000), 0x100000,
			PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(pload == MAP_FAILED) {
		perror("mmap for loader area");
		close(fd);
		exit(-5);
	}
	ptext = mmap((void *)mbhdr.load_addr, stextp,
			PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(ptext == MAP_FAILED) {
		perror("mmap for text");
		close(fd);
		exit(-5);
	}
	pdata = mmap((void *)(mbhdr.load_addr + stextp), sdatap,
			PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(pdata == MAP_FAILED) {
		perror("mmap for data");
		close(fd);
		exit(-5);
	}
	printf("text at %08lx, data at %08lx\n", (unsigned long)ptext, (unsigned long)pdata);
	if(lseek(fd, sizeof(hdr), SEEK_SET) != sizeof(hdr)) {
		perror("seek to the text segment");
		close(fd);
		exit(-6);
	}
	offsyms = stext + sidata + sizeof(hdr);
	printf("text size %08lx, adjusted to page %08lx\n", stext, stextp);
	printf("data size %08lx, adjusted to page %08lx\n", sdata, sdatap);
	printf("symbol table at %08lx, size %ld\n", offsyms, ssyms);
	if(read(fd, (char *)ptext, stext) == -1) {
		perror("read text segment");
		close(fd);
		exit(-7);
	}
	hostlink(fd, offsyms, ssyms);
	if(mprotect((void *)mbhdr.load_addr, stextp, PROT_READ|PROT_EXEC) == -1) {
		perror("mprotect text segment");
		close(fd);
		exit(-15);
	}
	if(read(fd, (char *)pdata, sidata) == -1) {
		perror("read data segment");
		close(fd);
		exit(-8);
	}
	close(fd);
	(*(kstart)entry)();
	exit(0);
}
