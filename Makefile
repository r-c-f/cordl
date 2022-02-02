SRC = main.c xmem.h

all: cordl

clean:
	rm cordl

cordl: ${SRC}
	${CC} ${CFLAGS} -lcurses main.c -o cordl
