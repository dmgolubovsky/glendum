/* Print Plan9 kernel image a.out header */

#include <sys/types.h>
#include <asm/byteorder.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/time.h>

typedef long long vlong;
typedef unsigned long long uvlong;

#include "a.out.h"
#include "multiboot.h"

extern char phys, end; /*top of bss, defined by linker */

typedef long (*hostcall) (int, void **);
typedef void (*kstart)(hostcall);

char *plan9ini=
	"debug=1\r\n"
	"bootfile=sdC0!9fat!9pcdisk\r\n"
	"cfs=#S/sdC0/cache\r\n"
	"*nomp=1\r\n"
	"*maxmem=33554432\r\n"
	"foo=bar\r\n"
	"distname=plan9\r\n";


#define PHYSMEM 64*1024*1024

#define NPAGE (PHYSMEM / 4096)


void *physmem(ulong size) {
	int buf = 0, fd, rc;
	void *ret;
	fd = open("/tmp/physmem", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
	if(fd == -1) {
		perror("create phys mem file");
		exit(-25);
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
	ret = mmap((void *)0x1000, PHYSMEM, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mapping phys memory");
		close(fd);
		exit(-27);
	}
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

uvlong botsp(char x) {
	return (uvlong)&x;
}

long hostsvc(int svcn, void * parms[]) {
	switch(svcn) {
		case 1:	/* kprintA: 0: string to print */
			printf("%s", (char *)parms[0]);
			fflush(stdout);
			break;
		case 2:	/* getconfAU: 0: config buffer, 1: buffer size */
			strncpy((char *)parms[0], plan9ini, (size_t)(parms[1] - 1));
			break;
		case 3:	/* kprintf: formatted output */
			vprintf(parms[0], (va_list)(parms + 1));
			fflush(stdout);
			break;
		case 4:	/* kwriteAU: 0: buffer address, 1: buffer size */
			fflush(stdout);
			write(1, (char *)parms[0], (int)parms[1]);
			break;
		case 5:	/* confmemAA: 0: return NPAGE to, 1: return UPAGES to */
			*((ulong *)parms[0]) = NPAGE;
			*((ulong *)parms[1]) = NPAGE;
			break;
		case 6:	/* hostmemAA: 0: return base to, 1: return number of pages to */
			{
				void *phmem = physmem(PHYSMEM);
				*((void **)parms[0]) = phmem;
				*((ulong *)parms[1]) = NPAGE;
			}
			break;
		case 7:	/* mallocVA: 0: requested size, 1: return allocated address to */
			{
				ulong size = (ulong)parms[0];
				void *mem = malloc(size);
				if(mem == NULL) {
					fprintf(stderr, "malloc(%lu) failed\n", size);
					exit(-28);
				}
				*((void **)parms[1]) = mem;
			}
			break;
		case 8:	/* freeA: 0: address to be freed */
			free(parms[0]);
			break;
		case 9:	/* cpufreqA: 0: address where to put CPU frequency */
			*((ulong *)parms[0]) = getmhz();
			break;
		case 10:/* hosttimeAA: 0: address to put timer hz, 1: address to put time in ns */
			{
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
				if(parms[0] != NULL)
					*((ulong *)parms[0]) = 50;
				*((uvlong *)parms[1]) = hosttm;
			}
			break;
		case 11:/* maxpoolA: 0: address to return approx kernel pool value */
			{
				uvlong end = (uvlong)parms[1];
				*((uvlong *)parms[0]) = ((uvlong)botsp(0) - end) / 2;
			}
			break;
		case 12:/*ustktopA: 0: address to return the virtual address of user stack top */
			*((ulong *)parms[0]) = (ulong)&phys - 0x1000;
			break;
		default:
			printf("unknown svcn = %d\n", svcn);
			break;
	}
	return 0;
}

int main(int argc, char **argv) {
	int i, fd, rc;
	unsigned long magbuf;
       	void *ptext, *pdata, *pload;
	long mbpos;
	multiboot_header_t mbhdr;
	unsigned long magic, entry, stext, stextp, sdata, sdatap;
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
	printf("text size %08lx, adjusted to page %08lx\n", stext, stextp);
	printf("data size %08lx, adjusted to page %08lx\n", sdata, sdatap);
	if(read(fd, (char *)ptext, stext) == -1) {
		perror("read text segment");
		close(fd);
		exit(-7);
	}
	if(mprotect((void *)mbhdr.load_addr, stextp, PROT_READ|PROT_EXEC) == -1) {
		perror("mprotect text segment");
		close(fd);
		exit(-15);
	}
	if(read(fd, (char *)pdata, sdata) == -1) {
		perror("read data segment");
		close(fd);
		exit(-8);
	}
	close(fd);
	(* (kstart)entry)(hostsvc);
	exit(0);
}
