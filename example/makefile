# SPDX-License-Identifier: BSD-2-Clause

# Intel Copyright 2018

INSTALL_PATH ?= /usr/bin

SRC := rsu_client.c

CFLAGS := -I../include/ -Wall -Wsign-compare -Wpedantic -Werror -Wfatal-errors
LDFLAGS := -L../lib/ -lrsu -lz

all: rsu_client

install: rsu_client lib
	cd ../lib/; make install
	cp rsu_client ${INSTALL_PATH}/rsu_client

rsu_client: $(SRC:.c=.o) lib
	$(CROSS_COMPILE)gcc -o $@ $(SRC:.c=.o) $(LDFLAGS)

%.o : %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

lib: ../lib/librsu.so

../lib/librsu.so:
	cd ../lib/; make all

clean:
	rm -rf $(SRC:.c=.o) rsu_client
	cd ../lib/; make clean
