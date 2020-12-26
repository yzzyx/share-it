//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <gtk/gtk.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "shareit.h"
#include "grab.h"
#include "handlers.h"
#include "framebuffer.h"
#include "packet.h"
#include "password.h"

static gboolean stop_screen_share(shareit_app_t *app);

static shareit_app_t *setup() {
    shareit_app_t *app;

    // Allocate new app and initialize it to 0
    app = calloc(1, sizeof(shareit_app_t));
    if (app == NULL) {
        return NULL;
    }

    // block is used to transfer an updated section of the screen
    app->block = malloc(128*128*sizeof(uint32_t));
    if (app->block == NULL) {
        return NULL;
    }

    return app;
}

void show_error(shareit_app_t *app, const char *fmt, ...) {
    GtkWidget *dlg;
    va_list ap;
    int nb = 0;
    char *msg = "cannot allocate memory";
    char *allocated = NULL;

    // Allocate memory for message
    va_start(ap, fmt);
    nb = vsnprintf(NULL, nb, fmt, ap);
    va_end(ap);

    if (nb >= 0) {
        nb++;
        if ((allocated = malloc(nb)) != NULL) {
            msg = allocated;
            va_start(ap, fmt);
            vsnprintf(msg, nb, fmt, ap);
            va_end(ap);
        }
    }

    dlg = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", msg);
    g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_widget_destroy), dlg);
    gtk_widget_show_all(dlg);

    if (allocated != NULL) {
        free(allocated);
    }
}

static gboolean screen_share_timer(shareit_app_t *app) {
    int ret;
    uint32_t *tmp;

    if (app->share_screen != TRUE) {
        return FALSE;
    }

    int mx, my;
    grab_cursor_position(app->grabber, &mx, &my);
    if (mx != app->mouse_pos_x && my != app->mouse_pos_y) {
        if (pkt_send_cursorinfo(app->conn->socket, mx, my, 0) != 0) {
            show_error(app, "could not send cursor info to server");
            gdk_threads_add_idle(G_SOURCE_FUNC(stop_screen_share), app);
            return FALSE;
        }
        app->mouse_pos_x = mx;
        app->mouse_pos_y = my;
    }

    if (app->current_screen == NULL) {
        app->current_screen = malloc(sizeof(uint32_t) * app->width * app->height);
        if (app->current_screen == NULL) {
            fprintf(stderr, "could not allocate screen memory: %s\n", strerror(errno));
            return -1;
        }
    }

    ret = grab_window(app->grabber, (uint8_t *)app->current_screen);
    if (ret != 0) {
        fprintf(stderr, "could not read window data\n");
        return -1;
    }

    framebuffer_update_t *update;
    if (compare_screens(app, &update)) {
        if (pkt_send_framebuffer_update(app->conn->socket, app->output_stream, update) == -1) {
            show_error(app, "could not send block data to server");
            gdk_threads_add_idle(G_SOURCE_FUNC(stop_screen_share), app);
            return FALSE;
        }
    }

    // Switch prev and current buffers, so that we don't have to allocate
    // and free the memory all the time
    tmp = app->prev_screen;
    app->prev_screen = app->current_screen;
    app->current_screen = tmp;
    return TRUE;
}

static gboolean stop_screen_share(shareit_app_t *app) {
    app->share_screen = FALSE;
    gtk_button_set_label(GTK_BUTTON(app->btn_sharescreen), "Share screen");

    grab_shutdown(app->grabber);
    app->grabber = NULL;

    free(app->current_screen);
    free(app->prev_screen);
    app->current_screen = NULL;
    app->prev_screen = NULL;

    app->mouse_pos_x = 0;
    app->mouse_pos_y = 0;
    return FALSE;
}

static gboolean dlg_share_ok_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gboolean visible, public;
    gtk_widget_hide(app->dlg_share_options);
    visible = gtk_toggle_button_get_active(app->dlg_share_visible_checkbox);
    public = gtk_toggle_button_get_active(app->dlg_share_public_checkbox);

    void *grabber;
    grabber = grab_initialize();
    if (grabber == NULL) {
        gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not initialize screen grabber");
        return FALSE;
    }

    app->grabber = grabber;
    if (grab_window_size(app->grabber, &app->width, &app->height) != 0) {
        show_error(app, "could not read window size");
        grab_shutdown(app->grabber);
        return FALSE;
    }

    if (pkt_send_session_screenshare_request(app->conn->socket, app->width, app->height) == -1) {
        show_error(app, "could not send screeninfo to server");
        grab_shutdown(app->grabber);
        return FALSE;
    }

    app->share_screen = TRUE;
    gtk_button_set_label(GTK_BUTTON(app->btn_sharescreen), "Stop sharing screen");
    gdk_threads_add_timeout(100, G_SOURCE_FUNC(screen_share_timer), app);
    return FALSE;
}

#define BUILDER_GET(out, type, name) out = type(gtk_builder_get_object(builder, name)); \
if (out == NULL) { \
	g_critical("Widget \"%s\" is missing in UI file", #name); \
}

static gboolean btn_start_share_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    // If we're currently sharing, stop it
    if (app->share_screen == TRUE) {
        return stop_screen_share(app);
    }

    // Show sharing options with defaults setup
    gtk_toggle_button_set_active(app->dlg_share_visible_checkbox, TRUE);
    gtk_toggle_button_set_active(app->dlg_share_public_checkbox, FALSE);
    gtk_widget_show_all(app->dlg_share_options);
    return FALSE;
}

static gboolean btn_exit_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_destroy(app->window);
    return FALSE;
}

static gboolean dlg_share_cancel_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_hide(app->dlg_share_options);
    return FALSE;
}

static gboolean dlg_share_generate_pw_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_entry_set_text(app->dlg_share_password_entry, generate_password());
    return FALSE;
}

static gboolean btn_viewscreen_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_show_all(app->dlg_select_session);
    return FALSE;
}

static gboolean dlg_select_session_connect_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    int ret;
    ret = pkt_send_session_join_request(app->conn->socket,
                                        gtk_entry_get_text(app->dlg_select_session_entry),
                                        "");
    if (ret) {
        show_error(app, "could not join session: network error");
        return FALSE;
    }
    // Receive the response...
    gtk_widget_hide(app->dlg_select_session);
    return FALSE;
}

static gboolean dlg_select_session_cancel_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_hide(app->dlg_select_session);
    return FALSE;
}

static gboolean data_available(GIOChannel *source, GIOCondition condition, shareit_app_t *app);
static gboolean app_setup_connection(shareit_app_t *app) {
    // Setup connection
    char *err;
    app->conn = net_connect(app->host, &err);
    if (app->conn == NULL) {
        show_error(app, "cannot connect to %s: %s", app->host, err);
        return FALSE;
    }

    if (app->channel != NULL) {
        g_io_channel_shutdown(app->channel, TRUE, NULL);
    }

    app->channel = g_io_channel_unix_new(app->conn->socket);
    g_io_channel_set_encoding(app->channel, NULL, NULL);
    g_io_channel_set_buffered(app->channel, FALSE);
    g_io_add_watch(app->channel, G_IO_IN | G_IO_HUP | G_IO_ERR, (GIOFunc)data_available, app);

    return TRUE;
}

static gboolean dlg_connect_connect_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gboolean ret;
    char *name;

    name = gtk_combo_box_text_get_active_text(app->dlg_connect_server_dropdown);
    app->host = strdup(name);

    if (app_setup_connection(app) == TRUE) {
        gtk_widget_hide(app->dlg_connect);
    }
    return FALSE;
}

static gboolean dlg_connect_cancel_clicked_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_destroy(app->window);
    return FALSE;
}

static gboolean screen_area_draw_cb(GtkWidget *widget, GdkEvent *event, shareit_app_t *app) {
    GdkWindow *window = gtk_widget_get_window(widget);
    GdkDrawingContext *ctx;
    if (app->view == NULL || app->view->pixels == NULL) {
        return FALSE;
    }

    cairo_region_t *region = cairo_region_create();

    cairo_surface_t *surface = cairo_image_surface_create_for_data (app->view->pixels,
                               CAIRO_FORMAT_RGB24,
                               app->view->width,
                               app->view->height,
                               app->view->row_stride);
    ctx = gdk_window_begin_draw_frame(window, region);
    cairo_t *cr = gdk_drawing_context_get_cairo_context(ctx);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_surface_flush(surface);
    gdk_window_end_draw_frame(window, ctx);
    cairo_surface_destroy(surface);
    cairo_region_destroy(region);
    return FALSE;
}

static void screen_area_size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation, shareit_app_t *app) {
    gtk_widget_queue_draw(widget);
}

static void screen_area_realize_cb(GtkWidget *widget, shareit_app_t *app) {
    gtk_widget_queue_draw(widget);
}


static gboolean data_available(GIOChannel *source, GIOCondition condition, shareit_app_t *app) {
    size_t nb;
    if (condition & G_IO_ERR) {
        printf("error!\n");
        return FALSE;
    }

    uint8_t type;
    nb = recv(app->conn->socket, &type, sizeof(type), 0);
    if (nb != sizeof(type)) {
        printf("could not read type: %s\n", strerror(errno));
        return FALSE;
    }

    switch (type) {
    case packet_type_session_join_response:
        printf("join response!\n");
        app_handle_join_response(app);
        break;
    case packet_type_cursor_info:
        app_handle_cursor_info(app);
        break;
    case packet_type_session_screenshare_start:
        app_handle_screenshare_start(app);
        break;
    case packet_type_framebuffer_update:
        app_handle_framebuffer_update(app);
        break;
    default:
        printf("unknown packet type: %d!\n", type);
        return FALSE;
    }

    return TRUE;
}

static void activate_builder (GtkApplication *gtk_application, shareit_app_t *app) {
    GtkBuilder *builder;

    builder = gtk_builder_new_from_file("share-it.xml");

    gtk_builder_add_callback_symbol(builder, "btn_exit_clicked_cb", G_CALLBACK(btn_exit_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "btn_viewscreen_clicked_cb", G_CALLBACK(btn_viewscreen_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "btn_start_share_clicked_cb", G_CALLBACK(btn_start_share_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_share_ok_clicked_cb", G_CALLBACK(dlg_share_ok_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_share_cancel_clicked_cb", G_CALLBACK(dlg_share_cancel_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_share_generate_pw_clicked_cb", G_CALLBACK(dlg_share_generate_pw_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_select_session_connect_clicked_cb", G_CALLBACK(dlg_select_session_connect_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_select_session_cancel_clicked_cb", G_CALLBACK(dlg_select_session_cancel_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_connect_connect_clicked_cb", G_CALLBACK(dlg_connect_connect_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_connect_cancel_clicked_cb", G_CALLBACK(dlg_connect_cancel_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "dlg_connect_destroy_cb", G_CALLBACK(dlg_connect_cancel_clicked_cb));
    gtk_builder_add_callback_symbol(builder, "screen_area_draw_cb", G_CALLBACK(screen_area_draw_cb));
    gtk_builder_add_callback_symbol(builder, "screen_area_realize_cb", G_CALLBACK(screen_area_realize_cb));
    gtk_builder_add_callback_symbol(builder, "screen_area_size_allocate_cb", G_CALLBACK(screen_area_size_allocate_cb));
    gtk_builder_connect_signals(builder, app);

    BUILDER_GET(app->window, GTK_WIDGET, "win_toolbar");
    g_object_unref(app->window);
    gtk_window_set_application(GTK_WINDOW(app->window), GTK_APPLICATION (gtk_application));
    gtk_window_set_keep_above(GTK_WINDOW(app->window), TRUE);

    // Place window on the middle of the screen on the right hand side
    int window_height, window_width;
    gtk_window_get_size(GTK_WINDOW(app->window), &window_width, &window_height);
    gtk_window_set_gravity(GTK_WINDOW(app->window), GDK_GRAVITY_NORTH_WEST);
    gtk_window_move(GTK_WINDOW(app->window),
                    gdk_screen_width() - window_width,
                    gdk_screen_height()/2 - window_height/2);

    BUILDER_GET(app->btn_sharescreen, GTK_WIDGET, "btn_sharescreen");
    BUILDER_GET(app->dlg_share_options, GTK_WIDGET, "dlg_share_options");
    BUILDER_GET(app->dlg_share_visible_checkbox, GTK_TOGGLE_BUTTON, "dlg_share_visible_checkbox");
    BUILDER_GET(app->dlg_share_public_checkbox, GTK_TOGGLE_BUTTON, "dlg_share_public_checkbox");
    BUILDER_GET(app->dlg_share_password_entry, GTK_ENTRY, "dlg_share_password_entry");
    BUILDER_GET(app->dlg_select_session, GTK_WIDGET, "dlg_select_session");
    BUILDER_GET(app->dlg_select_session_entry, GTK_ENTRY, "dlg_select_session_entry");
    BUILDER_GET(app->dlg_connect, GTK_WIDGET, "dlg_connect");
    BUILDER_GET(app->dlg_connect_server_dropdown, GTK_COMBO_BOX_TEXT, "dlg_connect_server_dropdown");
    BUILDER_GET(app->screen_share_window, GTK_WIDGET, "win_screen_share");
    BUILDER_GET(app->screen_share_area, GTK_DRAWING_AREA, "screen_share_area");

    // Show the connect dialog at start unless we've already been called with a hostname
    gboolean skip_connect_dialog = FALSE;
    if (app->host != NULL) {
        skip_connect_dialog = app_setup_connection(app);
    }

    if (!skip_connect_dialog) {
        gtk_widget_show_all(app->dlg_connect);
    }
}

int main(int argc, char **argv) {
    GtkApplication *gtk_app;
    shareit_app_t *app;
    int status;
    int opt;
    char *hostname = NULL;

    while ((opt = getopt(argc, argv, "h:")) != -1) {
        switch (opt) {
        case 'h':
            hostname = strdup(optarg);
            break;
        default:
            fprintf(stderr, "Usage: %s [-h hostname]\n", argv[0]);
            return 1;
        }
    }

    app = setup();
    if (app == NULL) {
        fprintf(stderr, "cannot setup application\n");
        return -1;
    }

    if (hostname != NULL) {
        app->host = hostname;
    }

    gtk_app = gtk_application_new (NULL, G_APPLICATION_FLAGS_NONE);
    g_signal_connect (gtk_app, "activate", G_CALLBACK (activate_builder), app);
    status = g_application_run (G_APPLICATION (gtk_app), argc, argv);
    g_object_unref (gtk_app);

    if (app->share_screen) {
        stop_screen_share(app);
    }

    if (app->conn != NULL) {
        net_disconnect(app->conn);
        app->conn = NULL;
    }
    return status;
}
