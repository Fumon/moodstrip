BINARY = listen

OBJS = $(BINARY).o
OBJS += arduino-serial-lib.o hsl_to_rgb.o

# LIBS += -lrt -lusb-1.0

CC = gcc
LD = gcc

CFLAGS += -Wall -Wextra -g #I$(SDK)/usr/include
#CFLAGS += -ggdb3
LDFLAGS += -L$(SDK)/usr/lib -L$(TOOL)/lib -L$(TOOL)/usr/lib -Wl,-rpath=$(TOOL)/lib $(LIBS)

all: exe

exe: $(BINARY).exe

%.exe: $(OBJS) 
	$(LD) -pg -o $(*) $(OBJS) $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o
	rm -f $(BINARY)

.PHONY: exe clean
