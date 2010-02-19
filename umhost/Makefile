all:	umhost

umhost:	umhost.c glendum.ld
	gcc -static -g -Wall -D__i386__ -o $@ glendum.ld $<

clean:
	rm -f *.o umhost

