/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMPresetsController.h ($Id: TMPresetsController.h,v 1.12 2009/02/09 06:00:53 slurslee Exp $)

	This controller connects the Preset model to
	the Mapping view and the main TMController.
*/

@class TMAreaChooserTablet, TMAreaChooserScreen, TMPreset;

@interface TMPresetsController : NSObject {
	int				activePresetIndex;
	NSMutableArray	*presetsArray;
	TMPreset		*activePreset;			// the active preset, unarchived from the dictionary

	IBOutlet NSWindow *prefsWindow;
	IBOutlet NSWindow *sheetAdd;
	IBOutlet NSWindow *sheetDelete;
	IBOutlet NSWindow *sheetRename;

	IBOutlet NSButton *buttonAdd;
	IBOutlet NSButton *buttonDelete;
	IBOutlet NSButton *buttonRename;

	IBOutlet NSTextField *editAdd;
	IBOutlet NSTextField *editRename;
	IBOutlet NSTextField *textDelete;

	IBOutlet NSButton *checkMouseMode;

	IBOutlet NSPopUpButton *popupEraser;
	IBOutlet NSPopUpButton *popupPresets;
	IBOutlet NSPopUpButton *popupStylusTip;
	IBOutlet NSPopUpButton *popupSwitch1;
	IBOutlet NSPopUpButton *popupSwitch2;

	IBOutlet NSSlider *sliderScaling;

	IBOutlet TMAreaChooserTablet *chooserTablet;
	IBOutlet TMAreaChooserScreen *chooserScreen;
    IBOutlet NSButton *buttonConstrain;
}

// Initialization
- (void)mainViewDidLoad;
- (BOOL)apply2b8Patches;	// 2b8 is a minor release to automate the new button mappings

// Methods
- (void)loadPresets;
- (void)activatePresetIndex:(int)i;
- (void)sendPresetToDaemon;
- (void)sendMouseModeToDaemon;
- (void)updatePresetFromControls;
- (void)activatePresetMatching:(char*)geom;
- (void)synchronizePresetInArray;
- (void)geometryReceived:(char*)geom;
- (void)reflectConstrainSetting:(BOOL)b;
- (NSDictionary*)dictionary;

- (void)initControls;
- (void)updatePresetsMenu;
- (void)updateControlsForActivePreset;
- (void)screenChanged;

- (void)setTabletRangeX:(unsigned)x y:(unsigned)y;
- (void)updateTabletScaleX:(unsigned)x y:(unsigned)y;
- (void)setScreenWidth:(unsigned)w height:(unsigned)h;
- (void)setTabletAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)setScreenAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)setMappingForTip:(int)tip side1:(int)s1 side2:(int)s2 eraser:(int)e;
- (void)setMouseMode:(BOOL)enabled andScaling:(float)s;

- (void)addPresetNamed:(NSString*)n;

- (void)setTabletConstraint:(NSSize)size;
- (BOOL)tabletIsConstrained;
- (TMPreset*)activePreset;

// Actions
- (IBAction)selectedPreset:(id)sender;
- (IBAction)addPreset:(id)sender;
- (IBAction)doAddPreset:(id)sender;
- (IBAction)renamePreset:(id)sender;
- (IBAction)doRenamePreset:(id)sender;
- (IBAction)deletePreset:(id)sender;
- (IBAction)doDeletePreset:(id)sender;
- (IBAction)presetChanged:(id)sender;
- (IBAction)toggleConstrain:(id)sender;
- (IBAction)toggleMouseMode:(id)sender;
- (IBAction)cancelSheet:(id)sender;

@end
