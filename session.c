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
#include "shareit.h"
#include "appwin.h"
#include "framebuffer.h"
#include "session.h"
#include "screenshare.h"

// Used to show different parts of the window
#define STATUS_NORMAL 0   // Just a regular chat
#define STATUS_VIEWING 1  // Someone else is sharing their screen

struct _ShareitSession {
    // Parent class
    GtkWindow parent;

    // Parent window
    ShareitAppWindow *appwin;
    connection_t *conn;

    GtkScrolledWindow *scrolled_window;
    GtkWidget *viewport;
    GtkWidget *drawing;
    GtkWidget *view_buttons;
    GtkWidget *default_buttons;
    GtkWidget *btn_toggle_zoom_fit;
    GtkWidget *btn_zoom_original;
    GtkWidget *btn_leave;
    GtkWidget *btn_start_share;
    GtkWidget *btn_stop_share;
    GtkAdjustment *drawing_adjust_horizontal;
    GtkAdjustment *drawing_adjust_vertical;

    gpointer handler_screen_share_start;
    gpointer handler_cursor_update;
    gpointer handler_framebuffer_update;
    gpointer handler_leave_session;

    int status;

    // View from server
    viewinfo_t *view;
    int cursor_x;
    int cursor_y;

    // Size of window / widget our image should be drawn to
    int window_width;
    int window_height;

    // Scaling factors of window
    double scale_x;
    double scale_y;
    gboolean fit_to_window;

};

G_DEFINE_TYPE(ShareitSession, shareit_session, GTK_TYPE_WINDOW);

/**
 * show different parts of the window depending on current status
 *
 * @param win
 * @param status
 */
static void update_window(ShareitSession *win, int status) {
    win->status = status;
    if (status == STATUS_NORMAL) {
        gtk_widget_show(GTK_WIDGET(win->default_buttons));
    } else {
        gtk_widget_hide(GTK_WIDGET(win->default_buttons));
    }

    if (status == STATUS_VIEWING) {
        gtk_widget_show(GTK_WIDGET(win->view_buttons));
    } else {
        gtk_widget_hide(GTK_WIDGET(win->view_buttons));
    }
}

static gboolean window_draw_cb(GtkWidget *widget, cairo_t *cr) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(widget)));
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean drawing_draw_cb(GtkWidget *widget, cairo_t *cr) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(widget)));

    // Position image in center if it's smaller than the window
    viewinfo_t *view = win->view;
    if (view == NULL) {
        return FALSE;
    }

    double w = (double)view->width * win->scale_x;
    double h = (double)view->height * win->scale_y;
    double x, y;
    x = y = 0;

    if (w < win->window_width) {
        x = (double)win->window_width / 2 - w / 2;
    }
    if (h < win->window_height) {
        y = (double)win->window_height / 2 - h / 2;
    }

    // Draw from raw pixels to cairo surface
    cairo_surface_t *surface = cairo_image_surface_create_for_data (view->pixels,
                               CAIRO_FORMAT_RGB24,
                               view->width,
                               view->height,
                               view->row_stride);
    gtk_widget_set_size_request(widget, (int)w, (int) h);
    cairo_scale(cr, win->scale_x, win->scale_y);
    cairo_set_source_surface(cr, surface, x, y);
    cairo_paint(cr);
    return FALSE;
}

static gboolean drawing_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event_p) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(widget)));

    if (win->view == NULL) {
        return FALSE;
    }

    if (win->fit_to_window) {
        win->scale_x = (double)win->window_width / win->view->width;
        win->scale_y = (double)win->window_height / win->view->height;
    } else {
        // Synchronize scales
        win->scale_x = win->scale_y;
    }
    return FALSE;
}

static gboolean scrolled_window_size_allocate_cb(GtkWidget *widget, GdkEvent *ev) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(widget)));
    GtkAllocation sz;
    gtk_widget_get_allocation(widget, &sz);
    win->window_width = sz.width;
    win->window_height = sz.height;

    if (win->fit_to_window) {
        drawing_configure_event_cb(win->drawing, NULL);
        gtk_widget_queue_draw(win->drawing);
    }
    return FALSE;
}

static gboolean drawing_scroll_event_cb(GtkWidget *drawing, GdkEventScroll *ev) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(drawing)));

    gboolean scroll = FALSE;
    double new_x, new_y;

    if (ev->direction == GDK_SCROLL_UP) {
        win->scale_x *= 1.2;
        win->scale_y *= 1.2;

        gtk_adjustment_set_upper(win->drawing_adjust_horizontal,
                                 gtk_adjustment_get_upper(win->drawing_adjust_horizontal)
                                 * 1.2 + 1);
        gtk_adjustment_set_upper(win->drawing_adjust_vertical,
                                 gtk_adjustment_get_upper(win->drawing_adjust_vertical)
                                 * 1.2 + 1);
        new_x = ev->x * 1.2;
        new_y = ev->y * 1.2;
        scroll = TRUE;
    } else if (ev->direction == GDK_SCROLL_DOWN) {
        win->scale_x /= 1.2;
        win->scale_y /= 1.2;
        new_x = ev->x / 1.2;
        new_y = ev->y / 1.2;
        scroll = TRUE;
    }

    if (scroll) {
        if (win->fit_to_window) {
            win->fit_to_window = FALSE;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->btn_toggle_zoom_fit), FALSE);
        }

        gtk_adjustment_set_value(win->drawing_adjust_horizontal, new_x + gtk_adjustment_get_value(win->drawing_adjust_horizontal) - ev->x);
        gtk_adjustment_set_value(win->drawing_adjust_vertical, new_y + gtk_adjustment_get_value(win->drawing_adjust_vertical) - ev->y);

        drawing_configure_event_cb(drawing, NULL);
        gtk_widget_queue_draw(drawing);
    }
    return FALSE;
}

static gboolean btn_zoom_original_clicked_cb(GtkWidget *btn) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(btn)));

    win->scale_x = 1.0;
    win->scale_y = 1.0;
    win->fit_to_window = FALSE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->btn_toggle_zoom_fit), FALSE);
    drawing_configure_event_cb(win->drawing, NULL);
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean btn_toggle_zoom_fit_toggled_cb(GtkWidget *btn) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(btn)));

    win->fit_to_window = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
    drawing_configure_event_cb(win->drawing, NULL);
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean btn_start_share_clicked_cb(GtkWidget *btn) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(btn)));
    ShareitScreenshare *screenshare = shareit_screenshare_new(win->appwin);
    if (screenshare == NULL) {
        return FALSE;
    }

    gtk_widget_hide(GTK_WIDGET(win));
    gtk_window_present(GTK_WINDOW(screenshare));
    g_signal_connect_swapped(screenshare, "destroy", (GCallback) gtk_widget_show, win);
    return FALSE;
}

static gboolean btn_leave_clicked_cb(GtkWidget *btn) {
    ShareitSession *win = SHAREIT_SESSION(gtk_widget_get_toplevel (GTK_WIDGET(btn)));

    gtk_widget_destroy(GTK_WIDGET(win));
    return FALSE;
}

static gboolean queue_redraw(void *ptr) {
    ShareitSession *win = SHAREIT_SESSION(ptr);
    gtk_widget_queue_draw(win->drawing);

    // Remove from idle loop
    return FALSE;
}

static gboolean handle_screen_share_start(screensize_t *rect, ShareitSession *win) {
    if (win->view == NULL) {
        win->view = calloc(1, sizeof(viewinfo_t));
        if (win->view == NULL) {
            perror("calloc(app->view)");
            return -1;
        }
    }

    if (win->view->pixels != NULL) {
        free(win->view->pixels);
    }

    win->view->pixels = calloc(rect->width*rect->height, sizeof(uint32_t));
    win->view->row_stride = rect->width*sizeof(uint32_t);
    win->view->width = rect->width;
    win->view->height = rect->height;

    update_window(win, STATUS_VIEWING);

    g_idle_add(queue_redraw, win);
    return TRUE;
}

static gboolean handle_cursor_update(cursorinfo_t *info, ShareitSession *win) {
    if (info == NULL) {
        return TRUE;
    }

    win->cursor_x = info->x;
    win->cursor_y = info->y;

    g_idle_add(queue_redraw, win);
    return TRUE;
}

static gboolean handle_framebuffer_update(framebuffer_update_t *update, ShareitSession *win) {
    if (win->view != NULL) {
        draw_update(win->view, update);
    }

    g_idle_add(queue_redraw, win);
    return TRUE;
}

static gboolean handle_leave_session(void *data, ShareitSession *win) {
    // If we receive SIGNAL_SESSION_LEFT, it probably means that
    // something else caused us to leave the session, so we just close our window
    // (application win assumes we left the session if session window is closed)
    gtk_widget_destroy(GTK_WIDGET(win));
    return TRUE;
}

static void shareit_session_init(ShareitSession *win) {
    gtk_widget_init_template(GTK_WIDGET(win));

    win->scale_x = 1.0;
    win->scale_y = 1.0;
    win->fit_to_window = TRUE;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->btn_toggle_zoom_fit), win->fit_to_window);
    gtk_scrolled_window_set_policy(win->scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    win->drawing_adjust_horizontal = gtk_scrolled_window_get_hadjustment(win->scrolled_window);
    win->drawing_adjust_vertical = gtk_scrolled_window_get_vadjustment(win->scrolled_window);
}

static void shareit_session_activate(ShareitSession *win) {
    win->conn = win->appwin->conn;

    gtk_widget_set_vexpand(GTK_WIDGET(win->scrolled_window), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(win->scrolled_window), TRUE);

    win->handler_screen_share_start = net_signal_connect(win->conn, SIGNAL_SCREEN_SHARE_START, (handlefunc_t)handle_screen_share_start, win);
    win->handler_cursor_update = net_signal_connect(win->conn, SIGNAL_CURSOR_UPDATE, (handlefunc_t)handle_cursor_update, win);
    win->handler_framebuffer_update = net_signal_connect(win->conn, SIGNAL_FRAMEBUFFER_UPDATE, (handlefunc_t)handle_framebuffer_update, win);
    win->handler_leave_session = net_signal_connect(win->conn, SIGNAL_SESSION_LEFT, (handlefunc_t)handle_leave_session, win);

    // Initialize in default mode
    update_window(win, STATUS_NORMAL);
}

static void shareit_session_dispose(GObject *object) {
    ShareitSession *win = SHAREIT_SESSION(object);

    if (win->handler_screen_share_start) {
        net_signal_disconnect(win->conn, SIGNAL_SCREEN_SHARE_START, win->handler_screen_share_start);
        win->handler_screen_share_start = NULL;
    }

    if (win->handler_cursor_update) {
        net_signal_disconnect(win->conn, SIGNAL_CURSOR_UPDATE, win->handler_cursor_update);
        win->handler_cursor_update = NULL;
    }

    if (win->handler_framebuffer_update) {
        net_signal_disconnect(win->conn, SIGNAL_FRAMEBUFFER_UPDATE, win->handler_framebuffer_update);
        win->handler_framebuffer_update = NULL;
    }

    if (win->handler_leave_session) {
        net_signal_disconnect(win->conn, SIGNAL_SESSION_LEFT, win->handler_leave_session);
        win->handler_leave_session = NULL;
    }

    if (win->view != NULL) {
        if (win->view->pixels != NULL) {
            free(win->view->pixels);
        }
        free(win->view);
        win->view = NULL;
    }
    G_OBJECT_CLASS(shareit_session_parent_class)->dispose(object);
}

static void shareit_session_class_init(ShareitSessionClass *class) {
    G_OBJECT_CLASS(class)->dispose = shareit_session_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/se/oddbike/shareit/session.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, scrolled_window);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, viewport);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, drawing);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, default_buttons);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, view_buttons);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, btn_toggle_zoom_fit);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, btn_zoom_original);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, btn_leave);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitSession, btn_start_share);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), drawing_configure_event_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), drawing_draw_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), drawing_scroll_event_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), window_draw_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), scrolled_window_size_allocate_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_zoom_original_clicked_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_toggle_zoom_fit_toggled_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_leave_clicked_cb);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), btn_start_share_clicked_cb);
}

ShareitSession *shareit_session_new(ShareitAppWindow *win) {
    ShareitSession *view = g_object_new(SHAREIT_SESSION_TYPE, NULL);
    if (view == NULL) {
        return NULL;
    }
    view->appwin = win;
    shareit_session_activate(view);
    return view;
}
