/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMController.h
*/

@class TMScratchpad, TMPresetsController;

@interface TMController : NSObject {

	NSDistributedNotificationCenter	*ncenter;
	NSPort				*mPort;
	CFMessagePortRef	cfPortIn;
	CFMessagePortRef	cfPortOut;
	NSString			*bundleID;
    AuthorizationRef	fAuthorization;
	BOOL				tabletEnabled;
	BOOL				autoLaunchEnabled;
	NSTimer				*settingTimer, *scaleTimer, *autoStartTimer;
	NSTimer				*streamTimer, *killTimer, *pingTimer;
	BOOL				modelUD, modelCT, modelSD, modelPL, modelGD, modelPC;
	BOOL				ignore_next_info;
	int					stream_reply_size;
	int					stream_reply_count;
	BOOL				hackintosh;
	BOOL				scale_from_stylus;
	int					current_tab;

	IBOutlet NSTabView	*tabview;

	// Top controls
    IBOutlet NSPopUpButton			*popupSerialPort;
    IBOutlet NSTextField			*textTabletInfo;
    IBOutlet NSTextField			*textVersion;
    IBOutlet NSButton				*checkEnabled;

	// Memory bank controls
    IBOutlet NSMatrix *matrixMem;
    IBOutlet NSButtonCell *cellMem0;
    IBOutlet NSButtonCell *cellMem1;
    IBOutlet NSButtonCell *cellMem2;
    IBOutlet NSButton *buttonSetBank;

	// Serial connection controls
    IBOutlet NSPopUpButton *popupBaud;
    IBOutlet NSPopUpButton *popupDataBits;
    IBOutlet NSPopUpButton *popupParity;
    IBOutlet NSPopUpButton *popupStopBits;
    IBOutlet NSButton *checkCTS;
    IBOutlet NSButton *checkDSR;

	// Settings controls
    IBOutlet NSPopUpButton *popupCommandSet;
    IBOutlet NSPopUpButton *popupOutputFormat;
    IBOutlet NSPopUpButton *popupCoordSys;
    IBOutlet NSPopUpButton *popupTerminator;
    IBOutlet NSPopUpButton *popupOrigin;
    IBOutlet NSPopUpButton *popupReadHeight;
    IBOutlet NSPopUpButton *popupSensitivity;
    IBOutlet NSPopUpButton *popupTransferMode;
    IBOutlet NSPopUpButton *popupTransferRate;
    IBOutlet NSPopUpButton *popupResolution;
    IBOutlet NSButton *checkTilt;
    IBOutlet NSButton *checkMultiMode;
    IBOutlet NSButton *checkPlugPlay;
    IBOutlet NSButton *checkOORData;
    IBOutlet NSTextField *editIncrement;
    IBOutlet NSTextField *editInterval;
    IBOutlet NSTextField *editXRez;
    IBOutlet NSTextField *editYRez;
    IBOutlet NSTextField *editXScale;
    IBOutlet NSTextField *editYScale;
    IBOutlet NSStepper *stepperIncrement;
    IBOutlet NSStepper *stepperInterval;
    IBOutlet NSStepper *stepperXRez;
    IBOutlet NSStepper *stepperYRez;
    IBOutlet NSStepper *stepperXScale;
    IBOutlet NSStepper *stepperYScale;

    IBOutlet NSBox *groupMemory;
    IBOutlet NSBox *groupSerial;
    IBOutlet NSBox *groupProtocol;
    IBOutlet NSBox *groupDigitizer;

	// Data stream and commands
    IBOutlet NSBox *groupDatastream;
    IBOutlet NSTextField *textDatastream;
    IBOutlet NSPopUpButton *popupCommands;
    IBOutlet NSButton *buttonSendCommand;

	// Tablet data rates
    IBOutlet NSTextField *textRateBytes;
    IBOutlet NSTextField *textRatePackets;

	// Tablet data interpreted
    IBOutlet NSTextField *textPosX;
    IBOutlet NSTextField *textPosY;
    IBOutlet NSTextField *textTiltX;
    IBOutlet NSTextField *textTiltY;
    IBOutlet NSTextField *textEvent;
    IBOutlet NSTextField *textPressure;
    IBOutlet NSButton *buttonView1;
    IBOutlet NSButton *buttonView2;
    IBOutlet NSButton *buttonView3;
    IBOutlet NSButton *buttonView4;
    IBOutlet NSProgressIndicator *progPressure;

	// A scratch pad to draw inside
    IBOutlet TMScratchpad *scratchPad;
    IBOutlet NSColorWell *colorPen;
    IBOutlet NSColorWell *colorEraser;
    IBOutlet NSSlider *sliderFlowPen;
    IBOutlet NSSlider *sliderFlowEraser;

	// Geometry controls
    IBOutlet TMPresetsController *presetsController;

    IBOutlet NSPopUpButton *popupStylusTip;
    IBOutlet NSPopUpButton *popupBarrelSwitch1;
    IBOutlet NSPopUpButton *popupBarrelSwitch2;
    IBOutlet NSPopUpButton *popupEraser;

    IBOutlet NSButton *checkMouseMode;
    IBOutlet NSSlider *sliderMouseScaling;

    IBOutlet NSButton *checkDonation;

	// Extras
    IBOutlet NSBox *groupAutoStart;
	IBOutlet NSButton *checkAutoStart;
	IBOutlet NSButton *buttonInk;
	IBOutlet NSButton *buttonKill;
	IBOutlet NSButton *buttonPanic;

	// TabletPC
	IBOutlet NSButton *buttonEnableDigitizer;
	IBOutlet NSButton *checkTabletPC;
	IBOutlet NSButton *checkGetFromStylus;
    IBOutlet NSTextField *textTweakScaleX;
    IBOutlet NSTextField *textTweakScaleY;
}

// Interface Actions
- (IBAction)toggledDaemon:(id)sender;
- (IBAction)selectedSerialPort:(id)sender;
- (IBAction)chooseMemoryBank:(id)sender;
- (IBAction)activateMem:(id)sender;
- (IBAction)settingsChanged:(id)sender;
- (IBAction)serialSettingsChanged:(id)sender;
- (IBAction)toggledTiltOrMM:(id)sender;
- (IBAction)actionIncrement:(id)sender;
- (IBAction)actionInterval:(id)sender;
- (IBAction)actionRezX:(id)sender;
- (IBAction)actionRezY:(id)sender;
- (IBAction)actionScaleX:(id)sender;
- (IBAction)actionScaleY:(id)sender;

// Testing Actions
- (IBAction)sendSelectedCommand:(id)sender;

// Extras Actions
- (IBAction)toggleAutoStart:(id)sender;
- (IBAction)enableInk:(id)sender;
- (IBAction)killTheDaemon:(id)sender;
- (IBAction)panicButton:(id)sender;

// TabletPC Actions
- (BOOL)detectTabletPC;
- (IBAction)enableDigitizer:(id)sender;
- (IBAction)toggledTabletPC:(id)sender;
- (IBAction)toggledGetFromStylus:(id)sender;
- (IBAction)tweakScaleChanged:(id)sender;
- (void)expandScaleX:(int)x y:(int)y;
- (void)handleEnablerResponse:(char *)reply;
- (void)hackDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void)failDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

// Convenience methods
- (BOOL)setValueOfControl:(id)control toInt:(int)value;
- (void)setText:(id)control andStepper:(id)control2 toInt:(int)value;
- (NSString*)getStartupArguments:(BOOL)daemonize;

// Incoming Messages

#if ASYNCHRONOUS_MESSAGES
- (CFDataRef)handleMessageFromPort:(CFMessagePortRef)loc withData:(CFDataRef)data andInfo:(void*)info;
#endif

- (void)handleDaemonMessageData:(CFDataRef)data;
- (void)handleDaemonMessage:(char*)message;

// Outgoing Messages
- (CFMessagePortRef)getRemoteMessagePort;
- (BOOL)remotePortExists;
- (void)disposeRemoteMessagePort;
- (void)remoteMessagePortWentAway;
- (void)sendNSStringToDaemon:(NSString*)string;
- (void)sendMessageToDaemon:(char*)message;
- (CFDataRef)sendRequestToDaemon:(char*)message;

// A timer to ping the daemon every 1/2 second
- (void)doPingTimer:(NSTimer*)theTimer;

// Stream timer
- (void)startStreamIfShowing;
- (void)setStreamMonitorEnabled:(BOOL)enable;
- (void)doStreamTimer:(NSTimer*)theTimer;

// Superclass overrides
- (void)mainViewDidLoad;
- (void)paneTerminating;
- (void) paneDidSelect;
- (void)paneWillUnselect;

// Preferences
- (void)loadPreferences;
- (void)savePreferences;

// Authorization
- (void)setupAuthorization;
- (void)cleanUpAuthorization;
- (BOOL)isFileSuidRoot:(char*)fullpath;

// Daemon control
- (BOOL)startDaemon;
- (BOOL)killDaemon;
- (BOOL)isDaemonLoaded:(BOOL)refresh;
- (OSStatus)updateAutoStart;
- (void)updateAutoStartSoon;
- (void)doAutoStartTimer:(NSTimer*)theTimer;
- (char*)runLaunchHelper:(NSString*)argsString;

// Controls
- (void)requestCurrentSettings;
- (void)setControlsForSettings:(char*)settings;
- (void)setStreamHeadingForSet:(int)set andFormat:(int)fmt;
- (void)setControlsForCommandSet;
- (void)setControlsForConnect:(BOOL)connected;
- (void)updatePortPopup;
- (void)updateAutoStartCheckbox;
- (NSArray *)getSerialPorts;
- (void)selectPortByName:(NSString*)portName;

// Donate Tab Methods
- (IBAction)donate:(id)sender;
- (IBAction)visit:(id)sender;

// A timer to delay settings for 1/2 second
- (void)doSettingsTimer:(NSTimer*)theTimer;
- (void)doScaleTimer:(NSTimer*)theTimer;

- (void)forgetTabletInfo;

- (void)scaleChanged;
- (void)screenChanged;

- (NSImage*)retainedHighlightedImageForImage:(NSImage*)image;

- (void)killDialogEnded:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (char*)getCStringFromString:(NSString*)string;

@end

extern TMController *theController;
