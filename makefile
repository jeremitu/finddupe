#--------------------------------
# finddupe makefile for Mingw
# Needs mingw-w64 for Unicode https://sourceforge.net/p/mingw-w64/wiki2/Unicode%20apps/
#--------------------------------

# comment out to disable
Unicode=1
#Debug=1


CC=gcc
CFLAGS=-O3 -Wall
LINKER=gcc
LDFLAGS=


ifdef Unicode
  CFLAGS += -D_UNICODE -DUNICODE
  LDFLAGS += -municode
endif

ifdef Debug
  CFLAGS += -g
  LDFLAGS += -g
endif


all:finddupe.exe myglob.exe

OBJ = .

OBJECTS_FINDDUPE = $(OBJ)\finddupe.o \
                $(OBJ)\myglob.o \

.c.o:
	$(CC) -c $(CFLAGS) $<

finddupe.exe: $(OBJECTS_FINDDUPE)
	$(LINKER) $(LDFLAGS) -o $@ $(OBJECTS_FINDDUPE)

# for testing
myglob.exe: myglob.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEBUGGING -o $@ $<

clean:
	rm *.o *.exe
