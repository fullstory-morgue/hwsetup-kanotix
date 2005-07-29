prefix		= $(DESTDIR)
CC		= cc
# define BLACKLIST to avoid loading potentially dangerous modules
CFLAGS		= -fPIC
LDFLAGS		= -s
OPT_DEFINES	= -DBLACKLIST -DCHECK_CONFLICT
INC		= -I/usr/include/kudzu
LIBS		= -lkudzu -lpci
BIN_OWNER	= root
BIN_GROUP	= root

INSTALL		= install -o $(BIN_OWNER) -g $(BIN_GROUP)

.PHONY: all
all:	hwsetup

.PHONY: hwsetup
hwsetup: hwsetup.c
	$(CC) $(INC) $(CFLAGS) $(OPT_DEFINES) $(LDFLAGS) -o $@ $< $(LIBS)

.PHONY: install
install:
	[ -d $(prefix)/sbin ] || mkdir -p $(prefix)/sbin
	$(INSTALL) -m 0755 hwsetup $(prefix)/sbin/hwsetup

.PHONY: clean
clean:
	rm -f hwsetup *.o core

#dist: clean
#	cd .. ; \
#	tar -cvf - hwsetup/{Makefile,*.[ch]} | \
#	bzip2 -9 > $(HOME)/redhat/SOURCES/hwsetup.tar.bz2

