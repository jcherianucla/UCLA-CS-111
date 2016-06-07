.SILENT:
sub_files = client.c server.c Makefile my.key README
default: client server

client:
	gcc -lmcrypt -pthread -o client -g client.c

server:
	gcc -lmcrypt -pthread -o server -g server.c

clean:
	rm -rf client server 

dist:
	tar -zcvf lab1b-104436427.tar.gz $(sub_files) 
