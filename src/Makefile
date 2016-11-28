CC = gcc
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -O3 -g -I ./include
OBJS = wiser.o util.o token.o search.o postings.o database.o wikiload.o
DATE=$(shell date "+%Y%m%d")
DIR_NAME=wiser-${DATE}

wiser: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -l sqlite3 -l expat -l m

.c.o:
	$(CC) $(CFLAGS) -c $<

wiser.o: wiser.h util.h token.h search.h postings.h database.h wikiload.h
util.o: util.h
token.o: wiser.h token.h
search.o: wiser.h util.h token.h search.h postings.h
postings.o: wiser.h util.h postings.h database.h
database.o: wiser.h util.h database.h
wikipedia.o: wiser.h wikiload.h

.PHONY: clean
clean:
	rm *.o

dist:
	rm -rf $(DIR_NAME)
	mkdir $(DIR_NAME)
	cp -R *.c *.h include Makefile README $(DIR_NAME)
	tar cvfz $(DIR_NAME).tar.gz $(DIR_NAME)
	rm -rf $(DIR_NAME)
