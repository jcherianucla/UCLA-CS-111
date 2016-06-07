.SILENT:

subfiles = SortedList.h SortedList.c lab2b.c Makefile README *.png

default:
	gcc -pthread -lrt -o lab2b -g SortedList.c lab2b.c

clean:
	rm -rf lab2b

dist:
	tar -zcvf lab2b-104436427.tar.gz $(subfiles)
