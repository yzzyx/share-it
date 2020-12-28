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

typedef struct {
    shareit_app_t *app;

    GtkWidget *window;
    GtkScrolledWindow *scrolled_window;
    GtkWidget *viewport;
    GtkWidget *drawing;
    GtkWidget *btn_toggle_zoom_fit;
    GtkWidget *btn_zoom_original;
    GtkWidget *btn_leave;
    GtkAdjustment *drawing_adjust_horizontal;
    GtkAdjustment *drawing_adjust_vertical;

    // Size of window / widget our image should be drawn to
    int window_width;
    int window_height;

    // Scaling factors of window
    double scale_x;
    double scale_y;
    gboolean fit_to_window;
}viewer_win_t;

static gboolean window_draw(GtkWidget *widget, cairo_t *cr, viewer_win_t *win) {
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean drawing_draw(GtkWidget *widget, cairo_t *cr, viewer_win_t *win) {
    // Position image in center if it's smaller than the window
    viewinfo_t *view = win->app->view;
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

static gboolean drawing_configure(GtkWidget *widget, GdkEventConfigure *event_p, viewer_win_t *win) {
    if (win->fit_to_window) {
        win->scale_x = (double)win->window_width / win->app->view->width;
        win->scale_y = (double)win->window_height / win->app->view->height;
    } else {
        // Synchronize scales
        win->scale_x = win->scale_y;
    }
    return FALSE;
}

static gboolean scroll_win_size_event(GtkWidget *widget, GdkEvent *ev, viewer_win_t *win) {
    GtkAllocation sz;
    gtk_widget_get_allocation(widget, &sz);
    win->window_width = sz.width;
    win->window_height = sz.height;

    if (win->fit_to_window) {
        drawing_configure(win->drawing, NULL, win);
        gtk_widget_queue_draw(win->drawing);
    }
    return FALSE;
}

static gboolean drawing_scroll(GtkWidget *drawing, GdkEventScroll *ev, viewer_win_t *win) {
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
    } else if (ev->direction == GDK_SCROLL_DOWN){
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

        drawing_configure(drawing, NULL, win);
        gtk_widget_queue_draw(drawing);
    }
    return FALSE;
}

static gboolean zoom_original(GtkWidget *btn, viewer_win_t *win) {
    win->scale_x = 1.0;
    win->scale_y = 1.0;
    win->fit_to_window = FALSE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->btn_toggle_zoom_fit), FALSE);
    drawing_configure(win->drawing, NULL, win);
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean zoom_fit(GtkWidget *btn, viewer_win_t *win) {
    win->fit_to_window = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
    drawing_configure(win->drawing, NULL, win);
    gtk_widget_queue_draw(win->drawing);
    return FALSE;
}

static gboolean leave_session(GtkWidget *btn, viewer_win_t *win) {
    gtk_widget_hide(win->window);
    return FALSE;
}

// FIXME - create a new WidgetClass and use templating instead
GtkWidget *viewer_initialize(shareit_app_t *app) {
    GtkBuilder *builder = gtk_builder_new_from_file("viewer.ui");

    viewer_win_t *win = calloc(1, sizeof(viewer_win_t));
    if (win == NULL) {
        return NULL;
    };

    win->app = app;
    win->scale_x = 1.0;
    win->scale_y = 1.0;
    win->fit_to_window = TRUE;

    // Setup all references
    BUILDER_GET(win->window, GTK_WIDGET, "window");
    BUILDER_GET(win->scrolled_window, GTK_SCROLLED_WINDOW, "scrolled_window");
    BUILDER_GET(win->viewport, GTK_WIDGET, "viewport");
    BUILDER_GET(win->drawing, GTK_WIDGET, "drawing");
    BUILDER_GET(win->btn_toggle_zoom_fit, GTK_WIDGET, "btn_toggle_zoom_fit");
    BUILDER_GET(win->btn_zoom_original, GTK_WIDGET, "btn_zoom_original");
    BUILDER_GET(win->btn_leave, GTK_WIDGET, "btn_leave");

    gtk_scrolled_window_set_policy(win->scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    win->drawing_adjust_horizontal = gtk_scrolled_window_get_hadjustment(win->scrolled_window);
    win->drawing_adjust_vertical = gtk_scrolled_window_get_vadjustment(win->scrolled_window);
    gtk_widget_set_vexpand(GTK_WIDGET(win->scrolled_window), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(win->scrolled_window), TRUE);

    // Receive scroll events for drawing area
    gtk_widget_set_events(win->drawing, gtk_widget_get_events(win->drawing) | GDK_SCROLL_MASK);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->btn_toggle_zoom_fit), win->fit_to_window);

    // Register all signal handlers
    g_signal_connect(G_OBJECT(win->drawing), "configure-event", G_CALLBACK(drawing_configure), win);
    g_signal_connect(G_OBJECT(win->drawing), "draw", G_CALLBACK(drawing_draw), win);
    g_signal_connect(G_OBJECT(win->window), "draw", G_CALLBACK(window_draw), win);
    g_signal_connect(G_OBJECT(win->drawing), "scroll-event", G_CALLBACK(drawing_scroll), win);
    g_signal_connect(G_OBJECT(win->scrolled_window), "size-allocate", G_CALLBACK(scroll_win_size_event), win);
    g_signal_connect(G_OBJECT(win->btn_zoom_original), "clicked", G_CALLBACK(zoom_original), win);
    g_signal_connect(G_OBJECT(win->btn_toggle_zoom_fit), "toggled", G_CALLBACK(zoom_fit), win);
    g_signal_connect(G_OBJECT(win->btn_leave), "clicked", G_CALLBACK(leave_session), win);
    g_signal_connect(G_OBJECT(win->window), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), win);

    return win->window;
}