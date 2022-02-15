SRC = main.c xmem.h sopt.h rnd.h

install: all
	install -D cordl ${PREFIX}/bin/cordl

all: cordl

clean:
	rm cordl

cordl: ${SRC}
	${CC} ${CFLAGS} -lcurses main.c -o cordl
