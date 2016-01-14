CPP = gcc
CFLGA = -g -Wall -Wsign-compare -Wformat -O2

SRC=pc.c pc.h ss.c
OBJ=$(SRC:.c=.o)


%.o: %.c
	${CPP} ${CFLGA} -c $^ -o $@ 

hypc: ${OBJ}
	${CPP} ${CFLGA} -o realpc ${OBJ} -lm -lrt

all: realpc

clean: 
	rm -f *.o
	rm -f realpc
