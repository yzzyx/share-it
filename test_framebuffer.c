#include <stdio.h>
#include <stdlib.h>
#include "shareit.h"
#include "framebuffer.h"
#include "net.h"
#include "packet.h"
#include "compress.h"

#define ASSERT(x, ...) if (!(x)) { fprintf(stderr, "error: "); fprintf(stderr, __VA_ARGS__); putc('\n', stderr); return 1;}

int show_image(GdkPixbuf *pixbuf);

static int png2screenbuf(char *filename, uint32_t *output, int width, int height) {
    GError *err = NULL;
    GdkPixbuf *pixbuf;
    pixbuf = gdk_pixbuf_new_from_file(filename, &err);
    ASSERT(pixbuf != NULL, "could not load file: %s", err->message);

    guchar *pixels;
    guint row_stride;
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    row_stride = gdk_pixbuf_get_rowstride(pixbuf);

    // pixels contain RGB (but no alpha), so we'll have to convert it to match
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            output[x + y * width] =
                (pixels[x * 3 + y * row_stride] & 0xff) |
                (pixels[x * 3 + y * row_stride + 1] << 8) |
                (pixels[x * 3 + y * row_stride + 2] << 16) |
                0xff000000;
        }
    }
    g_object_unref(pixbuf);
    return 0;
}

int check_update(framebuffer_update_t *update, int expected_pixelcount) {
    int i, x, y;
    int updated_pixels = 0;
    uint8_t *pixels = calloc(640*480, sizeof(uint8_t));

    for (i = 0; i < update->n_rects; i++) {
        framebuffer_rect_t  *rect = update->rects[i];

        ASSERT(rect->width > 0, "rect %d has no width", i);
        ASSERT(rect->height > 0, "rect %d has no height", i);
        for (x = rect->xpos; x < rect->xpos+rect->width && x < 640; x++ ) {
            for (y = rect->ypos; y < rect->ypos+rect->height && y < 480; y++ ) {
                pixels[x + y *640] = 1;
            }
        }
    }

    for (x = 0; x < 640; x++ ) {
        for (y = 0; y < 480; y++ ) {
            updated_pixels += pixels[x + y *640];
        }
    }
    // 448


    ASSERT(updated_pixels == expected_pixelcount, "expected %d pixels to be updated, but got %d", expected_pixelcount, updated_pixels);
    return 0;
}

int main (int argc, char *argv[]) {
    shareit_app_t app;
    framebuffer_update_t *update;
    int is_updated;
    int ret;
    int show = 0;

    if (argc == 2 && strcmp(argv[1], "--show") == 0) {
        argc--;
        argv++;
        show = 1;
        gtk_init(&argc, &argv);
    }

    app.width = 640;
    app.height = 480;
    app.current_screen = calloc(640*480, sizeof(uint32_t));
    app.prev_screen = calloc(640*480, sizeof(uint32_t));
    compress_setup(&app);

    memset(app.current_screen, 0xff, sizeof(uint32_t)*640*480);

    // Create output pixbuf
    GdkPixbuf *px = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 640, 480);
    ASSERT(px != NULL, "could not create pixbuf");
    app.view = malloc(sizeof(viewinfo_t));
    app.view->pixels = gdk_pixbuf_get_pixels(px);
    app.view->row_stride = gdk_pixbuf_get_rowstride(px);
    app.view->width = 640;
    app.view->height = 480;

    // WHEN whole framebuffer has changed
    // THEN compare screens returns TRUE, and all pixels are marked as changed
    is_updated = compare_screens(&app, &update);
    ASSERT(is_updated == TRUE, "compare_screens did not return change for differing buffers");
    ASSERT(!check_update(update, 640*480), "expected whole screen to be updated");

    ret = draw_update(app.view, update);
    ASSERT(ret == 0, "draw update failed");

    if (show) {
        show_image(px);
    }

    free_framebuffer_update(update);

    // Make the screens equal
    memcpy(app.prev_screen, app.current_screen, 640*480*sizeof(uint32_t));

    // WHEN screen has not been changed
    // THEN compare screens returns FALSE
    is_updated = compare_screens(&app, &update);
    ASSERT(is_updated == FALSE, "compare_screens returned change for equal buffers");

    png2screenbuf("test/02-50-black-50-white.png", app.current_screen, app.width, app.height);
    is_updated = compare_screens(&app, &update);
    ASSERT(is_updated == TRUE, "compare_screens did not return change for differings buffers");
    ASSERT(update->n_rects > 0, "expected at least one rect");
    ASSERT(!check_update(update, (640*480) / 2), "expected half of the screen to be updated");

    ret = draw_update(app.view, update);
    ASSERT(ret == 0, "draw update failed");

    if (show) {
        show_image(px);
    }

    free_framebuffer_update(update);

    png2screenbuf("test/04-pine-hello.png", app.current_screen, app.width, app.height);
    is_updated = compare_screens(&app, &update);
    ASSERT(is_updated == TRUE, "compare_screens did not return change for differings buffers");
    ASSERT(update->n_rects > 0, "expected at least one rect");
    //ASSERT(!check_update(update, (640*480) / 2), "expected half of the screen to be updated");

    ret = draw_update(app.view, update);
    ASSERT(ret == 0, "draw update failed");

    if (show) {
        show_image(px);
    }

    free_framebuffer_update(update);
    return 0;
}

static gboolean expose(GtkWidget *widget, GdkEvent *event, GdkPixbuf *pixbuf) {
    GdkWindow *window = gtk_widget_get_window(widget);
    GdkDrawingContext *ctx;
    cairo_region_t *region;

    region = cairo_region_create();
    ctx = gdk_window_begin_draw_frame(window, region);
    cairo_t *cr = gdk_drawing_context_get_cairo_context(ctx);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);
    gdk_window_end_draw_frame(window, ctx);
    cairo_region_destroy(region);
    return FALSE;
}

int show_image(GdkPixbuf *pixbuf) {
    GtkWidget *window;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW(window), 640, 480);
    gtk_window_set_title(GTK_WINDOW(window), "show-image");
    g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

    GtkWidget *drawing = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawing);
    g_signal_connect(G_OBJECT(drawing), "draw", G_CALLBACK(expose), pixbuf);

    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

