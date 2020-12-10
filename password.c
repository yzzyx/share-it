//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <stdlib.h>
#include <string.h>
#include "password.h"

// generate_pw generates a password
// to a static buffer, and returns a pointer to it.
// Do not change the contents of the returned data directly
const char *generate_password() {
    static char pw[7];
    const char *available="0123456789abcdefghijklmnopqrstuvwxyz-.";
    int i, available_len, pw_len;
    pw_len = sizeof(pw)-1;

    available_len = strlen(available);
    for (i = 0; i < pw_len; i++) {
        pw[i] = available[random() % available_len];
    }
    pw[pw_len] = '\0';

    return pw;
}