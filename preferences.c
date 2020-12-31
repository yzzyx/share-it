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
#include <pwd.h>
#include "app.h"
#include "appwin.h"
#include "preferences.h"

struct _ShareitPrefs
{
    GtkDialog parent;
};

typedef struct _ShareitPrefsPrivate ShareitPrefsPrivate;

struct _ShareitPrefsPrivate
{
    GSettings *settings;
    GtkEntry *entry_server;
    GtkEntry *entry_username;
};

G_DEFINE_TYPE_WITH_PRIVATE(ShareitPrefs, shareit_prefs, GTK_TYPE_DIALOG);


static gboolean save_clicked(GtkButton *btn) {
    ShareitPrefs *prefs;
    ShareitPrefsPrivate *priv;

    prefs = SHAREIT_PREFS(gtk_widget_get_toplevel (GTK_WIDGET(btn)));
    priv = shareit_prefs_get_instance_private(prefs);

    g_settings_set_string(priv->settings, "server", gtk_entry_get_text(priv->entry_server));
    g_settings_set_string(priv->settings, "username", gtk_entry_get_text(priv->entry_username));

    gtk_widget_destroy(GTK_WIDGET(prefs));
    return FALSE;
}

static gboolean cancel_clicked(GtkButton *btn) {
    ShareitPrefs *prefs;
    prefs = SHAREIT_PREFS(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
    gtk_widget_destroy(GTK_WIDGET(prefs));
    return FALSE;
}

static void shareit_prefs_init(ShareitPrefs *prefs) {
    ShareitPrefsPrivate *priv;

    priv = shareit_prefs_get_instance_private(prefs);
    gtk_widget_init_template(GTK_WIDGET(prefs));
    priv->settings = g_settings_new ("se.oddbike.shareit");

    char *server = g_settings_get_string(priv->settings, "server");
    char *username = g_settings_get_string(priv->settings, "username");

    if (strcmp(username, "") == 0) {
        username = strdup(g_get_user_name());
    }

    gtk_entry_set_text(priv->entry_server, server);
    gtk_entry_set_text(priv->entry_username, username);

    free(server);
    free(username);
}

static void shareit_prefs_dispose(GObject *object) {
    ShareitPrefs *win;
    ShareitPrefsPrivate *priv;

    win = SHAREIT_PREFS(object);
    priv = shareit_prefs_get_instance_private(win);
    g_clear_object(&priv->settings);
    G_OBJECT_CLASS(shareit_prefs_parent_class)->dispose(object);
}

static void shareit_prefs_class_init(ShareitPrefsClass *class) {
    G_OBJECT_CLASS (class)->dispose = shareit_prefs_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/se/oddbike/shareit/prefs.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitPrefs, entry_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class), ShareitPrefs, entry_username);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), save_clicked);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), cancel_clicked);
}

ShareitPrefs *shareit_prefs_new(ShareitAppWindow *win) {
    return g_object_new(SHAREIT_PREFS_TYPE, "transient-for", win, NULL);
}