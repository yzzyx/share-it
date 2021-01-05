//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHARE_IT_SESSIONSHARE_H
#define SHARE_IT_SESSIONSHARE_H

#define SHAREIT_SCREENSHARE_TYPE (shareit_screenshare_get_type())
G_DECLARE_FINAL_TYPE(ShareitScreenshare, shareit_screenshare, SHAREIT, SCREENSHARE, GtkWindow)

ShareitScreenshare *shareit_screenshare_new(ShareitAppWindow *win);

#endif //SHARE_IT_SESSIONSHARE_H