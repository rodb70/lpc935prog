#  File:         Makefile
#  Written by:   Rod Boyce
#  e-mail:       rod@boyce.net.nz
#
#  This file is part of lpc935-prog
#
#  lpc935-prog is free software; you can redistribute it and/or modify
#  it under the terms of the Lesser GNU General Public License as
#  published by the Free Software Foundation; either version 2.1 of the
#  License, or (at your option) any later version.
#
#  lpc935-prog is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser
#  GNU General Public License for more details.
#
#  You should have received a copy of the Lesser GNU General Public
#  License along with lpc935-prog; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
#  USA
-include config.mak
CC := $(CROSS_COMPILE)gcc

LDFLAGS :=
SRC :=
SRC += ihex.c
SRC += lpc935-prog.c


ifeq ($(WINDOWS),yes)
SRC += ser_win.c
CFLAGS += -DWINDOWS
EXT := .exe
else
SRC += ser_linux.c
CFLAGS += -DLINUX
endif
LDFLAGS += -lpopt

all: lpc935-prog$(EXT)

lpc935-prog$(EXT): $(patsubst %.c,%.o, $(SRC))
	$(CC) $(LDFLAGS) -o $@ $+

.PHONY : clean
clean :
	rm $(patsubst %.c,%.o,$(SRC)) $(patsubst %.c,%.d,$(SRC)) lpc935-prog$(EXT)

%.d : %.c 
	$(CC) -MM $(INCLUDES) $(CFLAGS) $< > $*.d

%.o: %.c
	$(CC)  $(CFLAGS) -c $< -o $*.o

include $(patsubst %.c,%.d,$(SRC))
