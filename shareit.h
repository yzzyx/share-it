#ifndef SHAREIT_APP_H
#define SHAREIT_APP_H
#include <gtk/gtk.h>
#include "net.h"

typedef struct {
    uint8_t *pixels; // pointer to buffer for all pixels
    int row_stride;  // n. of bytes for each row
    int width;       // width of view
    int height;      // height of view
} viewinfo_t;

typedef struct {
    gboolean share_screen;

    // Variables used in presentation mode
    void *grabber;
    int width;
    int height;

    uint32_t *block;
    uint32_t *current_screen;
    uint32_t *prev_screen;

    uint16_t mouse_pos_x;
    uint16_t mouse_pos_y;

    viewinfo_t *view;

    // Network settings
    connection_t *conn;
    char *host;
    GIOChannel  *channel;

    // Widgets
    GtkWidget *window;
    GtkWidget *btn_sharescreen;
    GtkWidget *btn_exit;
    GtkWidget *dlg_share_options;
    GtkWidget *dlg_select_session;
    GtkToggleButton *dlg_share_visible_checkbox;
    GtkToggleButton *dlg_share_public_checkbox;
    GtkEntry *dlg_share_password_entry;
    GtkEntry *dlg_select_session_entry;
    GtkWidget *dlg_connect;
    GtkComboBoxText *dlg_connect_server_dropdown;

    GtkWidget *screen_share_window;
    GtkDrawingArea *screen_share_area;
} shareit_app_t;

void show_error(shareit_app_t *app, const char *fmt, ...);
#endif