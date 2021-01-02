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
#include "net.h"

typedef struct _ShareitAppWindowPrivate ShareitAppWindowPrivate;

struct _ShareitAppWindowPrivate
{
    GSettings *settings;
    GtkButton *btn_create_session;
    GtkButton *btn_join_session;
    GtkBox *box_connected; // only shown if we're connected
    GtkBox *box_reconnect; // only shown if we can reconnect
    connection_t *conn;
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


#define STATUS_INITIALIZING 0
#define STATUS_CONNECTED 1
#define STATUS_DISCONNECTED 2
#define STATUS_ERROR 3

static void show_on_status(ShareitAppWindowPrivate  *priv, int status) {
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

    gtk_label_set_text(win->lbl_info, err);
    show_on_status(priv, STATUS_ERROR);
    return TRUE;
}

static int handle_conn_connected(char *err, ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv = shareit_app_window_get_instance_private(win);

    gtk_label_set_text(win->lbl_info, err);
    show_on_status(priv, STATUS_CONNECTED);
    return TRUE;
}


static void shareit_app_window_init (ShareitAppWindow *win) {
    ShareitAppWindowPrivate *priv;
    priv = shareit_app_window_get_instance_private(win);
    gtk_widget_init_template(GTK_WIDGET(win));
    priv->settings = g_settings_new ("se.oddbike.shareit");

    show_on_status(priv, STATUS_INITIALIZING);
    priv->conn = net_new();

    char *servername = g_settings_get_string(priv->settings, "server");
    if (strcmp(servername, "") != 0) {
        char *txt = g_strdup_printf("Connecting to <b>%s</b>...", servername);
        gtk_label_set_markup(win->lbl_info, txt);
        g_free(txt);

        char *err;
        if (net_connect(priv->conn, "localhost", &err) < 0) {
            txt = g_strdup_printf("Error connecting <b>%s</b>: %s", servername, err);
            gtk_label_set_markup(win->lbl_info, txt);
            g_free(txt);
        }
    } else {
        gtk_label_set_markup(win->lbl_info, "No server defined, check preferences.");
    }

    net_signal_connect(priv->conn, SIGNAL_ERROR, (handlefunc_t)handle_conn_error, priv);
    net_signal_connect(priv->conn, SIGNAL_CONNECTED, (handlefunc_t)handle_conn_connected, priv);
    g_free(servername);
}

static void shareit_app_window_dispose(GObject *object)
{
    ShareitAppWindow *win;
    ShareitAppWindowPrivate *priv;

    win = SHAREIT_APP_WINDOW(object);
    priv = shareit_app_window_get_instance_private(win);
    if (priv->conn != NULL) {
        net_free(priv->conn);
        priv->conn = NULL;
    }
    g_clear_object(&priv->settings);
    G_OBJECT_CLASS(shareit_app_window_parent_class)->dispose(object);
}


static void shareit_app_window_class_init (ShareitAppWindowClass *class) {
    G_OBJECT_CLASS(class)->dispose = shareit_app_window_dispose;
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/se/oddbike/shareit/main.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, btn_create_session);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, btn_join_session);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, box_connected);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitAppWindow, box_reconnect);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ShareitAppWindow, lbl_info);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), preferences_clicked);
}

ShareitAppWindow * shareit_app_window_new(ShareitApp *app) {
    return g_object_new(SHAREIT_APP_WINDOW_TYPE, "application", app, NULL);
}