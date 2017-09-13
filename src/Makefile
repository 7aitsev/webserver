.PHONY: all, clean

# source dirs
SRCDIR = .
CFGDIR = ./config
MNGRDIR = ./manager
SERVDIR = ./server

# output dirs
BINDIR = ../bin
OBJDIR = ../obj

# compilation flags
CC = gcc
CFLAGS = -Wall -I$(SRCDIR) -g

#SOURCES := $(shell find $(SRCDIR) -name '*.c')
#SERVDEPS = $(CFGDIR)/servconf.h
DEPS = $(CFGDIR)/conf.h

_SERVOBJ = handler.o worker.o server.o stepik.o
SERVOBJ = $(patsubst %, $(OBJDIR)/%, $(_SERVOBJ))

_MNGROBJ = config.o handler.o worker.o server.o manager.o main.o
MNGROBJ = $(patsubst %, $(OBJDIR)/%, $(_MNGROBJ))

# Match targets in SRCDIR
$(OBJDIR)/%.o: %.c
	$(CC) $< -o $@ $(CFLAGS) -c

# Match targets in CFGDIR
$(OBJDIR)/%.o: $(CFGDIR)/%.c $(CFGDIR)/%.h $(DEPS)
	$(CC) $< -o $@ $(CFLAGS) -c

# Match targets in SERVDIR
$(OBJDIR)/%.o: $(SERVDIR)/%.c $(SERVDIR)/%.h
	$(CC) $< -o $@ $(CFLAGS) -c

# Match targets in MNGRDIR
$(OBJDIR)/%.o: $(MNGRDIR)/%.c $(MNGRDIR)/%.h $(DEPS)
	$(CC) $< -o $@ $(CFLAGS) -c

$(OBJDIR)/%.o: $(SERVDIR)/%.c
	$(CC) $< -o $@ $(CFLAGS) -c

manager: $(MNGROBJ)
	$(CC) $^ -o $(BINDIR)/$@ $(CFLAGS) -pthread

server: $(SERVOBJ)
	$(CC) $^ -o $(BINDIR)/$@ $(CFLAGS) -pthread

#server: $(SERVOBJ)
#	$(CC) $^ -o $(BINDIR)/$@ $(CFLAGS) -pthread

clean:
	rm -f $(OBJDIR)/*.o $(BINDIR)/*
