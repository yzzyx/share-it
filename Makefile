CFLAGS=$(shell pkg-config --cflags gtk+-3.0) -g -Wall
LIBS=$(shell pkg-config --libs gtk+-3.0) -g
PKGCONFIG = $(shell which pkg-config)
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)
SRC = main.c app.c appwin.c preferences.c session.c screenshare.c grab_gdk.c net.c packet.c password.c buf.c handlers.c framebuffer.c
BUILT_SRC = resources.c
OBJS = $(BUILT_SRC:.c=.o) $(SRC:.c=.o)

all: share-it

.PHONY: format clean

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

resources.c: shareit.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies shareit.gresource.xml)
	$(GLIB_COMPILE_RESOURCES) shareit.gresource.xml --target=$@ --sourcedir=. --generate-source

shareit.gschema.valid: shareit.gschema.xml
	$(GLIB_COMPILE_SCHEMAS) --strict --dry-run --schema-file=$< && mkdir -p $(@D) && touch $@

gschemas.compiled: shareit.gschema.valid
	$(GLIB_COMPILE_SCHEMAS) .

share-it: $(OBJS) gschemas.compiled
	$(CC) -o share-it $(OBJS) $(LIBS)

view: view.o xcb.o packet.o
	$(CC) -o view view.o xcb.o packet.o $(LDFLAGS)

test: test_framebuffer
	./test_framebuffer

test_framebuffer: test_framebuffer.o packet.o framebuffer.o buf.o net.o
	$(CC) -o test_framebuffer $^ $(LDFLAGS)

server: server.o buf.o packet.o framebuffer.o
	$(CC) -o server $^ $(LDFLAGS)

clean:
	rm -f shareit.gschema.valid
	rm -f gschemas.compiled
	rm -f $(BUILT_SRC)
	rm -f $(OBJS)
	rm -f share-it

format:
	astyle \
		--style=attach \
		--indent=spaces=4 \
		--pad-comma \
		--pad-header \
		--align-pointer=name \
		--align-reference=name \
		--attach-return-type \
		--attach-return-type-decl \
		--mode=c \
		-r "*.c,*.h" \
		--suffix=none \
		--ignore-exclude-errors \
		--exclude=include \
		--exclude=build \
		--exclude=cmake-build-debug
