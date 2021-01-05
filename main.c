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
#include "app.h"
#include "grab.h"
#include "handlers.h"
#include "framebuffer.h"
#include "packet.h"
#include "password.h"

static gboolean stop_screen_share(shareit_app_t *app);

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
    if (mx != -1 && my != -1 && mx != app->mouse_pos_x && my != app->mouse_pos_y) {
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
        if (pkt_send_framebuffer_update(app->conn->socket, update) == -1) {
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

int main(int argc, char **argv) {
    ShareitApp *gtk_app;
    int status;

    g_setenv("GSETTINGS_SCHEMA_DIR", ".", FALSE);
    gtk_init(&argc, &argv);
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    gtk_app = shareit_app_new();
    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    return status;
}
