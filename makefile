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

# needs a different DevKit for 64-bit build
all: finddupe.exe #finddupe64.exe

OBJ =

OBJECTS_FINDDUPE = $(OBJ)finddupe.o \
                $(OBJ)myglob.o \

OBJECTS_FINDDUPE64 = $(OBJECTS_FINDDUPE:.o=.o64)

%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o64 : %.c
	$(CC) -c $(CFLAGS) -m64 -o $@ $<

finddupe.exe: $(OBJECTS_FINDDUPE)
	$(LINKER) $(LDFLAGS) -o $@ $(OBJECTS_FINDDUPE)

finddupe64.exe: $(OBJECTS_FINDDUPE64)
	$(LINKER) $(LDFLAGS) -m64 -o $@ $(OBJECTS_FINDDUPE64)

# for testing only
myglob.exe: myglob.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEBUGGING -o $@ $<

clean:
	rm *.o *.o64 *.exe
