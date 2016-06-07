.SILENT:

subfiles = SortedList.h SortedList.c lab2c.c Makefile README *.png

default:
	gcc -pthread -lrt -o lab2c SortedList.c lab2c.c

clean:
	rm -rf lab2c gmon.out

dist:
	tar -zcvf lab2c-104436427.tar.gz $(subfiles)
