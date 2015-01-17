# Variables
CC := clang
CFLAGS := -std=gnu99 -DHYLISP_HV_DEBUG=1 -O0 -g3 -Wall -pthread
LDFLAGS := -lpthread -lm

SOURCEDIR := src
ETCDIR := etc
HEADERDIR := hdr
BUILDDIR := build

# File names
EXEC := hylisphv
RELEXEC := $(BUILDDIR)/$(EXEC)
SOURCES := $(SOURCEDIR)/main.c $(SOURCEDIR)/common/common.c $(SOURCEDIR)/controlpackets/demux.c $(SOURCEDIR)/controlpackets/inject.c $(SOURCEDIR)/controlpackets/parsedatagram.c $(SOURCEDIR)/controlplanes/assignments.c $(SOURCEDIR)/controlplanes/connected.c $(SOURCEDIR)/controlplanes/register.c $(SOURCEDIR)/controlplanes/sorting.c $(SOURCEDIR)/mapmessages/demux.c $(SOURCEDIR)/mapmessages/mux.c $(SOURCEDIR)/mapmessages/parsemessage.c

# Main target
all: init $(RELEXEC)

# Creating directory tree
init:
	mkdir -p $(BUILDDIR)
	rm -fr $(BUILDDIR)/*

# Compiling
$(RELEXEC): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o $(RELEXEC)

# Cleaning
clean:
	rm -fr $(BUILDDIR)

# Installing
install:
	cp -f $(RELEXEC) /usr/sbin/$(EXEC)
	chmod 755 /usr/sbin/$(EXEC)
	cp -f $(HEADERDIR)/hylisphv.h /usr/local/include/hylisphv.h
	chmod 444 /usr/local/include/hylisphv.h
	mkdir -p /var/hylisphv/sockets
	chmod 755 /var/hylisphv
	chmod 777 /var/hylisphv/sockets

# Installing service
install-service:
	cp -f $(ETCDIR)/service /etc/rc.d/hylisphv
	chmod 555 /etc/rc.d/hylisphv

# Uninstalling
uninstall:
	rm -fr /var/hylisphv
	rm -f /usr/sbin/$(EXEC)
	rm -f /usr/local/include/hylisphv.h
	rm -f /etc/rc.d/hylisphv
