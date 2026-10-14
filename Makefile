# Override defaults in the file named ".config" in the same directory as this
# Makefile.
#
# Sources live in src/. Everything the build produces (objects, dependency
# files and the library) goes in build-<cpu>/ (eg build-x86_64), so
# "make clean" just removes that directory.
#
# For verbose output, to see the exact command used for building each at each
# step, define the "V" environment variable. Example: $ V=1 make
#
# Dependencies:
#  - curl     (for http downloading configs/etc)
#  - openssl  (for cloudadmin crypto and https)
#  - sqlite   (for IP context lookups (vpn/asn)
#  - zlib     (required by openssl)
#
# If the build setup is not changed, pre-compiled bundled dependencies will be
# used instead of shared libraries on the build system. The binary will still
# be dynamically linked, but the required libraries will be baked in. This
# results in the final q2admin binary being rather large (~8-10MB). This is not
# an issue on modern hardware with large amounts of ram. If you're running
# multiple gameservers on the same hardware and want to save some memory, you
# can link with the shared libraries on the host system instead by adding the
# following to your .config file:
#
#    SHARED_DEPS=1
#


-include .config

ifndef CPU
    CPU := $(shell uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/ -e s/sun4u/sparc64/ -e s/aarch64/arm64/ -e s/armv7l/arm/ -e s/sa110/arm/ -e s/alpha/axp/)
endif

ifndef REV
    REV := $(shell git rev-list HEAD | wc -l | tr -d " ")
endif

ifndef VER
    VER := $(REV)~$(shell git rev-parse --short HEAD)
endif

ifndef YEAR
    YEAR := $(shell date +%Y)
endif

CC ?= gcc
LD ?= ld
WINDRES ?= windres
STRIP ?= strip
RM ?= rm -f
PYTHON ?= python3

# Assuming linux at this point, update later for windows/mac
ifndef SHARED_DEPS
    INCLUDES ?= -Ideps/$(CPU)/curl/include \
                -Ideps/$(CPU)/zlib/include \
                -Ideps/$(CPU)/openssl/include \
                -Ideps/$(CPU)/sqlite/include
    CFLAGS += -Wall -O3 -fno-strict-aliasing -g -MMD -DCURL_STATICLIB $(INCLUDES)
    LDFLAGS ?= -shared
    LIBS ?=     deps/$(CPU)/curl/lib/libcurl.a \
                deps/$(CPU)/zlib/lib/libz.a \
                deps/$(CPU)/openssl/lib/libssl.a \
                deps/$(CPU)/openssl/lib/libcrypto.a \
                deps/$(CPU)/sqlite/lib/libsqlite3.a \
                -lpthread \
                -ldl
else
    INCLUDES ?= -I/usr/include
    CFLAGS += -Wall -O3 -fno-strict-aliasing -g -MMD
    LDFLAGS ?= -shared
    LIBS ?= -lcurl -lz -lssl -lcrypto -lsqlite3 -lpthread -ldl
endif

ifdef CONFIG_MACOS
    LDFLAGS += -framework CoreFoundation \
               -framework CoreServices \
               -framework SystemConfiguration
endif

ifdef CONFIG_WINDOWS
    CC = i686-w64-mingw32-gcc
    STRIP = i686-w64-mingw32-strip
    WINDRES = i686-w64-mingw32-windres
    OBJCOPY = i686-w64-mingw32-objcopy
    # Some of the prebuilt libraries in deps/win32 carry DWARF 5 debug
    # sections the MinGW linker doesn't know how to place. It puts them ahead
    # of .text, which leaves the DLL an invalid image that Windows (and Wine)
    # refuse to load, so they're removed after linking. The rest of the debug
    # info is kept.
    POSTLINK = $(OBJCOPY) --remove-section=.debug_loclists \
               --remove-section=.debug_rnglists \
               --remove-section=.debug_line_str $@
    CFLAGS += -DQ2ADMINCLIB=1 -static -static-libgcc -DCURL_STATICLIB
    CFLAGS += -Wno-unknown-pragmas
    LDFLAGS += -mconsole
    LDFLAGS += -Wl,--nxcompat,--dynamicbase
    LIBS =  deps/win32/lib/libws2_32.a \
            deps/win32/lib/libcurl.a \
            deps/win32/lib/libcrypto.a \
            deps/win32/lib/libcrypt32.a \
            deps/win32/lib/libz.a \
            deps/win32/lib/libssl.a \
            deps/win32/lib/libssh2.a \
            deps/win32/lib/libidn2.a \
            deps/win32/lib/libssp.a \
            deps/win32/lib/libiconv.a \
            deps/win32/lib/libintl.a \
            deps/win32/lib/libwldap32.a \
            deps/win32/lib/libgdi32.a \
            deps/win32/lib/libcrypto.a \
            deps/win32/lib/libcrypt32.a \
            deps/win32/lib/libws2_32.a \
            deps/win32/lib/libSDL2main.a \
            deps/win32/lib/libmingw32.a \
            deps/win32/lib/libdl.a \
            deps/win32/lib/libsqlite3.a \
            -static -static-libgcc \
            -lpthread
    INCLUDES = -Ideps/win32/include \
               -I/usr/i686-w64-mingw32/sys-root/mingw/include
    # Vista or later, for inet_pton()/inet_ntop() from ws2tcpip.h. MinGW
    # otherwise targets XP and leaves them undeclared.
    CFLAGS = -Wall -O3 -fno-strict-aliasing -g -MMD -DCURL_STATICLIB -D_WIN32_WINNT=0x0600 $(INCLUDES)
else
    # No -w here. It used to sit at the end of this line, where it cancelled
    # the -Wall set above and hid every warning in the build - including the
    # ones that turned out to be real bugs: a player-controlled printf format
    # string, an out-of-bounds write into logFiles[], and a ban check that
    # could never fire.
    #
    # Two groups are silenced because they are not about this code:
    # OpenSSL deprecates most of the RSA/EVP API we call, and clang does not
    # know the -Wstringop-truncation pragma g_http.c uses for GCC.
    CFLAGS += -fPIC -ffast-math -DLINUX
    CFLAGS += -Wno-deprecated-declarations -Wno-unknown-warning-option
endif

CFLAGS += -DQ2A_COMMIT='"$(VER)"' -DQ2A_REVISION=$(REV) -DCPU='"$(CPU)"'
RCFLAGS += -DQ2A_REVISION=$(REV) -DYEAR='\"$(YEAR)\"'

SRCDIR := src

HEADERS :=  game.h \
            g_admin.h \
            g_anticheat.h \
            g_ban.h \
            g_cloud.h \
            g_checkvar.h \
            g_client.h \
            g_cmd.h \
            g_crypto.h \
            g_disable.h \
            g_flood.h \
            g_http.h \
            g_init.h \
            g_ip.h \
            g_json.h \
            g_local.h \
            g_log.h \
            g_lrcon.h \
            g_net.h \
            g_protocol.h \
            g_queue.h \
            g_regex.h \
            g_signal.h \
            g_spawn.h \
            g_timer.h \
            g_util.h \
            g_vote.h \
            g_vpn.h \
            g_whois.h \
            platform.h \
            profile.h \
            shared.h

OBJS :=     g_admin.o \
            g_anticheat.o \
            g_ban.o \
            g_checkvar.o \
            g_client.o \
            g_cloud.o \
            g_cmd.o \
            g_crypto.o \
            g_disable.o \
            g_flood.o \
            g_http.o \
            g_init.o \
            g_ip.o \
            g_json.o \
            g_libc.o \
            g_log.o \
            g_lrcon.o \
            g_main.o \
            g_net.o \
            g_protocol.o \
            g_queue.o \
            g_regex.o \
            g_signal.o \
            g_spawn.o \
            g_timer.o \
            g_util.o \
            g_vote.o \
            g_vpn.o \
            g_whois.o

ifdef CONFIG_WINDOWS
    EXT ?= dll
    CPU := x86
    TARGET ?= game$(CPU)-q2admin-r$(VER).$(EXT)
    OBJS += q2admin.o
else
    EXT ?= so
    LIBS += -lm
    TARGET ?= game$(CPU)-q2admin-r$(VER).$(EXT)
endif

# Everything the build produces (objects, dependency files and the library)
# goes in one directory per architecture, so "make clean" only has to remove
# that directory. Set after the Windows block above, which changes CPU.
BUILDDIR ?= build-$(CPU)

HEADERS := $(addprefix $(SRCDIR)/,$(HEADERS))
OBJS := $(addprefix $(BUILDDIR)/,$(OBJS))
OUTPUT := $(BUILDDIR)/$(TARGET)

all: $(OUTPUT)

default: all

.PHONY: all default clean strip genkeys example-cfg

# Define V=1 to show command line.
ifdef V
    Q :=
    E := @true
else
    Q := @
    E := @echo
endif

-include $(OBJS:.o=.d)

$(BUILDDIR):
	$(Q)mkdir -p $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(BUILDDIR)
	$(E) [CC] $@
	$(Q)$(CC) -c $(CFLAGS) -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.rc | $(BUILDDIR)
	$(E) [RC] $@
	$(Q)$(WINDRES) $(RCFLAGS) -o $@ $<

$(OUTPUT): $(OBJS)
	$(E) [LD] $@
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
ifdef POSTLINK
	$(Q)$(POSTLINK)
endif

clean:
	$(E) [CLEAN] $(BUILDDIR)
	$(Q)$(RM) -r $(BUILDDIR)

strip: $(OUTPUT)
	$(E) [STRIP] $(OUTPUT)
	$(Q)$(STRIP) $(OUTPUT)

genkeys: $(BUILDDIR)/genkeys

$(BUILDDIR)/genkeys: $(SRCDIR)/genkeys.c | $(BUILDDIR)
	$(E) [CC] $@
	$(Q)$(CC) -o $@ $< -lcrypto

# runtime-config/example.cfg lists the config options an operator is likely to
# change - the ones whose q2aCommands[] entry sets "common" - with the default
# each takes. It is generated from that table rather than maintained by hand,
# so it has to be regenerated whenever a command is added, renamed or removed.
# It writes into the source tree, so it is deliberately NOT part of "all" - run
# it yourself and commit the result.
#
# For every config option rather than just the common ones, run the generator
# directly with --all and send it somewhere else:
#
#     python3 tools/gen-example-cfg.py --all /tmp/everything.cfg
#
# The generator also warns about settings in runtime-config/*.cfg that match no
# command and are therefore silently ignored by the real parser.
EXAMPLECFG := runtime-config/example.cfg

example-cfg: $(EXAMPLECFG)

$(EXAMPLECFG): tools/gen-example-cfg.py $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/*.h)
	$(E) [GEN] $@
	$(Q)$(PYTHON) $< $@

# Unit tests. Each tests/test_*.c becomes its own program, linked against the
# real objects from $(BUILDDIR) so the tests exercise shipped code rather than
# a copy. tests/stub_engine.c stands in for the engine and the wrapped mod.
#
#     make test              build and run them all
#     make test TESTS=net    just tests/test_net.c
#
# Only code that does not need a live engine is testable this way; see the
# comment at the top of tests/stub_engine.c for what is faked and what is not.
TESTDIR  := tests
TESTS    ?= $(patsubst $(TESTDIR)/test_%.c,%,$(wildcard $(TESTDIR)/test_*.c))
TESTBINS := $(addprefix $(BUILDDIR)/test_,$(TESTS))

# Objects every test links. g_util.o supplies the string helpers, g_regex.o
# the pattern matching that identifyClient() uses.
TESTOBJS := $(BUILDDIR)/g_net.o $(BUILDDIR)/g_protocol.o \
            $(BUILDDIR)/g_util.o $(BUILDDIR)/g_regex.o $(BUILDDIR)/g_log.o \
            $(BUILDDIR)/stub_engine.o

.PHONY: test
test: $(TESTBINS)
	$(Q)fail=0; for t in $(TESTBINS); do \
	    echo "=== $$t ==="; $$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo; echo "TESTS FAILED"; exit 1; fi

$(BUILDDIR)/stub_engine.o: $(TESTDIR)/stub_engine.c $(HEADERS) | $(BUILDDIR)
	$(E) [CC] $@
	$(Q)$(CC) -c $(CFLAGS) -I$(SRCDIR) -I$(TESTDIR) -o $@ $<

$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(TESTOBJS) $(HEADERS) | $(BUILDDIR)
	$(E) [CC] $@
	$(Q)$(CC) $(CFLAGS) -I$(SRCDIR) -I$(TESTDIR) -o $@ $< $(TESTOBJS) $(LIBS)
