CPP = g++
CFLGA = -lm -g -Wall -Wsign-compare -Wformat  -std=c++0x 

SRC=hypc.cc sp_tree.cc
OBJ=$(SRC:.cc=.o)


%.o: %.cc
	${CPP} ${CFLGA} -c $^ -o $@ 

hypc: ${OBJ}
	${CPP} ${CFLGA} -o hypc ${OBJ} 

all: hypc

clean: 
	rm -f *.o
	rm -f hypc
