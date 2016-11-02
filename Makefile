ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS := $(shell pkg-config --libs glib-2.0)

ifndef REV
    REV := $(shell git rev-list HEAD | wc -l)
    #REV := $(shell echo $$((181+$REV1)))
endif

ifndef VER
    VER := r$(REV)-$(shell git rev-parse --short HEAD)-pf
endif

#CFLAGS = -O -g -DNDEBUG -DLINUX -Dstricmp=Q_stricmp -fPIC
CFLAGS = -ffast-math -O3 -w -DGAME_INCLUDE -DLINUX -fPIC $(GLIB_CFLAGS)
CFLAGS += -DOPENTDM_VERSION='"$(VER)"' -DOPENTDM_REVISION=$(REV)
LDFLAGS = -S $(GLIB_LDFLAGS)
ORIGDIR=src

OBJS = ra_main.o fopen.o g_main.o md4.o regex.o zb_ban.o zb_acexcp.o zb_hashl.o zb_checkvar.o zb_clib.o zb_cmd.o zb_disable.o zb_flood.o zb_init.o zb_log.o zb_lrcon.o zb_msgqueue.o zb_spawn.o zb_util.o zb_vote.o zb_zbot.o zb_zbotcheck.o

game$(ARCH)-q2admin-$(VER).so: $(OBJS)
	ld -lcurl -lm -shared -o $@ $(OBJS) $(LDFLAGS)
	chmod 0755 $@
	cp $@ game$(ARCH).so
	#ldd $@

clean: 
	/bin/rm -f $(OBJS) game*.so

$*.o: $*.c
	$(CC) $(CFLAGS) -c $*.c

$*.c: $(ORIGDIR)/$*.c
	tr -d '\015' < $(ORIGDIR)/$*.c > $*.c

$*.h: $(ORIGDIR)/$*.h
	tr -d '\015' < $(ORIGDIR)/$*.h > $*.h

# DO NOT DELETE

ra_main.o: g_local.h q_shared.h game.h
fopen.o: g_local.h q_shared.h game.h
g_main.o: g_local.h q_shared.h game.h
md4.o: g_local.h q_shared.h game.h
regex.o: g_local.h q_shared.h game.h
zb_ban.o: g_local.h q_shared.h game.h
zb_acexcp.o: g_local.h q_shared.h game.h
zb_hashl.o: g_local.h q_shared.h game.h
zb_checkvar.o: g_local.h q_shared.h game.h
zb_clib.o: g_local.h q_shared.h game.h
zb_cmd.o: g_local.h q_shared.h game.h
zb_disable.o: g_local.h q_shared.h game.h
zb_flood.o: g_local.h q_shared.h game.h
zb_init.o: g_local.h q_shared.h game.h
zb_log.o: g_local.h q_shared.h game.h
zb_lrcon.o: g_local.h q_shared.h game.h
zb_msgqueue.o: g_local.h q_shared.h game.h
zb_spawn.o: g_local.h q_shared.h game.h
zb_util.o: g_local.h q_shared.h game.h
zb_vote.o: g_local.h q_shared.h game.h
zb_zbot.o: g_local.h q_shared.h game.h
zb_zbotcheck.o: g_local.h q_shared.h game.h
