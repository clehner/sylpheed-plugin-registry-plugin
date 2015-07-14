NAME = registry
LIB = $(NAME).so

MSGFMT=msgfmt --verbose
MSGMERGE=msgmerge
XGETTEXT=xgettext

PREFIX ?= /usr/local
PLUGINS_DIR ?= $(PREFIX)/lib/sylpheed/plugins
LOCALE_DIR ?= $(PREFIX)/share/locale

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
PO = $(wildcard po/*.po)
MO = $(PO:%.po=%.mo)
POT = po/$(NAME).pot
DIRS = $(PLUGINS_DIR) $(LOCALE_DIR) $(PO:po/%.po=$(LOCALE_DIR)/%/LC_MESSAGES)

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

all: $(LIB) $(PO)

$(LIB): $(OBJ)
	$(CC) $(LDFLAGS) -shared $^ -o $@

pot: $(POT)
$(POT): $(SRC)
	$(XGETTEXT) -k_ -o $@ $<

po: $(PO)
po/%.po: $(POT)
	if [ -f $@ ]; then $(MSGMERGE) $@ $< -o $@; else cp $< $@; fi

mo: $(MO)
%.mo: %.po
	$(MSGFMT) --check --statistics -o $@ $<

$(DIRS):
	mkdir -p $@

install: install-lib install-locale

install-lib: $(LIB) | $(PLUGINS_DIR)
	cp $< $|

install-locale: $(MO:po/%.mo=install-locale-%)
install-locale-%: po/%.mo | $(LOCALE_DIR)/%/LC_MESSAGES
	cp $< $|/$(NAME).mo

uninstall:
	rm $(PLUGINS_DIR)/$(LIB)

clean:
	rm -f *.o $(LIB)

.PHONY: clean install
