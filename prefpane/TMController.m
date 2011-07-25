/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMController.m ($Id: TMController.m,v 1.30 2009/08/05 20:40:31 slurslee Exp $)
*/

#import "TMController.h"
#import "TMPresetsController.h"
#import "TMPreset.h"
#import "TabletMagicPref.h"
#import "Digitizers.h"
#import "Constants.h"
#import "GetPID.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#import <sys/stat.h>
#import <unistd.h>
#import <notify.h>

#define kLookingForTablet	@"Looking for a tablet..."
#define kNoTabletFound		@"No Tablet Found"
#define kDriverNotLoaded	@"Daemon Not Running"
#define kCDaemonName		"TabletMagicDaemon"
#define kDaemonName			@kCDaemonName
#define kLaunchHelperName	@"LaunchHelper"
#define kLaunchPlistName	@"com.thinkyhead.TabletMagic.plist"
#define	kLaunchdKeyName		@"com.thinkyhead.TabletMagic"
#define	kStartupItem		@"/Library/StartupItems/TabletMagic"


@implementation TMController

TMController *theController;

CFDataRef message_callback(CFMessagePortRef mp, SInt32 msgid, CFDataRef data, void *info) {
	#if ASYNCHRONOUS_MESSAGES
		return [theController handleMessageFromPort:mp withData:data andInfo:info];
	#else
		return nil;
	#endif
}

void invalidation_callback(CFMessagePortRef mp, void *info) {
	[theController remoteMessagePortWentAway];
}

enum {
	COMP_NONE	= 0x00,		COMP_SD	= 0x01,
	COMP_PL		= 0x02,		COMP_CT	= 0x04,
	COMP_GD		= 0x08,		COMP_PC	= 0x10,
	ONLY_IIS	= 0x20,		COMP_IV	= 0x40,
	COMP_UD		= 0x80
};

typedef struct _TMCommand {
	BOOL		divider;			// display a divider before the item
	BOOL		get_settings;		// the command changes the tablet setup string
	BOOL		ignore_next_info;	// the command reply should not be interpreted
	UInt16		compatibility;		// a bit-mask of compatibility
	int			replysize;			// the expected size of the reply
	char		*command;			// the command string reference
	NSString	*description;		// the menu title and command description
} TMCommand;

TMCommand tabletCommands[] = {
//	  div	set		ign		compatibility flags					replysize	command				description
			
	{ NO,	NO,		NO,		COMP_UD|COMP_SD|COMP_PL|COMP_CT,			2, WAC_SelfTest,		@"Do Self-Test" },
	{ NO,	NO,		NO,		COMP_UD|COMP_SD|COMP_PL|COMP_CT|COMP_GD,	1, WAC_TabletID,		@"Get Model & ROM Version" },
	{ NO,	NO,		NO,		COMP_UD|COMP_PL|COMP_CT|COMP_GD,			1, WAC_TabletSize,		@"Get Maximum Coordinates" },
	{ NO,	NO,		YES,	COMP_UD|COMP_PL|COMP_CT,					1, WAC_ReadSetting,		@"Get Current Settings" },
	{ NO,	NO,		YES,	COMP_UD|COMP_PL|COMP_CT,					1, WAC_ReadSettingM1,	@"Get Setting M1" },
	{ NO,	NO,		YES,	COMP_UD|COMP_PL|COMP_CT,					1, WAC_ReadSettingM2,	@"Get Setting M2" },

	{ YES,	NO,		NO,		COMP_UD|COMP_SD|COMP_PL|COMP_CT|COMP_GD,	0, WAC_StartTablet,		@"Start" },
	{ NO,	NO,		NO,		COMP_UD|COMP_SD|COMP_PL|COMP_CT|COMP_GD,	0, WAC_StopTablet,		@"Stop" },

	// Known TabletPC Commands
	{ YES,	NO,		YES,	COMP_PC,									1, TPC_TabletID,		@"Get Tablet Info" },

	{ YES,	NO,		NO,		COMP_PC,									0, TPC_StopTablet,		@"Stop" },
	{ NO,	NO,		NO,		COMP_PC,									0, TPC_Sample133pps,	@"Sample at 133pps" },
	{ NO,	NO,		NO,		COMP_PC,									0, TPC_Sample80pps,		@"Sample at 80pps" },
	{ NO,	NO,		NO,		COMP_PC,									0, TPC_Sample40pps,		@"Sample at 40pps" },
	{ NO,	NO,		NO,		COMP_PC,									0, TPC_SurveyScanOn,	@"Enable Survey Scan" },
	{ NO,	NO,		NO,		COMP_PC,									0, TPC_SurveyScanOff,	@"Disable Survey Scan" },

	// All below are unavailable on CT tablets
	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL,					0, WAC_ResetBitpad2,	@"Reset to Bit Pad Two" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL,					0, WAC_ResetMM1201,		@"Reset to MM1201" },
	{ NO,	YES,	NO,		COMP_UD|COMP_PL,							0, WAC_ResetWacomII,	@"Reset to WACOM II-S" },
	{ NO,	YES,	NO,		COMP_UD|COMP_PL,							0, WAC_ResetWacomIV,	@"Reset to WACOM IV" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL,					0, WAC_ResetDefaults,	@"Reset to Defaults of Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_PL,							0, WAC_TiltModeOn,		@"Enable Tilt Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_PL,							0, WAC_TiltModeOff,		@"Disable Tilt Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_SuppressIN2,		@"Suppressed Mode (IN2)" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_PointMode,		@"Point Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_SwitchStreamMode, @"Switch Stream Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_StreamMode,		@"Stream Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_DataContinuous,	@"Continuous Data Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_DataTrailing,	@"Trailing Data Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_DataNormal,		@"Normal Data Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_OriginUL,		@"Origin UL" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|COMP_GD,			0, WAC_OriginLL,		@"Origin LL" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL,					0, "SC15240,15240\r",	@"Scale 15240 x 15240" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL,					0, "SC32768,32768\r",	@"Scale 32768 x 32768" },

	// Wacom II-S Specific Commands
	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_PressureModeOn,	@"Enable Pressure Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_PressureModeOff,	@"Disable Pressure Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_ASCIIMode,		@"ASCII Data Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_BinaryMode,		@"Binary Data Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_RelativeModeOn,	@"Enable Relative Mode" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_RelativeModeOff,	@"Disable Relative Mode" },

	{ YES,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_Rez1000ppi,		@"Set 1000p/i Resolution" },
	{ NO,	YES,	NO,		COMP_UD|COMP_SD|COMP_PL|ONLY_IIS,			0, WAC_Rez50ppmm,		@"Set 50p/mm Resolution" },

	// Macro button commands
	{ YES,	NO,		NO,		COMP_UD,									0, WAC_MacroAll,		@"Enable All Menu Buttons" },
	{ NO,	NO,		NO,		COMP_UD,									0, WAC_MacroNoSetup,	@"Disable Setup Button" },
	{ NO,	NO,		NO,		COMP_UD,									0, WAC_MacroNoFunction,	@"Disable Function Buttons" },
	{ NO,	NO,		NO,		COMP_UD,									0, WAC_MacroNoPressure,	@"Disable Pressure Buttons" },
	{ NO,	NO,		NO,		COMP_UD,									0, WAC_MacroExtended,	@"Pressure Buttons as Macros" }

//	// Wacom II commands: BA, LA, CA, OR, RC, RS, SB, YR
//	{ YES,	NO,		NO,		COMP_NONE,									0, "BA\r",	@"BA Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "LA\r",	@"LA Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "CA\r",	@"CA Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "OR\r",	@"OR Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "RC\r",	@"RC Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "RS\r",	@"RS Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "SB\r",	@"SB Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "YR\r",	@"YR Command" },
//
//	// Obsolete II-S Commands: AS, DE, IC, PH, SC 
//	{ YES,	NO,		NO,		COMP_NONE,									0, "AS\r",	@"AS Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "DE\r",	@"DE Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "IC\r",	@"IC Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "PH\r",	@"PH Command" },
//	{ NO,	NO,		NO,		COMP_NONE,									0, "SC\r",	@"SC Command" }
};

- (id)init {
	[super init];

	// Set some values we'll use later
	theController		= self;
	streamTimer			= nil;
	killTimer			= nil;
	pingTimer			= nil;
	settingTimer		= nil;
	scaleTimer			= nil;
	autoStartTimer		= nil;

	modelCT				= NO;
	ignore_next_info	= NO;
	scale_from_stylus	= NO;
	current_tab			= 0;

	// Get authorization when needed to start the daemon
	[ self setupAuthorization ];

	cfPortIn = CFMessagePortCreateLocal(kCFAllocatorDefault, CFSTR("com.thinkyhead.tabletmagic.prefpane"), message_callback, nil, false);
	CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, cfPortIn, 0);
	CFRunLoopAddSource( CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode );

	return self;
}

- (void)mainViewDidLoad {
	bundleID		= [[thePane bundle] bundleIdentifier];

	NSString		*imagePath = [[thePane bundle] pathForImageResource:@"trigger-off" ];
	NSImage			*img1 = [[NSImage alloc] initWithContentsOfFile:imagePath];

					imagePath = [[thePane bundle] pathForImageResource:@"trigger-on" ];
	NSImage			*img2 = [[NSImage alloc] initWithContentsOfFile:imagePath];

	[ textVersion setStringValue: [ NSString localizedStringWithFormat:@"Version %@", @TABLETMAGIC_VERSION ] ];

	NSButtonCell	*cell = [buttonView1 cell];
	[cell setImage:img1];
	[cell setAlternateImage:img2];
	[cell setShowsStateBy:NSContentsCellMask];
	[cell setHighlightsBy:NSNoCellMask];
	[ buttonView1 setTitle:@"1" ];

	cell = [buttonView2 cell];
	[cell setImage:img1];
	[cell setAlternateImage:img2];
	[cell setShowsStateBy:NSContentsCellMask];
	[cell setHighlightsBy:NSNoCellMask];
	[ buttonView2 setTitle:@"2" ];

	cell = [buttonView3 cell];
	[cell setImage:img1];
	[cell setAlternateImage:img2];
	[cell setShowsStateBy:NSContentsCellMask];
	[cell setHighlightsBy:NSNoCellMask];
	[ buttonView3 setTitle:@"3" ];

	cell = [buttonView4 cell];
	[cell setImage:img1];
	[cell setAlternateImage:img2];
	[cell setShowsStateBy:NSContentsCellMask];
	[cell setHighlightsBy:NSNoCellMask];
	[ buttonView4 setTitle:@"E" ];

	// Set up the ports menu before loading preferences
	[ self updatePortPopup ];

	// Load preferences here because it affects control states
	// and the serial popup needs to be set first too.
	[ self loadPreferences ];

	// Detect whether this is a TabletPC or not
	hackintosh = [ self detectTabletPC ];

	// delete the tab having identifier "6" if this isn't a TabletPC
	if (!hackintosh) {
		[ tabview removeTabViewItem: [ tabview tabViewItemAtIndex: [ tabview indexOfTabViewItemWithIdentifier: @"6" ] ] ];
		[ popupBaud removeItemAtIndex:4 ];
	}

	// Initialize the controls for non-connected
	[ self forgetTabletInfo ];
	[ self setControlsForConnect:NO ];

	[ presetsController mainViewDidLoad ];

	// Apply patches for 2b8 (invisible release)
	if ([ presetsController apply2b8Patches ])
		[ presetsController sendPresetToDaemon ];

	BOOL loaded = [ self isDaemonLoaded:YES ];
	[ buttonKill setEnabled:loaded ];
	[ buttonPanic setEnabled:loaded ];

	// If the daemon is already loaded request its info now.
	// The "Enable Daemon" checkbox can load it also.
	if ( loaded ) {
		[ self sendRequestToDaemon:"?port" ];
		[ self sendRequestToDaemon:"?model" ];
		[ self sendRequestToDaemon:"?info" ];
		[ self sendRequestToDaemon:"?geom" ];
		[ self sendRequestToDaemon:"?scale" ];
	}

	// Not sure why the serial settings are being loaded here...
	// perhaps because the reply to "info" may have updated the prefs
	NSDictionary *prefs = [ [NSUserDefaults standardUserDefaults] persistentDomainForName:bundleID ];
	if (prefs != nil) {
		[ popupBaud selectItemAtIndex:[ [ prefs objectForKey:keySerialBaudRate ] intValue ] ];
		[ popupDataBits selectItemAtIndex:[ [ prefs objectForKey:keySerialDataBits ] intValue ] ];
		[ popupStopBits selectItemAtIndex:[ [ prefs objectForKey:keySerialStopBits] intValue ] ];
		popupSelectTag(popupParity, [[prefs objectForKey:keySerialParity] intValue]);
		[ checkCTS setState:[[prefs objectForKey:keySerialCTS] boolValue] ? NSOnState : NSOffState ];
		[ checkDSR setState:[[prefs objectForKey:keySerialDSR] boolValue] ? NSOnState : NSOffState ];
	}

	[ self updateAutoStartCheckbox ];
}

- (void)paneTerminating {
	[ self cleanUpAuthorization ];
	[ self disposeRemoteMessagePort ];

	CFMessagePortInvalidate(cfPortIn);
}

- (void) paneDidSelect {
	// Timer to update the settings
	settingTimer =	[ NSTimer scheduledTimerWithTimeInterval:(60*60*24*7)
								target:self
								selector:@selector(doSettingsTimer:)
								userInfo:nil
								repeats:YES ];

	// Timer to update the scale values
	scaleTimer =	[ NSTimer scheduledTimerWithTimeInterval:(60*60*24*7)
								target:self
								selector:@selector(doScaleTimer:)
								userInfo:nil
								repeats:YES ];

	// Timer to auto-update the auto-start
	autoStartTimer = [ NSTimer scheduledTimerWithTimeInterval:(60*60*24*7)
								target:self
								selector:@selector(doAutoStartTimer:)
								userInfo:nil
								repeats:YES ];

	pingTimer =		[ NSTimer scheduledTimerWithTimeInterval:0.2
								target:self
								selector:@selector(doPingTimer:)
								userInfo:nil
								repeats:YES ];

	if ([ self isDaemonLoaded:NO ]) {
		[ self sendMessageToDaemon:"hello" ];
		[ self startStreamIfShowing ];
	}
}

- (void) paneWillUnselect {
	[ self sendMessageToDaemon:"bye" ];
	[ pingTimer invalidate ];
	[ settingTimer invalidate ];
	[ scaleTimer invalidate ];
	[ autoStartTimer invalidate ];
	[ self savePreferences ];
	[ self setStreamMonitorEnabled:NO ];
	[ self updateAutoStart ];
}

#pragma mark -
- (void)loadPreferences {
	NSDictionary *prefs = [ [NSUserDefaults standardUserDefaults] persistentDomainForName:bundleID ];

	if (prefs != nil) {
		[ prefs retain ];

		// Tablet Enabled
		[ checkEnabled setState:[ [prefs objectForKey:keyTabletEnabled] boolValue ] ? NSMixedState : NSOffState ];

		// Last Selected SerialPort
		[ self selectPortByName:[prefs objectForKey:keySerialPort] ];

		// The donated checkbox
		BOOL donated = [ [ prefs objectForKey:keyIDonated ] boolValue ];
		[ checkDonation setState:donated ];

		// The selected tab
		NSString *tabID = donated ? [prefs objectForKey:keySelectedTab] : @"4";
		if (tabID)
			[ tabview selectTabViewItemWithIdentifier:tabID ];

		// Make a note of the tab
		current_tab = [[[tabview selectedTabViewItem] identifier] intValue];

		// Pen and eraser colors
		NSData	*theData;
		NSColor *aColor;

		if ((theData = [ prefs objectForKey:keyPenColor ])) {
			aColor = (NSColor*)[ NSUnarchiver unarchiveObjectWithData:theData ];
			[ colorPen setColor:aColor ];
		}
		else
			[ colorPen setColor:[NSColor blackColor] ];

		if ((theData = [ prefs objectForKey:keyEraserColor ])) {
			aColor = (NSColor*)[ NSUnarchiver unarchiveObjectWithData:theData ];
			[ colorEraser setColor:aColor ];
		}
		else
			[ colorEraser setColor:[NSColor whiteColor] ];

		// These settings are especially for TabletPC systems
		if (hackintosh) {
			[ checkTabletPC setState:[ [prefs objectForKey:keyTabletPC] boolValue ] ? NSOnState : NSOffState ];
			[ textTweakScaleX setIntValue:[[prefs objectForKey:keyTabletPCScaleX] intValue] ];
			[ textTweakScaleY setIntValue:[[prefs objectForKey:keyTabletPCScaleY] intValue] ];
		}

		[ prefs release ];
	}
}

- (void)savePreferences {
	NSData	*penColor = [NSArchiver archivedDataWithRootObject:[colorPen color]];
	NSData	*eraserColor = [NSArchiver archivedDataWithRootObject:[colorEraser color]];
	NSString *portName = [popupSerialPort indexOfSelectedItem] ? [[popupSerialPort selectedItem] title] : @"";
	BOOL	donated = [checkDonation state];
	id		tabIdentifier = [[tabview selectedTabViewItem] identifier];

	// Set preferences keys based on control states
    NSMutableDictionary *prefs = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        portName,									keySerialPort,
        NSBOOL([checkEnabled state] != NSOffState),	keyTabletEnabled,
        NSINT([popupBaud indexOfSelectedItem]),		keySerialBaudRate,
        NSINT([popupDataBits indexOfSelectedItem]),	keySerialDataBits,
        NSINT([popupStopBits indexOfSelectedItem]),	keySerialStopBits,
        NSINT([[popupParity selectedItem] tag]),	keySerialParity,
        NSBOOL([checkCTS state] == NSOnState),		keySerialCTS,
        NSBOOL([checkDSR state] == NSOnState),		keySerialDSR,
        tabIdentifier,								keySelectedTab,
		penColor,									keyPenColor,
		eraserColor,								keyEraserColor,
		NSBOOL(donated),							keyIDonated,
		[presetsController dictionary],				keyPresets,
		NSBOOL(YES),								keyDidFixButtons,
		nil];

	if (hackintosh) {
		[ prefs setObject:NSBOOL([checkTabletPC state] == NSOnState) forKey:keyTabletPC ];
		[ prefs setObject:NSINT([textTweakScaleX intValue]) forKey:keyTabletPCScaleX ];
		[ prefs setObject:NSINT([textTweakScaleY intValue]) forKey:keyTabletPCScaleY ];
	}

	[ [NSUserDefaults standardUserDefaults] removePersistentDomainForName:bundleID ];
	[ [NSUserDefaults standardUserDefaults] setPersistentDomain:prefs forName:bundleID ];

	// Normally sync is deferred until System Preferences exits
	// This writes to the prefs file right away just for fun
	[ [NSUserDefaults standardUserDefaults] synchronize ];
}

#pragma mark -
- (void)selectPortByName:(NSString*)portName {
	int i;
	for (i=0; i<[popupSerialPort numberOfItems]; i++) {
		if ([portName isEqualToString:[[popupSerialPort itemAtIndex:i] title]]) {
			[popupSerialPort selectItemAtIndex:i];
			break;
		}
	}
}

- (void)updatePortPopup {
	unsigned i;
	NSArray *portsArray = [ self getSerialPorts ];

	[ popupSerialPort removeAllItems ];
	[ popupSerialPort addItemWithTitle:@"Automatic" ];

	for(i=0; i<[portsArray count]; i++)
		[ popupSerialPort addItemWithTitle:[portsArray objectAtIndex:i] ];
}

//
// getSerialPorts
//	Returns an array with all the serial port names
//
- (NSArray *)getSerialPorts {
	NSMutableArray *serialArray = [ [ NSMutableArray alloc ] init ];

	kern_return_t			kernResult;
	mach_port_t				masterPort;
	CFMutableDictionaryRef  classesToMatch;
	io_iterator_t			serialPortIterator; // An iterator for eligible serial ports

	//
	// Get the masterPort, whatever that is
	//
	kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (KERN_SUCCESS != kernResult)
		goto exit;

	//
	// Serial devices are instances of class IOSerialBSDClient.
	// Here I search for ports with RS232Type because with my
	// Keyspan USB-Serial adapter the first port shows up
	//
	classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
	if (classesToMatch)
		CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));

	//
	// Find the next matching service
	//
	kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, &serialPortIterator);

	if (KERN_SUCCESS == kernResult) {
//		while (IOIteratorIsValid(serialPortIterator))
//		{
			io_object_t portService;

			// Keep iterating until successful
			while ((portService = IOIteratorNext(serialPortIterator))) {
				NSString *devicePath = (NSString*)IORegistryEntryCreateCFProperty(portService, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);

				if (devicePath) {
					NSMutableString *devPath = [ NSMutableString stringWithString:devicePath ];
					[ devPath replaceOccurrencesOfString:@"/dev/cu." withString:@"" options:NSLiteralSearch range:NSMakeRange(0, [devPath length]) ];
					[ devPath replaceOccurrencesOfString:@"/dev/" withString:@"" options:NSLiteralSearch range:NSMakeRange(0, [devPath length]) ];
					[ serialArray addObject:devPath ];
				}

				(void) IOObjectRelease(portService);
			}
//		}
	}

exit:

	return serialArray;
}

#pragma mark -
#pragma mark Messages

#if ASYNCHRONOUS_MESSAGES
- (CFDataRef)handleMessageFromPort:(CFMessagePortRef)loc withData:(CFDataRef)data andInfo:(void*)info {
	CFIndex len = CFDataGetLength(data);

	char *rawData = (char*)CFDataGetBytePtr(data);
//	NSLog(@"Daemon Sent   : \"%s\"", rawData);		// CFDataGetBytePtr(data)

	//
	// Handle all known messages from the daemon here
	//
	[ self handleDaemonMessageData:data ];

	// DEBUG: Echo back what was sent
//	CFDataRef returnData = CFDataCreate(kCFAllocatorDefault, (UInt8*)rawData, len);
//	return returnData;		// data and returnData will be released for us after callback returns
	return nil;
}
#endif

- (void)handleDaemonMessageData:(CFDataRef)data {
	[ self handleDaemonMessage:(char*)CFDataGetBytePtr(data) ];
/*
	char message[100];
	int len = CFDataGetLength(data);
	CFDataGetBytes(data, CFRangeMake(0, len), (UInt8*)message);
	message[len] = '\0';
	[ self handleDaemonMessage:message ];
*/
}

- (void)handleDaemonMessage:(char*)message {
	static SInt16 oldSet = -1, oldFmt = -1;

	// [raw] comes back as a reply to "stream"
	if (0 == strncmp(message, "[raw]", 5)) {
		char *packet = &message[6];

		char *event = strstr(packet, ":");
		*event = '\0';
		event++;

		char *stats = strstr(event, ":");
		*stats = '\0';
		stats += 2;

		int set, fmt, sx, sy, tx, ty, ev, b1, b2, b3, b4, p, bps, pps;
		sscanf(stats, "%d %d : %d %d : %d %d : %d : %d %d %d %d : %d : %d %d", &set, &fmt, &sx, &sy, &tx, &ty, &ev, &b1, &b2, &b3, &b4, &p, &bps, &pps);

		if (current_tab == 3) {
			NSString *streamString = [ [ NSString stringWithCString:packet encoding:NSASCIIStringEncoding ] retain ];

			// Replace or append the stream based on expectation
			if (stream_reply_size == 0) {
				[ textDatastream setStringValue: [NSString stringWithFormat:@"\r%@",streamString] ];
			}
			else {
				if (stream_reply_count < stream_reply_size) {
					if (stream_reply_count == 0)
						[ textDatastream setStringValue:((stream_reply_size == 1) ? @"\r" : @"") ];

					[ textDatastream setStringValue: [[textDatastream stringValue] stringByAppendingString:streamString] ];

					stream_reply_count++;
				}
				else
					stream_reply_size = 0;
			}

			[ textEvent setStringValue:[ thePane localizedString:[NSString stringWithCString:event encoding:NSASCIIStringEncoding] ] ];

			if (set != oldSet || fmt != oldFmt) {
				[ self setStreamHeadingForSet:set andFormat:fmt ];
				popupSelectTag(popupCommandSet, set);
				popupSelectTag(popupOutputFormat, fmt);
				oldSet = set; oldFmt = fmt;
			}

			[ textRateBytes setIntValue:bps ];
			[ textRatePackets setIntValue:pps ];

			[ textPosX setIntValue:sx ];
			[ textPosY setIntValue:sy ];
			[ textTiltX setIntValue:tx ];
			[ textTiltY setIntValue:ty ];

			[ textPressure setIntValue:p/256 ];
			[ progPressure setDoubleValue:(double)p ];

			[ buttonView1 setState:b1 ? NSOnState : NSOffState ];
			[ buttonView2 setState:b2 ? NSOnState : NSOffState ];
			[ buttonView3 setState:b3 ? NSOnState : NSOffState ];
			[ buttonView4 setState:b4 ? NSOnState : NSOffState ];
		}
		else if (current_tab == 6) {
			[ self expandScaleX: sx y: sy ];
		}
	}

	// Tablet settings (current or memory bank)
	else if (0 == strncmp(message, "[info]", 6)) {
		NSLog(@"Received %s", message);

		if (ignore_next_info)
			ignore_next_info = NO;
		else {
			int	bank;
			char setup[30], status[20];

			if (3 == sscanf(message, "%*s %d %s %s", &bank, setup, status)) {
//				NSLog(@"Responding to the [info] message");

				tabletEnabled = (0 == strcmp(status, "active"));
				[ checkEnabled setState:(tabletEnabled ? NSOnState : NSOffState) ];
				[ self setControlsForSettings:setup ];

				if (bank == 0)
					[ self setControlsForConnect:YES ];
			}
		}
	}

	// The daemon is replying with its scale values
	else if (0 == strncmp(message, "[scale]", 7)) {
		NSLog(@"Received %s", message);

		int h, v;
		if (2 == sscanf(message, "%*s %d %d", &h, &v)) {
			[ editXScale setIntValue:h ];
			[ editYScale setIntValue:v ];

			[ stepperXScale setIntValue:h ];
			[ stepperYScale setIntValue:v ];

			if (hackintosh) {
				[ textTweakScaleX setIntValue:h ];
				[ textTweakScaleY setIntValue:v ];
			}

			[ presetsController updateTabletScaleX:h y:v ];
		}
	}

	// [noraw] arrives if the stream stops for a second
	else if (0 == strcmp(message, "[noraw]")) {
		[ textRateBytes setIntValue:0 ];
		[ textRatePackets setIntValue:0 ];
	}

	// Tablet settings (current or memory bank)
	else if (0 == strncmp(message, "[geom]", 6)) {
		NSLog(@"Received %s", message);
		[ presetsController geometryReceived:message ];
	}

	// Tablet model
	else if (0 == strncmp(message, "[model]", 7)) {
		NSLog(@"Received %s", message);

		[ textTabletInfo setStringValue:[ NSString stringWithCString:&message[8] encoding:NSASCIIStringEncoding ] ];

		modelCT = (0 == strncmp(&message[8], kTabletModelPenPartner, strlen(kTabletModelPenPartner)));
		modelSD = (0 == strncmp(&message[8], kTabletModelSDSeries, strlen(kTabletModelSDSeries)));
		modelPC = (0 == strncmp(&message[8], kTabletModelTabletPC, strlen(kTabletModelTabletPC)));
		modelUD = (0 == strncmp(&message[8], kTabletModelUDSeries, strlen(kTabletModelUDSeries)));
		modelPL = (0 == strncmp(&message[8], kTabletModelPLSeries, strlen(kTabletModelPLSeries)));
		modelGD =	(0 == strncmp(&message[8], kTabletModelIntuos, strlen(kTabletModelIntuos)))
				||	(0 == strncmp(&message[8], kTabletModelIntuos2, strlen(kTabletModelIntuos2)));

		// For TabletPC systems override the scale values
		if (modelPC && [textTweakScaleX intValue] != 0 && [textTweakScaleY intValue] != 0)
			[ self scaleChanged ];
	}

	// The daemon started
	else if (0 == strcmp(message, "[hello]")) {
		NSLog(@"Received %s", message);

		[ buttonKill setEnabled:YES ];
		[ buttonPanic setEnabled:YES ];

		[ self sendRequestToDaemon:"?port" ];

		if ([ popupSerialPort numberOfItems ] == 1)
			[ textTabletInfo setStringValue:[ thePane localizedString:kNoTabletFound ] ];
		else
			[ textTabletInfo setStringValue:[ thePane localizedString:kLookingForTablet ] ];
	}

	// The daemon found a tablet it can talk to
	else if (0 == strcmp(message, "[ready]")) {
		NSLog(@"Received %s", message);

		[ self forgetTabletInfo ];
//		[ self setControlsForConnect:NO ];
		[ self sendRequestToDaemon:"?model" ];
		[ self sendRequestToDaemon:"?info" ];
		[ self sendRequestToDaemon:"?geom" ];
		[ self sendRequestToDaemon:"?scale" ];
		[ self startStreamIfShowing ];
//		[ presetsController sendPresetToDaemon ];
		[ buttonKill setEnabled:YES ];
		[ buttonPanic setEnabled:YES ];
	}

	// The daemon died
	else if (0 == strcmp(message, "[bye]")) {
		NSLog(@"Received %s", message);

		tabletEnabled = NO;
		[ checkEnabled setState:[checkEnabled state]==NSOffState ? NSOffState : NSMixedState ];
		[ self forgetTabletInfo ];
		[ self setControlsForConnect:NO ];
		[ textTabletInfo setStringValue:[ thePane localizedString:kDriverNotLoaded ] ];
		[ buttonKill setEnabled:NO ];
		[ buttonPanic setEnabled:NO ];
	}

	// The daemon found no tablets it can talk to
	else if (0 == strcmp(message, "[none]")) {
		NSLog(@"Received %s", message);

		[ self forgetTabletInfo ];
		[ self setControlsForConnect:NO ];
	}

	// The daemon is replying with its current serial port
	else if (0 == strncmp(message, "[port]", 6)) {
		NSLog(@"Received %s", message);
		[ self selectPortByName:(strlen(message)>=8 ? [NSString stringWithCString:&message[7] encoding:NSASCIIStringEncoding] : @"Automatic") ];
	}

/*	// The command reply was the default "[ok]"
	else if (0 == strncmp(message, "[ok]", 4)) {
		NSLog(@"Received %s", message);
	}
	//*/
}

#pragma mark -
#pragma mark Message Sending
- (CFMessagePortRef)getRemoteMessagePort {
	if ( ![self remotePortExists]
		&& (cfPortOut = CFMessagePortCreateRemote(kCFAllocatorDefault, CFSTR("com.thinkyhead.tabletmagic.daemon"))) )
			CFMessagePortSetInvalidationCallBack(cfPortOut, invalidation_callback);

	return cfPortOut;
}

- (BOOL)remotePortExists { return (cfPortOut != nil); }

- (void)remoteMessagePortWentAway { cfPortOut = nil; }

- (void)disposeRemoteMessagePort {
	if (cfPortOut != nil) {
		CFMessagePortInvalidate(cfPortOut);
		cfPortOut = nil;
	}
}

//
// sendNSStringToDaemon
// Send an NSString and ignore any reply
//
- (void)sendNSStringToDaemon:(NSString*)string {
	char *message = [ self getCStringFromString:string ];
	[ self sendMessageToDaemon:message ];
	free(message);
}

//
// sendMessageToDaemon
// Send a simple message and ignore any reply
//
- (void)sendMessageToDaemon:(char*)message {
	CFDataRef returnData = nil;
	CFMessagePortRef port = [ self getRemoteMessagePort ];

	if (port != nil) {
		CFDataRef data = CFDataCreate( nil, (UInt8*)message, strlen(message)+1 );
		(void)CFMessagePortSendRequest( port, 0, data, 1, 1, kCFRunLoopDefaultMode, &returnData);
		CFRelease(data);

		if ( nil != returnData )
			CFRelease(returnData);
	}
}

/*
	sendRequestToDaemon

	Send a request to the daemon and process the reply

	CFMessagePortSendRequest may time-out while waiting for the
		daemon to respond to a message. When this happens, the
		returned result can get swallowed and lost.

	This wouldn't matter a whole lot, but the pref pane uses a
		polling system to get around the limitation of CFMessagePort
		that it can't do two-way messaging across bootstrap domains.

	The polling system could be replaced by UNIX domain sockets,
	but so far no luck with the implementation.
*/
- (CFDataRef)sendRequestToDaemon:(char*)message {
	CFDataRef returnData = nil;
	CFMessagePortRef port = [self getRemoteMessagePort];

	if (port != nil) {
		CFDataRef data = CFDataCreate( nil, (UInt8*)message, strlen(message)+1 );
		SInt32 err = CFMessagePortSendRequest( port, 0, data, 4, 4, kCFRunLoopDefaultMode, &returnData);
		if (err == kCFMessagePortReceiveTimeout)
			NSLog(@"[ERR ] Timeout on \"%s\" reply", message);
		CFRelease(data);

		if ( nil != returnData )
			[ self handleDaemonMessageData:returnData ];
	}

	return returnData;
}

/*

	doPingTimer

	This is my workaround for the CFMessage namespace issue.

	At startup the daemon is launched by launchd in the
	root bootstrap session. Applications are launched in
	the user session. Although the application (Prefs)
	can send RPCs to the daemon, the daemon cannot send
	messages to the remote port of the preference pane.

	This timer sends a "next" RPC to the daemon to fetch
	any messages the daemon has. All messages are processed.

	This timer also notifies us if the daemon's message port
	went away, by relaying a "[bye]" message, which is what
	the daemon would have sent us if it could. The daemon
	could queue up "[bye]" and hang around for a half-second
	to give us time to fetch it, but this way works 100% of
	the time.

*/
- (void)doPingTimer:(NSTimer*)theTimer {
	#define failMax 2
	static BOOL daemonIsGone = YES;
	static int failCount = failMax;
	if ([self getRemoteMessagePort] != nil) {
		if (daemonIsGone) {
			daemonIsGone = NO;
			failCount = failMax;
//			[self handleDaemonMessage:"[hello]"];
		}
		(void)[self sendRequestToDaemon:"next"];
	} else {
		if (!daemonIsGone) {
			if (failCount-- <= 0) {
				[self handleDaemonMessage:"[bye]"];
				daemonIsGone = YES;
			}
		}
	}
}

#pragma mark -
#pragma mark Daemon Process

- (BOOL)startDaemon {
    FILE		*file = nil;
    OSStatus	result = -1;

	if ( ![ self isDaemonLoaded:YES ] ) {
		[ textTabletInfo setStringValue:[ thePane localizedString:@"Starting Daemon..." ] ];

		NSString *daemonPath = [[thePane bundle] pathForResource:kDaemonName ofType:nil ];
		char *fullpath = [ self getCStringFromString:daemonPath ];

		/*
				- When quitting System Preference in 10.5 the non-daemonized daemon is adopted by launchd
				- Not sure if the same happens in 10.4
				- It probably should daemonize in 10.3

			TODO:
				- Daemonize on start only on systems earlier than 10.3.9.
				- Always create the launchd item in 10.4 and higher when none exists.
				- Start with "launchctl -F" on 10.4 and higher

		*/
		NSMutableArray *argsArray = [ NSMutableArray arrayWithArray:[ [ self getStartupArguments:NO ] componentsSeparatedByString:@" " ] ];

		if ([ self isFileSuidRoot:fullpath ]) {
			NSTask *task = [NSTask launchedTaskWithLaunchPath:daemonPath arguments:argsArray ];
			int pid = [ task processIdentifier ];
			if (pid) result = errAuthorizationSuccess;
		}
		else {
			int i, count = [argsArray count];
			char **args = calloc(count+1, sizeof(char*));
			static char argstring[100];
			strcpy(argstring, "[ TabletMagicDaemon ");
			for (i=0; i<count; i++) {
				args[i] = [ self getCStringFromString:[argsArray objectAtIndex:i] ];
				strcat(argstring, args[i]);
			}
			strcat(argstring, "]\r\n");
			NSLog(@"%s", argstring);

			args[count] = nil;

			result = AuthorizationExecuteWithPrivileges(fAuthorization, fullpath, kAuthorizationFlagDefaults, args, &file);

			for (i=0; i<count; i++)
				free(args[i]);

			free(args);
		}

		free(fullpath);

		if (file) fclose(file);

		if ( errAuthorizationSuccess != result ) {
			[ textTabletInfo setStringValue:[ thePane localizedString:kDriverNotLoaded ] ];
			NSLog(@"Failed to start the daemon: %d", result);
		}
	}

	return (result == errAuthorizationSuccess || [ self isDaemonLoaded:YES ]);
}

- (void)doKillTimer:(NSTimer*)theTimer {
	BOOL ping = [ self isDaemonLoaded:YES ];
	ping = [ self killDaemon ];
}

- (void)killDaemonSoftly {
	[ self sendMessageToDaemon: "quit" ];

	killTimer = [ NSTimer scheduledTimerWithTimeInterval:1
					target:self
					selector:@selector(doKillTimer:)
					userInfo:nil
					repeats:NO ];
}

- (BOOL)killDaemon {
    FILE		*file = nil;
    OSStatus	result = errAuthorizationCanceled;

	if ( [ self isDaemonLoaded:NO ] ) {
		int i;
		pid_t pids[10];
		char *args[] = { nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil };

		if ([ thePane systemVersion ] < 0x1030) {
			int err = 0;
			unsigned num = 0;

			if (0 == GetAllPIDsForProcessName(kCDaemonName, pids, 10, &num, nil)) {
				args[num] = nil;
				for(i=0; i<num; i++)
					if (0 > asprintf(&args[i], "%d", pids[i]))
						err = -1;

				if (err == 0)
					result = AuthorizationExecuteWithPrivileges(fAuthorization, "/bin/kill", kAuthorizationFlagDefaults, args, &file);

				for(i=0; i<num; i++)
					if (args[i]) free(args[i]), args[i] = nil;
			}
		}
		else {
			if (0 < asprintf(&args[0], kCDaemonName))
				result = AuthorizationExecuteWithPrivileges(fAuthorization, "/usr/bin/killall", kAuthorizationFlagDefaults, args, &file);
		}

		for(i=0; i<10; i++)
			if (args[i]) free(args[i]);

		if ( errAuthorizationSuccess != result )
			NSLog(@"Failed to kill the daemon: %d", result);

		if (file) fclose(file);
	}

	return [ self isDaemonLoaded:YES ];
}

- (BOOL)isDaemonLoaded:(BOOL)refresh {
	static BOOL isLoaded = NO;

	if (refresh)
		isLoaded = (0 < GetFirstPIDForProcessName(kCDaemonName));

	return isLoaded;
}

- (void)setupAuthorization {
    OSStatus    result;
    
    result = AuthorizationCreate(nil, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &fAuthorization);
    if (result != errAuthorizationSuccess) {
        NSLog(@"Failed to create an authorization record: %d", result);
        fAuthorization = nil;
    }
}

- (void)cleanUpAuthorization {
    OSStatus result;
    
    result = AuthorizationFree(fAuthorization, kAuthorizationFlagDestroyRights);
    
    if ( result != errAuthorizationSuccess )
        NSLog(@"Failed to free the authorization record: %d", result);
}

//
// isFileSuidRoot
//
- (BOOL)isFileSuidRoot:(char*)fullpath {
	BOOL suid_root = NO;
	struct stat st;
	int fd_tool;

	/* Open tool exclusively, so noone can change it while we bless it */
	fd_tool = open(fullpath, O_NONBLOCK|O_RDONLY|O_EXLOCK, 0);

	if (fd_tool != -1) {
		int sr = fstat(fd_tool, &st);
		int uid = st.st_uid;
		if (sr == 0 && uid == 0)
			suid_root = YES;

		close(fd_tool);
	}

	return suid_root;
}

#pragma mark -
#pragma mark Control States

- (void)requestCurrentSettings {
	[ matrixMem selectCellWithTag:0 ];
	[ buttonSetBank setEnabled:NO ];
	[ self sendRequestToDaemon:"?bank 0" ];
}

//
// setControlsForSettings
// Set the controls to reflect a given settings string
//
#define twobitv(sh) ((bits>>(sh))&0x03)
#define onebitv(sh) ((bits>>(sh))&0x01)
- (void)setControlsForSettings:(char*)setup {
	unsigned bits = 0;
	unsigned increment, interval, rezx, rezy;

	if (5 == sscanf(setup, "%8X,%3u,%2u,%4u,%4u", &bits, &increment, &interval, &rezx, &rezy )) {
		popupSelectTag(popupCommandSet, twobitv(30));

		[ popupBaud			selectItemAtIndex:twobitv(28) ];
		
		popupSelectTag(popupParity, twobitv(26));

		[ popupDataBits		selectItemAtIndex:onebitv(25) ];
		[ popupStopBits		selectItemAtIndex:onebitv(24) ];
		
		[ checkCTS			setState:onebitv(23) ? NSOnState : NSOffState ];
		[ checkDSR			setState:onebitv(22) ? NSOnState : NSOffState ];
		[ popupTransferMode	selectItemAtIndex:twobitv(20) ];
		
		[ popupOutputFormat	selectItemAtIndex:onebitv(19) ];
		[ popupCoordSys		selectItemAtIndex:onebitv(18) ];
		[ popupTransferRate	selectItemAtIndex:twobitv(16) ];
		
		[ popupResolution	selectItemAtIndex:twobitv(14) ];
		[ popupOrigin		selectItemAtIndex:onebitv(13) ];
		[ checkOORData		setState:onebitv(12) ? NSOnState : NSOffState ];

		[ popupTerminator	selectItemAtIndex:twobitv(10) ];
					// bit 9 unused
		[ checkPlugPlay		setState:onebitv(8) ? NSOnState : NSOffState ];

		[ popupSensitivity	selectItemAtIndex:onebitv(7) ];
		[ popupReadHeight	selectItemAtIndex:onebitv(6) ];
		[ checkMultiMode	setState:onebitv(5) ? NSOnState : NSOffState ];
		[ checkTilt			setState:onebitv(4) ? NSOnState : NSOffState ];
					// bits 0123 unused here

		[ editIncrement setIntValue: increment ];
		[ stepperIncrement setIntValue: increment ];

		[ editInterval setIntValue: interval ];
		[ stepperInterval setIntValue: interval ];

		[ editXRez setIntValue: rezx ];
		[ stepperXRez setIntValue: rezx ];

		[ editYRez setIntValue: rezy ];
		[ stepperYRez setIntValue: rezy ];

		if (0 == [ [ matrixMem selectedCell ] tag ])
			[ self setControlsForCommandSet ];
	}
}

- (void)setStreamHeadingForSet:(int)set andFormat:(int)fmt {
	NSString *setname[] = { @"", @"", @"II-S", @"IV", @"V", @"ISDV4" };
	NSString *typename[] = { @"Binary", @"ASCII" };

	[ groupDatastream setTitle:
		[ NSString stringWithFormat:[ thePane localizedString:@"Datastream (%@ %@)" ], setname[set], [ thePane localizedString:typename[fmt] ] ] ];
}

- (void)setControlsForCommandSet {
	int set = [[popupCommandSet selectedItem] tag];
	int fmt = set == kCommandSetWacomIV ? 0 : [popupOutputFormat indexOfSelectedItem];

	// Datastream Box Heading
	[ self setStreamHeadingForSet:set andFormat:fmt ];

	// Command popup
	int oldselection = [[popupCommands selectedItem] tag];
	[ popupCommands removeAllItems ];

	BOOL addedOne = NO;
	BOOL divFlag = NO;
	int i;
	for (i=0; i<sizeof(tabletCommands)/sizeof(tabletCommands[0]); i++) {
		TMCommand *item = &tabletCommands[i];

		if (item->divider) divFlag = YES;

		UInt16 mask = ONLY_IIS
					| (modelSD ? COMP_SD : 0)
					| (modelPL ? COMP_PL : 0)
					| (modelCT ? COMP_CT : 0)
					| (modelUD ? COMP_UD : 0)
					| (modelGD ? COMP_GD : 0)
					| (modelPC ? COMP_PC : 0);

		if ( (item->compatibility & mask)
			&& !((item->compatibility & ONLY_IIS) && (set != kCommandSetWacomIIS))
		) {
			if (divFlag && addedOne)
				[[popupCommands menu] addItem:[NSMenuItem separatorItem]];

			[ popupCommands addItemWithTitle:[ thePane localizedString:item->description ] ];
			[ [popupCommands lastItem] setTag:i ];
			addedOne = YES;
			divFlag = NO;
		}
	}

	popupSelectTag(popupCommands, oldselection);
}

/*

	A mysterious method, what does it do?

	It sets the setup and Testing controls on or off
	based on: running daemon > claim of connection > non-configurable models

*/
- (void)setControlsForConnect:(BOOL)connected {
	BOOL loaded = [self isDaemonLoaded:YES];
	if (!loaded)
		[ textTabletInfo setStringValue:[ thePane localizedString:kDriverNotLoaded ] ];
	else if (!connected)
		[ textTabletInfo setStringValue:[ thePane localizedString:kNoTabletFound ] ];

	BOOL doesCommands = !(modelSD || modelPC) && !modelPL && !modelGD;
 	BOOL enable = loaded && connected && doesCommands;
	BOOL serial = !loaded || (doesCommands && !modelCT);	// TODO: SD Fallback should leave serial enabled

	[ matrixMem			selectCellWithTag:0 ];
	[ matrixMem			setEnabled:enable ];

	[ popupBaud			setEnabled:serial ];
	[ popupParity		setEnabled:serial ];
	[ popupDataBits		setEnabled:serial ];
	[ popupStopBits		setEnabled:serial ];
	[ checkCTS			setEnabled:serial ];
	[ checkDSR			setEnabled:serial ];

	[ popupCommandSet	setEnabled:enable ];
	[ popupTransferMode	setEnabled:enable ];
	[ popupOutputFormat	setEnabled:enable ];
	[ popupCoordSys		setEnabled:enable ];
	[ popupTransferRate	setEnabled:enable ];
	[ popupResolution	setEnabled:enable ];
	[ popupOrigin		setEnabled:enable ];
	[ checkOORData		setEnabled:enable ];
	[ popupTerminator	setEnabled:enable ];

	[ checkPlugPlay		setEnabled:enable ];

	[ popupSensitivity	setEnabled:enable ];
	[ popupReadHeight	setEnabled:enable ];
	[ checkMultiMode	setEnabled:enable ];
	[ checkTilt			setEnabled:enable ];

	[ editIncrement		setEnabled:enable ];
	[ stepperIncrement	setEnabled:enable ];

	[ editInterval		setEnabled:enable ];
	[ stepperInterval	setEnabled:enable ];

	[ editXRez			setEnabled:enable ];
	[ stepperXRez		setEnabled:enable ];

	[ editYRez			setEnabled:enable ];
	[ stepperYRez		setEnabled:enable ];

	[ editXScale		setEnabled:enable ];
	[ stepperXScale		setEnabled:enable ];

	[ editYScale		setEnabled:enable ];
	[ stepperYScale		setEnabled:enable ];

//	if (!connected && !loaded)
//		[ self setControlsForSettings:"E202C900,002,02,1270,1270" ];

	// Testing Area
	[ popupCommands		setEnabled:loaded && connected ];
	[ buttonSendCommand	setEnabled:loaded && connected ];
}


//
// settingsStringFromControls
// Translate the current "Settings" controls into a Setup String
//
// TODO: Make a class called SettingsController to encapsulate
//	the MVC of settings. It deals with all controls pertaining
//	only to settings. It can be queried for any one setting.
//
- (NSString*)settingsStringFromControls {
	unsigned long bits = 0;

	// Command Set popup
	bits |= 0x40000000 * [ [ popupCommandSet selectedItem ] tag ];

	// Baud Rate popup
	bits |= 0x10000000 * ([ popupBaud indexOfSelectedItem ] & 0x03);

	// Parity popup
	bits |= 0x04000000 * [ [ popupParity selectedItem ] tag ];

	// Data Bits popup
	bits |= 0x02000000 * [ popupDataBits indexOfSelectedItem ];

	// Stop Bits popup
	bits |= 0x01000000 * [ popupStopBits indexOfSelectedItem ];

	// CTS checkbox
	if (NSOnState == [ checkCTS state ]) bits |= 0x00800000;

	// DSR checkbox
	if (NSOnState == [ checkDSR state ]) bits |= 0x00400000;

	// Transfer Mode popup
	bits |= 0x00100000 * [ popupTransferMode indexOfSelectedItem ];

	// Output Format popup
	bits |= 0x00080000 * [ popupOutputFormat indexOfSelectedItem ];

	// Coordinate System popup
	bits |= 0x00040000 * [ popupCoordSys indexOfSelectedItem ];

	// Transfer Rate popup
	bits |= 0x00010000 * [ popupTransferRate indexOfSelectedItem ];

	// Resolution popup
	bits |= 0x00004000 * [ popupResolution indexOfSelectedItem ];

	// Origin popup
	bits |= 0x00002000 * [ popupOrigin indexOfSelectedItem ];

	// OOR Data checkbox
	if (NSOnState == [ checkOORData state ]) bits |= 0x00001000;

	// Terminator popup
	bits |= 0x00000400 * [ popupTerminator indexOfSelectedItem ];

	// (bit 2 unused)

	// PnP checkbox
	if (NSOnState == [ checkPlugPlay state ]) bits |= 0x00000100;

	// Pressure Sensitivity popup
	bits |= 0x00000080 * [ popupSensitivity indexOfSelectedItem ];

	// Read Height popup
	bits |= 0x00000040 * [ popupReadHeight indexOfSelectedItem ];

	// Multi-Data Mode checkbox
	if (NSOnState == [ checkMultiMode state ]) bits |= 0x00000020;

	// Tilt checkbox
	if (NSOnState == [ checkTilt state ]) bits |= 0x00000010;

	return [ NSString stringWithFormat:@"%08X,%03d,%02d,%04d,%04d",
						bits,
						[ editIncrement intValue ],
						[ editInterval intValue ],
						[ editXRez intValue ],
						[ editYRez intValue] ];

}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem {
	static	int lastTab = 0;
	current_tab = [[tabViewItem identifier] intValue];

	if ([ self isDaemonLoaded:NO ]) {
		if (current_tab == 3 || (current_tab == 6 && scale_from_stylus == YES))
			[ self setStreamMonitorEnabled:YES ];
		else if (lastTab == 3 || (lastTab == 6 && scale_from_stylus == YES))
			[ self setStreamMonitorEnabled:NO ];
	}

	lastTab = current_tab;
}

#pragma mark -
#pragma mark Stream Monitoring

//
// setStreamMonitorEnabled
//
// Tell the daemon to turn stream monitoring on or off
//
// With Stream Monitoring enabled the daemon will
// continuously send the preference pane the current state.
//
- (void)setStreamMonitorEnabled:(BOOL)enable {
	if (enable) {
		[ self sendMessageToDaemon: "stron" ];

		if (streamTimer == nil)
			streamTimer	= [ NSTimer scheduledTimerWithTimeInterval:0.1
							target:self
							selector:@selector(doStreamTimer:)
							userInfo:nil
							repeats:YES ];
	}
	else {
		[ streamTimer invalidate ];
		[ self sendMessageToDaemon: "stroff" ];
		streamTimer = nil;
	}
}

- (void)doStreamTimer:(NSTimer*)theTimer {
	[ self sendRequestToDaemon:"stream" ];
}

- (void)startStreamIfShowing {
	if (current_tab == 3 || (current_tab == 6 && scale_from_stylus == YES) )
		[ self setStreamMonitorEnabled:YES ];
}

#pragma mark -
#pragma mark Actions

//
// selectedSerialPort
// A different serial port was selected, tell the daemon
//
- (IBAction)selectedSerialPort:(id)sender {
	[ self forgetTabletInfo ];
	[ self setControlsForConnect:NO ];

	if ([self isDaemonLoaded:NO]) {
		[ textTabletInfo setStringValue:[ thePane localizedString:kLookingForTablet ] ];

		if ([sender indexOfSelectedItem]) {
			NSString *portString = [ NSString stringWithFormat:@"port %@", [[sender selectedItem]title] ];

			[ self sendNSStringToDaemon:portString ];

//			[ portString release ];
		}
		else
			[ self sendMessageToDaemon:"port" ];
	}
}

//
// chooseMemoryBank
//
- (IBAction)chooseMemoryBank:(id)sender {
	int bank = [ [ sender selectedCell ] tag ];
	char message[10];
	sprintf(message, "?bank %d", bank);
	[ self sendMessageToDaemon:message ];

	[ buttonSetBank setEnabled:(bank > 0) ? YES : NO ];
}

//
// activateMem
//
- (IBAction)activateMem:(id)sender {
	NSString *settingsString = [ NSString stringWithFormat:@"reinit %@", [ self settingsStringFromControls ] ];

	[ self sendNSStringToDaemon:settingsString ];

	[ settingsString autorelease ];
	[ self requestCurrentSettings ];
}

//
// toggledDaemon
//
// The Enabled button was clicked
// Start the daemon and tell it to pause or resume
//
- (IBAction)toggledDaemon:(id)sender {
	tabletEnabled = NO;

	if ([sender state] != NSOffState) {
		[ sender setState:NSMixedState ];
		if ([self startDaemon]) {
			tabletEnabled = YES;
			[ sender setState:NSOnState ];
			[ self sendMessageToDaemon:"start" ];
		}
	}
	else {
		[ self sendMessageToDaemon:"stop" ];
		[ sender setState:NSOffState ];
	}

	[ self savePreferences ];
}

//
// sendSelectedCommand
// Send the selected command to the tablet
//
- (IBAction)sendSelectedCommand:(id)sender {
	int item = [ [popupCommands selectedItem] tag ];

	NSString *commandString = [ NSString stringWithFormat:@"command %s", tabletCommands[item].command ];

	ignore_next_info = tabletCommands[item].ignore_next_info;
	stream_reply_size = tabletCommands[item].replysize;
	stream_reply_count = 0;

	bool tpc = (0 == strcmp(TPC_TabletID, tabletCommands[item].command));
	if (tpc) {
		[ self sendNSStringToDaemon:[ NSString stringWithFormat:@"command %s", TPC_StopTablet ] ];
		(void)usleep(100000);	// 0.1 seconds
	}

	[ self sendNSStringToDaemon:commandString ];

	if (tpc) {
		[ self sendNSStringToDaemon:[ NSString stringWithFormat:@"command %s", TPC_Sample133pps ] ];
	}

	// If the command affects settings, request them from newer tablets
	if (tabletCommands[item].get_settings && !(modelSD || modelPC) && !modelPL && !modelGD)
		[ self requestCurrentSettings ];
}

//
// settingsChanged
// The settings changed, so tell the daemon about it
//
- (IBAction)settingsChanged:(id)sender {
	[ self scaleChanged ];

	int banknum = [ [ matrixMem selectedCell ] tag ];

	NSString *settingsString;

	if (banknum == 0)
		settingsString = [ NSString stringWithFormat:@"setup %@", [ self settingsStringFromControls ] ];
	else
		settingsString = [ NSString stringWithFormat:@"setmem %d %@", banknum, [ self settingsStringFromControls ] ];

	[ self sendNSStringToDaemon:settingsString ];

	if (banknum == 0) [ self setControlsForCommandSet ];
}

- (IBAction)toggledTiltOrMM:(id)sender {
	if (sender == checkTilt) {
		if ([ checkTilt state ] == NSOnState)
			[ checkMultiMode setState:NSOffState ];
	}
	else {
		if ([ checkMultiMode state ] == NSOnState)
			[ checkTilt setState:NSOffState ];
	}

	[ self settingsChanged:sender ];
}

- (IBAction)serialSettingsChanged:(id)sender {
	if (0 == [ [ matrixMem selectedCell ] tag ]) {
		NSString *settingsString = [ NSString stringWithFormat:@"reinit %@", [ self settingsStringFromControls ] ];

		[ self sendNSStringToDaemon:settingsString ];

		[ self setControlsForCommandSet ];
	}
	else
		[ self settingsChanged:sender ];
}

//
// scaleChanged
// The scale changed, so tell the daemon about it and
// update the tablet geometry control
//
- (void)scaleChanged {
	int x, y;

	if (modelPC) {
		x = [textTweakScaleX intValue];
		y = [textTweakScaleY intValue];
	} else {
		x = [editXScale intValue];
		y = [editYScale intValue];
	}

	if (x >= 0 && y >= 0) {
		[ self sendNSStringToDaemon:[ NSString stringWithFormat:@"scale %d %d", x, y ] ];
		[ presetsController updateTabletScaleX:x y:y ];
	}
}

#pragma mark -
#pragma mark Daemon Auto Start
//
// toggleAutoStart
// The "Launch at Startup" checkbox was toggled
//
- (IBAction)toggleAutoStart:(id)sender {
	OSStatus result = [ self updateAutoStart ];

	BOOL state = [sender state] == NSOnState;
	if (result != errAuthorizationSuccess)
		[ sender setState:(state ? NSOffState : NSOnState) ];
}

//
// getStartupArguments
//
- (NSString*)getStartupArguments:(BOOL)daemonize {
	TMPreset	*preset = [presetsController activePreset];

	NSMutableString *argString = [ NSMutableString stringWithFormat:
		@"-c -n-20 -L%d -T%d -R%d -B%d -l%d -t%d -r%d -b%d -M1:%d -M2:%d -M3:%d -M4:%d",
			preset.tabletLeft, preset.tabletTop, preset.tabletRight, preset.tabletBottom,
			preset.screenLeft, preset.screenTop, preset.screenRight, preset.screenBottom,
			preset.buttonTip, preset.buttonSwitch1, preset.buttonSwitch2, preset.buttonEraser
		];

	if ( preset.mouseMode ) {
		[ argString appendString:[NSString stringWithFormat:@" -m -s%.4f", preset.mouseScaling ] ];
	}

	if ( daemonize )
		[ argString appendString:@" -d" ];

	if ( hackintosh ) {
		if ( [checkTabletPC state] == NSOnState )
			[ argString appendString:@" -F" ];

		char *digi_string = get_digitizer_string();
		if ( digi_string && (0 == strcmp(digi_string, "WACF008") || 0 == strcmp(digi_string, "WACF009")) )
			[ argString appendString:@" -3" ];
	}

	if ( [checkEnabled state] == NSOffState )
		[ argString appendString:@" -o" ];

	if ( [popupSerialPort indexOfSelectedItem] > 0 )
		[ argString appendString:[NSString stringWithFormat:@" -p%@",[[popupSerialPort selectedItem]title]] ];

	return argString;
}

//
// updateAutoStart
//
- (OSStatus)updateAutoStart {
	BOOL		state = ([checkAutoStart state] == NSOnState);
    OSStatus	result = errAuthorizationSuccess;
	long		version = [ thePane systemVersion ];

	if (version < 0x1040)
		(void) [ self runLaunchHelper:(state ? [ self getStartupArguments:YES ] : @"disable") ];

	else {
		NSMutableArray *args = [ NSMutableArray arrayWithArray:[ [ self getStartupArguments:NO ] componentsSeparatedByString:@" " ] ];
		[ args insertObject:@"/Library/PreferencePanes/TabletMagic.prefPane/Contents/Resources/TabletMagicDaemon" atIndex:0 ];

		// Create a Dictionary for launchd, either enabled or disabled
		NSMutableDictionary *launcher = [NSMutableDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithBool:!state],			@"Disabled",
			kLaunchdKeyName,							@"Label",
			args,										@"ProgramArguments",
			[NSNumber numberWithBool:YES],				@"RunAtLoad",
			@"Daemon to support Wacom serial tablets",	@"ServiceDescription",	
			@"root",									@"UserName",
			nil];

		// In Leopard load as a LaunchAgent in both LoginWindow and Aqua window sessions
		if (version >= 0x1050) {
			[ launcher	setObject: [ @"LoginWindow Aqua" componentsSeparatedByString:@" " ]
						forKey: @"LimitLoadToSessionType" ];
		}

		// Write the Dictionary out as a temporary file
		[ launcher writeToFile:(@"/tmp/" kLaunchPlistName) atomically:NO ];

		// Run the LaunchHelper bin to install the agent or daemon item
		if (version < 0x1050)
			(void) [ self runLaunchHelper:@"launchd" ];
		else
			(void) [ self runLaunchHelper:@"launchd10.5" ];
	}

	return result;
}

- (void)updateAutoStartSoon {
	[ autoStartTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:1.5 ] ];
}


//
// updateAutoStartCheckbox
//
// Look for either a StartupItems or a launchd entry
// Set the Auto Start checkbox to the proper state
//
- (void)updateAutoStartCheckbox {
	BOOL isAutoStarting = [[NSFileManager defaultManager] fileExistsAtPath:kStartupItem];

	if (!isAutoStarting && [ thePane systemVersion ] >= 0x1040) {
		NSDictionary *launchDict = [ NSDictionary dictionaryWithContentsOfFile: @"/Library/LaunchDaemons/" kLaunchPlistName ];

		if (launchDict == nil)
			launchDict = [ NSDictionary dictionaryWithContentsOfFile: @"/Library/LaunchAgents/" kLaunchPlistName ];

		if (launchDict != nil)
			isAutoStarting = ![[launchDict objectForKey:@"Disabled"] boolValue];

	}

	[ checkAutoStart setState:isAutoStarting ? NSOnState : NSOffState ];
}

//
// runLaunchHelper
//
- (char*)runLaunchHelper:(NSString*)argsString {
    OSStatus	result = errAuthorizationSuccess;
	static char	outputBuffer[20];
	int			buflen = 0;

	NSString *helperPath = [[thePane bundle] pathForResource:kLaunchHelperName ofType:nil ];
	char *fullpath = [ self getCStringFromString:helperPath ];

	outputBuffer[0] = '\0';

	// Skip authorization if it the helper is already SUID
	if ([ self isFileSuidRoot:fullpath ]) {
		// Use a pipe to capture the helper's output
		NSPipe *pipe = [ NSPipe pipe ];
		NSFileHandle *INPIPE = [ pipe fileHandleForReading ];

		NSTask *task = [[NSTask alloc] init];
		[ task setLaunchPath:helperPath ];
		[ task setArguments:[NSArray arrayWithObjects:argsString, nil] ];
		[ task setStandardOutput:pipe ];
		[ task launch ];

		int pid = [ task processIdentifier ];
		if (pid) result = errAuthorizationSuccess;

		NSData *data = [ INPIPE readDataToEndOfFile ];
		[ data getBytes:outputBuffer ];
		buflen = [data length];
		outputBuffer[buflen] = '\0';
	}
	else {
		// This code will only execute the first time the tool runs

		char *args[] = { nil, nil };

		if (argsString != nil)
			args[0] = [ self getCStringFromString:argsString ];

		FILE *pipe = nil;

		// TODO: Open a "file" and pass it as the last argument so the output can be captured
		result = AuthorizationExecuteWithPrivileges(fAuthorization, fullpath, kAuthorizationFlagDefaults, args, &pipe);

		if (pipe != nil) {
			fgets(outputBuffer, 20, pipe);
			fclose(pipe);
		}
	}

//	NSLog(@"The Helper said: %s (%d)", outputBuffer, strlen(outputBuffer));

	if ( errAuthorizationSuccess != result )
		NSLog(@"Failed to run the Launch Helper: %d", result);

	return outputBuffer;
}

#pragma mark -
#pragma mark Other Extras Controls
- (IBAction)enableInk:(id)sender {
//	NSString	*com1 = @"/usr/bin/defaults write com.apple.ink.framework inkWindowVisible -bool true",

	NSTask *task = [[NSTask alloc] init];
	[ task setLaunchPath:@"/usr/bin/osascript" ];
	[ task setArguments:[ NSArray arrayWithObjects:@"-e", @"'tell application \"InkServer\" to quit'", nil ] ];
	[ task launch ];
	[ task waitUntilExit ];
	[ task release ];

	[ NSThread sleepUntilDate:[ NSDate dateWithTimeIntervalSinceNow:0.5 ] ];

	static NSString *inkBundle = @"com.apple.ink.framework";
	NSMutableDictionary *inkprefs = [ [ NSMutableDictionary dictionaryWithCapacity:100 ] retain ];
	[ inkprefs addEntriesFromDictionary:[ [NSUserDefaults standardUserDefaults] persistentDomainForName:inkBundle ] ];
	[ inkprefs setValue:NSBOOL(YES) forKey:@"recognitionEnabled" ];
	[ inkprefs setValue:NSBOOL(YES) forKey:@"inkMenuVisible" ];
	[ inkprefs setValue:NSBOOL(YES) forKey:@"inkMasterSwitchOn" ];
	[ inkprefs setValue:NSBOOL(YES) forKey:@"inkWindowVisible" ];
	[ [NSUserDefaults standardUserDefaults] setPersistentDomain:inkprefs forName:inkBundle ];
	[ [NSUserDefaults standardUserDefaults] synchronize ];
	[ inkprefs release ];

//				*com2 = @"/usr/bin/open ";

	task = [[NSTask alloc] init];
	[ task setLaunchPath:@"/usr/bin/open" ];
	[ task setArguments:[ NSArray arrayWithObjects:@"/System/Library/Components/Ink.component/Contents/SharedSupport/InkServer.app", nil ] ];
	[ task launch ];
	[ task release ];
}

#define kKillHeading	@"Kill TabletMagicDaemon?"
#define kKillMessage	@"This will kill the TabletMagicDaemon process. Click the Enabled checkbox to start it up again."
#define kKillOkay		@"Kill It!"
#define kKillCancel		@"Cancel"

- (IBAction)killTheDaemon:(id)sender {
	int result = 0;

	static NSString *kill = nil, *okay, *cancel, *detail;

	if (kill == nil) {
		kill = [ thePane localizedString:kKillHeading ];
		detail = [ thePane localizedString:kKillMessage ];
		okay = [ thePane localizedString:kKillOkay ];
		cancel = [ thePane localizedString:kKillCancel ];
	}

	if ([thePane systemVersion] < 0x1030) {
		result = NSRunAlertPanelRelativeToWindow( kill, detail, okay, cancel, nil, [ [thePane mainView] window ] );

		if (result == 1)
			[ self killDaemonSoftly ];
	}
	else {
		[ [ NSAlert
				alertWithMessageText: kill
				defaultButton: okay
				alternateButton: cancel
				otherButton:nil
				informativeTextWithFormat: detail
				]	beginSheetModalForWindow: [[thePane mainView] window]
					modalDelegate:self
					didEndSelector:@selector(killDialogEnded:returnCode:contextInfo:)
					contextInfo:self
		];
	}

}

- (void)killDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	if (returnCode == 1)
		[ self killDaemonSoftly ];
}

- (IBAction)panicButton:(id)sender {
	[ self sendMessageToDaemon:"panic" ];
}


#pragma mark -
#pragma mark TabletPC Extras
#define kHackHeading	@"Enable TabletPC Digitizer?"
#define kHackMessage	@"This will modify a core system component to enable your digitizer, if possible."
#define kHackOkay		@"Modify and Reboot"
#define kHackCancel		@"Cancel"

- (IBAction)enableDigitizer:(id)sender {
	int result = 0;

	static NSString *hack = nil, *okay, *cancel, *detail;

	if (hack == nil) {
		hack = [ thePane localizedString:kHackHeading ];
		detail = [ thePane localizedString:kHackMessage ];
		okay = [ thePane localizedString:kHackOkay ];
		cancel = [ thePane localizedString:kHackCancel ];
	}

	if ([thePane systemVersion] < 0x1030) {
		result = NSRunAlertPanelRelativeToWindow( hack, detail, okay, cancel, nil, [ [thePane mainView] window ] );

		if (result == 1) {
			char *reply = [ self runLaunchHelper:@"enabletabletpc" ];
			[ self handleEnablerResponse:reply ];
		}
	}
	else {
		[ [ NSAlert
				alertWithMessageText: hack
				defaultButton: okay
				alternateButton: cancel
				otherButton:nil
				informativeTextWithFormat: detail
				]	beginSheetModalForWindow: [[thePane mainView] window]
					modalDelegate:self
					didEndSelector:@selector(hackDialogEnded:returnCode:contextInfo:)
					contextInfo:self
		];
	}

}

- (void)hackDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	if (returnCode == 1) {
		TMController *controller = ((TMController*)contextInfo);

	(void) [ NSTimer scheduledTimerWithTimeInterval:0.1
				target:controller
				selector:@selector(doApplyHackTimer:)
				userInfo:nil
				repeats:NO ];
	}
}

- (void)doApplyHackTimer:(NSTimer*)theTimer {
	char *reply = [ self runLaunchHelper:@"enabletabletpc" ];
	[ self handleEnablerResponse: reply ];
}

#define kFailHeading	@"No Digitizer Found!"
#define kFailMessage	@"Sorry, but the enabler couldn't find a digitizer in the I/O Registry."
#define kFailMessage2	@"Sorry, but the enabler failed to execute."
#define kFailOkay		@"Okay"
- (void)handleEnablerResponse:(char *)reply {
	static NSString *heading = nil, *detail, *detail2, *okay;

	if (heading == nil) {
		heading = [ thePane localizedString:kFailHeading ];
		detail = [ thePane localizedString:kFailMessage ];
		detail2 = [ thePane localizedString:kFailMessage2 ];
		okay = [ thePane localizedString:kFailOkay ];
	}

	BOOL foundNone = (NULL != strstr(reply, "none"));
	BOOL didFail = (NULL != strstr(reply, "fail"));

	if (foundNone || didFail) {
		NSString *msg = didFail ? detail2 : detail;

		if ([thePane systemVersion] < 0x1030) {
			(void) NSRunAlertPanelRelativeToWindow( heading, msg, okay, nil, nil, [ [thePane mainView] window ] );
		}
		else {
			[ [ NSAlert
					alertWithMessageText: heading
					defaultButton: okay
					alternateButton: nil
					otherButton:nil
					informativeTextWithFormat: msg
					]	beginSheetModalForWindow: [[thePane mainView] window]
						modalDelegate:self
						didEndSelector:@selector(failDialogEnded:returnCode:contextInfo:)
						contextInfo:nil
			];
		}
	}
	else {
		NSDictionary *error;
		NSAppleScript *restartScript = [[NSAppleScript alloc] initWithSource:@"tell application \"Finder\" to restart" ];
		[restartScript executeAndReturnError: &error];
	}
}

- (void)failDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo {
}

- (void)expandScaleX:(int)x y:(int)y {
	int oldx = [ textTweakScaleX intValue ],
		oldy = [ textTweakScaleY intValue ];

	if (x > oldx || y > oldy) {
		if (x > oldx)
			[ textTweakScaleX setIntValue:x ];

		if (y > oldy)
			[ textTweakScaleY setIntValue:y ];

		[ scaleTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:0.6 ] ];
	}
}

- (IBAction)toggledTabletPC:(id)sender {
	[ self sendMessageToDaemon:([sender state] == NSOnState ? "tabletpc 1" : "tabletpc 0") ];
	[ self savePreferences ];
}

- (IBAction)toggledGetFromStylus:(id)sender {
	scale_from_stylus = ([sender state] == NSOnState);
	[ self setStreamMonitorEnabled: scale_from_stylus ];

	if (scale_from_stylus) {
		[ textTweakScaleX setIntValue:0 ];
		[ textTweakScaleY setIntValue:0 ];
	}
}

- (IBAction)tweakScaleChanged:(id)sender {
	[ scaleTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:0.6 ] ];
}

- (BOOL)detectTabletPC {
	char *digi_string = get_digitizer_string();
	BOOL has_digitizer_entry = (digi_string && strlen(digi_string));
	BOOL is_known_mac = is_known_machine(NULL);

	return (has_digitizer_entry || !is_known_mac);
}

#pragma mark -
#pragma mark Donate Controls
- (IBAction)donate:(id)sender {
	char *url_donate = "http://www.thinkyhead.com/tabletmagic/contribute";
	CFURLRef url = CFURLCreateWithBytes( kCFAllocatorDefault, (const UInt8 *) url_donate, strlen(url_donate), kCFStringEncodingASCII, nil);
	(void)LSOpenCFURLRef(url, nil);
	CFRelease(url);
}

- (IBAction)visit:(id)sender {
	char *url_visit = "http://sourceforge.net/projects/tabletmagic/";
	CFURLRef url = CFURLCreateWithBytes( kCFAllocatorDefault, (const UInt8 *) url_visit, strlen(url_visit), kCFStringEncodingASCII, nil);
	(void)LSOpenCFURLRef(url, nil);
	CFRelease(url);
}

- (IBAction)toggledDonation:(id)sender {
	[ self savePreferences ];
}

#pragma mark -

//
// Handle steppers and edit fields in the Settings panel
//
- (IBAction)actionIncrement:(id)sender {
	[ self setText:editIncrement andStepper:stepperIncrement toInt:[sender intValue] ];
}

- (IBAction)actionInterval:(id)sender {
	[ self setText:editInterval andStepper:stepperInterval toInt:[sender intValue] ];
}

- (IBAction)actionRezX:(id)sender {
	[ self setText:editXRez andStepper:stepperXRez toInt:[sender intValue] ];
}

- (IBAction)actionRezY:(id)sender {
	[ self setText:editYRez andStepper:stepperYRez toInt:[sender intValue] ];
}

- (IBAction)actionScaleX:(id)sender {
	if ([self setValueOfControl:editXScale toInt:[sender intValue] ] || [self setValueOfControl:stepperXScale toInt:[sender intValue] ])
		[ scaleTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:0.6 ] ];
}

- (IBAction)actionScaleY:(id)sender {
	if ([self setValueOfControl:editYScale toInt:[sender intValue] ] || [self setValueOfControl:stepperYScale toInt:[sender intValue] ])
		[ scaleTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:0.6 ] ];
}

#pragma mark -
- (void)setText:(id)control1 andStepper:(id)control2 toInt:(int)value {
	if ([self setValueOfControl:control1 toInt:value ] || [self setValueOfControl:control2 toInt:value ])
		[ settingTimer setFireDate: [ NSDate dateWithTimeIntervalSinceNow:0.6 ] ];
}

- (BOOL)setValueOfControl:(id)control toInt:(int)value {
	if (value != [ control intValue ]) {
		[control setIntValue:value];
		return YES;
	}

	return NO;
}

- (void)doSettingsTimer:(NSTimer*)theTimer {
	[ self settingsChanged:self ];
}

- (void)doScaleTimer:(NSTimer*)theTimer {
	[ self scaleChanged ];
}

- (void)doAutoStartTimer:(NSTimer*)theTimer {
	[ self updateAutoStart ];
}

- (NSImage*)retainedHighlightedImageForImage:(NSImage*)image {
	if (!image) return nil;

	NSSize	newSize = [ image size ];
	NSImage	*newImage = [ [NSImage alloc] initWithSize:newSize ];

	[ newImage lockFocus ];
	[ image	drawAtPoint:NSZeroPoint
			fromRect:NSMakeRect(0, 0, newSize.width, newSize.height)
			operation:NSCompositeSourceOver
			fraction:1.0 ];

	[ [[NSColor blueColor] colorWithAlphaComponent: 0.3] set ];
	NSRectFillUsingOperation(NSMakeRect(0, 0, newSize.width, newSize.height), NSCompositeSourceAtop);
	[ newImage unlockFocus ];

	return newImage;
}

#pragma mark -
- (void)forgetTabletInfo {
	modelCT = modelSD = modelPL = modelGD = modelPC = NO;
}

- (void)screenChanged {
	[ presetsController screenChanged ];
}

- (char*)getCStringFromString:(NSString*)string {
	char *outstring;
	int len=0;

	if ([ thePane systemVersion ] < 0x1040) {
		NSData *data = [ string dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES ];
		len = [ data length ];
		outstring = malloc(len + 1);
		[ data getBytes:outstring ];
	}
	else {
		len = [string length];
		outstring = malloc(len + 2);
		if (![ string getCString:outstring maxLength:len+1 encoding:NSASCIIStringEncoding ])
			len = 0;
	}

	outstring[len] = '\0';

	return outstring;
}

@end

