# =============================================================================
# Rocks'n'Diamonds Makefile (game_em)
# -----------------------------------------------------------------------------
# (c) 1995-2006 Holger Schemel <info@artsoft.org>
# -----------------------------------------------------------------------------
# Emerald Mine for X11 � 2000,2001 David Tritscher
# =============================================================================

# -----------------------------------------------------------------------------
# configuration
# -----------------------------------------------------------------------------

SRCS =	cave.c		\
	convert.c	\
	graphics.c	\
	init.c		\
	input.c		\
	main.c		\
	sound.c		\
	synchro_1.c	\
	synchro_2.c	\
	synchro_3.c	\
	tab_generate.c	\
	ulaw_generate.c

OBJS =	cave.o		\
	convert.o	\
	graphics.o	\
	init.o		\
	input.o		\
	main.o		\
	sound.o		\
	synchro_1.o	\
	synchro_2.o	\
	synchro_3.o	\
	tab_generate.o	\
	ulaw_generate.o

GAME_EM = game_em.a


# -----------------------------------------------------------------------------
# build targets
# -----------------------------------------------------------------------------

all: $(GAME_EM)

$(GAME_EM): $(OBJS)
	$(AR) cru $(GAME_EM) $(OBJS)
	$(RANLIB) $(GAME_EM)

.c.o:
	$(CC) $(PROFILING) $(CFLAGS) -c $*.c

clean:
	$(RM) $(OBJS)
	$(RM) $(GAME_EM)


# -----------------------------------------------------------------------------
# development only
# -----------------------------------------------------------------------------

depend:
	for i in $(SRCS); do $(CPP) $(CFLAGS) -M $$i; done > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
