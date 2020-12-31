//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHARE_IT_PREFERENCES_H
#define SHARE_IT_PREFERENCES_H
#include <gtk/gtk.h>
#include "app.h"

#define SHAREIT_PREFS_TYPE (shareit_prefs_get_type())
G_DECLARE_FINAL_TYPE(ShareitPrefs, shareit_prefs, SHAREIT, PREFS, GtkDialog)

ShareitPrefs *shareit_prefs_new(ShareitAppWindow *win);
#endif //SHARE_IT_PREFERENCES_H
