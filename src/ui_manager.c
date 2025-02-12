/**************************************************************************
 **
 ** sngrep - SIP callflow viewer using ngrep
 **
 ** Copyright (C) 2013 Ivan Alonso (Kaian)
 ** Copyright (C) 2013 Irontec SL. All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ****************************************************************************/
/**
 * @file ui_manager.c
 * @author Ivan Alonso [aka Kaian] <kaian@irontec.com>
 *
 * @brief Source of functions defined in ui_manager.h
 *
 */
#include "option.h"
#include "ui_manager.h"
#include "ui_call_list.h"
#include "ui_call_flow.h"
#include "ui_call_raw.h"
#include "ui_filter.h"
#include "ui_save_pcap.h"
#include "ui_save_raw.h"

/**
 * @brief Warranty thread-safe ui refresh
 *
 * This lock should be used whenever the ui is replaced or updated
 * preventing the ui swap during the screen refresh
 */
pthread_mutex_t refresh_lock;

/**
 * @brief Available panel windows list
 *
 * This list contein the available list of windows
 * and pointer to their main functions.
 *
 * XXX If the panel count increase a lot, it will be required to
 *     load panels as modules and provide a way to register
 *     themselfs into the panel pool dynamically.
 */
static ui_t panel_pool[] = {
    {
        .type = MAIN_PANEL,
        .panel = NULL,
        .create = call_list_create,
        .redraw_required = call_list_redraw_required,
        .draw = call_list_draw,
        .handle_key = call_list_handle_key,
        .help = call_list_help,
        .destroy = call_list_destroy, },
    {
        .type = DETAILS_PANEL,
        .panel = NULL,
        .create = call_flow_create,
        .redraw_required = call_flow_redraw_required,
        .draw = call_flow_draw,
        .handle_key = call_flow_handle_key,
        .help = call_flow_help },
    {
        .type = RAW_PANEL,
        .panel = NULL,
        .create = call_raw_create,
        .redraw_required = call_raw_redraw_required,
        .draw = call_raw_draw,
        .handle_key = call_raw_handle_key },
    {
        .type = FILTER_PANEL,
        .panel = NULL,
        .create = filter_create,
        .handle_key = filter_handle_key,
        .destroy = filter_destroy },
    {
        .type = SAVE_PANEL,
        .panel = NULL,
        .create = save_create,
        .handle_key = save_handle_key,
        .destroy = save_destroy },
    {
        .type = SAVE_RAW_PANEL,
        .panel = NULL,
        .create = save_raw_create,
        .handle_key = save_raw_handle_key,
        .destroy = save_raw_destroy } };

int
init_interface()
{
    // Initialize curses
    if (!initscr()) {
        fprintf(stderr, "Unable to initialize ncurses mode.\n");
        return -1;
    }
    cbreak();

    // Dont write user input on screen
    noecho();
    // Hide the cursor
    curs_set(0);
    // Only delay ESC Sequences 25 ms (we dont want Escape sequences)
    ESCDELAY = 25;
    start_color();
    toggle_color(is_option_enabled("color"));

    // Start showing call list
    wait_for_input(ui_create(ui_find_by_type(MAIN_PANEL)));

    // End ncurses mode
    endwin();
    return 0;
}

ui_t *
ui_create(ui_t *ui)
{
    // If already has a panel, just return it
    if (ui_get_panel(ui)) return ui;

    // Otherwise, request the ui to create the panel
    if (ui->create) {
        ui->panel = ui->create();
    }

    // And return it
    return ui;
}

void
ui_destroy(ui_t *ui)
{
    PANEL *panel;
    // If thre is no ui panel, we're done
    if (!(panel = ui_get_panel(ui))) return;

    // If panel has a destructor function use it
    if (ui->destroy) ui->destroy(panel);

    // Initialize panel pointer
    ui->panel = NULL;
}

PANEL *
ui_get_panel(ui_t *ui)
{
    // Return panel pointer of ui struct
    return (ui) ? ui->panel : NULL;
}

int
ui_redraw_required(ui_t *ui, sip_msg_t *msg)
{
    int ret = 0;
    //! Sanity check, this should not happen
    if (!ui) return -1;
    pthread_mutex_lock(&ui->lock);
    // Request the panel to draw on the scren
    if (ui->redraw_required) {
        ret = ui->redraw_required(ui_get_panel(ui), msg);
    }
    pthread_mutex_unlock(&ui->lock);
    // If no redraw capabilities, never redraw
    return ret;
}

int
ui_draw_panel(ui_t *ui)
{
    //! Sanity check, this should not happen
    if (!ui) return -1;

    pthread_mutex_lock(&ui->lock);

    // Create the panel if it does not exist
    if (!(ui_create(ui))) return -1;

    // Make this panel the topmost panel
    top_panel(ui_get_panel(ui));

    // Request the panel to draw on the scren
    if (ui->draw) {
        if (ui->draw(ui_get_panel(ui)) != 0) {
            pthread_mutex_unlock(&ui->lock);
            return -1;
        }
    }
    // Update panel stack
    update_panels();
    doupdate();
    pthread_mutex_unlock(&ui->lock);
    return 0;
}

void
ui_help(ui_t *ui)
{
    // If current ui has help function
    if (ui->help) {
        ui->help(ui_get_panel(ui));
    }
}

int
ui_handle_key(ui_t *ui, int key)
{
    if (ui->handle_key) return ui->handle_key(ui_get_panel(ui), key);
    return -1;
}

ui_t *
ui_find_by_panel(PANEL *panel)
{
    int i;
    // Panel pool is fixed size
    int panelcnt = sizeof(panel_pool) / sizeof(ui_t);
    // Return ui pointer if found
    for (i = 0; i < panelcnt; i++) {
        if (panel_pool[i].panel == panel) return &panel_pool[i];
    }
    return NULL;
}

ui_t *
ui_find_by_type(int type)
{
    int i;
    // Panel pool is fixed size
    int panelcnt = sizeof(panel_pool) / sizeof(ui_t);
    // Return ui pointer if found
    for (i = 0; i < panelcnt; i++) {
        if (panel_pool[i].type == type) return &panel_pool[i];
    }
    return NULL;
}

int
wait_for_input(ui_t *ui)
{
    ui_t *replace;
    WINDOW *win;

    // Keep getting keys until panel is destroyed
    while (ui_get_panel(ui)) {
        pthread_mutex_lock(&refresh_lock);
        // If ui requested replacement
        if (ui->replace) {
            replace = ui->replace;
            ui->replace = NULL;
            ui_destroy(ui);
            ui = replace;
        }
        pthread_mutex_unlock(&refresh_lock);

        if (ui_draw_panel(ui) != 0) return -1;

        // Enable key input on current panel
        win = panel_window(ui_get_panel(ui));
        keypad(win, TRUE);

        // Get pressed key
        int c = wgetch(win);

        // Check if current panel has custom bindings for that key
        if ((c = ui_handle_key(ui, c)) == 0) continue;

        // Otherwise, use standard keybindings
        switch (c) {
        case 'c':
            // @todo general application config structure
            toggle_option("color");
            toggle_color(is_option_enabled("color"));
            break;
        case 'C':
            if (is_option_enabled("color.request")) {
                toggle_option("color.request");
                toggle_option("color.callid");
            } else if (is_option_enabled("color.callid")) {
                toggle_option("color.callid");
                toggle_option("color.cseq");
            } else if (is_option_enabled("color.cseq")) {
                toggle_option("color.cseq");
                toggle_option("color.request");
            }
            break;
        case 'p':
            // Toggle capture option
            toggle_option("sip.capture");
            break;
        case 'h':
        case 265: /* KEY_F1 */
            ui_help(ui);
            break;
        case 'q':
        case 'Q':
        case 27: /* KEY_ESC */
            ui_destroy(ui);
            return 0;
            break;
        }
    }

    return -1;
}

void
toggle_color(int on)
{
    if (on) {
        // Initialize some colors
        init_pair(HIGHLIGHT_COLOR, COLOR_WHITE, COLOR_BLUE);
        init_pair(HELP_COLOR, COLOR_CYAN, COLOR_BLACK);
        init_pair(OUTGOING_COLOR, COLOR_RED, COLOR_BLACK);
        init_pair(INCOMING_COLOR, COLOR_GREEN, COLOR_BLACK);
        init_pair(DETAIL_BORDER_COLOR, COLOR_BLUE, COLOR_BLACK);
        init_pair(CALLID1_COLOR, COLOR_CYAN, COLOR_BLACK);
        init_pair(CALLID2_COLOR, COLOR_YELLOW, COLOR_BLACK);
        init_pair(CALLID3_COLOR, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(CALLID4_COLOR, COLOR_GREEN, COLOR_BLACK);
        init_pair(CALLID5_COLOR, COLOR_RED, COLOR_BLACK);
        init_pair(CALLID6_COLOR, COLOR_BLUE, COLOR_BLACK);
        init_pair(CALLID7_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(SELECTED_COLOR, COLOR_WHITE, COLOR_BLACK);
    } else {
        init_pair(HIGHLIGHT_COLOR, COLOR_BLACK, COLOR_WHITE);
        init_pair(HELP_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(OUTGOING_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(INCOMING_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(DETAIL_BORDER_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID1_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID2_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID3_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID4_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID5_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID6_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(CALLID7_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(SELECTED_COLOR, COLOR_WHITE, COLOR_BLACK);
    }
}

void
ui_new_msg_refresh(sip_msg_t *msg)
{
    PANEL *panel;

    pthread_mutex_lock(&refresh_lock);
    // Get the topmost panel
    if ((panel = panel_below(NULL))) {
        // Get ui information for that panel
        if (ui_redraw_required(ui_find_by_panel(panel), msg) == 0) {
            // If ui needs to be update, redraw it
            ui_draw_panel(ui_find_by_panel(panel));
        }
    }
    pthread_mutex_unlock(&refresh_lock);
}

void
title_foot_box(WINDOW *win)
{
    int height, width;

    // Get window size
    getmaxyx(win, height, width);
    box(win, 0, 0);
    mvwaddch(win, 2, 0, ACS_LTEE);
    mvwhline(win, 2, 1, ACS_HLINE, width - 2);
    mvwaddch(win, 2, width - 1, ACS_RTEE);
    mvwaddch(win, height - 3, 0, ACS_LTEE);
    mvwhline(win, height - 3, 1, ACS_HLINE, width - 2);
    mvwaddch(win, height - 3, width - 1, ACS_RTEE);

}

int
ui_set_replace(ui_t *original, ui_t *replace)
{
    pthread_mutex_lock(&refresh_lock);
    if (!original || !replace) return -1;
    original->replace = replace;
    return 0;
    pthread_mutex_unlock(&refresh_lock);
}
