SRC = main.c cursutil.h xmem.h sopt.h rnd.h

all: cordl

clean:
	rm cordl

cordl: ${SRC}
	${CC} ${CFLAGS} -lcurses main.c -o cordl
