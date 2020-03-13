SRC    = daemon_client.c dom.c
O := .
OBJ    = $(SRC:.c=.o)
LIBS   = -lplatform -lrt -lpthread -lsilipc
EXE    = $(O)/dom_process
SIL_DIR =/vobs/platform/linux/usr/lib/sil

CC      := $(CROSS_COMPILE)gcc
CFLAGS  = -I$(SIL_DIR)/include -I$(SIL_DIR)/ipc/include
LIBPATH = -L$(SIL_DIR)/build
LDFLAGS = -o $(EXE) $(LIBPATH) $(LIBS)
CFDEBUG = -ansi -pedantic -Wall -g -DDEBUG $(LDFLAGS)
RM      = /bin/rm -f

%.o: %.c
	 $(CC) -c $(CFLAGS) $*.c

$(EXE): $(OBJ)
	 $(CC) $(LDFLAGS) $(OBJ)

$(OBJ): $(INCL)

debug:
	$(CC) $(CFDEBUG) $(SRC)

clean:
	$(RM) $(OBJ) $(EXE) core a.out

all: $(EXE)
