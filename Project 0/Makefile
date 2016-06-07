sub_files = lab0.c Makefile README *.png
.SILENT:

default: 
	gcc -o lab0 -g lab0.c
check: default allTests
	if [[ -s logfile.txt ]]; then \
	  echo "Tests Failed"; \
	  cat logfile.txt; \
	  rm -f logfile.txt; \
	else \
	  echo "All tests passed."; \
	fi 
clean:
	rm -f *.o lab0 *.txt *.tar.gz
dist: default
	tar -czf lab0-104436427.tar.gz $(sub_files)

# Smoke-tests to make sure program works. Called by make check

catchTest:
	echo | ./lab0 -s -c &>/dev/null; \
	if [[ $$? -ne 3 ]]; then \
	echo "Error: Catch argument was incorrect" >> logfile.txt; \
	fi
successINOUT:
	echo "Hello Jahan" > ti.txt; \
	./lab0 -i ti.txt -o to.txt; \
	if [[ $$? -ne 0 ]]; then \
	echo "Error: Program did not finish" >> logfile.txt; \
	fi; \
	rm -f ti.txt to.txt
openTest:
	echo "Hello Jahan" > ti.txt; \
	chmod u-r ti.txt; \
	echo | ./lab0 -i ti.txt &>/dev/null;\
	if [[ $$? -ne 1 ]]; then \
	echo "Error: File is not readable, but program doesn't catch error." >> logfile.txt; \
	fi; \
	rm -f ti.txt
creatTest:
	echo "Hello Jahan" > ti.txt; \
	touch to.txt; \
	chmod u-w to.txt; \
	./lab0 -i ti.txt -o to.txt &>/dev/null; \
	if [[ $$? -ne 2 ]]; then \
	echo "Error: File created without write permission, but program doesn't catch error." >> logfile.txt; \
	fi; \
	rm -f ti.txt to.txt
diffTest:
	echo "Hell Jahan" > ti.txt; \
	./lab0 -i ti.txt -o to.txt &>/dev/null; \
	cmp ti.txt to.txt; \
	if [[ $$? -ne 0 ]]; then \
	echo "Error: The input and output file contents are not the same." >> logfile.txt; \
	fi; \
	rm -f ti.txt to.txt
allTests: catchTest successINOUT openTest creatTest diffTest
