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

INCLUDES ?= -Ideps/$(CPU)/curl/include \
            -Ideps/$(CPU)/zlib/include \
            -Ideps/$(CPU)/openssl/include
            
CFLAGS += -Wall -O3 -fno-strict-aliasing -g -MMD -DCURL_STATICLIB $(INCLUDES)

ifdef CONFIG_MACOS
    LDFLAGS ?= -shared -framework CoreFoundation -framework CoreServices -framework SystemConfiguration
else
    LDFLAGS ?= -shared
endif

LIBS ?= deps/$(CPU)/curl/lib/libcurl.a \
        deps/$(CPU)/zlib/lib/libz.a \
        deps/$(CPU)/openssl/lib/libssl.a \
        deps/$(CPU)/openssl/lib/libcrypto.a \
        -lpthread \
        -ldl

ifdef CONFIG_WINDOWS
    CFLAGS += -DQ2ADMINCLIB=1
    CFLAGS += -Wno-unknown-pragmas
    LDFLAGS += -mconsole
    LDFLAGS += -Wl,--nxcompat,--dynamicbase
else
    CFLAGS += -fPIC -ffast-math -w -DLINUX
endif

CFLAGS += -DQ2A_COMMIT='"$(VER)"' -DQ2A_REVISION=$(REV) -DCPU='"$(CPU)"'
RCFLAGS += -DQ2A_REVISION=$(REV) -DYEAR='\"$(YEAR)\"'

HEADERS :=  game.h \
            g_admin.h \
            g_anticheat.h \
            g_ban.h \
            g_cloud.h \
            g_checkvar.h \
            g_cmd.h \
            g_crypto.h \
            g_disable.h \
            g_flood.h \
            g_http.h \
            g_init.h \
            g_json.h \
            g_local.h \
            g_log.h \
            g_lrcon.h \
            g_net.h \
            g_queue.h \
            g_spawn.h \
            g_timer.h \
            g_util.h \
            g_vote.h \
            g_vpn.h \
            g_whois.h \
            platform.h \
            profile.h \
            regex.h \
            shared.h

OBJS :=     g_admin.o \
            g_anticheat.o \
            g_ban.o \
            g_checkvar.o \
            g_cloud.o \
            g_cmd.o \
            g_crypto.o \
            g_disable.o \
            g_flood.o \
            g_http.o \
            g_init.o \
            g_json.o \
            g_libc.o \
            g_log.o \
            g_lrcon.o \
            g_main.o \
            g_net.o \
            g_queue.o \
            g_spawn.o \
            g_timer.o \
            g_util.o \
            g_vote.o \
            g_vpn.o \
            g_whois.o \
            g_zbot.o \
            g_zbotcheck.o \
            regex.o

ifdef CONFIG_SQLITE
    SQLITE_CFLAGS ?=
    SQLITE_LIBS ?= -lsqlite3
    CFLAGS += -DUSE_SQLITE=1 $(SQLITE_CFLAGS)
    LIBS += $(SQLITE_LIBS)
    OBJS += g_sqlite.o
endif

ifdef CONFIG_WINDOWS
    CPU := x86
    TARGET ?= game$(CPU)-q2admin-r$(VER).dll
    OBJS += q2admin.o
else
    LIBS += -lm
    TARGET ?= game$(CPU)-q2admin-r$(VER).so
endif

all: $(TARGET)

default: all

.PHONY: all default clean strip

# Define V=1 to show command line.
ifdef V
    Q :=
    E := @true
else
    Q := @
    E := @echo
endif

-include $(OBJS:.o=.d)

%.o: %.c $(HEADERS)
	$(E) [CC] $@
	$(Q)$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.rc
	$(E) [RC] $@
	$(Q)$(WINDRES) $(RCFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	$(E) [LD] $@
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	$(E) [CLEAN]
	$(Q)$(RM) *.o *.d $(TARGET) genkeys

strip: $(TARGET)
	$(E) [STRIP]
	$(Q)$(STRIP) $(TARGET)

genkeys:
	$(E) [CC] genkeys
	$(Q)$(CC) -o genkeys genkeys.c -lcrypto
