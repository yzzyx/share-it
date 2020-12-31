//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHARE_IT_APPWIN_H
#define SHARE_IT_APPWIN_H
#include <gtk/gtk.h>
#include "app.h"

struct _ShareitAppWindow
{
    GtkApplicationWindow parent;
    GtkLabel *lbl_info;
};

#define SHAREIT_APP_WINDOW_TYPE (shareit_app_window_get_type())
G_DECLARE_FINAL_TYPE(ShareitAppWindow, shareit_app_window, SHAREIT, APP_WINDOW, GtkApplicationWindow)

ShareitAppWindow *shareit_app_window_new(ShareitApp *app);

#endif //SHARE_IT_APPWIN_H
