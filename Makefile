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


SRC :=
SRC += ihex.c
SRC += lpc935-prog.c


# If building for windows
ifeq ($(WINDOWS),yes)
SRC += ser_win.c
CFLAGS += -g -DWINDOWS -I popt
# It would seem that I need to link this here to make it work under 
# windows.  How weird
LOCAL_LIBS += popt/libpopt.a
EXT := .exe
else
SRC += ser_linux.c
CFLAGS += -g -DLINUX
LDFLAGS += -lpopt
endif
LDFLAGS += -g

# New improved extra sexy verbose mode
V ?= 0
ifeq ($(V),1)
OUTPUT := > /dev/null
else
MAKEFLAGS += -s
endif


all: lpc935-prog$(EXT)

lpc935-prog$(EXT): $(patsubst %.c,%.o, $(SRC))
	@echo "Linking   : $@" $(OUTPUT)
	$(CC) $(LDFLAGS) -o $@ $+ $(LOCAL_LIBS)

.PHONY : clean
clean :
	@echo "Cleaning" $(OUTPUT)
	rm -rf $(patsubst %.c,%.o,$(SRC)) $(patsubst %.c,%.d,$(SRC)) lpc935-prog$(EXT) *~

%.o: %.c
	@echo "Compiling : $(notdir $<)" $(OUTPUT)
	$(CC) $(CFLAGS) -c -MD $< -o $@

# Do auto dependencies like http://make.paulandlesley.org/autodep.html
-include $(patsubst %.c,%.d,$(SRC))
