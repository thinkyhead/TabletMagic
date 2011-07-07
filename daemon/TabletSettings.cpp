/*
	TabletMagicDaemon
	Thinkyhead Software

	TabletSettings.cpp ($Id: TabletSettings.cpp,v 1.15 2009/06/16 18:28:02 slurslee Exp $)

	This program is a component of TabletMagic. See the
	accompanying documentation for more details about the
	TabletMagic project.

	LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "TabletSettings.h"

//===================================================================
//  TabletSettings
//===================================================================

#define straddf(x, s) sprintf(tmp,x,s); strcat(q, tmp)
#define onebitval(x) ((mask>>x)&1);
#define twobitval(x) ((mask>>x)&3);
#define onebit(x, y) ((y&1)<<x)
#define twobit(x, y) ((y&3)<<x)

int TabletSettings::index = 0;

TabletSettings::TabletSettings() {
	mem_slot = index++;		// set my index while instantiating in a C++ array

	asprintf(&bankname, "Active Settings");

	//
	// The ArtZ-II likes E202C910 as the setting
	//

	// E
	command_set		= kCommandSetWacomIV;
	baud_rate		= kBaudRate9600;

	// 2
	data_bits		= kDataBits8;
	parity			= kParityNone;
	stop_bits		= kStopBits1;

	// 0
	cts				= kCTSDisabled;
	dsr				= kDSRDisabled;
	transfer_mode	= kTransferModeSuppressed;

	// 2
	output_format   = kOutputFormatBinary;
	coordsys		= kCoordSysAbsolute;
	transfer_rate	= kTransferRate100pps;

	// C
	resolution		= kResolution1270lpi;
	origin			= kOriginUL;
	oor_data		= kOORDisabled;

	// 9
	terminator		= kTerminatorCRLF;
	// unused bit 9 here
	pnp				= kPNPEnabled;

	// 1
	sensitivity		= kSensitivityFirm;
	read_height		= kReadHeight8mm;
	mdm				= kMDMDisabled;
	tilt			= kTiltEnabled;

	// 0
	mm_comm			= kMMCommandSet1201;
	orientation		= kOrientationLandscape;
	cursor_data		= kCursorData1234;
	remote_mode		= kRemoteModeDisabled;

	increment		= 2;	// 10ms
	interval		= 2;	// Space between events
	xrez			= 1270;		// Dot density
	yrez			= 1270;
	xscale			= k12inches1270ppi;	// Max / Scale X
	yscale			= k12inches1270ppi;	// Max / Scale Y
}

TabletSettings::~TabletSettings() {
}

//
// Import(state)
// Interpret a tablet status message and store the results
//
bool TabletSettings::Import(const char *state) {
	unsigned int mask;

	sscanf(&state[(state[0] == '~') ? ((state[1] == 'W') ? 3 : 2) : 0], "%X,%d,%d,%d,%d", &mask, &increment, &interval, &xrez, &yrez);

	command_set		= twobitval(30);	// E = 11 10
	baud_rate		= twobitval(28);

	parity			= twobitval(26);	// 2 = 00 1 0
	data_bits		= onebitval(25);
	stop_bits		= onebitval(24);

	cts				= onebitval(23);	// 0 = 0 0 00
	dsr				= onebitval(22);
	transfer_mode   = twobitval(20);

	output_format   = onebitval(19);	// 2 = 0 0 10
	coordsys		= onebitval(18);
	transfer_rate   = twobitval(16);

	resolution		= twobitval(14);	// C = 11 0 0
	origin			= onebitval(13);
	oor_data		= onebitval(12);

	terminator		= twobitval(10);	// 9 = 10 x 1
	// (2^9) unused
	pnp				= onebitval(8);

	sensitivity		= onebitval(7);		// 0 = 0 0 0 0
	read_height		= onebitval(6);
	mdm				= onebitval(5);
	tilt			= onebitval(4);

	mm_comm			= onebitval(3);		// 0 = 0 0 0 0
	orientation		= onebitval(2);
	cursor_data		= onebitval(1);
	remote_mode		= onebitval(0);

	packet_size		= (command_set == kCommandSetWacomIIS && output_format == kOutputFormatBinary) ? 7
					: (command_set == kCommandSetWacomIV && tilt == kTiltEnabled) ? 9 : 7;

	return true;
}

//
// InitForSD
// Apply pseudo-settings for SD-series tablets
//
void TabletSettings::InitForSD() {
	Import("~RA203C800,000,00,1270,1270");
}

//
// InitForPL
// Apply pseudo-settings for PL-series tablets
//
void TabletSettings::InitForPL() {
	Import("~RE202C000,001,01,1260,1260");
}

//
// InitForPenPartner
// Apply pseudo-settings for PenPartner tablets
//
void TabletSettings::InitForPenPartner() {
	Import("~RE202C000,001,01,1260,1260");
}

//
// InitForTabletPC
// Apply pseudo-settings for the TabletPC digitizer
//
void TabletSettings::InitForTabletPC(Boolean use38400) {
	command_set		= kCommandSetTabletPC;		// Synthetic Override
	packet_size		= 9;						// Confirmed

	baud_rate		= use38400 ? kBaudRate38400 : kBaudRate19200;	// Confirmed serial settings for TabletPC
	data_bits		= kDataBits8;
	parity			= kParityNone;
	stop_bits		= kStopBits1;
	cts				= kCTSDisabled;
	dsr				= kDSRDisabled;

	transfer_mode	= kTransferModeSuppressed;	// Unknown
	output_format   = kOutputFormatBinary;		// Confirmed for TabletPC
	coordsys		= kCoordSysAbsolute;		// Of course it is
	transfer_rate	= kTransferRate200pps;		// Doesn't Matter
	resolution		= kResolution1270lpi;		// Doesn't Matter
	origin			= kOriginUL;				// Most likely
	oor_data		= kOORDisabled;				// Doesn't Matter
	terminator		= kTerminatorCRLF;			// Doesn't Matter
	pnp				= kPNPEnabled;				// Doesn't Matter
	sensitivity		= kSensitivityFirm;			// Doesn't Matter
	read_height		= kReadHeight8mm;			// Doesn't Matter
	mdm				= kMDMDisabled;				// Stylus Only
	tilt			= kTiltEnabled;				// Unlikely on TabletPC
	mm_comm			= kMMCommandSet1201;		// Doesn't Matter
	orientation		= kOrientationLandscape;	// Confirmed
	cursor_data		= kCursorData1234;			// Doesn't Matter
	remote_mode		= kRemoteModeDisabled;		// Doesn't Matter

	increment		= 2;						// Doesn't matter
	interval		= 2;						// Doesn't matter
	xrez = yrez		= 1270;						// Doesn't matter, so just insert a sane value
	xscale			= 24570;					// These approximate values from tester Carpao
	yscale			= 18430;					// Other suggested values: 21136x15900 (wacdump), 24780x18630 (hack)
}

//
// InitForIntuos
// Apply pseudo-settings for the Intuos
//
// TODO: Set 38400 speed for ROM 2.0 and above and apply it in the daemon
//
void TabletSettings::InitForIntuos() {
	command_set		= kCommandSetWacomV;		// Synthetic Override
	packet_size		= 9;

	baud_rate		= kBaudRate9600;			// Override (Intuos supports 19200, Intuos2 38400)
	data_bits		= kDataBits8;
	parity			= kParityNone;
	stop_bits		= kStopBits1;
	cts				= kCTSDisabled;
	dsr				= kDSRDisabled;

	transfer_mode	= kTransferModeSuppressed;
	output_format   = kOutputFormatBinary;
	coordsys		= kCoordSysAbsolute;
	transfer_rate	= kTransferRate200pps;		// Synthetic Override
	resolution		= kResolution2540lpi;		// Synthetic Override
	origin			= kOriginUL;
	oor_data		= kOORDisabled;
	terminator		= kTerminatorCRLF;
	pnp				= kPNPEnabled;
	sensitivity		= kSensitivityFirm;
	read_height		= kReadHeight8mm;
	mdm				= kMDMEnabled;				// Override
	tilt			= kTiltEnabled;
	mm_comm			= kMMCommandSet1201;
	orientation		= kOrientationLandscape;
	cursor_data		= kCursorData1234;
	remote_mode		= kRemoteModeDisabled;

	increment		= 2;						// 10ms
	interval		= 2;
	xrez = yrez		= 2540;						// Override
	xscale = yscale	= 30480;					// Override (assumes a 12x12 tablet)
}

//
// SettingsString
// Send the settings to the tablet
//
char* TabletSettings::SettingsString(bool notail) {
	int			setup_body;
	static char setstr[32];

	setup_body =
			twobit(30, command_set)
		+   twobit(28, baud_rate)
		
		+   twobit(26, parity)
		+   onebit(25, data_bits)
		+   onebit(24, stop_bits)
		
		+   onebit(23, cts)
		+   onebit(22, dsr)
		+   twobit(20, transfer_mode)
		
		+   onebit(19, output_format)
		+   onebit(18, coordsys)
		+   twobit(16, transfer_rate)
		
		+   twobit(14, resolution)
		+   onebit(13, origin)
		+   onebit(12, oor_data)

		+   twobit(10, terminator)
		+   onebit(9, 0)				// unused
		+   onebit(8, pnp)

		+   onebit(7, sensitivity)
		+   onebit(6, read_height)
		+   onebit(5, mdm)
		+   onebit(4, tilt)

		+   onebit(3, mm_comm)
		+   onebit(2, orientation)
		+   onebit(1, cursor_data)
		+   onebit(0, remote_mode);
  
	if (notail)
		sprintf(setstr, "%08X", setup_body);
	else
		sprintf(setstr, "%08X,%03d,%02d,%04d,%04d", setup_body, increment, interval, xrez, yrez);

	return setstr;
}


//
// Description()
//
// The settings in a human-readable form
//
char* TabletSettings::Description() {
	char tmp[32];
	static char q[1024];

	clearstr(q);

	// command set
	const char *set[] = {"BitPad", "MM 1201","WACOM II-S","WACOM IV","WACOM V","Tablet PC"};
	straddf("command set ..... %s\n", set[command_set]);

	// ASCII / Binary
	straddf("output format ... %s\n", output_format ? "ASCII" : "BINARY");

	// terminator
	const char *terms[] = {"CR","LF","CRLF","CRLF"};
	straddf("terminator ...... %s\n", terms[terminator]);

	// packet size
	straddf("packet size ..... %d\n", packet_size);

	// baud rate
	straddf("baud rate ....... %d\n", 2400 << baud_rate);

	// data bits
	straddf("data bits ....... %d\n", data_bits ? 8 : 7);

	// parity
	const char *par = "NNOE";
	straddf("parity .......... %c\n", par[parity]);

	// stop bits
	straddf("stop bits ....... %d\n", stop_bits ? 2 : 1);

	// CTS
	straddf("cts ............. %s\n", cts ? "ON" : "OFF");

	// DSR
	straddf("dsr ............. %s\n", dsr ? "ON" : "OFF");

	// mode
	const char *mode[] = { "suppressed","point","sw stream","stream" };
	straddf("transfer mode ... %s\n", mode[transfer_mode]);

	// Rel / Abs
	straddf("coord sys ....... %s\n", coordsys ? "REL" : "ABS");

	// transfer rate (pps)
	const char *rates[] = {"50","67", "100","MAX","200"};
	straddf("transfer rate ... %s\n", rates[transfer_rate]);

	// resolution (lpi)
	const int	rezs[] = { 500, 508, 1000, 1270 };
	straddf("resolution ...... %d lpi\n", rezs[resolution]);

	// Origin
	straddf("origin .......... %s\n", origin ? "LL" : "UL");

	// Out-Of-Range Data
	straddf("oor data ........ %s\n", oor_data ? "ON" : "OFF");

	// PnP
	straddf("pnp ............. %s\n", pnp ? "ON" : "OFF");

	// Pressure Sensitivity
	straddf("sensitivity ..... %s\n", sensitivity ? "soft" : "firm");

	// Reading Height
	straddf("read height ..... %s\n", read_height ? "2mm" : "8mm+");

	// Multi Device Mode
	straddf("mdm ............. %s\n", mdm ? "ON" : "OFF");

	// Tilt Mode
	straddf("tilt ............ %s\n", tilt ? "ON" : "OFF");

	// MM Command Set
	straddf("mm command set .. %s\n", mm_comm ? "MM961" : "MM1201");

	// MM961 Orientation
	straddf("orientation ..... %s\n", orientation ? "portrait" : "landscape");

	// BitPad II Cursor Data
	straddf("cursor data ..... %s\n", cursor_data ? "1248" : "1234");

	// Remote Mode
	straddf("remote mode ..... %s\n", remote_mode ? "ON" : "OFF");

	// Increment
	straddf("increment ....... %d\n", increment);

	// Interval
	straddf("interval ........ %d\n", interval);

	// Xrez
	straddf("xrez ............ %d\n", xrez);

	// Yrez
	straddf("yrez ............ %d\n", yrez);

	// X Size
	straddf("tablet width .... %ld\n", (long)xscale);

	// Y Size
	straddf("tablet height ... %ld\n", (long)yscale);

	return q;
}

