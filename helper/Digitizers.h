/*
	TabletMagic
	(c) 2011 Thinkyhead Software

	Digitizers.h ($Id: Digitizers.h,v 1.1 2009/02/09 05:47:41 slurslee Exp $)

	Utility functions to help retrieve a digitizer string from the IO Registry
*/

#define KNOWN_MACHINES {"AAPL","iMac","PowerBook","PowerMac","RackMac","Macmini","MacPro","MacBookPro","MacBook"}

char* get_digitizer_string();
char* run_tool(char *cmd);
void clean_string(char *string);
char is_known_machine(char **machine_string);
