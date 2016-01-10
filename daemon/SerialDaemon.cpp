/**
 * SerialDaemon.cpp
 *
 * TabletMagicDaemon
 * Thinkyhead Software
 *
 * This program is a component of TabletMagic. See the
 * accompanying documentation for more details about the
 * TabletMagic project.
 *
 * Startup Stages:
 *  1. Set up signal handlers so the daemon can quit cleanly.
 *  2. Create a WacomTablet object to represent a virtual tablet.
 *  3. Create a message port for the preference pane if running in command mode.
 *  4. Create a CFTimer to poll the serial port for data.
 *  5. Look for a tablet on the specified serial port (or all ports).
 *  6. Bail out if no tablet found (unless running in "command mode")
 *
 * Tablet Decoding:
 *  - Continuously read the datastream from the serial port.
 *  - Maintain a state representation of stylus and tablet.
 *  - Generate system mouse and tablet events in sync with the internal state.
 *  - Message the preference pane as requested and on spurious events.
 *
 * Current Development:
 *  - Tested with several tablets and adapters
 *  - Fully implemented II-S (ASCII and Binary), Wacom IV, Wacom V, and TabletPC
 *  - Adapted for use with PenPartner (CT) tablets
 *  - Adapted for Calcomp tablets
 *
 * In Progress:
 *  - Handle screen resolution changes in the preference pane
 *    (Looks like System Preferences swallows the notification!)
 *
 * LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SerialDaemon.h"
#include "TMSerialPort.h"

extern "C" {
#include "Digitizers.h"
}

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/hidsystem/IOHIDShared.h>

#include <syslog.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#define FOUR_CHAR(x)        ((x) >> 24) & 0xFF, ((x) >> 16) & 0xFF, ((x) >> 8) & 0xFF, (x) & 0xFF

#define EVENT_TIMER_IS_SEPARATE 0
#define PRESSURE_SCALE  65535.0
#define TILT_SCALE      32767.0
#define LOG_FILE        "/Users/Shared/tabletmagic.log"
#define POST_EVENT      PostCGEvent

enum {
    kToolNone       = 0,
    kToolInkingPen  = 0x0812,               // Intuos2 ink pen XP-110-00A
    kToolInkingPen2 = 0x0012,               // Inking pen
    kToolPen1       = 0x0822,               // Intuos Pen GP-300E-01H
    kToolPen2       = 0x0022,
    kToolPen3       = 0x0842,               // added from Cheng
    kToolGripPen    = 0x0852,               // Intuos2 Grip Pen XP-501E-00A
    kToolStrokePen1 = 0x0832,               // Intuos2 stroke pen XP-120-00A
    kToolStrokePen2 = 0x0032,               // Stroke pen
    kToolMouse2D    = 0x0007,               // 2D Mouse
    kToolMouse3D    = 0x009C,               // ?? Mouse (Not really sure)
    kToolMouse4D    = 0x0094,               // 4D Mouse
    kToolLens       = 0x0096,               // Lens cursor
    kToolEraser1    = 0x082A,
    kToolEraser2    = 0x085A,
    kToolEraser3    = 0x091A,
    kToolEraser4    = 0x00FA,               // Eraser
    kToolAirbrush   = 0x0112                // Airbrush
};

enum {
    kToolTypeNone = 0,
    kToolTypePen,
    kToolTypePencil,
    kToolTypeBrush,
    kToolTypeEraser,
    kToolTypeAirbrush,
    kToolTypeMouse,
    kToolTypeLens
};

char*   LogString(char *str);
void    ShortSleep();
bool    GetIntArgument(char *arg, int flag, int *dest);
bool    GetFloatArgument(char *arg, int flag, float *dest);
bool    process_arguments(int argc, char *argv[]);
void    usage();
void    signal_handler(int sig);
char*   ReadablePacket(char *packet, int pack_size);
char*   HexString(char *s, int size);
bool    UpdateDisplaysBounds();

void postNullEvent();
void postTabletEvent(SInt16 x, SInt16 y);

bool    quitProcessor;              // Flag to quit the processor.
FILE    *output = stderr;

// To calculate the byte rate:
// 1. count every byte received
// 2. Every second report that number and reset it to 0.
// 3. (Bits per second is bytes*10)
int byteCounter;
int packetCounter;
int bytesPerSecond;
int packetsPerSecond;
int stream_pause;       // The stream is paused for an interval

#if LOG_STREAM_TO_FILE
FILE    *logfile = NULL;
#endif

// Flag indicates Mac OS X is running on a PC
bool    hackintosh = false;

typedef struct SeriesDescription {
    const char  *code;
    const char  *name;
    SeriesIndex index;
} SeriesDescription;

SeriesDescription series_list[] = {
    { kTabletModelUnknown,      "Unknown",          kModelUnknown       },
    { kTabletModelIntuos2,      "Intuos 2",         kModelIntuos2       },
    { kTabletModelIntuos,       "Intuos",           kModelIntuos        },
    { kTabletModelGraphire3,    "Graphire 3",       kModelGraphire3     },
    { kTabletModelGraphire2,    "Graphire 2",       kModelGraphire2     },
    { kTabletModelGraphire,     "Graphire",         kModelGraphire      },
    { kTabletModelCintiq,       "Cintiq",           kModelCintiq        },
    { kTabletModelCintiqPartner,"Cintiq Partner",   kModelCintiqPartner },
    { kTabletModelArtZ,         "ArtZ / ArtZ-II",   kModelArtZ          },
    { kTabletModelArtPad,       "ArtPad",           kModelArtPad        },
    { kTabletModelPenPartner,   "PenPartner",       kModelPenPartner    },
    { kTabletModelSDSeries,     "SD Series",        kModelSDSeries      },
    { kTabletModelTabletPC,     "TabletPC",         kModelTabletPC      },
    { kTabletModelTabletPC,     "Fujitsu P Series", kModelFujitsuP      },
    { kTabletModelCalComp,      "CalComp",          kModelCalComp       },

    // These are redundant and should never match:
    { kTabletModelPLSeries,     "PL Series",        kModelPLSeries      },
    { kTabletModelUDSeries,     "UD Series",        kModelUDSeries      }
};


const char *sbuttons[] = { "Tip", "Button 1", "Button 2", "Eraser" };
const char *mbuttons[] = { "Disabled", "Left Button", "Right Button", "The Eraser", "Doubleclick", "Single Click", "Control Click", "Click-Hold", "Button 3", "Button 4", "Button 5" };

enum {
    // Main control commands
    PREF_SELECT_PORT    = 1000,
    PREF_REINIT_PORT,
    PREF_START,
    PREF_STOP,

    // Information requests
    PREF_NEXT_MESSAGE,
    PREF_PREFS_HELLO,
    PREF_PREFS_BYE,

    PREF_GET_INFO,
    PREF_GET_MODEL,
    PREF_GET_SCALE,
    PREF_GET_MEMORY_BANK,
    PREF_GET_GEOMETRY,
    PREF_GET_SERIALPORT,

    // Event streaming
    PREF_STREAM,
    PREF_DATASTREAM_START,
    PREF_DATASTREAM_STOP,

    // Pass a command directly to the tablet
    PREF_SEND_COMMAND,
    PREF_SEND_REQUEST,

    // Set all parameters at once
    PREF_SET_SETUP,
    PREF_SET_MEMORY,

    PREF_SET_REZ,
    PREF_SET_SCALE,

    PREF_SET_GEOMETRY,
    PREF_SET_MOUSE_MODE_AND_SCALING,

    PREF_PANIC,
    PREF_QUIT,

    PREF_TABLETPC
};

typedef struct TMCommandNode {
    const char* token_string;
    int         code_number;
} TMCommandNode;

static TMCommandNode TMParseData[] = {
    { "stream", PREF_STREAM },          // Respond with a stream packet
    { "next", PREF_NEXT_MESSAGE },      // Respond with the next queue item

    { "setup", PREF_SET_SETUP },        // Send a setup string to the tablet
    { "scale", PREF_SET_SCALE },        // Send scale values to the tablet
    { "geom", PREF_SET_GEOMETRY },  // Set the geometry and mapping
    { "mmode", PREF_SET_MOUSE_MODE_AND_SCALING },   // Set mouse mode and scaling
    { "reinit", PREF_REINIT_PORT },     // Refresh the serial port based on a setup string
    { "stron", PREF_DATASTREAM_START }, // Start stream logging
    { "stroff", PREF_DATASTREAM_STOP }, // Stop stream logging
    { "start", PREF_START },            // Start sending events to the system
    { "stop", PREF_STOP },              // Stop sending events to the system
    { "port", PREF_SELECT_PORT },       // Talk to the tablet on the given serial port

    // Queries that respond right away
    { "?info", PREF_GET_INFO },         // Respond with bank#, setup string, and active state
    { "?geom", PREF_GET_GEOMETRY },     // Respond with mapping info
    { "?model", PREF_GET_MODEL },       // Respond with model info
    { "?scale", PREF_GET_SCALE },       // Respond with tablet scale info
    { "?bank", PREF_GET_MEMORY_BANK },  // Respond (soon) with bank setup string
    { "?port", PREF_GET_SERIALPORT },   // Respond with the original port setting

    // Pass a command directly to the tablet
    { "command", PREF_SEND_COMMAND },
    { "request", PREF_SEND_REQUEST },

    { "setmem", PREF_SET_MEMORY },      // Set all parameters at once

    { "quit", PREF_QUIT },              // We've been asked to quit

    { "panic", PREF_PANIC },            // The panic button was pressed

    { "hello", PREF_PREFS_HELLO },      // Preferences started or stopped
    { "bye", PREF_PREFS_BYE },

    { "tabletpc", PREF_TABLETPC }
};

//
// These bits indicate which fields of a point event have valid data.
//
// These values come from Apple sample tablet driver code, but are
// not yet defined in their headers.
//
// These correspond exactly to the values in Wacom.h
// These will remain here until Apple determines the final naming.
// See InitStylus for usage of these bits.
//
/*
 #define kEventHasDeviceIDBit            0
 #define kEventHasAbsoluteXBit           1
 #define kEventHasAbsoluteYBit           2
 #define kEventHasButtonsBit             6
 #define kEventHasTiltXBit               7
 #define kEventHasTiltYBit               8
 #define kEventHasAbsoluteZBit           9
 #define kEventHasPressureBit            10
 #define kEventHasTangentialPressureBit  11
 #define kEventHasRotationBit            13

 #define kEventHasDeviceIDMask       1 << kEventHasDeviceIDBit
 #define kEventHasAbsoluteXMask      1 << kEventHasAbsoluteXBit
 #define kEventHasAbsoluteYMask      1 << kEventHasAbsoluteYBit
 #define kEventHasButtonsMask        1 << kEventHasButtonsBit
 #define kEventHasTiltXMask          1 << kEventHasTiltXBit
 #define kEventHasTiltYMask          1 << kEventHasTiltYBit
 #define kEventHasAbsoluteZMask      1 << kEventHasAbsoluteZBit
 #define kEventHasPressureMask       1 << kEventHasPressureBit
 #define kEventHasTangentialPressureMask 1 << kEventHasTangentialPressureBit
 #define kEventHasRotationMask       1 << kEventHasRotationBit
 */

CGRect  screenBounds;

init_arguments args;

int     button_mapping[] = { kSystemButton1, kSystemButton1, kSystemButton2, kSystemEraser };

#define SetButtons(x)       {stylus.button_click=((x)!=0);stylus.button_mask=x;int qq;for(qq=kButtonMax;qq--;stylus.button[qq]=((x)&(1<<qq))!=0);}
#define ResetButtons        SetButtons(0)

// A global tablet instance
WacomTablet     *tablet;
extern int errno;

int set_suid_root(char *path);

#pragma mark -

//
// main(argc, argv)
//

//#include "TMServer.h"

int main(int argc, char *argv[]) {
    int outErr = EX_OK;

    /*
     TMServer *server = NULL;

     try {
     server = new TMServer();
     }
     catch (int err) {
     fprintf(output, "[SOCK] Server failed with error: %d\n", err);
     }
     */

    if (process_arguments(argc, argv))
        usage();
    else {
        //
        // If running as super-user set myself SUID root
        //
        if (geteuid() == 0)
            (void)set_suid_root(argv[0]);


        //
        // Run as a daemon unless quitting
        //
        // Add the -d flag when launching this binary directly.
        // It will be started on 10.4 and later by launchd without the -d flag.
        //
        if (args.dodaemon && !args.quit) {
            int isdaemon = daemon(0, 1);
            if (isdaemon != 0)
                syslog(LOG_ERR, "TabletMagicDaemon failed to daemonize!");
        }


        //
        // The logging version redirects output to a file
        //
#if LOG_STREAM_TO_FILE
        output = logfile = fopen(LOG_FILE, "w");

        int file;
        if ((file = open(LOG_FILE, O_NONBLOCK|O_RDONLY|O_SHLOCK, 0))) {
            struct stat st;
            if (!fstat(file, &st))
                fchmod(file, st.st_mode|S_IRWXU|S_IRWXG|S_IRWXO);

            if (st.st_gid != 80)
                fchown(file, 501, 80);

            close(file);
        }
#endif


        //
        // Set signal handlers to exit cleanly if killed
        //
        // TODO: The UNIX Domain Sockets code includes a method to have
        // the signal handler pass its structure through a socket and
        // remain within the context of the run loop.
        //
        // Consider using this method instead of the current one.
        //
        struct sigaction sa, oldquit, oldterm, oldstop, oldhup, oldabrt, oldint;

        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGHUP,   &sa,    &oldhup);       // HUP should restart with the latest config (but there is no config yet)
        sigaction(SIGQUIT,  &sa,    &oldquit);      // The rest of these will just quit
        sigaction(SIGTERM,  &sa,    &oldterm);
        sigaction(SIGSTOP,  &sa,    &oldstop);
        sigaction(SIGABRT,  &sa,    &oldabrt);
        sigaction(SIGINT,   &sa,    &oldint);


        //
        // Print the title and credits
        //
        fprintf(output, "TabletMagicDaemon v" TABLETMAGIC_VERSION);
#if LOG_STREAM_TO_FILE
        fprintf(output, " (logging version)");
#endif
        fprintf(output, "\n(c) 2016 Thinkyhead Software <www.thinkyhead.com>\n\n");


        //
        // Set the process priority to the highest level
        //
        if (args.priority != 0) {
            errno = 0;
            int result = setpriority(PRIO_PROCESS, 0, args.priority);

            if (!args.quiet) {
                fprintf(output, "[INIT] renice %d %d : ", args.priority, getpid());
                if (result == 0)
                    fprintf(output, "Succeeded\n\n");
                else
                    fprintf(output, "Failed with code %d\n\n", result);
            }
        }

        // Get a digitizer string if there is one. Only TabletPCs should have them.
        args.digi = get_digitizer_string();
        Boolean has_digitizer_entry = (args.digi && strlen(args.digi));
        /*
         char *known_digis[] = KNOWN_DIGIS;
         for (int i=sizeof(known_digis)/sizeof(known_digis[0]); i--;) {
         if (!strncmp(args.digi, known_digis[i], strlen(known_digis[i]))) {
         has_digitizer_entry = true;
         break;
         }
         }
         */
        //
        // Detect the hardware platform
        //
        char *machine_string = NULL;
        Boolean is_known_mac = is_known_machine(&machine_string);

        hackintosh = has_digitizer_entry || !is_known_mac;

        if (machine_string) {
            fprintf(output, "[INIT] Machine Type: %s%s\n", machine_string, is_known_mac ? "" : " (Hackintosh?)");
            free(machine_string);
        }

        if (has_digitizer_entry)
            fprintf(output, "[INIT] Digitizer ID: %s\n", args.digi);


        //
        // Get the combined bounds of all displays
        //
        UpdateDisplaysBounds();


        try {
            // Instantiate the tablet object
            tablet = new WacomTablet(args);

            // Go into the event loop until the daemon is quit.
            tablet->RunEventLoop();

            if (!args.quiet)
                fprintf(output, "\n\nTabletMagicDaemon shutting down\n");

#if ASYNCHRONOUS_MESSAGES
            if (args.command) {
                tablet->SendMessage("[bye]");
                (void)usleep(250000);   // 0.25 seconds
            }
#endif
        }
        catch (const char *err) {
            fprintf(output, "[ERR ] Fatal Error: %s\n", err);
            outErr = EX_UNAVAILABLE;
        }

        delete tablet;

        //
        // Restore the original signal handlers
        //
        sigaction(SIGHUP,   &oldhup,    NULL);
        sigaction(SIGQUIT,  &oldquit,   NULL);
        sigaction(SIGTERM,  &oldterm,   NULL);
        sigaction(SIGSTOP,  &oldstop,   NULL);
        sigaction(SIGABRT,  &oldabrt,   NULL);
        sigaction(SIGINT,   &oldint,    NULL);
    }

    /*
     if (server)
     delete server;
     */

    if (args.port) free(args.port);
    if (args.init) free(args.init);

    return outErr;
}

//
// set_suid_root
//
int set_suid_root(char *path) {
    struct stat st;
    int fd_tool;

    /* Open tool exclusively, so noone can change it while we bless it */
    fd_tool = open(path, O_NONBLOCK|O_RDONLY|O_EXLOCK, 0);

    if (fd_tool == -1)
        return -4;

    if (fstat(fd_tool, &st))
        return -5;

    if (st.st_uid != 0) {
        fchown(fd_tool, 0, 0xFFFFFFFF);
        fchmod(fd_tool, S_ISUID|S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
        syslog(LOG_INFO, "[INIT] TabletMagicDaemon is now SUID root!\n");
    }

    close(fd_tool);
    return 0;
}

//
// Process command-line flags
//
// -q               Quiet
// -pn              Try Port named "n" first (i.e., choose this tablet)
// -ln -rn -tn -bn  Screen bounds: left, right, top, bottom (constrain pointer to this part of the screen)
// -Ln -Rn -Tn -Bn  Tablet bounds: left, right, top, bottom (ignore the stylus outside these bounds)
// -i <init>        Send an init string (via ~*) to the tablet
// -X               Quit after locating the tablet and printing out results
//
// Other cool options to add:
//
// -w               Constrain the pointer to the active window (tablet only, of course - good for 3D and Painter)
//

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

bool process_arguments(int argc, char *argv[]) {
    int     c, m1, m2;

    bool    usage = false;

    args.quiet          = false;        // DON'T suppress diagnostic messages
    args.command        = false;        // DON'T run in command mode
    args.dodaemon       = false;        // DON'T daemonize at startup
    args.forcepc        = false;        // DON'T always assume ISD-V4 at 19200
    args.baud38400      = false;        // DON'T initially try 38400 baud
    args.startoff       = false;        // DON'T start up in disabled mode
    args.quit           = false;        // DON'T always quit
    args.logging        = false;        // DON'T redirect output to a log file
    args.mouse          = false;        // DON'T operate in mouse mode
    args.port           = NULL;         // NO first named port to try
    args.init           = NULL;         // NO initial setup string to send to the tablet
    args.digi           = NULL;         // NO digitizer string
    args.rate           = B9600;        // initial speed is 9600
    args.scaling        = 1;            // initial mouse scaling 1

    args.priority       = 0;

    args.tab_left       = -1;
    args.tab_top        = -1;
    args.tab_right      = -1;
    args.tab_bottom     = -1;

    args.scr_left       = -1;
    args.scr_top        = -1;
    args.scr_right      = -1;
    args.scr_bottom     = -1;

    do {
        c = getopt(argc, argv, "3cdFhmoqwXi:p:n:l:r:t:b:L:R:T:B:M:s:");
        switch(c) {
            case EOF: break;
            case 'c': args.command      = true; break;
            case 'd': args.dodaemon     = true; break;
            case 'F': args.forcepc      = true; break;
            case '3': args.baud38400    = true; break;
            case 'q': args.quiet        = true; break;
            case 'w': args.logging      = true; break;
            case 'X': args.quit         = true; break;
            case 'm': args.mouse        = true; break;
            case 'o': args.startoff     = true; break;
            case 'p': asprintf(&args.port, "%s", optarg); break;
            case 'i': asprintf(&args.init, "%s", optarg); break;
            case 'l': if (!GetIntArgument(optarg, c, &args.scr_left))   { usage = true; }; break;
            case 't': if (!GetIntArgument(optarg, c, &args.scr_top))    { usage = true; }; break;
            case 'r': if (!GetIntArgument(optarg, c, &args.scr_right))  { usage = true; }; break;
            case 'b': if (!GetIntArgument(optarg, c, &args.scr_bottom)) { usage = true; }; break;
            case 'L': if (!GetIntArgument(optarg, c, &args.tab_left))   { usage = true; }; break;
            case 'T': if (!GetIntArgument(optarg, c, &args.tab_top))    { usage = true; }; break;
            case 'R': if (!GetIntArgument(optarg, c, &args.tab_right))  { usage = true; }; break;
            case 'B': if (!GetIntArgument(optarg, c, &args.tab_bottom)) { usage = true; }; break;

            case 'n':
                if (!GetIntArgument(optarg, c, &args.priority))
                    usage = true;
                else if (args.priority < -20 || args.priority > 20) {
                    fprintf(output, "Invalid priority value (-20 ... 20)\n");
                    usage = true;
                }
                break;

            case 's':
                if (!GetFloatArgument(optarg, c, &args.scaling))
                    usage = true;
                else if (args.scaling < 0.1 || args.scaling > 10) {
                    fprintf(output, "%.4f is an invalid scaling value (0.1 ... 10)\n", args.scaling);
                    usage = true;
                }
                break;

            case 'M':
                if (sscanf(optarg, "%d:%d", &m1, &m2) == 2 && m1 >= 1 && m1 <= kStylusButtonTypes && m2 >= 0 && m2 <= kSystemClickTypes-1) {
                    if (button_mapping[m1-1] != m2) {
                        button_mapping[m1-1] = m2;
                        if (!args.quiet) fprintf(output, "[INIT] Remapping Stylus %s to %s\n", sbuttons[m1-1], mbuttons[m2]);
                    }
                }
                else {
                    fprintf(output, "Invalid mapping option\n");
                    usage = true;
                }

                break;

            default:
            case 'h':
                usage = true;
                break;
        }
    } while (c != EOF);

    // start in a disabled state only in command mode or if quitting
    args.startoff = args.quit || (args.command && args.startoff);

    return usage;
}

//
// usage
//
void usage() {
    const char *fmt = "  %-17s%s.\n";
    printf("\nUsage: TabletMagicDaemon [options]\n");
    printf(fmt, "-3",               "Initially try 38400 baud");
    printf(fmt, "-c",               "Run in command mode");
    printf(fmt, "-d",               "Daemonize when starting");
    printf(fmt, "-F",               "Force TabletPC Mode");
    printf(fmt, "-h",               "Print this helpful message");
    printf(fmt, "-i setup",         "Initialize with a setup string");
    printf(fmt, "-l# -r# -t# -b#",  "Set screen boundaries");
    printf(fmt, "-L# -R# -T# -B#",  "Set tablet boundaries");
    printf(fmt, "-m",               "Enable mouse mode");
    printf(fmt, "-M#:#",            "Remap buttons (See the manual)");
    printf(fmt, "-n#",              "Renice the daemon (-20...20)");
    printf(fmt, "-o",               "Enabled state: off (command mode only)");
    printf(fmt, "-p portname",      "Connect to a particular serial port");
    printf(fmt, "-q",               "Quiet - no diagnostic output");
    printf(fmt, "-s#",              "Set mouse scaling (0.1 ... 10.0)");
    printf(fmt, "-X",               "Exit after initializing the tablet");
}

//
// signal_handler
//
// This intercepts signals and shuts down cleanly.
//
// TODO: For SIGHUP simply restart the daemon.
// When the daemon has a config file it'll make more sense.
//
void signal_handler(int sig) {
    quitProcessor = true;
};


#pragma mark - WacomTablet

//===================================================================
//  WacomTablet
//===================================================================

//
// CONSTRUCTOR
//
// The constructor does everything required to locate a tablet and start
// communication. If no tablet is found an error is thrown.
//
// The port_name argument names a single port to attempt connection with.
// If the argument is NULL then ports will be tested in sequence until a
// tablet is found or all ports have been examined.
//
// The q parameter instructs the class to suppress informative messages
// that would normally be sent to stdout. This corresponds to the -q flag.
//

WacomTablet::WacomTablet(init_arguments inArgs) {
    local_message_port = NULL;

#if ASYNCHRONOUS_MESSAGING
    remote_message_port = NULL;
#else
    messageQueueEnabled = true;
    outgoing_message_queue = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
#endif


    // Set up tablet state
    quiet_mode      = inArgs.quiet;             // Quiet mode does minimal logging
    tablet_on       = !inArgs.startoff;         // The tablet may start in an inactive mode
    mouse_mode      = false;                    // Mouse mode treats absolute motion as relative motion
    mouse_scaling   = 1.0f;                     // Scaling of the mouse is 1 by default
    test_mode       = false;                    // Test mode pings the tablet then quits
    gEventDriver    = MACH_PORT_NULL;           // No HID connection yet
    send_stream     = false;                    // Keep the stream to myself for now
    stream_size     = 0;

    //
    // Pass command-line arguments to the tablet object
    //
    SetTestMode(inArgs.quit);
    SetMouseMode(inArgs.mouse);
    InitTabletBounds(inArgs.tab_left, inArgs.tab_top, inArgs.tab_right, inArgs.tab_bottom);
    SetScreenMapping(inArgs.scr_left, inArgs.scr_top, inArgs.scr_right, inArgs.scr_bottom);

    // Get a notification whenever the resolution changes
    RegisterForNotifications();

    if (KERN_SUCCESS != OpenHIDService())
        throw "Can't connect to IO Master Port";

    if (!quiet_mode)
        serialPort.SetOutput(output);

    if (args.command) {
        CreateLocalMessagePort();
        SendMessage("[hello]");
        //      (void)usleep(1000000);  // 1 second
    }

    InitializeForPort(inArgs.port);
}

//
// DESTRUCTOR
//
WacomTablet::~WacomTablet() {
    // Disable notifications
    DisableNotifications();

    if (local_message_port)
        CFRelease(local_message_port);

#if ASYNCHRONOUS_MESSAGING
    if (remote_message_port)
        CFRelease(remote_message_port);
#else
    CFRelease(outgoing_message_queue);
#endif

    CloseHIDService();
    serialPort.Close();

#if LOG_STREAM_TO_FILE
    if (logfile) fclose(logfile);
#endif
}

//
// OpenHIDService
//
kern_return_t WacomTablet::OpenHIDService() {
    kern_return_t   kr;
    mach_port_t     ev, service;

    if (KERN_SUCCESS == (kr = CloseHIDService())) {
        if (KERN_SUCCESS == (kr = IOMasterPort(MACH_PORT_NULL, &io_master_port)) && io_master_port != MACH_PORT_NULL) {
            if ((service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kIOHIDSystemClass)))) {
                kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &ev);
                IOObjectRelease(service);

                if (KERN_SUCCESS == kr)
                    gEventDriver = ev;
            }
        }
    }

    return kr;
}

//
// CloseHIDService
//
kern_return_t WacomTablet::CloseHIDService() {
    kern_return_t   r = KERN_SUCCESS;

    if (gEventDriver != MACH_PORT_NULL)
        r = IOServiceClose(gEventDriver);

    gEventDriver = MACH_PORT_NULL;
    return r;
}


//
// RunEventLoop
// Add a CFTimer to the Run Loop to get the stream processor going.
//
void WacomTablet::RunEventLoop() {
    //
    // This timer polls the serial port 250 times per second
    //
    // At 19200 a tablet can theoretically send 213 or 274 packets
    // At 38400 (Intuos 2) there can theoretically be ~427 packets
    //
    CFRunLoopTimerContext ctx;
    bzero(&ctx, sizeof(ctx));
    ctx.info = this;
    CFRunLoopTimerRef serialTimer = CFRunLoopTimerCreate(
                                                         NULL,
                                                         CFAbsoluteTimeGetCurrent()+0.25,
                                                         1.0 / 250,
                                                         0,
                                                         0,
                                                         WacomTablet::TabletTimerCallback,
                                                         &ctx
                                                         );

    CFRunLoopAddTimer( CFRunLoopGetCurrent(), serialTimer, kCFRunLoopDefaultMode );

    //
    // Run separate event timer if flagged (experimental)
    //
#if EVENT_TIMER_IS_SEPARATE
    CFRunLoopTimerRef eventTimer = CFRunLoopTimerCreate(
                                                        NULL,
                                                        CFAbsoluteTimeGetCurrent()+0.125,
                                                        1.0 / 250,
                                                        0,
                                                        0,
                                                        WacomTablet::EventTimerCallback,
                                                        ctx
                                                        );
    CFRunLoopAddTimer( CFRunLoopGetCurrent(), eventTimer, kCFRunLoopDefaultMode );
#endif

    quitProcessor = false;


    //
    // Enter the Run Loop until it is exited
    //
    CFRunLoopRun();


    //
    // The run loop exited. Destroy the timers and return
    //
#if EVENT_TIMER_IS_SEPARATE
    CFRunLoopTimerInvalidate( eventTimer );
    CFRelease( eventTimer );
#endif

    CFRunLoopTimerInvalidate( serialTimer );
    CFRelease( serialTimer );
}


#pragma mark -

//
// InitializeForPort
//
// Initialization that should occur when selecting
// a different port
//
void WacomTablet::InitializeForPort(char *port_name) {
    serialPort.Close();

    in_packet       = false;                    // No packet marker received yet
    phrase_count    = 0;                        // No packet bytes received yet
    comma_count     = 0;

    clearstr(model_number);                     // No tablet identified yet
    clearstr(rom_version);                      // No ROM identified yet
    clearstr(buffer);                           // Mark the buffer empty
    base_version = 1.3f;                        // Assume at least this much
    series_index = kModelUnknown;               // Not sure which tablet yet
    can_parse_ud_setup = true;

    if (FindTabletOnPort(port_name)) {
        // Tell the Preference Pane we're all set!
        if (args.command)
            SendMessage("[ready]");

        InitStylus();
    }
    else {
        if (args.command)                   // Is the Preference Pane listening?
            SendMessage("[none]");          // Tell the Preference Pane we failed
        else
            throw "No Wacom Tablet Found!";
    }
}


//
// FindTabletOnPort
//
//  This pokes through all available RS232 serial ports looking for
//  tablets. It tries each port at 9600 and 19200 baud.
//
bool WacomTablet::FindTabletOnPort(char *port_name) {

    // Use 9600 and 19200 alternately, starting with the current speed
    // TODO: Include 38400 for Intuos 2 tablets
    int first_speed = (int)serialPort.Speed();
    int second_speed = (first_speed == B9600) ? B19200 : B9600;

    int typical_models[] = {    kModelUnknown, first_speed,
        kModelUnknown, second_speed,
#if FALLBACK_TO_SD
        kModelSDSeries, B9600,
#endif
        -1 };

    // This set of tests is idealized. Many TabletPC digitizers don't answer
    // any queries for information, so there's actually no way to confirm a connection.
    // For this reason, every test passing kModelTabletPC returns "success."
    // In other words, only the first item in this list ever gets tested.
    // We could test all of these entries first, then if they all fail assume the default.
    //
    // If it turns out there's a definitive way to confirm a valid baud rate then
    // this can be revised to handle that situation.
    //
    // Note: If the -3 argument was passed we test for 38400 instead of 19200.
    //       (-3 is passed when the digitizer string is WACF008)
    //
    int tablet_pc_models[] = {  kModelTabletPC, args.baud38400 ? B38400 : B19200,
        kModelTabletPC, args.baud38400 ? B19200 : B9600,
        kModelUnknown, first_speed,
        kModelUnknown, second_speed,
#if FALLBACK_TO_SD
        kModelSDSeries, B9600,
#endif
        -1 };

    int *models_to_try = hackintosh ? tablet_pc_models : typical_models;

    // Scan through all serial ports
    if (KERN_SUCCESS == serialPort.BeginPortScan()) {
        // Get the next device that matches the Port parameter
        while (serialPort.OpenNextMatchingPort(port_name)) {
            // Loop through the baud rates 9600/19200 (or vice-versa)
            for (int i=0; (models_to_try[i]!=-1); i+=2) {
                //              serialPort.Flush();
                serialPort.SetSpeed(models_to_try[i+1]);

                if (!quiet_mode)
                    fprintf(output, "[PORT] Setting speed to %ld\n", serialPort.Speed());

                // Try to initialize and exit the loop on success
                if (InitializeTablet(models_to_try[i]))
                    goto SCAN_DONE;
            }

            // On failure, close the port and continue
            serialPort.Close();
        }
    }

SCAN_DONE:
    serialPort.EndPortScan();

    // If the whole scan failed reset the baud rate
    if (!IsActive()) {
        serialPort.Close();
        serialPort.SetSpeed(first_speed);
    }

    if (!quiet_mode)
        fprintf(output, "\n%s.\n\n", IsActive() ? "Tablet initialized" : "Could not initialize tablet");

    return IsActive();
}


//
// InitializeTablet()
// This is used to test for the presence of a tablet
// and at the same time it happens to
// initialize a tablet on the chosen serial port
//
bool WacomTablet::InitializeTablet(int try_tablet_model) {
    bool    result = false;
    char    *tablet_id = NULL;

    do {    // Within this block "break" signifies failure

        if (try_tablet_model == kModelSDSeries) {
            asprintf(&tablet_id, "~#SD-Fallback V1.2");
        }
        else if (try_tablet_model == kModelTabletPC) {
            if (!args.forcepc) {
                // Tell the tablet to stop sending
                if (!SendCommandToTablet(TPC_StopTablet))
                    break;

                (void)usleep(100000);   // 0.1 seconds
                (void)Flush();

                // Send a "*" command to the tablet
                int reply_len = SendRequestToTablet(TPC_TabletID);
                if (!quiet_mode) fprintf(output, "[RCVD] %s\n", HexString(modalbuffer, reply_len));
                ProcessTabletPCCommandReply(modalbuffer);

                // If the tablet doesn't respond with a valid set of bytes, fail
                if (reply_len != TPC_QUERY_REPLY_SIZE)
                    break;

            }

            // Tablets that answer to "*" are probably Tablet PC
            asprintf(&tablet_id, "~#ISD V4");
        }
        else {
            // Tell the tablet to stop sending
            if (!SendCommandToTablet(WAC_StopTablet))
                break;

            (void)Flush();
            //          (void)usleep(100000);   // 0.1 seconds

            // Request the Tablet ID - try up to 3 times
            for (int x=3; x--;) {

                if (tablet_id != NULL) {
                    free(tablet_id);
                    tablet_id = NULL;
                }

                asprintf(&tablet_id, RequestTabletIDModal());

                if (strlen(tablet_id) && tablet_id[0] == '~')
                    break;
            }

            // Fail if there's no reply
            if (strlen(tablet_id) == 0)
                break;
        }

        ProcessCommandReply(tablet_id);

        if (tablet_id != NULL) {
            free(tablet_id);
            tablet_id = NULL;
        }

        //
        // SD, Penpartner, and Intuos tablets don't respond to settings requests
        //
        bool answers_settings = false;
        switch (series_index) {
            case kModelCintiq:
                if (!quiet_mode) fprintf(output, "[INIT] PL-Series or Cintiq Detected\n");
                settings[0].InitForPL();
                break;

            case kModelSDSeries:
                if (!quiet_mode) fprintf(output, "[INIT] SD-Series Detected\n");
                // Set SD tablets to the best possible state: II-S Binary, Pressure, INC=0, Normal Data Mode
                SendCommandToTablet(WAC_BinaryMode WAC_PressureModeOn WAC_SuppressIN2 WAC_IncrementOff WAC_DataNormal);
                settings[0].InitForSD();
                break;

            case kModelTabletPC:
                if (!quiet_mode) fprintf(output, "[INIT] TabletPC Detected\n");
                settings[0].InitForTabletPC(args.baud38400);
                break;

            case kModelPenPartner:
                if (!quiet_mode) fprintf(output, "[INIT] CT-PenPartner Detected\n");
                settings[0].InitForPenPartner();
                break;

            case kModelIntuos:
            case kModelIntuos2:
                if (!quiet_mode) fprintf(output, "[INIT] Intuos Detected\n");
                settings[0].InitForIntuos();
                break;

            default:
                // If an initialization string was passed send it to the tablet
                // and clear it so next time no init will occur
                if (args.init != NULL) {
                    char setstr[32];
                    sprintf(setstr, "%s%s\r", (args.init[0]=='~' ? "" : "~*"), args.init);
                    SendCommandToTablet(setstr);
                    ShortSleep();
                    free(args.init);
                    args.init = NULL;
                }

                // Request the initial tablet settings
                // If the tablet fails to respond, consider the tablet inactive
                answers_settings = true;
                if (!GetUDSettingsOrFail() && args.command)
                    SetProcessing(false);
        }

        // Print settings for tablets that don't announce them
        if (!answers_settings)
            fprintf(output, "\nTablet Settings (imposed):\n%s\n", settings[0].Description());

        //
        // Request Max Coordinates
        // SD tablets don't appear to answer this query
        //
        if ( series_index == kModelTabletPC ) {
            UpdateTabletScale(24570, 18430);
        }
        else if ( series_index == kModelSDSeries ) {
            SendScaleToTablet(15240, 15240);            // For SD impose this resolution so that we "know"
            UpdateTabletScale(15240, 15240);
        }
        else {
            char* maxc = RequestMaxCoordinatesModal();
            if (strlen(maxc) == 0) break;
            ProcessCommandReply(maxc);
        }

        // Tell the tablet to start sending
        if (args.forcepc) {
            if (series_index != kModelTabletPC)
                SendCommandToTablet(WAC_StartTablet);
        }
        else {
            switch (series_index) {
                case kModelTabletPC:
                    SendCommandToTablet(TPC_Sample133pps);
                    break;
                default:
                    SendCommandToTablet(WAC_StartTablet);
                    break;
            }
        }

        result = true;

    } while (false);

    return result;
}


//
// InitStylus
// Prepare the stylus state
//
void WacomTablet::InitStylus() {
    stylus.toolid       = kToolPen1;
    stylus.tool         = kToolTypePen;
    stylus.serialno     = 0;

    stylus.off_tablet   = true;
    stylus.pen_near     = false;
    stylus.eraser_flag  = false;

    stylus.button_click = false;
    ResetButtons;

    stylus.menu_button  = 0;
    stylus.raw_pressure = 0;
    stylus.pressure     = 0;
    stylus.tilt.x       = 0;
    stylus.tilt.y       = 0;
    stylus.point.x      = 0;
    stylus.point.y      = 0;

    CGEventRef ourEvent = CGEventCreate(NULL);
    CGPoint point = CGEventGetLocation(ourEvent);

    stylus.scrPos       = point;
    stylus.oldPos.x     = SHRT_MIN;
    stylus.oldPos.y     = SHRT_MIN;

    // The proximity record includes these identifiers
    stylus.proximity.vendorID = 0xBEEF;             // A made-up Vendor ID (Wacom's is 0x056A)
    stylus.proximity.tabletID = 0x0001;
    stylus.proximity.deviceID = 0x81;               // just a single device for now
    stylus.proximity.pointerID = 0x00;
    stylus.proximity.systemTabletID = 0x00;
    stylus.proximity.vendorPointerType = 0x0802;    // basic stylus
    stylus.proximity.pointerSerialNumber = 0x00000001;
    stylus.proximity.reserved1 = 0;

    // This will be replaced when a tablet is located
    stylus.proximity.uniqueID = 0;

    // Indicate which fields in the point event contain valid data. This allows
    // applications to handle devices with varying capabilities.

    stylus.proximity.capabilityMask =
    NX_TABLET_CAPABILITY_DEVICEIDMASK
    |   NX_TABLET_CAPABILITY_ABSXMASK
    |   NX_TABLET_CAPABILITY_ABSYMASK
    //      |   NX_TABLET_CAPABILITY_VENDOR1MASK
    //      |   NX_TABLET_CAPABILITY_VENDOR2MASK
    //      |   NX_TABLET_CAPABILITY_VENDOR3MASK
    |   NX_TABLET_CAPABILITY_BUTTONSMASK
    |   NX_TABLET_CAPABILITY_TILTXMASK
    |   NX_TABLET_CAPABILITY_TILTYMASK
    //      |   NX_TABLET_CAPABILITY_ABSZMASK
    |   NX_TABLET_CAPABILITY_PRESSUREMASK
    //      |   NX_TABLET_CAPABILITY_TANGENTIALPRESSUREMASK
    //      |   NX_TABLET_CAPABILITY_ORIENTINFOMASK
    //      |   NX_TABLET_CAPABILITY_ROTATIONMASK
    ;

    /*
     //
     // Use Wacom-supplied names
     //
     stylus.proximity.capabilityMask =   kTransducerAbsXBitMask
     | kTransducerAbsYBitMask
     | kTransducerButtonsBitMask
     | kTransducerTiltXBitMask
     | kTransducerTiltYBitMask
     | kTransducerDeviceIdBitMask    // no use for this today
     | kTransducerPressureBitMask;

     */

    bcopy(&stylus, &oldStylus, sizeof(StylusState));
    bzero(buttonState, sizeof(buttonState));
    bzero(oldButtonState, sizeof(oldButtonState));
}


//
// ResetStylus()
//  Simulate as if the pen were taken off the tablet
//
void WacomTablet::ResetStylus() {
    stylus.off_tablet   = true;
    stylus.pen_near     = false;
    stylus.eraser_flag  = false;

    ResetButtons;

    stylus.menu_button  = 0;
    stylus.pressure     = 0;
    stylus.tilt.x       = 0;
    stylus.tilt.y       = 0;

    PostChangeEvents();
}


#pragma mark -

//
// SendUDSetupString
//
// Sends a setup string to the tablet, waits, and
// requests the current settings from the tablet
//
// On tablet models that don't support the setup command
// just import the settings string to current settings
//
void WacomTablet::SendUDSetupString(char *setup, int bank, bool insist) {
    char *command = NULL;
    if (bank == 0)
        asprintf(&command, "~*%s\r", setup);
    else
        asprintf(&command, "~W%d%s\r", bank, setup);

    if (NULL != command) {
        // Import the settings to update to the intended state
        settings[bank].Import(command);

        if (can_parse_ud_setup) {
            SendCommandToTablet(WAC_StopTablet);
            SendCommandToTablet(command);
            (void)usleep(100000);   // 0.1 seconds

            // By default get the new settings from the tablet
            if (insist) {
                GetUDSettingsOrFail(bank);
            }
            //          else {
            //              RequestUDSettings(bank);
            //          }


            SendCommandToTablet(WAC_StartTablet);
        }

        free(command);
    }
}

//
// GetUDSettingsOrFail
//
bool WacomTablet::GetUDSettingsOrFail(int bank) {
    int tries = 15;

    while (tries--) {
        // Request the initial tablet settings
        char* sett = GetUDSettings(bank);

        if (strlen(sett) > 0 && sett[0] == '~') {
            ProcessCommandReply(sett);
            break;
        }
        else
            (void)usleep(100000);   // 0.1 seconds
    }

    return (tries > 0);
}

//
// GetUDSettings
//
static const char *getcoms[] = { WAC_ReadSetting, WAC_ReadSettingM1, WAC_ReadSettingM2 };

char* WacomTablet::GetUDSettings(int bank) {
    (void)SendRequestToTablet(getcoms[bank]);
    return modalbuffer;
}

//
// RequestUDSettings
//
void WacomTablet::RequestUDSettings(int bank) {
    if (can_parse_ud_setup)
        SendCommandToTablet(getcoms[bank]);
}

#pragma mark -

//
// SendCommandToTablet
//
// Send a command to the tablet and set the commandSent flag
// if successful. The serial queue processor can use commandFlag
// as an extra measure to distinguish command replies from
// spurious data, though it shouldn't really be necessary.
//
bool WacomTablet::SendCommandToTablet(const char *command) {
    char com[100];
    strcpy(com, command);

    if (strncmp("~R", command, 2) == 0)
        bank_last_requested = (com[2] == '\r') ? 0 : (com[2] - '0');

    int numBytes = serialPort.Write(com, (int)strlen(com));

    if (numBytes == kSerialError)
        syslog(LOG_ERR, "SendCommandToTablet Error %d: %s.\n", errno, strerror(errno));
    else {
        if (!quiet_mode)
            fprintf(output, "[SENT] \"%s\"\n", LogString(com));
    }

    return (numBytes >= (int)strlen(com));
}

//
// SendRequestToTablet
//
// Sends a command and waits around for a reply
// This is used on initialization as part of the startup test
//
int WacomTablet::SendRequestToTablet(const char *command) {
    doing_modal = true;
    int len = 0;

    clearstr(modalbuffer);
    if (SendCommandToTablet(command)) {
        // Get up to 1024 bytes allowing 0.1 seconds for a reply
        len = serialPort.ReadLine(modalbuffer, 1024, 100000);

        //      if (len > 0 && !quiet_mode)
        //          fprintf(output, "[RCVD] \"%s\"\n", LogString(modalbuffer));
    }

    doing_modal = false;
    return len;
}

bool WacomTablet::SendScaleToTablet(int h, int v) {
    bool result = true;
    char *command = NULL;
    if (0 <= asprintf(&command, "SC%d,%d\r", h, v)) {
        result = SendCommandToTablet(command);
        free(command);
    }
    return result;
}

#pragma mark -

//
// TabletTimerCallback
//
void WacomTablet::TabletTimerCallback( CFRunLoopTimerRef timer, void *info ) {
    //  static Boolean didInit = false;
    if (quitProcessor)
        CFRunLoopStop( CFRunLoopGetCurrent() );
    else {
        WacomTablet *t = (WacomTablet*)info;
        if ( t->IsActive() && !t->AwaitingModalReply() )
            t->ProcessSerialStream();
    }
}


//
// EventTimerCallback
//
void WacomTablet::EventTimerCallback( CFRunLoopTimerRef timer, void *info ) {
    ((WacomTablet*)info)->PostChangeEvents();
}

#pragma mark -

//
// SetStreamLogging
//
void WacomTablet::SetStreamLogging(bool do_stream) {
    if (do_stream != send_stream) {
        stream_pause = 0;
        if ((send_stream = do_stream)) {
            stream_size = 0;
            stream_event = NULL;

            streamTimer = CFRunLoopTimerCreate(
                                               NULL,
                                               CFAbsoluteTimeGetCurrent(),
                                               1,
                                               0,
                                               0,
                                               WacomTablet::StreamTimerCallback,
                                               NULL
                                               );

            CFRunLoopAddTimer( CFRunLoopGetCurrent(), streamTimer, kCFRunLoopDefaultMode );
        }
        else {
            CFRunLoopTimerInvalidate( streamTimer );
            CFRelease( streamTimer );
        }

        byteCounter = packetCounter = bytesPerSecond = packetsPerSecond = 0;
    }
}


//
// StreamTimerCallback
//
void WacomTablet::StreamTimerCallback( CFRunLoopTimerRef timer, void *info ) {
    bytesPerSecond = byteCounter;
    packetsPerSecond = packetCounter;
    byteCounter = packetCounter = 0;

    if (stream_pause) stream_pause--;
}


#pragma mark -

//
// ProcessSerialStream
//
// Process whatever data is waiting on the serial port.
// This method is periodically called from TabletTimerCallback.
//
// TODO: When switching settings that affect packet_size
//  sometimes extra bytes come through and are interpreted
//  as command replies. Fix this!
//
void WacomTablet::ProcessSerialStream() {
    char p[64], r[128], buff[1000];

    // Wait for input and time out after 3900 microseconds
    int n = serialPort.Select(3900);

    if (n > 0) do {
        clearstr(buff);

        int numBytes = serialPort.Read(buff, 1000);

        if (numBytes > 0) {
            if (send_stream)
                byteCounter += numBytes;

            int plen = 0,
            rlen = 0;

            for (int i=0; i<numBytes; i++) {
                unsigned char s = (unsigned char)buff[i];

                if (series_index == kModelFujitsuP) {
                    if (s > 130) {
                        plen = 0;
                        p[plen++] = (s == 136) ? 1 : (s == 138) ? 2 : 0;
                    } else {
                        p[plen++] = s;
                        if (plen == 5)
                            ProcessPacket(p, plen);
                    }
                }
                else {

#if LOG_STREAM_TO_FILE
                    short b = ((short)s) & 0x00FF;
                    if (logfile) fprintf(logfile, " %02X", b);
                    //fprintf(logfile, " %02X [%c]", b, b >= 0x20 && b < 0x80 ? b : '.');
#endif

                    // In binary mode the high bit indicates a new phrase
                    // All Wacom IV modes are binary. II-S has an ASCII mode
                    if (s & 0x80) {
                        if (in_packet) {
                            if (series_index == kModelTabletPC) {
                                if (phrase_count == TPC_QUERY_REPLY_SIZE) {
                                    ProcessTabletPCCommandReply(phrase);
#if LOG_STREAM_TO_FILE
                                    if (logfile) fprintf(logfile, " >R\n(%s)\n", HexString(phrase, phrase_count));
#endif
                                }
                                else if (phrase_count == settings[0].packet_size) {
                                    memcpy(p, phrase, phrase_count);
                                    plen = phrase_count;
                                }

                            } else {

                                // Sanity check packets before processing. This is redundant, as
                                // each processing function does its own sanity-checking.
                                if (phrase_count == settings[0].packet_size) {
                                    memcpy(p, phrase, phrase_count);
                                    plen = phrase_count;
                                }
                            }

                        } else {
                            // This is a hack for replies that don't end in CR
                            // (Which should be rare or non-existent)
                            memcpy(r, phrase, phrase_count);
                            rlen = phrase_count;
                        }

                        // Prepare to start capturing the binary packet
                        phrase_count = 0;
                        in_packet = true;
                    }

                    // Normalize line endings for ASCII packets
                    if (!in_packet) {
                        if (s == '\n') s = '\r';
                        if (i < numBytes-1 && s=='\r' && (buff[i+1]=='\n' || buff[i+1]=='\r')) i++;
                    }

                    // Extend the phrase, ignore any leading newlines
                    if (phrase_count || s != '\r')
                        phrase[phrase_count++] = s;

                    // Are we inside a binary packet?
                    if (in_packet) {
                        /* This block catches completed packets without having to wait for the next one.
                         * Most tablets send an extra trailing packet when there has been no change, so
                         * this was originally only intended to handle unusual UD tablet modes. It may
                         * ultimately be redundant.
                         */

                        // TabletPC may return a normal 9 byte packet or an 11 byte Info reply.
                        // Thus we can't assume the packet is complete after 9 bytes.
                        if (series_index == kModelTabletPC) {
                            if (phrase_count == TPC_QUERY_REPLY_SIZE) {
                                ProcessTabletPCCommandReply(phrase);
#if LOG_STREAM_TO_FILE
                                if (logfile) fprintf(logfile, " >R\n(%s)\n", HexString(phrase, phrase_count));
#endif
                                in_packet = false;
                                phrase_count = 0;
                            }
                        }
                        else {
                            // This sends a packet when it reaches the proper packet size
                            // The assumption here is that a trailing packet could get lost
                            if (phrase_count == settings[0].packet_size) {
                                memcpy(p, phrase, phrase_count);
                                plen = phrase_count;
                                in_packet = false;
                                phrase_count = 0;
                            }
                        }
                    }
                    else {
                        // The 3rd comma marks the end of an SD tablet info string
                        if (!can_parse_ud_setup && phrase_count > 4 && phrase[0] == '~' && s == ',' && ++comma_count == 3)
                            s = '\r';

                        // Then check for a newline, indicating the end of a line
                        // in a potential packet
                        if (s == '\r' && phrase_count > 0) {
                            // If We're in II-S ASCII mode, process valid data packets
                            if (settings[0].command_set == kCommandSetWacomIIS /* && settings[0].output_format == kOutputFormatASCII */) {
                                if (phrase[1] == ',' && (phrase[0] == '#' || phrase[0] == '!' || phrase[0] == '*')) {
                                    in_packet = false;
                                    plen = phrase_count-1;
                                    memcpy(p, phrase, plen);
                                }
                                else {
                                    memcpy(r, phrase, phrase_count);
                                    rlen = phrase_count;
                                }
                            }
                            else {
                                memcpy(r, phrase, phrase_count);
                                rlen = phrase_count;
                            }

                            phrase_count = 0;
                        }
                    }

                    if (plen) {
                        comma_count = 0;
                        p[plen] = '\0';
                        ProcessPacket(p, plen);
                        plen = 0;

#if LOG_STREAM_TO_FILE
                        if (logfile) fprintf(logfile, "\n");
#endif
                    }

                    if (rlen) {
                        comma_count = 0;
                        r[rlen] = '\0';
                        ProcessCommandReply(r);
                        rlen = 0;

#if LOG_STREAM_TO_FILE
                        if (logfile) fprintf(logfile, " >R\n(%s)\n", LogString(r));
#endif
                    }

                }
            }
        }

    } while (serialPort.BytesOnPort());

}


//
// ProcessCommandReply(response)
//
void WacomTablet::ProcessCommandReply(char *response) {
    char replyString[100];
    strncpy(replyString, response, 100);

    if (send_stream) {
        stream_size = 1;
        stream_pause = 2;
        memcpy(stream_packet, replyString, strlen(replyString) + 1);
    }

    unsigned int    i;
    UInt64          tid;

    if (!quiet_mode)
        fprintf(output, "[PROC] \"%s\"\n", LogString(replyString));

    if (replyString[0] == '~') switch (replyString[1]) {
        case 'R': {
            //
            // TODO: Look for REL mode and set the stylus
            // position to the center of the tablet boundary.
            //

            // The Wacom docs claim a reply of ~R1/~R2... will come back,
            // but it doesn't on my tablet. So instead we just remember
            // which bank was last requested and use that.
            int bank = (replyString[11] == ',') ? (replyString[2] - '0') : bank_last_requested;

            settings[bank].Import(replyString);

            if (!quiet_mode) {
                const char *bankname[] = { "Active","M1","M2" };
                fprintf(output, "\nTablet Settings (%s):\n%s\n", bankname[bank], settings[bank].Description());
            }

            if (args.command)
                SendMessageInfo(bank);

            quitProcessor = test_mode;
            break;
        }

        case '#': {
            //          strcpy(replyString, "~#310E,V3.3.1.01,3A41,");  // SD310E - Andy Kunz
            //          strcpy(replyString, "~#420,V3.0-01,9C9D,");     // SD420L - Just like mine!
            //          strcpy(replyString, "~#420E,V3.3.1.01,6657,");  // SD422E - (goestudio)
            //          strcpy(replyString, "~#SD51C,V3.2.1.01,7000,"); // SD510C - (mreathers)
            //          strcpy(replyString, "~#CT-0045R00,V1.3-5,");    // Penpartner 4x5
            //          strcpy(replyString, "~#GD-1212-R00,V1.2-7");    // Intuos 12x12
            //          strcpy(replyString, "~#GD-0608-R00,V1.2-7");    // Intuos 6x8
            //          strcpy(replyString, "~#PL-400-R00 V1.3-3,");    // PL400 - PL Series or Cintiq
            //          strcpy(replyString, "~#ISD V4");                // TabletPC override
            //          strcpy(replyString, "~#Fujitsu-P");             // Fujitsu P-Series override

            // Replace all commas with spaces
            for(char *c=replyString; (c=strstr(c,",")); *c=' ');

            // If the reply has only numbers, it's probably an old SD
            if (replyString[2] >= '0' && replyString[2] <= '9') {
                strcpy(rom_version, "SD-");
                sscanf(&replyString[2], "%s", &rom_version[3]);
            }
            else
                sscanf(&replyString[2], "%s", rom_version);

            //
            // Determine the Series Index
            //
            int matching_index = 0;
            for (i=0; i<(sizeof(series_list)/sizeof(SeriesDescription)); i++) {
                if ( 0 == strncmp(rom_version, series_list[i].code, strlen(series_list[i].code)) ) {
                    matching_index = i;
                    series_index = series_list[matching_index].index;
                    break;
                }
            }

            can_parse_ud_setup = (series_index != kModelSDSeries && series_index != kModelTabletPC && series_index != kModelCalComp);

            // All SD tablets assume ROM 1.2, the earliest supported
            if (series_index == kModelSDSeries)
                base_version = 1.2f;
            else
                (void)sscanf(&replyString[2], "%*s V%f", &base_version);        // Some tablets have a space...

            base_version = floorf(base_version * 10.0f + 0.1f) / 10.0f;

            if (!quiet_mode)
                fprintf(output, "[INFO] %s V%.2f (%s)\n", rom_version, base_version, series_list[matching_index].name);

            //
            // Generate a unique tablet ID based on device port
            // and tablet information. This is pretty arbitrary.
            //
            int q = 1;
            tid = 0LL;
            char *devPath = serialPort.DeviceFilePath();
            for(i=0; i<strlen(devPath); i++)
                tid += devPath[i] * (1 << (q++ % 24));

            for(i=0; i<strlen(replyString); i++)
                tid += replyString[i] * (1 << (q++ % 24));

            stylus.proximity.uniqueID = tid;

            break;
        }

        case 'C': {
            replyString[7] = replyString[13] = '\0';
            long h = atol(&replyString[2]), v = atol(&replyString[8]);

            // Notice that we don't do this during command mode,
            // because it could be a command reply request
            if (!send_stream)
                UpdateTabletScale((SInt32)h, (SInt32)v, true);

            break;
        }

        default: {
            //
            // This hack is required because I don't quite get the PenPartner's
            // response behavior yet.
            //
            if (series_index == kModelPenPartner) {
                if (strlen(replyString) >= 11 && replyString[5] == ',') {
                    replyString[7] = replyString[13] = '\0';
                    long h = atol(&replyString[2]), v = atol(&replyString[8]);
                    settings[0].xscale = (SInt32)h;
                    settings[0].yscale = (SInt32)v;
                    InitTabletBounds(0, 0, (SInt32)h-1, (SInt32)v-1);
                }
            }
            break;
        }
    }
}

//
// ProcessTabletPCCommandReply(packet)
//
void WacomTablet::ProcessTabletPCCommandReply(char *packet) {
    int firmware_maj = packet[9] & TPC_Query9_FirmwareHi;
    int firmware_min = packet[10] & TPC_Query10_FirmwareLo;

    //  int pressure_max = ((packet[6] & TPC_Query6_PressureHi) << 7) | (packet[5] & TPC_Query5_PressureLo);

    long    h = ((UInt16)(packet[1] & TPC_Query1_MaxX) << 9) | ((UInt16)(packet[2] & TPC_Query2_MaxX) << 2) | ((UInt16)(packet[6] & TPC_Query6_MaxX) >> 5),
    v = ((UInt16)(packet[3] & TPC_Query3_MaxY) << 9) | ((UInt16)(packet[4] & TPC_Query4_MaxY) << 2) | ((UInt16)(packet[6] & TPC_Query6_MaxY) >> 3);

    //
    // The following imitates ProcessCommandReply() though some actions are extraneous
    //

    series_index = kModelTabletPC;
    can_parse_ud_setup = false;
    stylus.proximity.uniqueID = 0xDEADBEEF;

    while (firmware_min >= 1) { firmware_min /= 10.0f; }
    base_version = firmware_maj + firmware_min;

    if (!quiet_mode) fprintf(output, "[INFO] ISD-V4 Firmware %.2f (TabletPC)\n", base_version);

    // When not a test command, use the reply parameters
    if (!send_stream) {
        settings[0].xscale = (SInt32)h;
        settings[0].yscale = (SInt32)v;
        InitTabletBounds(0, 0, (SInt32)h-1, (SInt32)v-1);
    }
}

//
// ProcessCalCompCommandReply(response)
//
void WacomTablet::ProcessCalCompCommandReply(char *response) {
    char replyString[100];
    strncpy(replyString, response, 100);

    if (send_stream) {
        stream_size = 1;
        stream_pause = 2;
        memcpy(stream_packet, replyString, strlen(replyString) + 1);
    }

    UInt32 i;

    if (!quiet_mode)
        fprintf(output, "[PROC] \"%s\"\n", LogString(replyString));
    if ( strncmp(&replyString[0], "CalComp",7 )) {
        series_index = kModelCalComp;
    }

    //only testing for one ROM version at moment but doesn't matter
    else if ( strncmp(&replyString[0], "70205A 04/03/95 19472-1",23 )) {
        int matching_index = 0;
        for (i=0; i<(sizeof(series_list)/sizeof(SeriesDescription)); i++) {
            if ( 0 == strncmp(rom_version, series_list[i].code, strlen(series_list[i].code)) ) {
                matching_index = i;
                series_index = series_list[matching_index].index;
                break;
            }
        }

        strcpy(rom_version , "70205A");
        base_version = 19472-1;
        if (!quiet_mode)
            fprintf(output, "[INFO] %s V%.2f (%s)\n", rom_version, base_version, series_list[matching_index].name);
    }
    //this is a size response - not working at moment
    //i think we can add the condition replyString[3]==','
    else {
        fprintf(output,"%s\n",response);
        settings[0].xscale = (int) replyString[2] + ((int)replyString[1] << 7) + (((int)replyString[0]&0x03)<<14);
        settings[0].yscale = (int)replyString[5] + ((int)replyString[4] << 7);
        fprintf(output, "[INFO] got size from CalComp\n");
    }

}

//
// ProcessPacket(packet)
//
// Handle the data packet with the appropriate command protocol
// If streaming is enabled then handle that too.
// Also track Binary / ASCII mode and report it to the pane.
//
void WacomTablet::ProcessPacket(char *packet, int pack_size) {
    static bool bc = false, ot = false, pn = false, ef = false;
    static UInt16 bm1 = 0;
    static SInt16 prev_bin = -1;
    SInt16 bin_mode;

    switch (series_index) {
        case kModelGraphire:
        case kModelGraphire2:
        case kModelGraphire3:
            ProcessGraphire(packet, pack_size);
            break;

        case kModelIntuos:
        case kModelIntuos2:
            ProcessWacomV(packet, pack_size);
            break;

        case kModelFujitsuP:
            ProcessFujitsuPSeries(packet);
            break;

        default:
            bin_mode = (packet[0] & 0x80) != 0;
            if (bin_mode != prev_bin) {
                settings[0].output_format = bin_mode ? kOutputFormatBinary : kOutputFormatASCII;
                prev_bin = bin_mode;
            }

            switch (settings[0].command_set) {
                case kCommandSetBitpadII:
                    // (pending)
                    break;

                case kCommandSetMM1201:
                    // (pending)
                    break;

                case kCommandSetWacomIIS:

                    if (bin_mode)
                        ProcessWacomIIS_Binary(packet, pack_size);
                    else
                        ProcessWacomIIS_ASCII(packet, pack_size);
                    break;

                case kCommandSetWacomIV:

                    if (base_version < 1.399)                   // < "1.4" won't work due to fp precision (a hazard of fp!)
                        ProcessWacomIV_13(packet, pack_size);
                    else
                        ProcessWacomIV_14(packet, pack_size);

                    break;

                case kCommandSetTabletPC:
                    if (bin_mode)
                        ProcessTabletPC(packet, pack_size);
                    break;
            }
            break;
    }


    //
    // Impose a pressure curve here...
    //
    /*
     if (stylus.pressure < 20000) {
     stylus.pressure = 0;
     stylus.button_mask &= ~kBitStylusTip;
     stylus.button[kStylusTip] = 0;

     }
     */

    if (!(stylus.button_mask & (kBitStylusTip|kBitStylusEraser)))
        stylus.pressure = 0;

    if (send_stream && stream_pause < 1) {
        packetCounter++;

        stream_size = pack_size;
        memcpy(stream_packet, packet, pack_size);
        stream_packet[pack_size] = '\0';

        bool    bc2 = stylus.button_click,
        ot2 = stylus.off_tablet,
        pn2 = stylus.pen_near;

        UInt16  bm2 = stylus.button_mask;

        if (bm2 != bm1 || bc != bc2 || ot != ot2 || pn != pn2) {
            bool ef2 = stylus.eraser_flag;

            if (ot != ot2)
                stream_event = (char*)(ot2 ? (ef ? "Eraser Disengaged" : "Pen Disengaged") : (ef2 ? "Eraser Engaged" : "Pen Engaged"));
            else {
                if (bm1 != bm2) {
                    stream_event = (char*)(bm2 > bm1
                                           ? (((bm2 ^ bm1) & 0x09) ? (ef2 ? "Eraser Down" : "Pen Down") : "Button Click")
                                           : (((bm2 ^ bm1) & 0x09) ? (ef ? "Eraser Up" : "Pen Up") : "Button Release"));
                }
                else if (pn != pn2)
                    stream_event = (char*)(pn2 ? (ef2 ? "Eraser Engaged" : "Pen Engaged") : (ef2 ? "Eraser Disengaged" : "Pen Disengaged"));
            }

            bm1 = bm2; bc = bc2; ot = ot2; pn = pn2; ef = ef2;
        }
    }

#if !EVENT_TIMER_IS_SEPARATE
    PostChangeEvents();
#endif

}

//
// PostChangeEvents
//
//  Compare the current state to the previous state and send
//  all the events necessary to get the system up to speed.
//
//  This method is analogous to the HID queue provider. It
//  determines the changes in tablet state and directly calls
//  our NXEvent dispatcher, whereas the HID stuff relies on a
//  formal data provider which is usually at the kernel level.
//
void WacomTablet::PostChangeEvents() {
    static bool dragState = false;

    //
    // Get Screen and Tablet areas
    //
    CGFloat swide = screenMapping.size.width, shigh = screenMapping.size.height,
    twide = tabletMapping.size.width, thigh = tabletMapping.size.height;

    //
    // Get the Screen Boundary
    //
    CGFloat sx1 = screenMapping.origin.x,
    sy1 = screenMapping.origin.y;

    //
    // And the Tablet Boundary
    //
    CGFloat nx, ny,
    tx1 = tabletMapping.origin.x,
    tx2 = tx1 + twide - 1,
    ty1 = tabletMapping.origin.y,
    ty2 = ty1 + thigh - 1;

    if (mouse_mode) {

        //
        // Get the minimal ratio of the tablet to the screen
        // and use this to get reasonable starting mouse motion
        //
        CGFloat hratio = screenBounds.size.width / twide,
        vratio = screenBounds.size.height / thigh,
        minratio = ((hratio < vratio) ? hratio : vratio) * 2.0f,
        mouse_mult = mouse_scaling * minratio;

        /*
         fprintf(stderr,
         "Screen Boundary: %.2f, %.2f - %.2f, %.2f  Tablet Boundary: %.2f, %.2f - %.2f, %.2f  Ratios H/V/X: %.2f %.2f %.2f\nOld Pos: %.2f %.2f  | Scr Pos: %.2f %.2f  |  Motion: %d %d\n\n",
         sx1, sy1, sx2, sy2,
         tx1, ty1, tx2, ty2,
         hratio, vratio, mouse_scaling,
         stylus.oldPos.x, stylus.oldPos.y,
         stylus.scrPos.x, stylus.scrPos.y,
         stylus.motion.x, stylus.motion.y);
         */

        // Apply the tablet:screen ratio to the amount of motion
        // (because it's usually a sane value)
        //
        // TODO: Replace with actual mouse acceleration.
        //
        nx = stylus.oldPos.x - screenBounds.origin.x + stylus.motion.x * mouse_mult;
        ny = stylus.oldPos.y - screenBounds.origin.y + stylus.motion.y * mouse_mult;

        // In mouse mode limit motion to the designated screen bounds
        if (nx < 0) nx = 0;
        if (nx >= swide) nx = swide - 1;
        if (ny < 0) ny = 0;
        if (ny >= shigh) ny = shigh - 1;
    }
    else {
        // Get the ratio of the screen to the tablet
        CGFloat hratio = swide / twide, vratio = shigh / thigh;

        // Constrain the stylus to the active tablet area
        CGFloat x = stylus.point.x, y = stylus.point.y;
        if (x < tx1)  x = tx1;
        if (x > tx2)  x = tx2;
        if (y < ty1)  y = ty1;
        if (y > ty2)  y = ty2;

        // Map the Stylus Point to the active Screen Area
        nx = (sx1 + (x - tx1) * hratio);
        ny = (sy1 + (y - ty1) * vratio);
    }

    stylus.scrPos.x = (SInt16)nx + screenBounds.origin.x;
    stylus.scrPos.y = (SInt16)ny + screenBounds.origin.y;

    //
    // Map Stylus buttons to system buttons
    //
    bzero(buttonState, sizeof(buttonState));
    buttonState[button_mapping[kStylusTip]]     |= stylus.button[kStylusTip];
    buttonState[button_mapping[kStylusButton1]] |= stylus.button[kStylusButton1];
    buttonState[button_mapping[kStylusButton2]] |= stylus.button[kStylusButton2];
    buttonState[button_mapping[kStylusEraser]]  |= stylus.button[kStylusEraser];

    int buttonEvent = (dragState || buttonState[kSystemClickOrRelease] || buttonState[kSystemButton1] || buttonState[kSystemEraser]) ? NX_LMOUSEDRAGGED : (buttonState[kSystemButton2] ? NX_RMOUSEDRAGGED : NX_MOUSEMOVED);

    //
    // TODO: Support eraser-via-button by sending a stream of events:
    //
    // 1. Current button up
    // 2. Current tip-type exit proximity
    // 3. Eraser enter proximity
    // 4. Eraser down
    //

    bool postedPosition = false;

    // Has the stylus moved in or out of range?
    if (oldStylus.off_tablet != stylus.off_tablet) {
        if ((stylus.proximity.enterProximity = !stylus.off_tablet))
            stylus.proximity.pointerType = (stylus.eraser_flag && (button_mapping[kStylusEraser] == kSystemEraser)) ? NX_TABLET_POINTER_ERASER : NX_TABLET_POINTER_PEN;
        POST_EVENT(buttonEvent, NX_SUBTYPE_TABLET_PROXIMITY);
        //      fprintf(stderr, "Stylus has %s proximity\n", stylus.off_tablet ? "exited" : "entered");
    }

    // Is a Double-Click warranted?
    // TODO: Use a timer to generate these events
    if (buttonState[kSystemDoubleClick] && !oldButtonState[kSystemDoubleClick]) {
        if (oldButtonState[kSystemButton1]) {
            POST_EVENT(NX_LMOUSEUP, NX_SUBTYPE_TABLET_POINT);
        }

        POST_EVENT(NX_LMOUSEDOWN, NX_SUBTYPE_TABLET_POINT);
        POST_EVENT(NX_LMOUSEUP, NX_SUBTYPE_TABLET_POINT);
        POST_EVENT(NX_LMOUSEDOWN, NX_SUBTYPE_TABLET_POINT, kCGMouseButtonLeft, 2);

        if (!oldButtonState[kSystemButton1]) {
            POST_EVENT(NX_LMOUSEUP, NX_SUBTYPE_TABLET_POINT, kCGMouseButtonLeft, 2);
        }

        postedPosition = true;
    }

    // Is a Single-Click warranted?
    // TODO: Use a timer to generate these events
    if (buttonState[kSystemSingleClick] && !oldButtonState[kSystemSingleClick]) {
        if (oldButtonState[kSystemButton1]) {
            POST_EVENT(NX_LMOUSEUP, NX_SUBTYPE_TABLET_POINT);
        }

        POST_EVENT(NX_LMOUSEDOWN, NX_SUBTYPE_TABLET_POINT);
        POST_EVENT(NX_LMOUSEUP, NX_SUBTYPE_TABLET_POINT);

        if (oldButtonState[kSystemButton1]) {
            POST_EVENT(NX_LMOUSEDOWN, NX_SUBTYPE_TABLET_POINT);
        }

        postedPosition = true;
    }

    // Is this a Grab or Drop ?
    if (!buttonState[kSystemClickOrRelease] && oldButtonState[kSystemClickOrRelease]) {
        dragState = !dragState;

        if (!dragState || !buttonState[kSystemButton1]) {
            POST_EVENT((dragState ? NX_LMOUSEDOWN : NX_LMOUSEUP), NX_SUBTYPE_TABLET_POINT);
            postedPosition = true;
            //          fprintf(stderr, "Drag %sed\n", dragState ? "Start" : "End");
        }
    }

    // Has Button 1 changed?
    if (oldButtonState[kSystemButton1] != buttonState[kSystemButton1]) {
        if (dragState && !buttonState[kSystemButton1]) {
            dragState = false;
            //          fprintf(stderr, "Drag Canceled\n");
        }

        if (!dragState) {
            POST_EVENT((buttonState[kSystemButton1] ? NX_LMOUSEDOWN : NX_LMOUSEUP), NX_SUBTYPE_TABLET_POINT);
            postedPosition = true;
        }
    }

    // Has Button 2 changed?
    if (oldButtonState[kSystemButton2] != buttonState[kSystemButton2]) {
        POST_EVENT((buttonState[kSystemButton2] ? NX_RMOUSEDOWN : NX_RMOUSEUP), NX_SUBTYPE_TABLET_POINT, kCGMouseButtonRight);
        postedPosition = true;
    }

    // Has the Eraser changed?
    if (oldButtonState[kSystemEraser] != buttonState[kSystemEraser]) {
        POST_EVENT((buttonState[kSystemEraser] ? NX_LMOUSEDOWN : NX_LMOUSEUP), NX_SUBTYPE_TABLET_POINT);
        postedPosition = true;
    }

    // Has Button 3 changed?
    if (oldButtonState[kSystemButton3] != buttonState[kSystemButton3])
        POST_EVENT((buttonState[kSystemButton3] ? NX_OMOUSEDOWN : NX_OMOUSEUP), NX_SUBTYPE_DEFAULT, kOtherButton3);

    // Has Button 4 changed?
    if (oldButtonState[kSystemButton4] != buttonState[kSystemButton4])
        POST_EVENT((buttonState[kSystemButton4] ? NX_OMOUSEDOWN : NX_OMOUSEUP), NX_SUBTYPE_DEFAULT, kOtherButton4);

    // Has Button 5 changed?
    if (oldButtonState[kSystemButton5] != buttonState[kSystemButton5])
        POST_EVENT((buttonState[kSystemButton5] ? NX_OMOUSEDOWN : NX_OMOUSEUP), NX_SUBTYPE_DEFAULT, kOtherButton5);

    // Has the stylus changed position?
    if (!postedPosition && (oldStylus.point.x != stylus.point.x || oldStylus.point.y != stylus.point.y))
        POST_EVENT(buttonEvent, NX_SUBTYPE_TABLET_POINT);

    // Finally, remember the current state for next time
    bcopy(&stylus, &oldStylus, sizeof(stylus));
    bcopy(&buttonState, &oldButtonState, sizeof(buttonState));
}

//
//  PostNXEvent
//
void WacomTablet::PostNXEvent(int eventType, SInt16 eventSubType, UInt8 otherButton) {
    if (!tablet_on)
        return;

#if LOG_STREAM_TO_FILE
    if (logfile) fprintf(logfile, " | PostNXEvent(%d, %d, %02X)", eventType, eventSubType, otherButton);
#endif

    static NXEventData eventData;

    switch (eventType) {
        case NX_OMOUSEUP:
        case NX_OMOUSEDOWN:
            // TODO: the CG version sets subtype, so I'm adding it here too
            eventData.mouse.subType = eventSubType;
            eventData.mouse.click = 0;
            eventData.mouse.buttonNumber = otherButton;

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " (other button)");
#endif
            break;

        case NX_LMOUSEUP:
        case NX_LMOUSEDOWN:
        case NX_RMOUSEUP:
        case NX_RMOUSEDOWN:

            //          fprintf(output, "[POST] Button Event %d\n", eventType);

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " | UP/DOWN | pressure=%u", stylus.pressure);
#endif
            eventData.mouse.subType = eventSubType;
            eventData.mouse.subx = 0;
            eventData.mouse.suby = 0;
            eventData.mouse.pressure = stylus.pressure;

            /* SInt16 */    // eventData.mouse.eventNum = 1;        /* unique identifier for this button */
            /* SInt32 */    eventData.mouse.click = 0;              /* click state of this event */
            /* UInt8 */     // eventData.mouse.buttonNumber = 1;    /* button generating other button event (0-31) */
            /* UInt8 */     eventData.mouse.reserved2 = 0;
            /* SInt32 */    eventData.mouse.reserved3 = 0;

            switch (eventSubType) {
                case NX_SUBTYPE_TABLET_POINT:
                    eventData.mouse.tablet.point.x = stylus.point.x;
                    eventData.mouse.tablet.point.y = stylus.point.y;
                    eventData.mouse.tablet.point.buttons = 0x0000;
                    eventData.mouse.tablet.point.tilt.x = stylus.tilt.x;
                    eventData.mouse.tablet.point.tilt.y = stylus.tilt.y;
                    eventData.mouse.tablet.point.deviceID = stylus.proximity.deviceID;

#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | point=(%d,%d) | tilt=(%d,%d)", stylus.point.x, stylus.point.y, stylus.tilt.x, stylus.tilt.y);
#endif

                    /* SInt32 */ eventData.mouse.tablet.point.z = 0;                    /* absolute z coordinate in tablet space at full tablet resolution */
                    /* UInt16 */ eventData.mouse.tablet.point.pressure = stylus.pressure;               /* scaled pressure value; MAX=(2^16)-1, MIN=0 */
                    /* UInt16 */ eventData.mouse.tablet.point.rotation = 0;             /* Fixed-point representation of device rotation in a 10.6 format */
                    /* SInt16 */ eventData.mouse.tablet.point.tangentialPressure = 0;   /* tangential pressure on the device; same range as tilt */
                    //                  /* SInt16 */ eventData.mouse.tablet.point.vendor1 = 0;              /* vendor-defined signed 16-bit integer */
                    //                  /* SInt16 */ eventData.mouse.tablet.point.vendor2 = 0;              /* vendor-defined signed 16-bit integer */
                    //                  /* SInt16 */ eventData.mouse.tablet.point.vendor3 = 0;              /* vendor-defined signed 16-bit integer */
                    break;

                case NX_SUBTYPE_TABLET_PROXIMITY:
                    bcopy(&stylus.proximity, &eventData.mouse.tablet.proximity, sizeof(stylus.proximity));
#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | PROXIMITY");
#endif
                    break;
            }
            break;

        case NX_MOUSEMOVED:
        case NX_LMOUSEDRAGGED:
        case NX_RMOUSEDRAGGED:

            //          fprintf(output, "[POST] Mouse Event %d Subtype %d\n", eventType, eventSubType);

            eventData.mouseMove.subType = eventSubType;
            /* UInt8 */     eventData.mouseMove.reserved1 = 0;
            /* SInt32 */    eventData.mouseMove.reserved2 = 0;

            switch (eventSubType) {
                case NX_SUBTYPE_TABLET_POINT:
                    eventData.mouseMove.tablet.point.x = stylus.point.x;
                    eventData.mouseMove.tablet.point.y = stylus.point.y;
                    eventData.mouseMove.tablet.point.buttons = 0x0000;
                    eventData.mouseMove.tablet.point.pressure = stylus.pressure;
                    eventData.mouseMove.tablet.point.tilt.x = stylus.tilt.x;
                    eventData.mouseMove.tablet.point.tilt.y = stylus.tilt.y;
                    eventData.mouseMove.tablet.point.deviceID = stylus.proximity.deviceID;

#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | MOVE | pressure=%u | point=(%d,%d) | tilt=(%d,%d)", stylus.pressure, stylus.point.x, stylus.point.y, stylus.tilt.x, stylus.tilt.y);
#endif

                    /* SInt32 */ eventData.mouseMove.tablet.point.z = 0;                    /* absolute z coordinate in tablet space at full tablet resolution */
                    /* UInt16 */ eventData.mouseMove.tablet.point.rotation = 0;             /* Fixed-point representation of device rotation in a 10.6 format */
                    /* SInt16 */ eventData.mouseMove.tablet.point.tangentialPressure = 0;   /* tangential pressure on the device; same range as tilt */
                    //                      /* SInt16 */ eventData.mouseMove.tablet.point.vendor1 = 0;              /* vendor-defined signed 16-bit integer */
                    //                      /* SInt16 */ eventData.mouseMove.tablet.point.vendor2 = 0;              /* vendor-defined signed 16-bit integer */
                    //                      /* SInt16 */ eventData.mouseMove.tablet.point.vendor3 = 0;              /* vendor-defined signed 16-bit integer */
                    break;

                case NX_SUBTYPE_TABLET_PROXIMITY:
                    bcopy(&stylus.proximity, &eventData.mouseMove.tablet.proximity, sizeof(NXTabletProximityData));
#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | PROXIMITY");
#endif
                    break;
            }

            // Relative motion is needed for the mouseMove event
            if (stylus.oldPos.x == SHRT_MIN) {
                eventData.mouseMove.dx = eventData.mouseMove.dy = 0;
            }
            else {
                eventData.mouseMove.dx = (SInt32)(stylus.scrPos.x - stylus.oldPos.x);
                eventData.mouseMove.dy = (SInt32)(stylus.scrPos.y - stylus.oldPos.y);
            }
            eventData.mouseMove.subx = 0;
            eventData.mouseMove.suby = 0;
            stylus.oldPos = stylus.scrPos;
#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " | delta=(%d,%d)", eventData.mouseMove.dx, eventData.mouseMove.dy);
#endif

            break;
    }

    // Generate the tablet event to the system event driver
    IOGPoint newPoint = { stylus.scrPos.x, stylus.scrPos.y };
    (void)IOHIDPostEvent(gEventDriver, eventType, newPoint, &eventData, kNXEventDataVersion, 0, kIOHIDSetCursorPosition);

#if LOG_STREAM_TO_FILE
    if (logfile) fprintf(logfile, " | xy=(%.2f,%.2f)", stylus.scrPos.x, stylus.scrPos.y);
#endif

    //
    // Some apps only expect proximity events to arrive as pure tablet events (Desktastic, for one).
    // Generate a pure tablet form of all proximity events as well.
    //
    if (eventSubType == NX_SUBTYPE_TABLET_PROXIMITY) {
        //      fprintf(output, "[POST] Proximity Event %d Subtype %d\n", NX_TABLETPROXIMITY, NX_SUBTYPE_TABLET_PROXIMITY);
        bcopy(&stylus.proximity, &eventData.proximity, sizeof(NXTabletProximityData));
        (void)IOHIDPostEvent(gEventDriver, NX_TABLETPROXIMITY, newPoint, &eventData, kNXEventDataVersion, 0, 0);
    }
}


//
//  PostCGEvent
//
void WacomTablet::PostCGEvent(CGEventType eventType, SInt16 eventSubType, CGMouseButton otherButton, UInt16 clickCount) {
    if (!tablet_on)
        return;

#if LOG_STREAM_TO_FILE
    if (logfile) fprintf(logfile, " | PostCGEvent(%d, %d, %02X)", eventType, eventSubType, otherButton);
#endif

    CGEventRef move1 = CGEventCreateMouseEvent(
                                               NULL, eventType,
                                               CGPointMake(stylus.scrPos.x, stylus.scrPos.y),
                                               otherButton  // Ignored unless eventType = kCGEventOtherMouseDown/Dragged/Up
                                               // [A.Bohm] uses kCGMouseButtonLeft
                                               );
    // maybe set these in the same way as NX
    switch (eventSubType) {
        case NX_SUBTYPE_TABLET_POINT:
            CGEventSetIntegerValueField(move1, kCGMouseEventSubtype, kCGEventMouseSubtypeTabletPoint);
            break;
        case NX_SUBTYPE_TABLET_PROXIMITY:
            CGEventSetIntegerValueField(move1, kCGMouseEventSubtype, kCGEventMouseSubtypeTabletProximity);
            break;
    }

    switch (eventType) {

        case kCGEventOtherMouseDown:
        case kCGEventOtherMouseUp:
            CGEventSetIntegerValueField(move1, kCGMouseEventClickState, clickCount);
            CGEventSetIntegerValueField(move1, kCGMouseEventButtonNumber, otherButton);

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " (other button)");
#endif
            break;

        case kCGEventLeftMouseDown:
        case kCGEventLeftMouseUp:
        case kCGEventRightMouseDown:
        case kCGEventRightMouseUp:
            //          fprintf(output, "[POST] Button Event %d\n", eventType);

            // Note: No subx/suby to set for CG
            CGEventSetDoubleValueField(move1, kCGMouseEventPressure, stylus.pressure / PRESSURE_SCALE);
            CGEventSetIntegerValueField(move1, kCGMouseEventNumber, 1); // unique identifier for this button
            CGEventSetIntegerValueField(move1, kCGMouseEventClickState, clickCount); // click count = 1 for single-click
            CGEventSetIntegerValueField(move1, kCGMouseEventButtonNumber, otherButton); // button generating other button event (0-31)

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " | UP/DOWN | pressure=%u", stylus.pressure);
#endif

            break;

        case kCGEventMouseMoved:
        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:

            // Relative motion is needed for the mouseMove event
            if (stylus.oldPos.x == SHRT_MIN) {
                CGEventSetIntegerValueField(move1, kCGMouseEventDeltaX, 0);
                CGEventSetIntegerValueField(move1, kCGMouseEventDeltaY, 0);
            }
            else {
                CGEventSetIntegerValueField(move1, kCGMouseEventDeltaX, stylus.scrPos.x - stylus.oldPos.x);
                CGEventSetIntegerValueField(move1, kCGMouseEventDeltaY, stylus.scrPos.y - stylus.oldPos.y);
            }

            stylus.oldPos = stylus.scrPos;

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, " | delta=(%d,%d)", (SInt32)(stylus.scrPos.x - stylus.oldPos.x), (SInt32)(stylus.scrPos.y - stylus.oldPos.y));
#endif

            break;
    }

    CGEventSetDoubleValueField(move1, kCGMouseEventPressure, stylus.pressure / PRESSURE_SCALE);
    CGEventSetDoubleValueField(move1, kCGTabletEventPointPressure, stylus.pressure / PRESSURE_SCALE);

    switch (eventType) {

        case kCGEventLeftMouseDown:
        case kCGEventLeftMouseUp:
        case kCGEventRightMouseDown:
        case kCGEventRightMouseUp:

        case kCGEventMouseMoved:
        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:

            switch (eventSubType) {
                case NX_SUBTYPE_TABLET_POINT:
                    CGEventSetIntegerValueField(move1, kCGTabletEventPointX, stylus.point.x);
                    CGEventSetIntegerValueField(move1, kCGTabletEventPointY, stylus.point.y);
                    CGEventSetIntegerValueField(move1, kCGTabletEventPointButtons, 0x0000);
                    CGEventSetDoubleValueField(move1, kCGTabletEventPointPressure, stylus.pressure / PRESSURE_SCALE);
                    CGEventSetDoubleValueField(move1, kCGTabletEventTiltX, stylus.tilt.x);
                    CGEventSetDoubleValueField(move1, kCGTabletEventTiltY, stylus.tilt.y);
                    CGEventSetIntegerValueField(move1, kCGTabletEventDeviceID, stylus.proximity.deviceID);
                    CGEventSetIntegerValueField(move1, kCGTabletEventPointZ, 0);
                    CGEventSetDoubleValueField(move1, kCGTabletEventRotation, 0);
                    CGEventSetDoubleValueField(move1, kCGTabletEventTangentialPressure, 0);


#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | MOVE | pressure=%u | point=(%d,%d) | tilt=(%d,%d)", stylus.pressure, stylus.point.x, stylus.point.y, stylus.tilt.x, stylus.tilt.y);
#endif

                    break;

                case NX_SUBTYPE_TABLET_PROXIMITY:
                    //                  CGEventSetIntegerValueField(move1, kCGMouseEventSubtype, kCGEventMouseSubtypeTabletProximity);

                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorID, stylus.proximity.vendorID);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventTabletID, stylus.proximity.tabletID);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventDeviceID, stylus.proximity.deviceID);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventSystemTabletID, stylus.proximity.systemTabletID);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorPointerType, stylus.proximity.vendorPointerType);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorPointerSerialNumber, stylus.proximity.pointerSerialNumber);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorUniqueID, stylus.proximity.uniqueID);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventCapabilityMask, stylus.proximity.capabilityMask);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventPointerType, stylus.proximity.pointerType);
                    CGEventSetIntegerValueField(move1, kCGTabletProximityEventEnterProximity, stylus.proximity.enterProximity);

                    //                    fprintf(stdout, "Post Mouse Event (subtype=proximity, pointerType=%d)\n", stylus.proximity.pointerType);

#if LOG_STREAM_TO_FILE
                    if (logfile) fprintf(logfile, " | PROXIMITY");
#endif
                    break;
            }

            break;
    }

    // Generate the tablet event to the system event driver
    CGEventPost(kCGHIDEventTap, move1);

#if LOG_STREAM_TO_FILE
    if (logfile) fprintf(logfile, " | xy=(%.2f,%.2f)", stylus.scrPos.x, stylus.scrPos.y);
#endif

    //
    // Some apps only expect proximity events to arrive as pure tablet events (Desktastic, for one).
    // Generate a pure tablet form of all proximity events as well.
    //
    if (eventSubType == NX_SUBTYPE_TABLET_PROXIMITY) {
        //      fprintf(output, "[POST] Proximity Event %d Subtype %d\n", NX_TABLETPROXIMITY, NX_SUBTYPE_TABLET_PROXIMITY);
        CGEventSetType(move1, kCGEventTabletProximity);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorID, stylus.proximity.vendorID);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventTabletID, stylus.proximity.tabletID);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventDeviceID, stylus.proximity.deviceID);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventSystemTabletID, stylus.proximity.systemTabletID);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorPointerType, stylus.proximity.vendorPointerType);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorPointerSerialNumber, stylus.proximity.pointerSerialNumber);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventVendorUniqueID, stylus.proximity.uniqueID);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventCapabilityMask, stylus.proximity.capabilityMask);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventPointerType, stylus.proximity.pointerType);
        CGEventSetIntegerValueField(move1, kCGTabletProximityEventEnterProximity, stylus.proximity.enterProximity);
        CGEventPost(kCGHIDEventTap, move1);
    }

    CFRelease(move1);
}


#pragma mark - Tablet Protocol Interpreters

//
// ProcessWacomIIS_Binary(packet, size)
//
void WacomTablet::ProcessWacomIIS_Binary(char *packet, int pack_size) {
    SInt32  h, v;
    UInt16  bm = 0;
    bool    ot = false;

    if (pack_size != 7) return;

    //
    // Populate Wacom V fields with defaults
    //
    stylus.tool = kToolTypePen;


    // Assume no button press
    ResetButtons;


    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    //
    // Get X/Y Coordinates (or motion)
    //
    h =     ((UInt32)   (packet[0] & IIs_Mask0_X) << 14)
    |   ((UInt32)   (packet[1] & IIs_Mask1_X) << 7)
    |               (packet[2] & IIs_Mask2_X);

    v =     ((UInt32)   (packet[3] & IIs_Mask3_Y) << 14)
    |   ((UInt32)   (packet[4] & IIs_Mask4_Y) << 7)
    |               (packet[5] & IIs_Mask5_Y);

    //
    // Relative Mode
    //
    if (settings[0].coordsys == kCoordSysRelative) {
        if (settings[0].origin == kOriginLL)
            v = -v;

        h += stylus.point.x;
        v += stylus.point.y;

        // This constrains to the tablet area, but it's wrong.
        if (v < 0) v = 0;
        if (v >= settings[0].yscale)
            v = settings[0].yscale - 1;

        if (h < 0) h = 0;
        if (h >= settings[0].xscale)
            h = settings[0].xscale - 1;

    }
    else if (settings[0].origin == kOriginLL)
        v = settings[0].yscale - v;

    //
    // Store new coordinates
    //
    stylus.point.x = h;
    stylus.point.y = v;

    //
    // proximity
    //
    stylus.pen_near = packet[0] & IIs_Mask0_Proximity;

    //
    // SD-Series Interpretations!
    //
    if (series_index == kModelSDSeries) {
        UInt16  press = 0;
        if (packet[0] & SD_Mask0_Pressure) {
            if (stylus.pen_near) {  // This bit actually works in II-S Continuous Mode
                press = (packet[6] & SD_Mask6_PressureLo);
                if (!(packet[6] & SD_Mask6_PressureHi))
                    press += 64;

                if (press <= 41)
                    press = 0;
                else {
                    if (press > 109) press = 109;
                    press = (UInt16)((press - 41) * PRESSURE_SCALE / 68.0);
                }
            }
        }
        else {
            char c = packet[6];

            // Values have to be interpreted smartly:
            // 21 = pen off tablet
            // 00 = low pressure
            // 23 = medium pressure
            // 22 = high pressure
            // 00 = extra high pressure

            stylus.raw_pressure = c;
            switch(c) {
                case 0x00:
                    if (oldStylus.raw_pressure == 0x22) {
                        press = (UInt16)PRESSURE_SCALE;
                        stylus.raw_pressure = 0x22;
                    }
                    else
                        press = (UInt16)(PRESSURE_SCALE / 4.0);
                    break;

                case 0x23:
                    press = (UInt16)(PRESSURE_SCALE / 2.0);
                    break;

                case 0x22:
                    press = (UInt16)(PRESSURE_SCALE * 3.0 / 4.0);
                    break;

                default:
                    press = 0;
            }

        }

        ot = (press == 0);
        stylus.pressure = press;
        if (press) bm |= kBitStylusTip;
    }

    //
    // WACOM II-S Pressure Mode Packet
    //
    else if (packet[0] & IIs_Mask0_Pressure) {
        ot = (packet[0] & IIs_Mask0_Proximity) == 0;

        UInt16  press = 0;
        if (!ot) {
            press = (packet[6] & IIs_Mask6_PressureLo);

            if (!(packet[6] & IIs_Mask6_PressureHi))
                press += 64;

            press = (press < 35) ? 0 : (UInt16)((press - 34) * PRESSURE_SCALE / 60.0);

            if (press > 0)
                bm |= kBitStylusTip;
        }

        stylus.pressure = press;
    }

    //
    // WACOM II-S Buttons
    //
    else if (packet[6] & IIs_Mask6_ButtonFlag) {

        if (packet[6] & IIs_Mask6_Button1)
            bm |= kBitStylusButton1;

        if (packet[6] & IIs_Mask6_EraserOr2) {
            //
            // we have to glean from this clue.
            // if the change is abrupt we know it's the eraser
            //
            if (stylus.off_tablet)
                stylus.eraser_flag = true;

            if (stylus.eraser_flag) {
                if (packet[6] & IIs_Mask6_EraserOrTip)
                    bm |= kBitStylusEraser;
            }
            else
                bm |= kBitStylusButton2;

        }
        else {
            if (packet[6] & IIs_Mask6_EraserOrTip)
                bm |= kBitStylusTip;
        }

        stylus.pressure = bm ? (UInt16)PRESSURE_SCALE : 0;

    }
    else
        stylus.pressure = 0;

    //
    // Stylus disengaged?
    // (This will not occur in Stream Mode)
    //
    if ((packet[0] & IIs_Mask0_Engaged) == IIs_Disengaged) {
        stylus.pen_near = false;
        stylus.eraser_flag = false;
        stylus.pressure = 0;
        stylus.tilt.x = 0;
        stylus.tilt.y = 0;
        ot = true;
    }

    //
    // When the tablet is in absolute mode and TM has "mouse mode" enabled
    // then use the active tablet area as the tablet boundary.
    // The downside of this behavior is that dragged items get dropped if you
    // go outside the active bounds. One could keep tracking pressed buttons,
    // but that would be extra work.
    //
    if (settings[0].coordsys == kCoordSysAbsolute
        && mouse_mode
        && (stylus.point.x < tabletMapping.origin.x
            || stylus.point.x > CGRectGetMaxX(tabletMapping)-1
            || stylus.point.y < tabletMapping.origin.y
            || stylus.point.y > CGRectGetMaxY(tabletMapping)-1
            )
        ) {
        stylus.point.x = stylus.old.x;
        stylus.point.y = stylus.old.y;
        stylus.pen_near = false;
        stylus.pressure = 0;
        ot = true;
        bm = 0;
    }

    if (stylus.off_tablet != ot) {
        stylus.off_tablet = ot;
        stylus.motion.x = stylus.motion.y = 0;
    }
    else {
        stylus.motion.x = stylus.point.x - stylus.old.x;
        stylus.motion.y = stylus.point.y - stylus.old.y;
    }

    SetButtons(bm);
}

//
// ProcessWacomIIS_ASCII(packet, size)
//
void WacomTablet::ProcessWacomIIS_ASCII(char *packet, int pack_size) {
    int     b;
    long    xx, yy;
    bool    ot = false;

    if (packet[1] != ',' || sscanf(packet+2, "%ld,%ld,%d", &xx, &yy, &b) < 3)
        return;

    //
    // Populate Wacom V fields with defaults
    //
    stylus.tool = kToolTypePen;


    // Assume no button press
    ResetButtons;


    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;


    //
    // Get X, Y
    //
    if (settings[0].coordsys == kCoordSysRelative) {
        if (settings[0].origin == kOriginLL)
            yy = -yy;

        // Update the position of the stylus
        xx += stylus.point.x;
        yy += stylus.point.y;

        // Constrain to the tablet coordinates for mapping purposes.
        //
        // When the tablet is in relative mode it automatically sets the
        // mouse_mode flag. Only dx, dy are actually relevant in this mode.
        // In mouse mode the tablet coordinates are pretty irrelevant.
        //
        if (xx < 0) xx = 0;
        if (xx >= settings[0].xscale)
            xx = settings[0].xscale - 1;

        if (xx < 0) xx = 0;
        if (xx >= settings[0].yscale)
            xx = settings[0].yscale - 1;
    }
    else if (settings[0].origin == kOriginLL)
        yy = settings[0].yscale - yy;

    //
    // Parse Buttons and Pressure
    //
    UInt16  press = 0, bm = 0;

    //
    // In mouse mode the area outside the tablet bounds behaves as though it is inactive.
    //
    if (settings[0].coordsys == kCoordSysAbsolute && mouse_mode && (xx < tabletMapping.origin.x || xx > CGRectGetMaxX(tabletMapping)-1 || yy < tabletMapping.origin.y || yy > CGRectGetMaxY(tabletMapping)-1)) {
        stylus.pen_near = false;
        ot = true;
    }
    else {
        stylus.point.x = (SInt32)xx;
        stylus.point.y = (SInt32)yy;

        switch (packet[0]) {
                // Stylus, button mode
            case '#':
                ot = (b == 99);
                if (series_index == kModelSDSeries) {       // This is based on an SD420L with DIPs set to: 11110000 11000001 11111100
                    stylus.raw_pressure = b;
                    ot = ot || (b == 0x01);
                    stylus.pen_near = !ot;
                    stylus.eraser_flag = false;

                    if ( !ot ) {
                        bm |= kBitStylusTip;

                        // Values have to be interpreted smartly:
                        // 01 = pen off tablet
                        // 00 = low pressure
                        // 03 = medium pressure
                        // 02 = high pressure
                        // 00 = extra high pressure

                        switch(b) {
                            case 0x00:
                                if (oldStylus.raw_pressure == 0x02) {
                                    press = (UInt16)PRESSURE_SCALE;
                                    stylus.raw_pressure = 0x02;
                                }
                                else
                                    press = (UInt16)(PRESSURE_SCALE / 4.0);
                                break;

                            case 0x03:
                                press = (UInt16)(PRESSURE_SCALE / 2.0);
                                break;

                            case 0x02:
                                press = (UInt16)(PRESSURE_SCALE * 3.0 / 4.0);
                                break;
                        }
                    }
                }
                else {
                    bm = ot ? 0 : b;

                    // Eraser or Barrel Switch 2?
                    if (bm & kBitStylusButton2) {
                        stylus.pen_near = true;

                        // Just entered proximity? Assume eraser.
                        if (stylus.off_tablet)
                            stylus.eraser_flag = true;

                        // Tracking the eraser?
                        if (stylus.eraser_flag)
                            bm = (bm & kBitStylusTip) ? kBitStylusEraser : 0x00;
                    }
                    else {
                        stylus.eraser_flag = false;
                        stylus.pen_near = !ot;
                    }

                    press = (bm & (kBitStylusTip|kBitStylusEraser)) ? (UInt16)PRESSURE_SCALE : 0;
                }
                break;

                // Stylus, pressure mode
            case '!': {
                ot = (b == -999);

                stylus.eraser_flag = false;

                // This is based on an SD420L with DIPs set to: 11110000 11000001 11111100
                if (!ot) {
                    if (series_index == kModelSDSeries) {
                        press = (b < -21) ? 0 : (UInt16)((b + 22) * PRESSURE_SCALE / 67);
                        ot = (press == 0);
                    } else
                        press = (UInt16)((b + 30) * PRESSURE_SCALE / 60);

                    bm = (press > 0) ? kBitStylusTip : 0x00;
                }

                stylus.pen_near = !ot;
                break;
            }

                // TODO: Cursor (puck)
            case '*':
                break;
        }

    }

    // Handle changes in proximity
    if (stylus.off_tablet != ot) {
        stylus.off_tablet = ot;
        stylus.motion.x = stylus.motion.y = 0;
    }
    else {
        stylus.motion.x = stylus.point.x - stylus.old.x;
        stylus.motion.y = stylus.point.y - stylus.old.y;
    }

    stylus.pressure = press;
    SetButtons(bm);
}

//
// ProcessWacomIV_Base(packet, size)
//
//  - Proximity (Engage/Disengage)
//  - Tool Type
//  - Position
//  - Pen Near
//  - Eraser Flag
//  - Off-Tablet
//  - Pressure
//
void WacomTablet::ProcessWacomIV_Base(char *packet, int pack_size) {
    bool    ot = false;

    //
    // Populate Wacom V fields with defaults
    //
    stylus.tool = kToolTypePen;

    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    stylus.pen_near = true;

    //
    // Proximity==0 && Pointer==1 ... Menu or Disengage
    //
    UInt16 press = 0;
    if ((packet[0] & IV_Mask0_Engagement) == IV_DisengagedOrMenu) {
        // Stylus has disengaged
        if (settings[0].transfer_mode == kTransferModeSuppressed) { // suppressed mode?
            stylus.pen_near = false;
            stylus.eraser_flag = false;
            stylus.tilt.x = stylus.tilt.y = 0;
            ot = true;
        }

        // Menu Button
        stylus.menu_button = packet[6];

    } else {

        //      stylus.tool = (packet[0] & IV_Mask0_Stylus) ? kToolTypePen : kToolTypeMouse;

        // x , y
        stylus.point.x =        ((short)    (packet[0] & IV_Mask0_X) << 14)
        |   ((short)    (packet[1] & IV_Mask1_X) <<  7)
        |               (packet[2] & IV_Mask2_X);

        stylus.point.y =        ((short)    (packet[3] & IV_Mask3_Y) << 14)
        |   ((short)    (packet[4] & IV_Mask4_Y) <<  7)
        |               (packet[5] & IV_Mask5_Y);

        if (settings[0].origin == kOriginLL)
            stylus.point.y = settings[0].yscale - stylus.point.y;

        // pressure
        if (base_version < 1.199)
            press = (UInt16)((packet[6] & IV_Mask6_PressureLo) | (~packet[6] & IV_Mask6_PressureHi)) << 1;
        else
            press = ((packet[3] & IV_Mask3_Pressure0) >> 2) | ((packet[6] & IV_Mask6_PressureLo) << 1) | ((~packet[6] & IV_Mask6_PressureHi) << 1);

        if (press > 0) press--;
    }

    //
    // If Mouse Mode is enabled and we're outside
    // the bounds then disengage the stylus
    //
    bool outbound = mouse_mode && (stylus.point.x < tabletMapping.origin.x || stylus.point.x > CGRectGetMaxX(tabletMapping)-1 || stylus.point.y < tabletMapping.origin.y || stylus.point.y > CGRectGetMaxY(tabletMapping)-1);

    if (outbound) {
        stylus.pen_near = false;
        press = 0;
        ot = true;
    }

    stylus.pressure = (UInt16)(press * PRESSURE_SCALE / 254);

    if (outbound || stylus.off_tablet != ot) {
        stylus.old.x = stylus.point.x;
        stylus.old.y = stylus.point.y;
    }

    stylus.motion.x = stylus.point.x - stylus.old.x;
    stylus.motion.y = stylus.point.y - stylus.old.y;

    stylus.off_tablet = ot;
}

//
// ProcessWacomIV_14(packet, size)
//
// Get IV 1.3 values plus Tilt
//
void WacomTablet::ProcessWacomIV_14(char *packet, int pack_size) {
    if ( pack_size != 7 && pack_size != 9 ) return;

    ProcessWacomIV_13(packet, 7);

    //
    // Get Tilt
    //
    if (pack_size == 9) {
        stylus.tilt.x = (SInt16)(((SInt16)(packet[7] & IV_Mask7_TiltX) - (SInt16)(packet[7] & IV_Mask7_TiltXBase)) * TILT_SCALE / 63);
        stylus.tilt.y = (SInt16)(((SInt16)(packet[8] & IV_Mask8_TiltY) - (SInt16)(packet[8] & IV_Mask8_TiltYBase)) * TILT_SCALE / 63);
    }
    else
        stylus.tilt.x = stylus.tilt.y = 0;
}

//
// ProcessWacomIV_13(packet, size)
//
// Get IV data plus Buttons
//
void WacomTablet::ProcessWacomIV_13(char *packet, int pack_size) {
    if (pack_size != 7) return;

    //
    // Button clicked?
    //
    UInt16 bm = 0;
    if (/* stylus.pen_near && */ packet[0] & IV_Mask0_ButtonFlag) {
        UInt8 b = ((packet[3] & IV_Mask3_Buttons) >> 3);    // IV button bits
        bm = b & (kBitStylusTip|kBitStylusButton1);

        if (b & kBitStylusButton2) {                        // button 3 or eraser?
            // if the change is abrupt it's the eraser
            if (stylus.off_tablet)
                stylus.eraser_flag = true;

            if (!stylus.eraser_flag)
                bm |= kBitStylusButton2;
            else if (b & kBitStylusTip) {
                bm |= kBitStylusEraser;
                bm &= (~kBitStylusTip);
            }
        }
    }

    ProcessWacomIV_Base(packet, 7);

    if (stylus.off_tablet)
        bm = 0;

    SetButtons(bm);
}

//
// ProcessWacomV(packet, size)
//
//  Portions of this code from:
//  linuxwacom
//
void WacomTablet::ProcessWacomV(char *packet, int pack_size) {
    if ( pack_size != 9 ) return;

    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    // Remember the previous button states too
    UInt16  bm, old_bm;
    bm = old_bm = stylus.button_mask;

    //
    // Device ID Packet?
    //
    // B & 1111:1100 == 1100:0000   (C0-C3)
    //
    if ((packet[0] & 0xFC) == 0xC0) {
        stylus.toolid =     ((short)(packet[1] & V_Mask1_ToolHi) << 5)
        |   ((short)(packet[2] & V_Mask2_ToolLo) >> 2);         // 04 50    0000:0100 0101:0000     0000:1001:0100      094

        stylus.serialno = ((long)(packet[2] & V_Mask2_Serial) << 30)
        | ((long)(packet[3] & V_Mask3_Serial) << 23)
        | ((long)(packet[4] & V_Mask4_Serial) << 16)
        | ((long)(packet[5] & V_Mask5_Serial) <<  9)
        | ((long)(packet[6] & V_Mask6_Serial) <<  2)
        | ((long)(packet[7] & V_Mask7_Serial) >>  5);

        UInt16 tool = kToolTypePen;
        bool ef = false;
        switch (stylus.toolid) {
            case kToolInkingPen:
            case kToolInkingPen2:
                tool = kToolTypePencil;
                break;

            case kToolPen1:
            case kToolPen2:
            case kToolPen3:
            case kToolGripPen:
                tool = kToolTypePen;
                break;

            case kToolStrokePen1:
            case kToolStrokePen2:
                tool = kToolTypeBrush;
                break;

            case kToolMouse2D:
            case kToolMouse3D:
            case kToolMouse4D:
                tool = kToolTypeMouse;
                break;

            case kToolLens:
                tool = kToolTypeLens;
                break;

            case kToolEraser1:
            case kToolEraser2:
            case kToolEraser3:
            case kToolEraser4:
                tool = kToolTypeEraser;
                ef = true;
                break;

            case kToolAirbrush:
                tool = kToolTypeAirbrush;
                break;
        }

#if LOG_STREAM_TO_FILE
        if (logfile) fprintf(logfile, "    Tool %04X Entered Proximity", stylus.toolid);
#endif

        stylus.off_tablet   = false;
        stylus.pen_near     = true;
        stylus.tool         = tool;
        stylus.eraser_flag  = ef;
    }

    //
    // The tool is out of bounds
    //
    // B & 1111:1110 == 1000:0000   (80-81)
    //
    else if ((packet[0] & 0xFE) == 0x80) {
#if LOG_STREAM_TO_FILE
        if (logfile) fprintf(logfile, "    Tool Disengaged");
#endif

        stylus.off_tablet   = true;
        stylus.pen_near     = false;
        stylus.eraser_flag  = false;
        stylus.pressure     = 0;
        stylus.wheel        = 0;
        stylus.rotation     = 0;
        stylus.throttle     = 0;

        bm  = 0;
        stylus.button[kButtonLeft]      = false;
        stylus.button[kButtonMiddle]    = false;
        stylus.button[kButtonRight]     = false;
        stylus.button[kButtonExtra]     = false;
        stylus.button[kButtonSide]      = false;
    }

    //
    // Pen Data
    //
    // B & 1011:1000 == 1010:0000   (A0-A7 E0-E7) Stylus with buttons + pressure
    // B & 1011:1110 == 1011:0100   (B4-B5 F4-F5) Stylus with wheel
    //
    else if ( (packet[0] & 0xB8) == 0xA0 || (packet[0] & 0xBE) == 0xB4 ) {
        stylus.point.x =  ((UInt32)(packet[1] & V_Mask1_X) <<  9)
        | ((UInt32)(packet[2] & V_Mask2_X) <<  2)
        | ((UInt32)(packet[3] & V_Mask3_X) >>  5);

        stylus.point.y =  ((UInt32)(packet[3] & V_Mask3_Y) << 11)
        | ((UInt32)(packet[4] & V_Mask4_Y) <<  4)
        | ((UInt32)(packet[5] & V_Mask5_Y) >>  3);

        stylus.tilt.x = packet[7] & V_Mask7_TiltX;
        if (packet[7] & V_Mask7_TiltXBase) stylus.tilt.x -= 0x40;

        stylus.tilt.y = packet[8] & V_Mask8_TiltY;
        if (packet[8] & V_Mask8_TiltYBase) stylus.tilt.y -= 0x40;

        //
        // Stylus with buttons + pressure
        //
        if ((packet[0] & 0xB8) == 0xA0) {
            UInt16 op, press = op = ((short)(packet[5] & V_Mask5_PressureHi) << 7)
            |  (short)(packet[6] & V_Mask6_PressureLo);

            press = (press < 100) ? 0 : (UInt16)((press - 99) * PRESSURE_SCALE / 924.0);

            stylus.pressure = press;

            bm = (press ? kBitStylusTip : 0)
            | ((packet[0] & V_Mask0_Button1) ? kBitStylusButton1 : 0)
            | ((packet[0] & V_Mask0_Button2) ? kBitStylusButton2 : 0);

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, "    Stylus X=%d Y=%d TX=%d TY=%d OP=%d P=%d B=%04X", stylus.point.x, stylus.point.y, stylus.tilt.x, stylus.tilt.y, op, press, bm);
#endif

        }

        //
        // Second Airbrush packet
        //
        else {
            stylus.pressure = 0;
            bm = 0;

            stylus.wheel =   ((short)(packet[5] & V_Mask5_WheelHi) << 7)
            | (short)(packet[6] & V_Mask6_WheelLo);

#if LOG_STREAM_TO_FILE
            if (logfile) fprintf(logfile, "    Airbrush X=%d Y=%d TX=%d TY=%d W=%d", stylus.point.x, stylus.point.y, stylus.tilt.x, stylus.tilt.y, stylus.wheel);
#endif
        }
    }

    //
    // Mouse Packet
    //
    // B & 1011:1110 == 1010:1000   (A8-A9 E8-E9)
    // B & 1011:1110 == 1011:0000   (B0-B1 F0-F1)
    //
    else if ( (packet[0] & 0xBE) == 0xA8 || (packet[0] & 0xBE) == 0xB0 ) {
        stylus.point.x =  ((UInt32)(packet[1] & V_Mask1_X) <<  9)
        | ((UInt32)(packet[2] & V_Mask2_X) <<  2)
        | ((UInt32)(packet[3] & V_Mask3_X) >>  5);

        stylus.point.y =  ((UInt32)(packet[3] & V_Mask3_Y) << 11)
        | ((UInt32)(packet[4] & V_Mask4_Y) <<  4)
        | ((UInt32)(packet[5] & V_Mask5_Y) >>  3);

        stylus.throttle = ((UInt32)(packet[5] & V_Mask5_ThrottleHi) << 7)
        |  (UInt32)(packet[6] & V_Mask6_ThrottleLo);

        if (packet[8] & V_Mask8_ThrottleSign) stylus.throttle = -stylus.throttle;

        SInt16  wheel = 0;

        //
        // 4D Mouse
        //
        if (stylus.toolid == kToolMouse4D) {
            bm = (((packet[8] & V_Mask8_4dButtonsHi) >> 1) | (packet[8] & V_Mask8_4dButtonsLo));

#if LOG_STREAM_TO_FILE
            fprintf(logfile, "    4D Mouse (1) X=%d Y=%d T=%d", stylus.point.x, stylus.point.y, stylus.throttle);
#endif
        }

        //
        // Lens Cursor
        //
        else if (stylus.toolid == kToolLens) {
            bm = packet[8] & V_Mask8_LensButtons;

#if LOG_STREAM_TO_FILE
            fprintf(logfile, "    Lens X=%d Y=%d T=%d", stylus.point.x, stylus.point.y, stylus.throttle);
#endif
        }

        //
        // 2D Mouse
        //
        else {
            wheel = ((packet[8] & V_Mask8_WheelUp) >> 1) - (packet[8] & V_Mask8_WheelDown);
            bm = (packet[8] & V_Mask8_2dButtons) >> 2;

#if LOG_STREAM_TO_FILE
            fprintf(logfile, "    2D Mouse X=%d Y=%d T=%d", stylus.point.x, stylus.point.y, stylus.throttle);
#endif
        }

        //
        // Map Mouse Buttons to the virtual stylus
        //
        stylus.button[kButtonLeft]      = 0!=(bm & 0x01);
        stylus.button[kButtonMiddle]    = 0!=(bm & 0x02);
        stylus.button[kButtonRight]     = 0!=(bm & 0x04);
        stylus.button[kButtonExtra]     = 0!=(bm & 0x08);
        stylus.button[kButtonSide]      = 0!=(bm & 0x10);

        stylus.wheel = wheel;

#if LOG_STREAM_TO_FILE
        if (logfile) fprintf(logfile, " W=%d B=%04X", stylus.wheel, bm);
#endif
    }

    //
    // Second 4D mouse packet
    //
    else {
        //
        // B & 1011:1110 == 1010:1010 (AA-AB EA-EB)
        //
        if ((packet[0] & 0xBE) == 0xAA) {
            stylus.point.x =  ((UInt32)(packet[1] & V_Mask1_X) <<  9)
            | ((UInt32)(packet[2] & V_Mask2_X) <<  2)
            | ((UInt32)(packet[3] & V_Mask3_X) >>  5);

            stylus.point.y =  ((UInt32)(packet[3] & V_Mask3_Y) << 11)
            | ((UInt32)(packet[4] & V_Mask4_Y) <<  4)
            | ((UInt32)(packet[5] & V_Mask5_Y) >>  3);

            //
            // 4D Mouse has Rotation
            //
            SInt16 rot = ((short)(packet[6] & V_Mask6_4dRotationHi) << 7)
            | (short)(packet[7] & V_Mask7_4dRotationLo);

            if (rot < 900) rot = -rot;
            else rot = 1799 - rot;
            stylus.rotation = rot;

#if LOG_STREAM_TO_FILE
            fprintf(logfile, "    4D Mouse (2) X=%d Y=%d R=%d", stylus.point.x, stylus.point.y, stylus.rotation);
#endif
        }
        else
            stylus.rotation = 0;
    }

    stylus.motion.x = stylus.point.x - stylus.old.x;
    stylus.motion.y = stylus.point.y - stylus.old.y;

    if (old_bm != bm)
        SetButtons(bm);
}

//
// ProcessGraphire(packet, size)
//
void WacomTablet::ProcessGraphire(char *packet, int pack_size) {
    UInt16  bm = 0;
    int     wheel = 0;

    // check for a valid packet
    if (pack_size != 7) return;

    // store old position for relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    // get pressure
    stylus.pressure = ((packet[6] & 0x3F) << 2 ) | ((packet[3] & 0x04) >> 1) | ((packet[3] & 0x40) >> 6) | ((packet[6] & 0x40) << 2);


    //
    // Get proximity, x/y, device type, (cursor if [0] & 0x20), and pressure
    //
    ProcessWacomIV_Base(packet, 7);

    // get buttons
    bm = (packet[3] & 0x38) >> 3;

    // get the mouse wheel if it's active
    if (stylus.tool == kToolTypeMouse) {
        stylus.wheel = (packet[6] & 0x30) >> 4;
        if (packet[6] & 0x40) stylus.wheel = -stylus.wheel;
    }


    // --------------------------------------------------------------------------------


    stylus.pen_near = true;
    stylus.off_tablet = false;
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    if (!quiet_mode && (packet[0] != 0x02))
        fprintf(output, "[PROC] Graphire: Unknown Packet #%d\n", packet[0]);

    if ( packet[1] & 0x80 ) {
        stylus.point.x = packet[2] | (packet[3] << 8);
        stylus.point.y = packet[4] | (packet[5] << 8);
    }
    stylus.motion.x = stylus.point.x - stylus.old.x;
    stylus.motion.y = stylus.point.y - stylus.old.y;

    int tool = (packet[1] >> 5) & 0x03;

    if (tool < 2) {
        stylus.pressure = packet[6] | (packet[7] << 8);

        bm =    ((packet[1] & 0x01) ? kBitStylusTip : 0)
        |   ((packet[1] & 0x02) ? kBitStylusButton1 : 0)
        |   ((packet[1] & 0x04) ? kBitStylusButton2 : 0);
    }

    switch (tool) {
        case 0: // Pen
            //          input_report_key(dev, BTN_TOOL_PEN, packet[1] & 0x80);
            stylus.tool = kToolTypePen;
            stylus.eraser_flag = false;
            break;

        case 1: // Eraser
            //          input_report_key(dev, BTN_TOOL_RUBBER, packet[1] & 0x80);
            stylus.tool = kToolTypePen;
            stylus.eraser_flag = true;
            break;

        case 2: // Mouse
            //          input_report_key(dev, BTN_TOOL_MOUSE, packet[7] > 24);
            //          input_report_abs(dev, ABS_DISTANCE, packet[7]);
            stylus.tool = kToolTypeMouse;
            stylus.eraser_flag = false;

            bm =    ((packet[1] & 0x01) ? BIT(kButtonLeft) : 0)
            |   ((packet[1] & 0x02) ? BIT(kButtonRight) : 0)
            |   ((packet[1] & 0x04) ? BIT(kButtonMiddle) : 0);

            wheel = packet[6];
            break;
    }

    stylus.wheel = wheel;
    SetButtons(bm);
}

//
// ProcessTabletPC(packet, size)
//
void WacomTablet::ProcessTabletPC(char *packet, int pack_size) {
    if (pack_size != 9) return;

    UInt16  bm = 0;   // math depends on these being 16-bit
    bool    ot = false;

    // Always assume the pen
    stylus.tool = kToolTypePen;


    // Assume no button press
    ResetButtons;


    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    //
    // Get X/Y Coordinates
    //
    stylus.point.x =    ((short)(packet[6] & TPC_Mask6_X) >> 5)
    |   ((short) packet[2] << 2)
    |   ((short) packet[1] << 9);

    stylus.point.y =    ((short)(packet[6] & TPC_Mask6_Y) >> 3)
    |   ((short) packet[4] << 2)
    |   ((short) packet[3] << 9);

    //
    // Proximity
    //
    ot = (packet[0] & TPC_Mask0_Proximity) == 0;
    stylus.pen_near = !ot;

    if (ot) {
        stylus.pressure = 0;
        stylus.eraser_flag = false;
    }
    else {
        //
        // Eraser
        //
        // Note that the Eraser bit is the same as the Switch 2 bit!
        // Thus on TabletPC the Eraser has to be remapped to Button 2
        //
        stylus.eraser_flag = (packet[0] & TPC_Mask0_Eraser) != 0;

        UInt16 press = ((UInt16)(packet[6] & TPC_Mask6_PressureHi) << 7) | (packet[5] & TPC_Mask5_PressureLo);
        press = (press < 25) ? 0 : (UInt16)((press - 24) * PRESSURE_SCALE / 231.0);
        stylus.pressure = press;

        if (press)
            bm = (packet[0] & TPC_Mask0_Touch) ? kBitStylusTip : 0;

        if (!stylus.eraser_flag) {
            //
            // Stylus Buttons
            //
            bm |= ((packet[0] & TPC_Mask0_Switch1) ? kBitStylusButton1 : 0)
            | ((packet[0] & TPC_Mask0_Switch2) ? kBitStylusButton2 : 0);
        }
    }

    if (stylus.off_tablet != ot) {
        stylus.off_tablet = ot;
        stylus.motion.x = stylus.motion.y = 0;
    }
    else {
        stylus.motion.x = stylus.point.x - stylus.old.x;
        stylus.motion.y = stylus.point.y - stylus.old.y;
    }

    SetButtons(bm);
}


//
// ProcessFinepoint(packet, size)
//
void WacomTablet::ProcessFinepoint(char *packet, int pack_size) {
}


//
// ProcessFujitsuPSeries(packet)
//
void WacomTablet::ProcessFujitsuPSeries(char *packet) {
    bool    ot = false;

    // Always assume the pen
    stylus.tool = kToolTypePen;

    // Remember the old position for tracking relative motion
    stylus.old.x = stylus.point.x;
    stylus.old.y = stylus.point.y;

    //
    // Get X/Y Coordinates
    //
    /*
     SInt32 xint = packet[1] * 128 + packet[0];
     SInt32 yint = packet[3] * 128 + packet[2];

     SInt32 x = 1024 * (((float)xint -  93) / 3940);
     SInt32 y =  768 * (((float)yint - 152) / 3848);
     */
    float xdec = packet[1] + ((float)packet[0] / 128);
    float ydec = packet[3] + ((float)packet[2] / 128);

    SInt32 x = (SInt32)(1024.0f * (xdec - 0.73f) / 30.78f);
    SInt32 y = (SInt32)( 768.0f * (ydec - 1.19f) / 30.06f);
    /**/
    if (x < 0) x = 0;
    if (x >= 1024) x = 1023;
    if (y < 0) y = 0;
    if (y >= 768) y = 767;

    stylus.point.x = x;
    stylus.point.y = y;

    stylus.pen_near = true;     // the pen is always "near" (move this to init?)
    switch(packet[0]) {
        case 1:     // button engaged
            SetButtons(kBitStylusTip);
            stylus.pressure = PRESSURE_SCALE;
            ot = false;
            break;
        case 2:     // button disengaged
            ResetButtons;
            stylus.pressure = 0;
            ot = true;
            break;
    }

    //
    // === RELATIVE MOTION
    //
    if (stylus.off_tablet != ot) {
        stylus.off_tablet = ot;
        stylus.motion.x = stylus.motion.y = 0;
    }
    else {
        stylus.motion.x = stylus.point.x - stylus.old.x;
        stylus.motion.y = stylus.point.y - stylus.old.y;
    }

}


#pragma mark - Screen and Tablet Mapping

//
// SetScreenMapping
//
// Set the screen region that corresponds to
// the active portion of the tablet.
//
void WacomTablet::SetScreenMapping(SInt16 x1, SInt16 y1, SInt16 x2, SInt16 y2) {
    x1 = (x1 == -1) ? 0 : x1;
    y1 = (y1 == -1) ? 0 : y1;

    screenMapping = CGRectMake(
                               x1, y1,
                               (x2 == -1) ? (screenBounds.size.width - x1) : x2 - x1 + 1,
                               (y2 == -1) ? (screenBounds.size.height - y1) : y2 - y1 + 1
                               );

    if (!quiet_mode)
        fprintf(output, "[PROC] Screen Bounds: (%.0f, %.0f) - (%.0f, %.0f)\n", CGRectGetMinX(screenMapping), CGRectGetMinY(screenMapping), CGRectGetMaxX(screenMapping)-1, CGRectGetMaxY(screenMapping)-1);
}


//
// InitTabletBounds
//
// Initialize the active tablet region.
// This is called to set the initial mapping of the tablet.
// Command-line arguments might leave some of these set to -1.
// The rest are set when the tablet dimensions are received.
//
void WacomTablet::InitTabletBounds(SInt32 x1, SInt32 y1, SInt32 x2, SInt32 y2) {
    tabletMapping.origin.x = (x1 != -1) ? x1 : 0;
    tabletMapping.origin.y = (y1 != -1) ? y1 : 0;
    tabletMapping.size.width = (x2 != -1) ? (x2 - tabletMapping.origin.x + 1) : settings[0].xscale;
    tabletMapping.size.height = (y2 != -1) ? (y2 - tabletMapping.origin.y + 1) : settings[0].yscale;

    if (!quiet_mode)
        fprintf(output, "[PROC] Tablet Bounds: (%.0f, %.0f) - (%.0f, %.0f)\n", tabletMapping.origin.x, tabletMapping.origin.y, CGRectGetMaxX(tabletMapping)-1, CGRectGetMaxY(tabletMapping)-1);
}

//
// UpdateTabletScale(h, v, tellprefs=false)
//
// Update the tablet scale and recalculate the bounds
//
void WacomTablet::UpdateTabletScale(SInt32 h, SInt32 v, bool tellprefs) {
    //  fprintf(output, "Updating Scale from %ld x %ld to %ld x %ld\n", settings[0].xscale, settings[0].yscale, h, v);

    bool didalter = false;

    if (settings[0].xscale != h) {
        float propX = (float)h / (float)settings[0].xscale;
        settings[0].xscale = h;
        tabletMapping.origin.x *= propX;
        tabletMapping.size.width *= propX;
        didalter = tellprefs;
    }

    if (settings[0].yscale != v) {
        float propY = (float)v / (float)settings[0].yscale;
        settings[0].yscale = v;
        tabletMapping.origin.y *= propY;
        tabletMapping.size.height *= propY;
        didalter = tellprefs;
    }

    // Tell the prefs pane about it
    if (didalter)
        SendMessageScale();
}

#pragma mark - Resolution Change

void WacomTablet::ScreenChanged() {
    if (!quiet_mode) fprintf(output, "The resolution changed!\n");

    CGFloat oldScreenWidth = screenBounds.size.width,
    oldScreenHeight = screenBounds.size.height;

    UpdateDisplaysBounds();

    if (screenBounds.size.width != oldScreenWidth) {
        CGFloat propX = screenBounds.size.width / oldScreenWidth;
        screenMapping.origin.x *= propX;
        screenMapping.size.width *= propX;
    }

    if (screenBounds.size.height != oldScreenHeight) {
        CGFloat propY = screenBounds.size.height / oldScreenHeight;
        screenMapping.origin.y *= propY;
        screenMapping.size.height *= propY;
    }
}

void WacomTablet::ResolutionChangeCallback( CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo ) {
    ((WacomTablet*)observer)->ScreenChanged();
}

void WacomTablet::DisplayCallback(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo) {
    ((WacomTablet*)userInfo)->ScreenChanged();
}

#pragma mark - Major system event handling

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

IONotificationPortRef   powerRef = NULL;
CFRunLoopSourceRef      pwrRunLoop = 0;
io_object_t             pwrNotifier = 0;
io_connect_t            pwrRootPort = 0;

void WacomTablet::RegisterForNotifications() {
    //  CFNotificationCenterAddObserver(
    //      CFNotificationCenterGetLocalCenter(),
    //      this,
    //      WacomTablet::ResolutionChangeCallback,
    //      NSApplicationDidChangeScreenParametersNotification,
    //      NULL,
    //      CFNotificationSuspensionBehaviorDeliverImmediately
    //      );

    (void)CGDisplayRegisterReconfigurationCallback(WacomTablet::DisplayCallback, this);

    // Enable "Wake from sleep" handling
    if ((pwrRootPort = IORegisterForSystemPower(0, &powerRef, WacomTablet::PowerCallBack, &pwrNotifier))) {
        pwrRunLoop = IONotificationPortGetRunLoopSource(powerRef);
        CFRunLoopAddSource(
                           CFRunLoopGetCurrent(),
                           pwrRunLoop,
                           kCFRunLoopDefaultMode
                           );
    }
}

void WacomTablet::DisableNotifications() {
    // Disable "Wake from sleep" handling
    if (powerRef) {
        CFRunLoopRemoveSource(
                              CFRunLoopGetCurrent(),
                              pwrRunLoop,
                              kCFRunLoopDefaultMode
                              );
        CFRunLoopSourceInvalidate(pwrRunLoop);
        CFRelease(pwrRunLoop);
        pwrRunLoop = NULL;
        powerRef = NULL;
    }

    IODeregisterForSystemPower(&pwrNotifier);
}

void WacomTablet::PowerCallBack(void *x, io_service_t y, natural_t messageType, void *messageArgument) {
    switch (messageType) {
        case kIOMessageSystemWillSleep:
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(pwrRootPort, (long)messageArgument);
            break;
        case kIOMessageSystemHasPoweredOn:
            if (tablet->IsActive())
                tablet->InitializeForPort(args.port);
            break;
    }
}


#pragma mark - Pref Pane Messaging

#include <CoreFoundation/CoreFoundation.h>

CFDataRef WacomTablet::message_callback(CFMessagePortRef loc, SInt32 msgid, CFDataRef data, void *info) {
    return ((WacomTablet*)info)->HandleMessage(loc, msgid, data);
}


//--------------------------------------------------------------------------------
// HandleMessage ( port , message , data )
//
// Message Format: command parameters
//
//  Parameters are separated by spaces.
//  Delimit with quotes if spaces are passed.
//
//  The parser matches the command string and calls the associated function.
//
CFDataRef WacomTablet::HandleMessage(CFMessagePortRef loc, SInt32 msgid, CFDataRef data) {
#pragma unused(loc)
#pragma unused(msgid)

    char *message = (char*) CFDataGetBytePtr(data);

    // Parse the message to find the first token
    bool ismatch = false;
    char *msgptr;
    unsigned i;
    for (i=0; i<sizeof(TMParseData) / sizeof(TMCommandNode); i++) {
        const char *token = TMParseData[i].token_string;
        msgptr = message;
        if (0 == strncmp(token, message, strlen(token))) {
            msgptr = message + strlen(token);

            if (*msgptr == '\n' || *msgptr == '\r')
                *msgptr = '\0';

            if (*msgptr == '\0' || *msgptr == ' ') {
                ismatch = true;
                break;
            }
        }

    }

    char message_reply[100];

    if (ismatch) {
        strcpy(message_reply, "[ok]");

        while (*msgptr == ' ') msgptr++;

        switch(TMParseData[i].code_number) {
            case PREF_SELECT_PORT:
                if (args.port) free(args.port);
                (void)asprintf(&args.port, "%s", msgptr);
                InitializeForPort(msgptr);
                break;

            case PREF_REINIT_PORT: {
                SendUDSetupString(msgptr, 0, false);
                serialPort.ReInit(&settings[0]);
                (void)usleep(100000);   // 0.1 seconds
                if (can_parse_ud_setup) GetUDSettingsOrFail(0);
                break;
            }

            case PREF_START:
                SetProcessing(true);
                break;
            case PREF_STOP:
                SetProcessing(false);
                break;

#if !ASYNCHRONOUS_MESSAGING
            case PREF_NEXT_MESSAGE:
                strcpy(message_reply, PopMessageQueue());
                break;
            case PREF_PREFS_HELLO:
                EnableMessageQueue();
                break;
            case PREF_PREFS_BYE:
                DisableMessageQueue();
                break;
#endif

            case PREF_GET_INFO:
                strcpy(message_reply, GetMessageInfo());
                break;
            case PREF_GET_MODEL:
                strcpy(message_reply, GetMessageModel());
                break;
            case PREF_GET_SCALE:
                strcpy(message_reply, GetMessageScale());
                break;
            case PREF_QUIT:
                quitProcessor = true;
                break;

            case PREF_GET_MEMORY_BANK: {
                int bank = 0;
                (void)sscanf(msgptr, "%d", &bank);
                RequestUDSettings(bank);
                break;
            }

            case PREF_GET_SERIALPORT:
                strcpy(message_reply, GetMessageSerialPort());
                break;

                //
                // Raw data streaming, for the preference pane
                //
            case PREF_STREAM:
                strcpy(message_reply, GetMessageStream());
                break;
            case PREF_DATASTREAM_START:
                SetStreamLogging(true);
                break;
            case PREF_DATASTREAM_STOP:
                SetStreamLogging(false);
                break;

            case PREF_SEND_COMMAND:
                SendCommandToTablet(msgptr);
                break;
            case PREF_SEND_REQUEST:
                (void)SendRequestToTablet(msgptr);
                break;

            case PREF_SET_SETUP:
                SendUDSetupString(msgptr);
                break;

            case PREF_SET_MEMORY: {
                int bank = 0;
                char setup[50];
                (void)sscanf(msgptr, "%d %s", &bank, setup);
                SendUDSetupString(setup, bank);
                break;
            }

            case PREF_SET_SCALE: {
                int scalex, scaley;
                if (2 == sscanf(msgptr, "%d %d", &scalex, &scaley)) {
                    switch(series_index) {
                        case kModelCintiq:
                        case kModelArtZ:
                        case kModelArtPad:
                        case kModelSDSeries:
                            SendScaleToTablet(scalex, scaley);
                        default:
                            break;
                    }

                    UpdateTabletScale(scalex, scaley);
                }
                break;
            }

            case PREF_GET_GEOMETRY:
                strcpy(message_reply, GetMessageGeometry());
                break;

            case PREF_SET_GEOMETRY: {
                unsigned    tl, tt, tr, tb, sl, st, sr, sb, b0, b1, b2, be;
                int         mm;
                float       ms;
                sscanf(msgptr, "%d %d %d %d : %d %d %d %d : %d %d %d %d : %d %f",
                       &tl, &tt, &tr, &tb,  &sl, &st, &sr, &sb,  &b0, &b1, &b2, &be,  &mm, &ms);

                SetMouseMode(mm != 0);
                InitTabletBounds(tl, tt, tr, tb);
                SetScreenMapping(sl, st, sr, sb);
                button_mapping[kStylusTip] = b0;
                button_mapping[kStylusButton1] = b1;
                button_mapping[kStylusButton2] = b2;
                button_mapping[kStylusEraser] = be;
                break;
            }

            case PREF_SET_MOUSE_MODE_AND_SCALING: {
                int mm;
                float ms;
                sscanf(msgptr, "%d %f", &mm, &ms);
                SetMouseMode(mm != 0);
                SetMouseScaling(ms);
                break;
            }

            case PREF_PANIC:
                ResetStylus();
                break;

            case PREF_TABLETPC:
                args.forcepc = (*msgptr == '1');
                InitializeForPort(args.port);
                break;

        }
    }
    else
        strcpy(message_reply, "[error]");

    // data and returnData will be released when the callback returns
    CFDataRef returnData = CFDataCreate(kCFAllocatorDefault, (UInt8*)message_reply, strlen(message_reply)+1);

    if (returnData == NULL)
        fprintf(stderr, "[ERR ] Couldn't create reply data!");

    return returnData;
}

/*
 *
 *  The Preference Pane looks for the daemon's MessagePort and
 *  communicates with the daemon through it.
 *
 *  The daemon will respond to messages like these:
 *
 *  - Suspend sending system events (i.e., disable)
 *  - Send me the tablet dimensions
 *  - Send me the tablet settings string
 *  - Send me the current raw stylus data
 *
 */

void WacomTablet::CreateLocalMessagePort() {
    CFMessagePortContext context = { 0, this, NULL, NULL, NULL };

    local_message_port = CFMessagePortCreateLocal(NULL, CFSTR("com.thinkyhead.tabletmagic.daemon"), message_callback, &context, false);
    if (NULL != local_message_port) {
        CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(NULL, local_message_port, 0);
        CFRunLoopAddSource( CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode );
    }
    else
        syslog(LOG_NOTICE, "[ERR ] Couldn't create a local message port!");
}


#pragma mark -

#if ASYNCHRONOUS_MESSAGING

CFMessagePortRef WacomTablet::GetRemoteMessagePort() {
    if (remote_message_port == NULL
        && (remote_message_port = CFMessagePortCreateRemote(kCFAllocatorDefault, CFSTR("com.thinkyhead.tabletmagic.prefpane"))) )
        CFMessagePortSetInvalidationCallBack(remote_message_port, invalidation_callback);

    return remote_message_port;
}

void WacomTablet::invalidation_callback(CFMessagePortRef mp, void *info) {
    tablet->HandleInvalidation(mp);
}

void WacomTablet::HandleInvalidation(CFMessagePortRef mp) {
    if (mp == remote_message_port) {
        CFRelease(remote_message_port);
        remote_message_port = NULL;
    }
}

#else

#pragma mark -

void WacomTablet::AddMessageToQueue(CFDataRef data) {
    CFArrayAppendValue(outgoing_message_queue, data);
}

char* WacomTablet::PopMessageQueue() {
    static char out[100];
    out[0] = '\0';
    if (CFArrayGetCount(outgoing_message_queue) > 0) {
        CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(outgoing_message_queue, 0);
        CFIndex len = CFDataGetLength(data);
        CFDataGetBytes(data, CFRangeMake(0, len), (UInt8*)out);
        out[len] = '\0';
        CFArrayRemoveValueAtIndex(outgoing_message_queue, 0);
    }

    return out;
}

void WacomTablet::FlushMessageQueue() {
    CFArrayRemoveAllValues(outgoing_message_queue);
}

#endif


#pragma mark -

//
// SendMessage(char* or CFDataRef)
// Send a message to the Preference Pane
//
void WacomTablet::SendMessage(const char *message) {
    CFDataRef data;
    data = CFDataCreate( NULL, (UInt8*)message, strlen(message)+1 );
    SendMessage(data);
    CFRelease(data);
}

void WacomTablet::SendMessage(CFDataRef data) {
#if ASYNCHRONOUS_MESSAGING
    CFMessagePortRef port = GetRemoteMessagePort();
    if (port) {
        CFDataRef returnData = NULL;
        kCFMessagePortSuccess == CFMessagePortSendRequest( port, 0, data, 1, 1, kCFRunLoopDefaultMode, &returnData);
        if (returnData != NULL)
            CFRelease(returnData);
    }
#else
    AddMessageToQueue(data);
#endif
}

//
// SendMessageScale()
// Tell the Preference Pane the current tablet scale
//
void WacomTablet::SendMessageScale() {
    SendMessage(GetMessageScale());
}

//
// SendMessageInfo([bank])
// Tell the Preference Pane about a given setting
//
void WacomTablet::SendMessageInfo(int bank) {
    SendMessage(GetMessageInfo(bank));
}

//
// SendMessageModel()
// Tell the Preference Pane the tablet name, rom version, and base version
//
void WacomTablet::SendMessageModel() {
    SendMessage(GetMessageModel());
}

//
// SendMessageProtocol()
// Tell the Preference Pane the current command set
//
void WacomTablet::SendMessageProtocol() {
    SendMessage(GetMessageProtocol());
}


#pragma mark -

char* WacomTablet::GetMessageScale() {
    sprintf(out_message, "[scale] %ld %ld", (long)settings[0].xscale, (long)settings[0].yscale);
    return out_message;
}

char* WacomTablet::GetMessageInfo(int bank) {
    sprintf(out_message, "[info] %d %s %sactive", bank, settings[bank].SettingsString(), tablet_on ? "" : "in");
    return out_message;
}

char* WacomTablet::GetMessageModel() {
    if (rom_version[0])
        sprintf(out_message, "[model] %s V%.1f (%s)", rom_version, base_version, series_list[series_index].name);
    else
        sprintf(out_message, "[none]");

    return out_message;
}

char* WacomTablet::GetMessageProtocol() {
    sprintf(out_message, "[prot] %d %d", settings[0].command_set, settings[0].output_format);
    return out_message;
}

char* WacomTablet::GetMessageGeometry() {
    sprintf(out_message, "[geom] %.0f %.0f %.0f %.0f : %.0f %.0f %.0f %.0f : %d %d %d %d : %d %.4f",
            CGRectGetMinX(tabletMapping), CGRectGetMinY(tabletMapping), CGRectGetMaxX(tabletMapping)-1, CGRectGetMaxY(tabletMapping)-1,
            CGRectGetMinX(screenMapping), CGRectGetMinY(screenMapping), CGRectGetMaxX(screenMapping)-1, CGRectGetMaxY(screenMapping)-1,
            button_mapping[kStylusTip],
            button_mapping[kStylusButton1],
            button_mapping[kStylusButton2],
            button_mapping[kStylusEraser],
            mouse_mode, mouse_scaling);

    return out_message;
}

char* WacomTablet::GetMessageStream() {
    if (stream_size) {
        sprintf(out_message, "[raw] %s:%s: %d %d : %ld %ld : %d %d : %d : %d %d %d %d : %d : %d %d",
                ReadablePacket(stream_packet, stream_size),
                stream_event ? stream_event : "---",
                settings[0].command_set,
                settings[0].output_format,
                (long)stylus.point.x,   (long)stylus.point.y,
                stylus.tilt.x,      stylus.tilt.y,
                0,
                stylus.button[kStylusTip],      stylus.button[kStylusButton1],
                stylus.button[kStylusButton2],  stylus.button[kStylusEraser],
                stylus.pressure,
                bytesPerSecond, packetsPerSecond );

        stream_size = 0;
    }
    else
        strcpy(out_message, "[noraw]");

    return out_message;
}

char* WacomTablet::GetMessageSerialPort() {
    if (args.port)
        sprintf(out_message, "[port] %s", args.port);
    else
        sprintf(out_message, "[port]");

    return out_message;
}


#pragma mark - Utility Functions

//
// LogString(str)
// Escape a string for logging purposes
//
char* LogString(char *str) {
    static char     buf[2048];
    char            *ptr = buf;
    int             i;

    *ptr = '\0';

    while (*str) {
        if (isprint(*str))
            *ptr++ = *str++;
        else {
            switch(*str) {
                case ' ':
                    *ptr++ = *str;
                    break;

                case 27:
                    *ptr++ = '\\';
                    *ptr++ = 'e';
                    break;

                case '\t':
                    *ptr++ = '\\';
                    *ptr++ = 't';
                    break;

                case '\n':
                    *ptr++ = '\\';
                    *ptr++ = 'n';
                    break;

                case '\r':
                    *ptr++ = '\\';
                    *ptr++ = 'r';
                    break;

                default:
                    i = *str;
                    (void)sprintf(ptr, "\\%02X", i);
                    ptr += 4;
                    break;
            }

            str++;
        }
        *ptr = '\0';
    }
    return buf;
}

//
// GetIntArgument(string, flag, int*)
// Get an int from a string or return false if not a valid number
//
bool GetIntArgument(char *arg, int flag, int *dest) {
    for (unsigned int i=0; i<strlen(arg); i++)
        if (!isdigit(arg[i]) && !(i==0 && arg[i]=='-')) {
            fprintf(output, "'%s' is a bad value for the -%c flag.\n", arg, flag);
            return false;
        }

    *dest = atoi(arg);
    return true;
}

//
// GetFloatArgument(string, flag, float*)
// Get an int from a string or return false if not a valid number
//
bool GetFloatArgument(char *arg, int flag, float *dest) {
    bool got_point = false, bad_arg = false;
    for (unsigned int i=0; i<strlen(arg); i++) {
        char c = arg[i];
        bool isdot = (c == '.');
        if ( !(isdigit(c) || (i==0 && c=='-') || (isdot && !got_point)) ) {
            bad_arg = true;
            break;
        }
        if (isdot) got_point = true;
    }
    if (bad_arg) {
        fprintf(output, "'%s' is a bad value for the -%c flag.\n", arg, flag);
        return false;
    }

    *dest = (float)atof(arg);
    return true;
}

//
// ShortSleep
//
void ShortSleep() {
    struct timespec rqtp = { 0, 50000000 };
    nanosleep(&rqtp, NULL);
}

//
// ReadablePacket
//
char* ReadablePacket(char *p, int pack_size) {
    static char pack[200];

    if (pack_size == 7)
        sprintf(pack, "%02X %02X %02X %02X %02X %02X %02X", p[0]&0xFF, p[1]&0xFF, p[2]&0xFF, p[3]&0xFF, p[4]&0xFF, p[5]&0xFF, p[6]&0xFF);
    else if (pack_size == 9)
        sprintf(pack, "%02X %02X %02X %02X %02X %02X %02X %02X %02X", p[0]&0xFF, p[1]&0xFF, p[2]&0xFF, p[3]&0xFF, p[4]&0xFF, p[5]&0xFF, p[6]&0xFF, p[7]&0xFF, p[8]&0xFF);
    else
        strcpy(pack, p);

    return pack;
}

//
// HexString
//
char* HexString(char *s, int size) {
    static char hexout[200];
    char hexpart[4];

    clearstr(hexout);
    for (int i=size;i--;) {
        sprintf(hexpart, "%02X ", (*s)&0xFF);
        strcat(hexout, hexpart);
        s++;
    }

    return hexout;
}

//
// UpdateDisplaysBounds
//
bool UpdateDisplaysBounds() {
    //  CGRect              activeDisplaysBounds;
    CGDirectDisplayID   *displays;
    CGDisplayCount      numDisplays;
    CGDisplayCount      i;
    CGDisplayErr        err;
    bool                result = false;

    screenBounds = CGRectMake(0.0, 0.0, 0.0, 0.0);

    err = CGGetActiveDisplayList(0, NULL, &numDisplays);

    if (err == CGDisplayNoErr && numDisplays > 0) {
        displays = (CGDirectDisplayID*)malloc(numDisplays * sizeof(CGDirectDisplayID));

        if (NULL != displays) {
            err = CGGetActiveDisplayList(numDisplays, displays, &numDisplays);

            if (err == CGDisplayNoErr)
                for (i = 0; i < numDisplays; i++)
                    screenBounds = CGRectUnion(screenBounds, CGDisplayBounds(displays[i]));

            free(displays);
            result = true;
        }
    }

exit:

    return result;
}
