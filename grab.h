//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHAREIT_GRAB_H
#define SHAREIT_GRAB_H

void *grab_initialize();
void grab_shutdown(void *);
int grab_window_size(void *, int *, int *);
int grab_window(void *, unsigned char *);
void grab_cursor_position(void *, int *x, int *y);
#endif