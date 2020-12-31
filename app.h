//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHARE_IT_APP_H
#define SHARE_IT_APP_H
#include <gtk/gtk.h>

#define SHAREIT_APP_TYPE (shareit_app_get_type())
G_DECLARE_FINAL_TYPE (ShareitApp, shareit_app, SHAREIT, APP, GtkApplication)
ShareitApp     *shareit_app_new(void);

#endif //SHARE_IT_APP_H
