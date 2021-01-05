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
#include "grab.h"
#include "shareit.h"
#include "appwin.h"
#include "framebuffer.h"
#include "session.h"
#include "screenshare.h"


struct _ShareitScreenshare {
    // Parent class
    GtkWindow parent;

    // Parent window
    ShareitAppWindow *appwin;

    connection_t *conn;
    gpointer grabber;
    int width;
    int height;

    GtkBox *box_participants;
};

G_DEFINE_TYPE(ShareitScreenshare, shareit_screenshare, GTK_TYPE_WINDOW);

static void shareit_screenshare_init(ShareitScreenshare *win) {
    gtk_widget_init_template(GTK_WIDGET(win));
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);

    // Place window on the middle of the screen on the right hand side
    int window_height, window_width;
    gtk_window_get_size(GTK_WINDOW(win), &window_width, &window_height);
    gtk_window_set_gravity(GTK_WINDOW(win), GDK_GRAVITY_NORTH_WEST);
    gtk_window_move(GTK_WINDOW(win),
                    gdk_screen_width() - window_width,
                    gdk_screen_height()/2 - window_height/2);
}


static void btn_stop_share_clicked_cb(GtkWidget *widget) {
    ShareitScreenshare *win = SHAREIT_SCREENSHARE(gtk_widget_get_toplevel(widget));
    gtk_widget_destroy(GTK_WIDGET(win));
}

static void btn_leave_clicked_cb(GtkWidget *widget) {
    ShareitScreenshare *win = SHAREIT_SCREENSHARE(gtk_widget_get_toplevel(widget));

    gtk_widget_destroy(GTK_WIDGET(win));
}

static int shareit_screenshare_activate(ShareitScreenshare *win) {
    win->conn = win->appwin->conn;
//
//    gtk_widget_set_vexpand(GTK_WIDGET(win->scrolled_window), TRUE);
//    gtk_widget_set_hexpand(GTK_WIDGET(win->scrolled_window), TRUE);
//
//    win->handler_screen_share_start = net_signal_connect(win->conn, SIGNAL_SCREEN_SHARE_START,
//                                                         (handlefunc_t) handle_screen_share_start, win);
//    win->handler_cursor_update = net_signal_connect(win->conn, SIGNAL_CURSOR_UPDATE,
//                                                    (handlefunc_t) handle_cursor_update, win);
//    win->handler_framebuffer_update = net_signal_connect(win->conn, SIGNAL_FRAMEBUFFER_UPDATE,
//                                                         (handlefunc_t) handle_framebuffer_update, win);

    void *grabber;
    grabber = grab_initialize();
    if (grabber == NULL) {
        gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not initialize screen grabber");
        return -1;
    }

    win->grabber = grabber;
    if (grab_window_size(win->grabber, &win->width, &win->height) != 0) {
//        show_error(app, "could not read window size");
        gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not read window size");
        grab_shutdown(win->grabber);
        return -1;
    }

    if (net_start_screenshare(win->conn, win->width, win->height) == -1) {
        gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not send screenshare info to server");
        grab_shutdown(win->grabber);
        return -1;
    }
    return 0;
}

static void shareit_screenshare_dispose(GObject *object) {
    ShareitScreenshare *win = SHAREIT_SCREENSHARE(object);

//    if (win->handler_screen_share_start) {
//        net_signal_disconnect(win->conn, SIGNAL_SCREEN_SHARE_START, win->handler_screen_share_start);
//        win->handler_screen_share_start = NULL;
//    }
//
//    if (win->handler_cursor_update) {
//        net_signal_disconnect(win->conn, SIGNAL_CURSOR_UPDATE, win->handler_cursor_update);
//        win->handler_cursor_update = NULL;
//    }
//
//    if (win->handler_framebuffer_update) {
//        net_signal_disconnect(win->conn, SIGNAL_FRAMEBUFFER_UPDATE, win->handler_framebuffer_update);
//        win->handler_framebuffer_update = NULL;
//    }
//
//    if (win->view != NULL) {
//        if (win->view->pixels != NULL) {
//            free(win->view->pixels);
//        }
//        free(win->view);
//        win->view = NULL;
//    }
    G_OBJECT_CLASS(shareit_screenshare_parent_class)->dispose(object);
}

static void shareit_screenshare_class_init(ShareitScreenshareClass *class) {
    G_OBJECT_CLASS(class)->dispose = shareit_screenshare_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/se/oddbike/shareit/screenshare.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitScreenshare , box_participants);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_stop_share_clicked_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_leave_clicked_cb);
}

ShareitScreenshare *shareit_screenshare_new(ShareitAppWindow *win) {
    ShareitScreenshare *screenshare = g_object_new(SHAREIT_SCREENSHARE_TYPE, NULL);
    if (screenshare == NULL) {
        return NULL;
    }
    screenshare->appwin = win;
    if (shareit_screenshare_activate(screenshare) == -1) {
            gtk_widget_destroy(GTK_WIDGET(screenshare));
            return NULL;
    }
    return screenshare;
}
