# Makefile

CC=gcc
CFLAGS_DBG=-D_REENTRANT -DDBG -g -ggdb -O0 -Wall
CFLAGS=-D_REENTRANT -Wall

all: talker_dgram

talker_dgram: talker_dgram.c
	${CC} ${CFLAGS_DBG} talker_dgram.c -o talker_dgram

clean:
	rm -f ${all}

