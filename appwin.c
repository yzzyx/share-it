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
#include "app.h"
#include "appwin.h"
#include "preferences.h"
#include "session.h"
#include "net.h"

// Used to show different parts of the window
#define STATUS_INITIALIZING 0
#define STATUS_CONNECTED 1
#define STATUS_DISCONNECTED 2
#define STATUS_ERROR 3

typedef struct _ShareitAppWindowPrivate ShareitAppWindowPrivate;

struct _ShareitAppWindowPrivate
{
    GSettings *settings;
    GtkButton *btn_create_session;
    GtkEntry *entry_session_name;
    GtkLabel *lbl_error;
    GtkBox *box_connected; // only shown if we're connected
    GtkBox *box_reconnect; // only shown if we can reconnect

    ShareitSession *session_win;
    int status;
};

G_DEFINE_TYPE_WITH_PRIVATE(ShareitAppWindow, shareit_app_window, GTK_TYPE_APPLICATION_WINDOW);

static gboolean preferences_clicked(GtkButton *btn) {
    ShareitAppWindow *win;
    ShareitPrefs *prefs;

    win = SHAREIT_APP_WINDOW(gtk_widget_get_toplevel (GTK_WIDGET(btn)));
    prefs = shareit_prefs_new(win);
    gtk_window_present(GTK_WINDOW(prefs));
    return FALSE;
}

static gboolean btn_create_session_clicked_cb(GtkButton *btn) {
    ShareitAppWindow *win = SHAREIT_APP_WINDOW(gtk_widget_get_toplevel (GTK_WIDGET(btn)));
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);

    // Clear error text
    gtk_label_set_text(priv->lbl_error, "");

    const char *session_name = gtk_entry_get_text(priv->entry_session_name);
    if (strcmp(session_name, "") == 0) {
        gtk_label_set_text(priv->lbl_error, "Session name must be specified");
        return FALSE;
    }

    char *error;
    if (net_join_session(win->conn, session_name, &error) != 0) {
        gtk_label_set_text(priv->lbl_error, error);
        free(error);
    }
    return FALSE;
}

static void update_window(ShareitAppWindow *win, int status) {
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);

    priv->status = status;
    if (!gtk_widget_is_visible(GTK_WIDGET(win))) {
        gtk_widget_show(GTK_WIDGET(win));
    }

    if (status == STATUS_CONNECTED) {
        gtk_widget_show(GTK_WIDGET(priv->box_connected));
    } else {
        gtk_widget_hide(GTK_WIDGET(priv->box_connected));
    }

    if (status == STATUS_ERROR || status == STATUS_DISCONNECTED) {
        gtk_widget_show(GTK_WIDGET(priv->box_reconnect));
    } else {
        gtk_widget_hide(GTK_WIDGET(priv->box_reconnect));
    }
}

static int handle_conn_error(char *err, ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);

    gtk_label_set_text(priv->lbl_error, err);
    update_window(win, STATUS_ERROR);
    return TRUE;
}

static int handle_conn_connected(connection_t *conn, ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);

    char *txt = g_strdup_printf("Connected to <b>%s</b>", conn->hostname);
    gtk_label_set_markup(win->lbl_info, txt);
    g_free(txt);
    update_window(win, STATUS_CONNECTED);
    return TRUE;
}

static int handle_conn_session_joined(void *data, ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);
    priv->session_win = shareit_session_new(win);
    gtk_window_present(GTK_WINDOW(priv->session_win));
    gtk_widget_hide(GTK_WIDGET(win));
    g_signal_connect_swapped(priv->session_win, "destroy", (GCallback)gtk_widget_show, GTK_WIDGET(win));
    return TRUE;
}


static void shareit_app_window_init (ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv;
    priv = shareit_app_window_get_instance_private(win);
    gtk_widget_init_template(GTK_WIDGET(win));
    priv->settings = g_settings_new ("se.oddbike.shareit");

    update_window(win, STATUS_INITIALIZING);
    win->conn = net_new();

    net_signal_connect(win->conn, SIGNAL_ERROR, (handlefunc_t)handle_conn_error, win);
    net_signal_connect(win->conn, SIGNAL_CONNECTED, (handlefunc_t)handle_conn_connected, win);
    net_signal_connect(win->conn, SIGNAL_SESSION_JOINED, (handlefunc_t)handle_conn_session_joined, win);

    char *servername = g_settings_get_string(priv->settings, "server");
    if (strcmp(servername, "") != 0) {
        char *txt = g_strdup_printf("Connecting to <b>%s</b>...", servername);
        gtk_label_set_markup(win->lbl_info, txt);
        g_free(txt);

        char *err;
        if (net_connect(win->conn, "localhost", &err) < 0) {
            txt = g_strdup_printf("Error connecting <b>%s</b>: %s", servername, err);
            gtk_label_set_markup(priv->lbl_error, txt);
            g_free(txt);
        }
    } else {
        gtk_label_set_markup(win->lbl_info, "No server defined, check preferences.");
    }

    g_free(servername);
}

static void shareit_app_window_dispose(GObject *object)
{
    ShareitAppWindow *win;
    ShareitAppWindowPrivate *priv;

    win = SHAREIT_APP_WINDOW(object);
    priv = shareit_app_window_get_instance_private(win);
    if (win->conn != NULL) {
        net_free(win->conn);
        win->conn = NULL;
    }
    g_clear_object(&priv->settings);
    G_OBJECT_CLASS(shareit_app_window_parent_class)->dispose(object);
}


static void shareit_app_window_class_init (ShareitAppWindowClass *class) {
    G_OBJECT_CLASS(class)->dispose = shareit_app_window_dispose;
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/se/oddbike/shareit/main.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, btn_create_session);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, box_connected);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, box_reconnect);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, entry_session_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, lbl_error);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitAppWindow, lbl_info);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), preferences_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), btn_create_session_clicked_cb);
}

ShareitAppWindow * shareit_app_window_new(ShareitApp *app) {
    return g_object_new(SHAREIT_APP_WINDOW_TYPE, "application", app, NULL);
}