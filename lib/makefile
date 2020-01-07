# SPDX-License-Identifier: BSD-2-Clause

# Intel Copyright 2018

INSTALL_PATH ?= /usr/lib
LIBRSU_VER = 0

SRC := $(wildcard *.c)

CFLAGS := -I../include/ -fPIC -fPIE -Wall -Wsign-compare -Wpedantic -Werror -Wfatal-errors
CFLAGS += -fstack-protector-strong
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security

LDFLAGS := -shared
LDFLAGS += -z noexecstack
LDFLAGS += -z relro -z now

all: librsu.so

install: librsu.so
	cp librsu.so $(INSTALL_PATH)/librsu.so.$(LIBRSU_VER)
	ln -s $(INSTALL_PATH)/librsu.so.$(LIBRSU_VER) $(INSTALL_PATH)/librsu.so

librsu.so: $(SRC:.c=.o)
	$(CROSS_COMPILE)gcc $(LDFLAGS) -o $@ $(SRC:.c=.o)

%.o : %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -DLIBRSU_VER=$(LIBRSU_VER) -fPIC -c $< -o $@

clean:
	rm -rf $(SRC:.c=.o) librsu.so
