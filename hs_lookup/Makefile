COMPILER = gcc
CCFLAGS = 

all: hs

hs: hs.o
	${COMPILER} ${CCFLAGS} hs.o -o hs -lrt

hs.o: hs.c hs.h
	${COMPILER} ${CCFLAGS} -c -g hs.c 

clean: 
	rm -rf *.o hs 

