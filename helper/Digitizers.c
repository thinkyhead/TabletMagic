/**
 * Digitizers.c
 *
 * TabletMagic
 * Thinkyhead Software
 *
 * Utility functions to help retrieve a digitizer string from the IO Registry
 */

#include "Digitizers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/sysctl.h>

const char *known_machines[] = KNOWN_MACHINES;

char* get_digitizer_string() {
    int     result = EX_OK;
    char    *cmd, *tmp;

    // Find a digitizer in the IO Registry
    cmd = "ioreg -l | grep -A15 -E \"\\+-o (DIGI|WACM|COMA)\" | grep -m1 \\\"name\\\" | sed -E \"s/.*<\\\"?([^\\\">]+)\\\"?>/\\\\1/\"";
    char *digitizer_string = run_tool(cmd);

    if (digitizer_string != NULL) {
        // If not found then try an alternative method
        if (strlen(digitizer_string) == 0) {
            cmd = "ioreg -l | grep -E \"\\\"name\\\" = <\\\"(WAC|FUJ|FPI|574143|46554a|465049)[0-9A-F]+\\\">\" | sed -E \"s/.*<\\\"?([^\\\">]+)\\\"?>/\\\\1/\"";
            tmp = digitizer_string;
            digitizer_string = run_tool(cmd);
            free(tmp);
        }
    }

    //          asprintf(&digitizer_string, "WACF005");             // FOR TESTING ONLY!

    // If we got a digitizer string, continue
    if (digitizer_string != NULL) {
        clean_string(digitizer_string);

        // If the string was very long, convert hex to decimal
        if (strlen(digitizer_string) > 12) {
            result = asprintf(&cmd, "echo %s | xxd -r -p", digitizer_string);
            tmp = digitizer_string;
            digitizer_string = run_tool(cmd);
            clean_string(digitizer_string);
            free(tmp);
            free(cmd);
        }
    }

    return digitizer_string;
}

char is_known_machine(char **machine_string_ptr) {
    char is_known_mac = 0;
    char *machine_string = NULL;

    const char *machine_key = "hw.model";
    size_t size = 0;
    (void)sysctlbyname(machine_key, NULL, &size, NULL, 0);

    if (size > 0) {
        machine_string = (char*)malloc(size);
        (void)sysctlbyname(machine_key, machine_string, &size, NULL, 0);

#if !FORCE_TABLETPC

        // Test for a known mac platform
        int i;
        for (i=sizeof(known_machines)/sizeof(known_machines[0]); i--;) {
            if (!strncmp(machine_string, known_machines[i], strlen(known_machines[i]))) {
                is_known_mac = 1;
                break;
            }
        }

#endif
    }

    if (machine_string_ptr == NULL)
        free(machine_string);
    else
        *machine_string_ptr = machine_string;


    return is_known_mac;
}

// Run tool and return the result, or null
// Caller is responsible for freeing the result
char* run_tool(char *cmd) {
    char *commandOut = NULL;

    FILE *pipe = popen(cmd, "r");

    if (pipe != NULL) {
        commandOut = malloc(100);
        commandOut[0] = '\0';
        (void)fgets(commandOut, 100, pipe);
        pclose(pipe);
    }

    return commandOut;
}

void clean_string(char *string) {
    int len = (int)strlen(string);

    while (len-- && (string[len] == '\n' || string[len] == '\r'))
        string[len] = '\0';
}
