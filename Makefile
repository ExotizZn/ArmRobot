CC       := gcc
CFLAGS   := -O3 -Wall -lSDL2 -lSDL2_ttf -lSDL2_gfx -lm
SRC      := ./src
INCLUDE  := ./include
OBJFILES := interface.o etoiles.o toast.o utils.o
TARGET   := interface

all: $(TARGET) clean

${TARGET}: ${OBJFILES}
	${CC} ${OBJFILES} -o ${TARGET} ${CFLAGS}

interface.o :  ${SRC}/interface.c
	${CC} -c ${SRC}/interface.c

etoiles.o :  ${SRC}/etoiles.c
	${CC} -c ${SRC}/etoiles.c

toast.o :  ${SRC}/toast.c
	${CC} -c ${SRC}/toast.c

utils.o :  ${SRC}/utils.c
	${CC} -c ${SRC}/utils.c

clean:
	rm -f *~ *.o