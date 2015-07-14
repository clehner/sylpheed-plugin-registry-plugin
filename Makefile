NAME = registry
LIB = $(NAME).so
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

PREFIX ?= /usr/local
PLUGINS_DIR ?= $(PREFIX)/lib/sylpheed/plugins

CFLAGS += `pkg-config --cflags gtk+-2.0` -fPIC -g \
		  -I$(PREFIX)/include/sylpheed \
		  -I$(PREFIX)/include/sylpheed/sylph
LDFLAGS += `pkg-config --libs gtk+-2.0` -L$(PREFIX)/lib \
		   -lsylpheed-plugin-0 -lsylph-0

ifdef SYLPHEED_DIR
	CFLAGS += -I$(SYLPHEED_DIR)/libsylph \
			  -I$(SYLPHEED_DIR)/src
	LDFLAGS += -L$(SYLPHEED_DIR)/src/.libs \
			   -L$(SYLPHEED_DIR)/libsylph/.libs
endif

OS := $(shell uname -s | tr A-Z a-z)
ARCH := $(shell uname -m)
ifneq (,$(findstring nt-,$(OS)))
	OS=win
endif

CFLAGS += -DPLATFORM=\""$(OS)-$(ARCH)"\"

$(LIB): $(OBJ)
	$(CC) $(LDFLAGS) -shared $^ -o $@

$(PLUGINS_DIR):
	mkdir -p $@

install: $(LIB) | $(PLUGINS_DIR)
	cp $(LIB) $(PLUGINS_DIR)

uninstall:
	rm $(PLUGINS_DIR)/$(LIB)

clean:
	rm -f *.o $(LIB)

.PHONY: clean install
