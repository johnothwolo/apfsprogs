SRCS = apfsprobe.c 				\
		../apfsck/btree.c 		\
		../apfsck/compress.c 	\
		../apfsck/crypto.c 		\
		../apfsck/dir.c			\
		../apfsck/extents.c 	\
		../apfsck/htable.c		\
        ../apfsck/inode.c 		\
	    ../apfsck/key.c 		\
		../apfsck/object.c 		\
		../apfsck/snapshot.c 	\
		../apfsck/spaceman.c 	\
		../apfsck/super.c 		\
		../apfsck/xattr.c		\

OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

DESTDIR ?= ~
BINDIR = /bin
MANDIR = /share/man/man8

SPARSE_VERSION := $(shell sparse --version 2>/dev/null)

override CFLAGS += -Wall -Wno-address-of-packed-member -fno-strict-aliasing -I$(CURDIR)/../include

apfsprobe: $(OBJS)
	@echo '  Linking...'
	@$(CC) $(CFLAGS) $(LDFLAGS) -o apfsprobe $(OBJS)
	@echo '  Build complete'

%.o: %.c
	@echo '  Compiling $<...'
	@$(CC) $(CFLAGS) -o $@ -MMD -MP -c $<
ifdef SPARSE_VERSION
	@sparse $(CFLAGS) $<
endif

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) apfsprobe
install:
	install -d $(DESTDIR)$(BINDIR)
	install -t $(DESTDIR)$(BINDIR) apfsprobe
	install -d $(DESTDIR)$(MANDIR)
	install -m 644 -t $(DESTDIR)$(MANDIR) apfsprobe.8
