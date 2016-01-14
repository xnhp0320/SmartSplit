CPP = g++
CFLGA = -lm -g -Wall -Wsign-compare -Wformat -std=c++0x

SRC=hypc.cc sp_tree.cc rtrie.cc
OBJ=$(SRC:.cc=.o)


%.o: %.cc
	${CPP} ${CFLGA} -c $^ -o $@ 

sp: ${OBJ}
	${CPP} ${CFLGA} -o sp ${OBJ} 

all: sp 

clean: 
	rm -f *.o
	rm -f sp
