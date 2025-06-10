CC       := gcc
CFLAGS   := -Wall -lSDL2 -lSDL2_ttf -lSDL2_gfx -lm
SRC      := ./src
INCLUDE  := ./include
OBJFILES := interface.o
TARGET   := interface

all: $(TARGET) clean

${TARGET}: ${OBJFILES}
	${CC} ${OBJFILES} -o ${TARGET} ${CFLAGS}

interface.o :  ${SRC}/interface.c
	${CC} -c ${SRC}/interface.c

clean:
	rm -f *~ *.o