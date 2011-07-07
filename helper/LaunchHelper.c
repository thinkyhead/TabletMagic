/*
	TabletMagic
	Thinkyhead Software

	LaunchHelper.c ($Id: LaunchHelper.c,v 1.12 2009/09/01 15:40:16 slurslee Exp $)

	This helper executable is used by the Preference Pane to:
		(no arguments)		Just SUID the LaunchHelper binary
		"launchd"			Create a LaunchD entry (which may be 'Disabled=YES')
		"launchd10.5"		Create a LaunchD entry suitable for Leopard
		"disable"			Delete the StartupItem
		"enabletabletpc"	Hack the serial driver plist to enable the digitizer
		"getdigitizer"		Return the digitizer code, if any
		(other)				Create a StartupItem using the passed arguments
*/

#include "Digitizers.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fcntl.h>


int set_suid_root(char *path);

#define LAUNCHER	"com.thinkyhead.TabletMagic.plist"
#define SRC			"/tmp/"
#define DSTD		"/Library/LaunchDaemons"
#define DSTA		"/Library/LaunchAgents"

#define STARTSRC	"TabletMagicStarter"
#define STARTDST	"/Library/StartupItems/TabletMagic"
#define STARTITEM	"TabletMagic"

#define	KEXTDIR		"/System/Library/Extensions/Apple16X50Serial.kext/Contents/PlugIns/Apple16X50ACPI.kext/Contents/"

int main(int argc, char *argv[]) {
	int			result = EX_OK;
	char		*dst, *cmd;
	char		*buff1, *buff2;
	struct stat	st;
	int			file;
	ssize_t		size, filesize;

	if (geteuid() != 0) {
		fprintf(stderr, "LaunchHelper must be run as root!\n");
		return EX_NOPERM;
	}

	// always make this binary suid root
	result = set_suid_root(argv[0]);

	if (argc == 2) {

		if (0 == strcmp(argv[1], "enabletabletpc")) {

			// Find a digitizer in the IO Registry
			cmd = "ioreg -l | grep -A15 -E \"\\+-o (DIGI|WACM|COMA)\" | grep -m1 \\\"name\\\" | sed -E \"s/.*<\\\"?([^\\\">]+)\\\"?>/\\\\1/\"";
			char *digitizer_string = get_digitizer_string();

			// If we got a digitizer string, continue
			if (digitizer_string && strlen(digitizer_string)) {
				// Get the original digitizer string from the KEXT (usually "PNP0501")
				cmd = "defaults read " KEXTDIR "/Info IOKitPersonalities | grep IONameMatch | sed -e \"s/[ \\\\t]*IONameMatch = \\([^;]*\\);/\\\\1/\"";
				char *orig = run_tool(cmd);
				clean_string(orig);

				// Replace the string and put the results in a temporary file
				asprintf(&cmd, "sed -e \"s/%s/%s/g\" " KEXTDIR "/Info.plist >/tmp/digifix.tmp", orig, digitizer_string);
				result = system(cmd);
				free(cmd);

				free(orig);

				// Copy the original file to a backup on the Desktop
				result = system("cp " KEXTDIR "/Info.plist ~/Desktop/Info-`date +%Y%m%d%H%M%S`.plist");

				// Replace the original plist
				result = rename("/tmp/digifix.tmp", KEXTDIR "/Info.plist");

				// On success clear the extension cache and return the digitizer code
				if (strlen(digitizer_string) > 0) {
					result = remove("/System/Library/Extensions.mkext");
					printf("%s", digitizer_string);
				}
				else
					printf("none");
			}
			else
				printf("fail");

		}
		else {
			// For other ops always remove the existing startup item
			result = system("rm -Rf " STARTDST);

			if (0 == strcmp(argv[1], "disable")) {
				// do nothing at all
			}
			else if (0 == strcmp(argv[1], "launchd") || 0 == strcmp(argv[1], "launchd10.5")) {
				// Install the LaunchD item (placed in /tmp before invocation)
				char *dir, *src = SRC LAUNCHER;

				if (0 == strcmp(argv[1], "launchd10.5")) {
					dir = DSTA;
					dst = DSTA "/" LAUNCHER;
				} else {
					dir = DSTD;
					dst = DSTD "/" LAUNCHER;
				}

				result = mkdir(dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH); // 755
				result = remove(DSTA "/" LAUNCHER);
				result = remove(DSTD "/" LAUNCHER);
				result = rename(src, dst);
				result = chown(dst, 0, 0);
				result = chmod(dst, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); // 644
			}
			else {
				// Copy our template into StartupItems
				result = asprintf(&cmd, "cp -Rf %s/%s %s", dirname(argv[0]), STARTSRC, STARTDST);
				result = system(cmd); free(cmd);

				// Alter the startup script, adding our parameters
				file = open(STARTDST "/" STARTITEM, O_RDONLY|O_EXLOCK, 0);
				if (file != -1)
				{
					if (0 == fstat(file, &st))
					{
						filesize = (int)st.st_size;

						buff1 = (char*)malloc(filesize+1);
						size = read(file, buff1, filesize);
						buff1[filesize] = '\0';
						result = close(file);

						if (asprintf(&buff2, buff1, argv[1]) > filesize)
						{
							file = open(STARTDST "/" STARTITEM, O_WRONLY|O_EXLOCK, 0);
							size = write(file, buff2, strlen(buff2));
							result = close(file);
							free(buff2);
						}

						free(buff1);
					}
				}

				// Set owners of the contents to root:wheel
				result = system("chown -Rf root:wheel " STARTDST);

				// Set permissions on the contents to 755
				result = system("chmod -Rf 755 " STARTDST);

				// Set permissions on all file resources to 644
				result = system("chmod -Rf 644 " STARTDST "/" "StartupParameters.plist " STARTDST "/Resources/*/*.strings");
			}
		}
	}

	return result;
}

int set_suid_root(char *path) {
	struct stat st;
	int fd_tool;

	/* Open tool exclusively, so no one can change it while we bless it */
	fd_tool = open(path, O_NONBLOCK|O_RDONLY|O_EXLOCK, 0);

	if (fd_tool == -1)
		return -4;

	if (fstat(fd_tool, &st))
		return -5;

	if (st.st_uid != 0) {
		if (0 == fchown(fd_tool, 0, 0xFFFFFFFF)
			&& 0 == fchmod(fd_tool, S_ISUID|S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
				fprintf(stderr, "LaunchHelper is now SUID root!\n");
	}

	close(fd_tool);

	return 0;
}


