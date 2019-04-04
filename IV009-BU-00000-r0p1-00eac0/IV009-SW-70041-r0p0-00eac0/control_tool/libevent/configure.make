SYSTEM ?= linux
GCC ?= $(shell which gcc)
GOD ?= $(shell which objdump)
GNM ?= $(shell which nm)
GAR ?= $(shell which ar)
GRL ?= $(shell which ranlib)

override COMMONCFGOPTS = --disable-openssl --enable-thread-support --disable-dependency-tracking --disable-libevent-install --disable-libevent-regress --disable-malloc-replacement --enable-silent-rules
override CFGOPTIONS = --host=$(TRIPLET) CC=$(GCC) OBJDUMP=$(GOD) NM=$(GNM) AR=$(GAR) RANLIB=$(GRL) CFLAGS="-w" CPPFLAGS="-w" $(COMMONCFGOPTS)

build ?= target/build

override cwd = $(shell pwd)

all: $(build)/include/event2/event-config.h

$(build)/include/event2/event-config.h: $(build)/Makefile
	make -s --directory=$(build) include/event2/event-config.h

$(build)/Makefile: Makefile.in
	@mkdir -p $(build)
	cd $(build) && $(cwd)/configure $(CFGOPTIONS)

Makefile.in: configure Makefile.am include/Makefile.am
	automake --add-missing --force-missing --copy

configure: m4/libtool.m4 configure.ac
	autoconf

m4/libtool.m4: m4/ac_backport_259_ssizet.m4 m4/acx_pthread.m4
	aclocal -I m4
	autoheader
	libtoolize

.PHONY: clean

clean:
	@rm -rf $(build)
	@rm -rf autom4te.cache
	@rm -f compile config.guess config.h.in config.sub configure depcomp
	@rm -f include/Makefile.in install-sh ltmain.sh missing Makefile.in aclocal.m4
	@rm -f m4/{libtool.m4,ltoptions.m4,ltsugar.m4,ltversion.m4,lt~obsolete.m4}
