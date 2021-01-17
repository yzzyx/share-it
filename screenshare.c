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

    int timer_id;

    connection_t *conn;
    gpointer grabber;

    screeninfo_t screeninfo;
    int mouse_pos_x;
    int mouse_pos_y;

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

static int screen_share_timer(void *ptr) {
    ShareitScreenshare *win = SHAREIT_SCREENSHARE(ptr);
    uint32_t *tmp;
    int ret;

    if (g_source_is_destroyed(g_main_current_source())) {
        return G_SOURCE_REMOVE;
    }

    int mx, my;
    grab_cursor_position(win->grabber, &mx, &my);
    if (mx != -1 && my != -1 && mx != win->mouse_pos_x && my != win->mouse_pos_y) {
        if (net_send_cursorinfo(win->conn, mx, my) == -1) {
            return G_SOURCE_REMOVE;
        }
        win->mouse_pos_x = mx;
        win->mouse_pos_y = my;
    }

    if (win->screeninfo.current_screen == NULL) {
        win->screeninfo.current_screen = malloc(sizeof(uint32_t) * win->screeninfo.width * win->screeninfo.height);
        if (win->screeninfo.current_screen == NULL) {
            perror("could not allocate screen memory");
            return G_SOURCE_REMOVE;
        }
    }

    ret = grab_window(win->grabber, (uint8_t *)win->screeninfo.current_screen);
    if (ret != 0) {
        fprintf(stderr, "could not read window data\n");
        return G_SOURCE_REMOVE;
    }

    framebuffer_update_t *update;
    if (compare_screens(&win->screeninfo, &update)) {
        if (net_send_framebuffer_update(win->conn, update) == -1) {
            return G_SOURCE_REMOVE;
        }
    }

    // Switch prev and current buffers, so that we don't have to allocate
    // and free the memory all the time
    tmp = win->screeninfo.prev_screen;
    win->screeninfo.prev_screen = win->screeninfo.current_screen;
    win->screeninfo.current_screen = tmp;

    // Reschedule
    return G_SOURCE_CONTINUE;
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
    if (grab_window_size(win->grabber, &win->screeninfo.width, &win->screeninfo.height) != 0) {
//        show_error(app, "could not read window size");
        gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not read window size");
        grab_shutdown(win->grabber);
        return -1;
    }

    if (net_start_screenshare(win->conn, win->screeninfo.width, win->screeninfo.height) == -1) {
        gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                               "could not send screenshare info to server");
        grab_shutdown(win->grabber);
        return -1;
    }

    win->timer_id = g_timeout_add(50, screen_share_timer, win);
    return 0;
}

static void shareit_screenshare_dispose(GObject *object) {
    ShareitScreenshare *win = SHAREIT_SCREENSHARE(object);

    if (win->timer_id) {
        g_source_remove(win->timer_id);
        win->timer_id = 0;
    }

    // FIXME - signal server that we're not sharing anymore
    // net_stop_screenshare()
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
