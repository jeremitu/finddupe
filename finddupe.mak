#--------------------------------
# finddupe makefile for Win32
#--------------------------------

CC=cl /nologo
CFLAGS=-c -G3 -Ox -W3 -Zp -Zd
LINKER=link
LINKCON = /nologo

all:finddupe.exe

OBJ = .

OBJECTS_FINDDUPE = $(OBJ)\finddupe.obj \
                $(OBJ)\myglob.obj \

$(OBJECTS_FINDDUPE): $(@B).c 
    $(CC) /Fo$(OBJ)\ $(CFLAGS) $(@B).c

FINDDUPE.exe: $(OBJECTS_FINDDUPE)
    $(LINKER) $(LINKCON) -OUT:finddupe.exe $(OBJECTS_FINDDUPE)
