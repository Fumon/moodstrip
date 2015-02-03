BINARY = listen

OBJS = $(BINARY).o
OBJS += arduino-serial-lib.o

# LIBS += -lrt -lusb-1.0

CC = gcc
LD = gcc

CFLAGS += -Os -Wall -Wextra -I$(SDK)/usr/include
# CFLAGS += -ggdb3
LDFLAGS += -L$(SDK)/usr/lib -L$(TOOL)/lib -L$(TOOL)/usr/lib -Wl,-rpath=$(TOOL)/lib $(LIBS)

all: exe

exe: $(BINARY).exe

%.exe: $(OBJS) 
	$(LD) -o $(*) $(OBJS) $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o
	rm -f $(BINARY)

.PHONY: exe clean
