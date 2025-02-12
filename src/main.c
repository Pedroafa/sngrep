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
 * @file main.c
 * @author Ivan Alonso [aka Kaian] <kaian@irontec.com>
 *
 * @brief Source of initial functions used by sngrep
 *
 * @todo This should be coded properly. We could use use -f flag argument to
 * load the pcap file (because ngrep has no -f flag) and assume any other
 * argument are ngrep arguments. Anyway, actual main code is awful.
 */

#include <stdio.h>
#include <unistd.h>
#include "option.h"
#include "ui_manager.h"
#include "spcap.h"
#include "exec.h"

/**
 * @brief Usage function
 *
 * Print all available options (which are none actually)
 */
void
usage(const char* progname)
{
    fprintf(stdout, "[%s] Copyright (C) 2013 Irontec S.L.\n\n", progname);
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "\t%s <file.pcap>\n", progname);
#ifdef WITH_NGREP
    fprintf(stdout, "\t%s <ngrep options>\n", progname);
    fprintf(stdout, "\tsee 'man ngrep' for available ngrep options\n\n");
    fprintf(stdout, "Note: some ngrep options are forced by %s\n", progname);
#else
    fprintf(stdout, "\t%s <pcap filter>\n", progname);
#endif
}

/**
 * @brief Main function logic
 *
 * Parse command line options and start running threads
 *
 * @note There are no params actually... if you supply one
 *    param, I will assume we are running offline mode with
 *    a pcap file. Otherwise the args will be passed to ngrep
 *    without any type of validation.
 *
 */
int
main(int argc, char* argv[])
{

    int ret = 0;
    //! ngrep thread attributes
    pthread_attr_t attr;
    //! ngrep running thread
    pthread_t exec_t;

    // Initialize configuration options
    init_options();

    // Parse arguments.. I mean..
    if (argc < 2) {
        // No arguments!
        usage(argv[0]);
        return 1;
    } else if (argc == 2) {
        // Show offline mode in ui
        set_option_value("sngrep.mode", "Offline");
        set_option_value("sngrep.file", argv[1]);

        // Assume Offline mode with pcap file
        if (load_from_file(argv[1]) != 0) {
            fprintf(stderr, "Error loading data from pcap file %s\n", argv[1]);
            return 1;
        }
    } else {
        // Show online mode in ui
        set_option_value("sngrep.mode", "Online");

        // Assume online mode, launch ngrep in a thread
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&exec_t, &attr, (void *) online_capture, argv)) {
            fprintf(stderr, "Unable to create Exec Thread!\n");
            return 1;
        }
        pthread_attr_destroy(&attr);
    }

    // Initialize interface
    // This is a blocking call. Interface have user action loops.
    init_interface();

    // Delete the temporal file (if any)
    if (!is_option_enabled("sngrep.keeptmpfile") &&
        !is_option_disabled(get_option_value("sngrep.tmpfile"))) {
        unlink(get_option_value("sngrep.tmpfile"));
    }

    // Leaving!
    return ret;
}
