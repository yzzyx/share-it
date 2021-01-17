#ifndef SHAREIT_APP_H
#define SHAREIT_APP_H
#include <gtk/gtk.h>
#include "net.h"

// Macro to simplify getting widgets from builder
#define BUILDER_GET(out, type, name) out = type(gtk_builder_get_object(builder, name)); \
if (out == NULL) { \
	g_critical("Widget \"%s\" is missing in UI file", #name); \
}


typedef struct {
    gboolean share_screen;

    // Variables used in presentation mode
    void *grabber;
    int width;
    int height;

    uint32_t *current_screen;
    uint32_t *prev_screen;

    uint16_t mouse_pos_x;
    uint16_t mouse_pos_y;

    viewinfo_t *view;

    // Network settings
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
} shareit_app_t;

void show_error(shareit_app_t *app, const char *fmt, ...);
#endif