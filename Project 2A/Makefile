.SILENT:

subfiles = lab2a.c Makefile README *.png

default:
	gcc -pthread -lrt -o lab2a -g lab2a.c

clean:
	rm -rf lab2a

dist:
	tar -zcvf lab2a-104436427.tar.gz $(subfiles)
