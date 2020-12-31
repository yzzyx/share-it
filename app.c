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
#include "net.h"
#include "packet.h"

struct _ShareitApp {
    GtkApplication parent;
    ShareitAppWindow *win;
};

guint signal_connecting = 0;

G_DEFINE_TYPE(ShareitApp, shareit_app, GTK_TYPE_APPLICATION);

static void shareit_app_init (ShareitApp *app) {
//    g_signal_connect (app, "connecting", (GCallback) connected_cb, app);
}

static void shareit_app_activate (GApplication *ga) {
    ShareitApp *app;

    app = SHAREIT_APP(ga);
    app->win = shareit_app_window_new(app);
    gtk_window_present(GTK_WINDOW(app->win));
}

static void shareit_app_class_init (ShareitAppClass *class) {
    G_APPLICATION_CLASS (class)->activate = shareit_app_activate;
    signal_connecting = g_signal_newv ("connecting",
                   G_TYPE_FROM_CLASS(class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                   NULL /* closure */,
                   NULL /* accumulator */,
                   NULL /* accumulator data */,
                   NULL /* C marshaller */,
                   G_TYPE_NONE /* return_type */,
                   0     /* n_params */,
                   NULL  /* param_types */);

}

ShareitApp *shareit_app_new (void) {
    ShareitApp *app =  g_object_new (SHAREIT_APP_TYPE,
                         "application-id", "se.oddbike.shareit",
                         NULL);

    return app;
}