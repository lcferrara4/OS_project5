virtmem: main.o page_table.o disk.o program.o
	/usr/bin/gcc main.o page_table.o disk.o program.o -o virtmem

main.o: main.c
	/usr/bin/gcc -Wall -g -c main.c -o main.o

page_table.o: page_table.c
	/usr/bin/gcc -Wall -g -c page_table.c -o page_table.o

disk.o: disk.c
	/usr/bin/gcc -Wall -g -c disk.c -o disk.o

program.o: program.c
	/usr/bin/gcc -Wall -g -c program.c -o program.o


clean:
	rm -f *.o virtmem
