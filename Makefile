NAME = registry
LIB = $(NAME).so

PLUGIN_NAME=Sylpheed Plug-in Registry
PLUGIN_VERSION=0.1.2
MSGID_BUGS_ADDRESS=msgs.cel@celehner.com

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
CVARS = PLUGIN_NAME PLUGIN_VERSION PLATFORM

OS := $(shell uname -s | tr A-Z a-z)
ARCH := $(shell uname -m)
ifneq (,$(findstring nt-,$(OS)))
	OS=win
endif
PLATFORM = $(OS)-$(ARCH)

CFLAGS += `pkg-config --cflags gtk+-2.0` -fPIC -g \
		  -I$(PREFIX)/include/sylpheed \
		  -I$(PREFIX)/include/sylpheed/sylph \
		  $(foreach var,$(CVARS),-D$(var)="\"$($(var))\"")
LDFLAGS += `pkg-config --libs gtk+-2.0` -L$(PREFIX)/lib \
		   -lsylpheed-plugin-0 -lsylph-0

ifdef SYLPHEED_DIR
	CFLAGS += -I$(SYLPHEED_DIR)/libsylph \
			  -I$(SYLPHEED_DIR)/src
	LDFLAGS += -L$(SYLPHEED_DIR)/src/.libs \
			   -L$(SYLPHEED_DIR)/libsylph/.libs
endif

all: $(LIB)

$(LIB): $(OBJ)
	$(CC) $(LDFLAGS) -shared $^ -o $@

$(POT): $(SRC)
	$(XGETTEXT) -k_ \
		--package-name="$(PLUGIN_NAME)" \
		--package-version="$(PLUGIN_VERSION)" \
		--msgid-bugs-address="$(MSGID_BUGS_ADDRESS)" \
		-o $@ $<

%.mo: %.po
	$(MSGFMT) --check --statistics -o $@ $<

update-po: $(POT) $(MO:po/%.mo=update-po-%)
update-po-%: po/%.po
	$(MSGMERGE) -U $< $(POT)

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
	rm -f $(LIB) $(OBJ) $(MO)

.PHONY: clean install update-po
