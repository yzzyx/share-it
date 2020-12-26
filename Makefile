CFLAGS=$(shell pkg-config --cflags gtk+-3.0) -g -Wall
LDFLAGS=$(shell pkg-config --libs gtk+-3.0) -g
all: share-it

.PHONY: format clean

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

share-it: main.o grab_gdk.o net.o packet.o password.o buf.o handlers.o framebuffer.o
	$(CC) -o share-it $^ $(LDFLAGS)

view: view.o xcb.o packet.o
	$(CC) -o view view.o xcb.o packet.o $(LDFLAGS)

test: test_framebuffer
	./test_framebuffer

test_framebuffer: test_framebuffer.o packet.o framebuffer.o buf.o net.o
	$(CC) -o test_framebuffer $^ $(LDFLAGS)

clean:
	rm -f *.o share-it

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
