#--------------------------------
# finddupe makefile for Mingw
#--------------------------------

CC=gcc
CFLAGS=-c -O3 -g -D_UNICODE -DUNICODE
# -G3 -Ox -W3 -Zp -Zd
LINKER=gcc
LINKCON =-g

all:finddupe.exe myglob.exe

OBJ = .

OBJECTS_FINDDUPE = $(OBJ)\finddupe.o \
                $(OBJ)\myglob.o \

#$(OBJECTS_FINDDUPE): $(@B).c
#	$(CC) /Fo$(OBJ)\ $(CFLAGS) $<
.c.o:
	$(CC) $(CFLAGS) $<

finddupe.exe: $(OBJECTS_FINDDUPE)
	$(LINKER) $(LINKCON) -o $@ $(OBJECTS_FINDDUPE)

# for testing
myglob.exe: myglob.c
	$(CC) $(CFLAGS) -DDEBUGGING -o $@ $<

clean:
	rm *.o *.exe
