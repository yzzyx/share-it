//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHARE_IT_VIEWER_H
#define SHARE_IT_VIEWER_H

typedef struct viewer_win viewer_win_t;

GtkWidget *viewer_initialize(shareit_app_t *app);

#endif //SHARE_IT_VIEWER_H
