/* nautilus-preferences-window.c - Functions to create and show the nautilus
 *  preference window.
 *
 *  Copyright (C) 2002 Jan Arne Petersen
 *  Copyright (C) 2016 Carlos Soriano <csoriano@gnome.com>
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
 */

#include <config.h>

#include "nautilus-preferences-window.h"

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

#include <eel/eel-glib-extensions.h>

#include <nautilus-extension.h>

#include "nautilus-column-chooser.h"
#include "nautilus-column-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-module.h"

/* string enum preferences */
#define NAUTILUS_PREFERENCES_DIALOG_DEFAULT_VIEW_WIDGET "default_view_combobox"
#define NAUTILUS_PREFERENCES_DIALOG_PREVIEW_FILES_WIDGET                       \
    "preview_image_combobox"
#define NAUTILUS_PREFERENCES_DIALOG_PREVIEW_FOLDER_WIDGET                      \
    "preview_folder_combobox"

/* bool preferences */
#define NAUTILUS_PREFERENCES_DIALOG_FOLDERS_FIRST_WIDGET                       \
    "sort_folders_first_checkbutton"
#define NAUTILUS_PREFERENCS_DIALOG_START_WITH_SIDEBAR                          \
    "show_sidebar_checkbutton"
#define NAUTILUS_PREFERENCES_DIALOG_DELETE_PERMANENTLY_WIDGET                  \
    "show_delete_permanently_checkbutton"
#define NAUTILUS_PREFERENCES_DIALOG_CREATE_LINK_WIDGET                         \
    "show_create_link_checkbutton"
#define NAUTILUS_PREFERENCES_DIALOG_LIST_VIEW_USE_TREE_WIDGET                  \
    "use_tree_view_checkbutton"
#define NAUTILUS_PREFERENCES_DIALOG_TRASH_CONFIRM_WIDGET                       \
    "trash_confirm_checkbutton"
#define NAUTILUS_PREFERENCES_DIALOG_USE_NEW_VIEWS_WIDGET                       \
    "use_new_views_checkbutton"

/* int enums */
#define NAUTILUS_PREFERENCES_DIALOG_THUMBNAIL_LIMIT_WIDGET                     \
    "preview_image_size_spinbutton"

static const char * const speed_tradeoff_values[] =
{
    "local-only", "always", "never",
    NULL
};

static const char * const click_behavior_components[] =
{
    "single_click_radiobutton", "double_click_radiobutton", NULL
};

static const char * const click_behavior_values[] = {"single", "double", NULL};

static const char * const executable_text_components[] =
{
    "scripts_execute_radiobutton", "scripts_view_radiobutton",
    "scripts_confirm_radiobutton", NULL
};

static const char * const executable_text_values[] =
{
    "launch", "display", "ask",
    NULL
};

static const char * const recursive_search_components[] =
{
    "search_recursive_only_this_computer_radiobutton", "search_recursive_all_locations_radiobutton", "search_recursive_never_radiobutton", NULL
};

static const char * const thumbnails_components[] =
{
    "thumbnails_only_this_computer_radiobutton", "thumbnails_all_files_radiobutton", "thumbnails_never_radiobutton", NULL
};

static const char * const count_components[] =
{
    "count_only_this_computer_radiobutton", "count_all_files_radiobutton", "count_never_radiobutton", NULL
};

static GtkWidget *preferences_window = NULL;

static void columns_changed_callback(NautilusColumnChooser *chooser,
                                     gpointer               callback_data)
{
    char **visible_columns;
    char **column_order;

    nautilus_column_chooser_get_settings (NAUTILUS_COLUMN_CHOOSER (chooser),
                                          &visible_columns, &column_order);

    g_settings_set_strv (nautilus_list_view_preferences,
                         NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                         (const char * const *) visible_columns);
    g_settings_set_strv (nautilus_list_view_preferences,
                         NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                         (const char * const *) column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void set_columns_from_settings(NautilusColumnChooser *chooser)
{
    char **visible_columns;
    char **column_order;

    visible_columns = g_settings_get_strv (
        nautilus_list_view_preferences,
        NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
    column_order =
        g_settings_get_strv (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

    nautilus_column_chooser_set_settings (NAUTILUS_COLUMN_CHOOSER (chooser),
                                          visible_columns, column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void use_default_callback(NautilusColumnChooser *chooser,
                                 gpointer               user_data)
{
    g_settings_reset (nautilus_list_view_preferences,
                      NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
    g_settings_reset (nautilus_list_view_preferences,
                      NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
    set_columns_from_settings (chooser);
}

static void
nautilus_preferences_window_setup_list_column_page (GtkBuilder *builder)
{
    GtkWidget *chooser;
    GtkWidget *box;

    chooser = nautilus_column_chooser_new (NULL);
    g_signal_connect (chooser, "changed", G_CALLBACK (columns_changed_callback),
                      chooser);
    g_signal_connect (chooser, "use-default", G_CALLBACK (use_default_callback),
                      chooser);

    set_columns_from_settings (NAUTILUS_COLUMN_CHOOSER (chooser));

    gtk_widget_show (chooser);
    box = GTK_WIDGET (gtk_builder_get_object (builder, "list_columns_vbox"));

    gtk_box_pack_start (GTK_BOX (box), chooser, TRUE, TRUE, 0);
}

static gboolean
format_spin_button (GtkSpinButton *spin_button,
                    gpointer       user_data)
{
    gint value;
    gchar *text;

    value = gtk_spin_button_get_value_as_int (spin_button);
    text = g_strdup_printf (_("%d MB"), value);
    gtk_entry_set_text (GTK_ENTRY (spin_button), text);

    return TRUE;
}

static void nautilus_preferences_window_setup_thumbnail_limit_formatting (GtkBuilder *builder)
{
    GtkSpinButton *spin;

    spin = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "preview_image_size_spinbutton"));

    g_signal_connect (spin, "output", G_CALLBACK (format_spin_button),
                      spin);
}

static void bind_builder_bool(GtkBuilder *builder,
                              GSettings  *settings,
                              const char *widget_name,
                              const char *prefs)
{
    g_settings_bind (settings, prefs, gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

static void bind_builder_uint_spin(GtkBuilder *builder,
                                   GSettings  *settings,
                                   const char *widget_name,
                                   const char *prefs)
{
    g_settings_bind (settings, prefs, gtk_builder_get_object (builder, widget_name),
                     "value", G_SETTINGS_BIND_DEFAULT);
}

static GVariant *radio_mapping_set(const GValue       *gvalue,
                                   const GVariantType *expected_type,
                                   gpointer            user_data)
{
    const gchar *widget_value = user_data;
    GVariant *retval = NULL;

    if (g_value_get_boolean (gvalue))
    {
        retval = g_variant_new_string (widget_value);
    }

    return retval;
}

static gboolean radio_mapping_get(GValue   *gvalue,
                                  GVariant *variant,
                                  gpointer  user_data)
{
    const gchar *widget_value = user_data;
    const gchar *value;

    value = g_variant_get_string (variant, NULL);

    if (g_strcmp0 (value, widget_value) == 0)
    {
        g_value_set_boolean (gvalue, TRUE);
    }
    else
    {
        g_value_set_boolean (gvalue, FALSE);
    }

    return TRUE;
}

static void bind_builder_radio(GtkBuilder  *builder,
                               GSettings   *settings,
                               const char **widget_names,
                               const char  *prefs,
                               const char **values)
{
    GtkWidget *button;
    int i;

    for (i = 0; widget_names[i] != NULL; i++)
    {
        button = GTK_WIDGET (gtk_builder_get_object (builder, widget_names[i]));

        g_settings_bind_with_mapping (settings, prefs, button, "active",
                                      G_SETTINGS_BIND_DEFAULT, radio_mapping_get,
                                      radio_mapping_set, (gpointer) values[i], NULL);
    }
}

static void nautilus_preferences_window_setup(GtkBuilder *builder,
                                              GtkWindow  *parent_window)
{
    GtkWidget *window;

    /* setup preferences */
    bind_builder_bool (builder, gtk_filechooser_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_FOLDERS_FIRST_WIDGET,
                       NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    bind_builder_bool (builder, nautilus_window_state,
                       NAUTILUS_PREFERENCS_DIALOG_START_WITH_SIDEBAR,
                       NAUTILUS_WINDOW_STATE_START_WITH_SIDEBAR);
    bind_builder_bool (builder, nautilus_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_TRASH_CONFIRM_WIDGET,
                       NAUTILUS_PREFERENCES_CONFIRM_TRASH);
    bind_builder_bool (builder, nautilus_list_view_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_LIST_VIEW_USE_TREE_WIDGET,
                       NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);
    bind_builder_bool (builder, nautilus_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_CREATE_LINK_WIDGET,
                       NAUTILUS_PREFERENCES_SHOW_CREATE_LINK);
    bind_builder_bool (builder, nautilus_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_DELETE_PERMANENTLY_WIDGET,
                       NAUTILUS_PREFERENCES_SHOW_DELETE_PERMANENTLY);

    bind_builder_radio (
        builder, nautilus_preferences, (const char **) click_behavior_components,
        NAUTILUS_PREFERENCES_CLICK_POLICY, (const char **) click_behavior_values);
    bind_builder_radio (builder, nautilus_preferences,
                        (const char **) executable_text_components,
                        NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
                        (const char **) executable_text_values);
    bind_builder_radio (builder, nautilus_preferences,
                        (const char **) recursive_search_components,
                        NAUTILUS_PREFERENCES_RECURSIVE_SEARCH,
                        (const char **) speed_tradeoff_values);
    bind_builder_radio (builder, nautilus_preferences,
                        (const char **) thumbnails_components,
                        NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS,
                        (const char **) speed_tradeoff_values);
    bind_builder_radio (builder, nautilus_preferences,
                        (const char **) count_components,
                        NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                        (const char **) speed_tradeoff_values);

    bind_builder_uint_spin (builder, nautilus_preferences,
                            NAUTILUS_PREFERENCES_DIALOG_THUMBNAIL_LIMIT_WIDGET,
                            NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT);

    nautilus_preferences_window_setup_thumbnail_limit_formatting (builder);
    //nautilus_preferences_window_setup_icon_caption_page (builder);
    nautilus_preferences_window_setup_list_column_page (builder);

    /* UI callbacks */
    window = GTK_WIDGET (gtk_builder_get_object (builder, "preferences_window"));
    preferences_window = window;

    gtk_window_set_icon_name (GTK_WINDOW (preferences_window), APPLICATION_ID);

    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &preferences_window);

    gtk_window_set_transient_for (GTK_WINDOW (preferences_window), parent_window);

    /* This statement is necessary because GtkDialog defaults to 2px. Will not
     * be necessary in GTK+4.
     */
    gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (preferences_window))),
                                    0);

    gtk_widget_show (preferences_window);
}

void nautilus_preferences_window_show(GtkWindow *window)
{
    GtkBuilder *builder;

    if (preferences_window != NULL)
    {
        gtk_window_present (GTK_WINDOW (preferences_window));
        return;
    }

    builder = gtk_builder_new ();

    gtk_builder_add_from_resource (
        builder, "/org/gnome/nautilus/ui/nautilus-preferences-window.ui", NULL);

    nautilus_preferences_window_setup (builder, window);

    g_object_unref (builder);
}
