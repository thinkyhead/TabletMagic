/**
 * TMPresetsController.m
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 *
 * This controller connects the Preset model to
 * the Mapping view and the main TMController.
 *
 * - If presets exist load them into a local array
 *
 * - When [geom] arrives, either select the corresponding preset
 *   or create/update the Custom preset
 *
 *   NOTE - [geom] only arrives if the daemon is already
 *   running when the preference pane starts.
 *
 * - When a preset changes automatically save it
 *
 * - The custom preset can be renamed
 *
 * - The last preset can't be deleted. If you delete the last preset
 *   it just gets renamed "Custom"
 */

#import "TMPreset.h"
#import "TMPresetsController.h"
#import "TMController.h"
#import "TMAreaChooserScreen.h"
#import "TMAreaChooserTablet.h"
#import "../common/Constants.h"

#import "TabletMagicPref.h"

#define kCustom @"Custom"

@implementation TMPresetsController

//
// init is called before the mainViewDidLoad of the controller
// so here we create the basic presets items
//
- (id)init {
    if ((self = [super init])) {
        activePresetIndex = 0;
        activePreset = nil;
        presetsArray = nil;
    }

    return self;
}

- (void)mainViewDidLoad {
    [ self loadPresets ];       // if preferences save on init this must be in init above
    [ self initControls ];      // the view has to be loaded before this works
    [ self updatePresetsMenu ];
}

- (BOOL)apply2b8Patches {
    BOOL didUpdates = YES;

    NSDictionary *prefs = [ [NSUserDefaults standardUserDefaults] persistentDomainForName:[[thePane bundle] bundleIdentifier] ];
    if (prefs != nil)
        didUpdates = ([ prefs objectForKey:keyDidFixButtons ] != nil);

    if (!didUpdates) {
        int b;
        for (int i=(int)[presetsArray count]; i--;) {
            TMPreset *preset = [ [ TMPreset alloc ] initWithDictionary:[presetsArray objectAtIndex:i] ];
            if ((b = [ preset buttonEraser ]) >= kSystemButton3)    [ preset setButtonEraser:b+3 ];
            if ((b = [ preset buttonTip ]) >= kSystemButton3)       [ preset setButtonTip:b+3 ];
            if ((b = [ preset buttonSwitch1 ]) >= kSystemButton3)   [ preset setButtonSwitch1:b+3 ];
            if ((b = [ preset buttonSwitch2 ]) >= kSystemButton3)   [ preset setButtonSwitch2:b+3 ];
            [ presetsArray replaceObjectAtIndex:i withObject:[preset dictionary] ];
        }

        [ self activatePresetIndex:-1 ];
    }

    return !didUpdates;
}

- (void)loadPresets {
    if (presetsArray == nil)
        presetsArray = [ [ NSMutableArray alloc ] initWithCapacity:10 ];

    if (activePreset == nil)
        activePreset = [ [ TMPreset alloc ] init ];

    activePresetIndex = 0;

    NSDictionary *prefs = [ [NSUserDefaults standardUserDefaults] persistentDomainForName:[[thePane bundle] bundleIdentifier] ];

    if (prefs != nil) {
#if !ARC_ENABLED
        [ prefs retain ];
#endif

        NSDictionary *dict = [ prefs objectForKey:keyPresets ];
        if (dict) {
            NSArray *anArray = [dict objectForKey:keyPresetList];

            if (anArray) {
                [ presetsArray setArray:[dict objectForKey:keyPresetList] ];

                activePresetIndex = [ [dict objectForKey:keySelectedPreset] intValue ];
            }
        }

#if !ARC_ENABLED
        [ prefs release ];
#endif
    }

    if (!presetsArray || [presetsArray count] == 0) {
        [ presetsArray addObject:[activePreset dictionary] ];
    }

    [ self activatePresetIndex:activePresetIndex ];
}

- (void)activatePresetIndex:(int)i {
    if (i < (int)[ presetsArray count ]) {
        (void)[ activePreset initWithDictionary:[presetsArray objectAtIndex:(i < 0) ? activePresetIndex : i] ];
        [ self updateControlsForActivePreset ];
    }
}

- (void)sendPresetToDaemon {
    [ theController sendMessageToDaemon:[ activePreset daemonMessage ] ];
}

- (void)sendMouseModeToDaemon {
    [ theController sendMessageToDaemon:[ activePreset mouseModeMessage ] ];
}

- (void)updatePresetsMenu {
    int count = (int)[presetsArray count];
    [ popupPresets removeAllItems ];

    if (count) {
        int i, uid;
        for (i=0; i<count; i++) {
            uid = 0;
            NSString *base_title = [[presetsArray objectAtIndex:i] objectForKey:@keyName];
            NSMutableString *title = [ NSMutableString stringWithString:base_title ];

            while ( [ popupPresets itemWithTitle:title ] && ++uid )
                [ title setString:[NSString stringWithFormat:@"%@ %d",base_title,uid] ];

            [ popupPresets addItemWithTitle:title ];
        }

        [ popupPresets selectItemAtIndex:activePresetIndex ];
        [ popupPresets setEnabled:YES ];
        [ buttonDelete setEnabled:(count>1) ];
        [ buttonRename setEnabled:YES ];
    }
    else {
        [ popupPresets setEnabled:NO ];
        [ buttonDelete setEnabled:NO ];
        [ buttonRename setEnabled:NO ];
    }
}

- (void)activatePresetMatching:(char*)geom {
    int     tl, tt, tr, tb, sl, st, sr, sb, b0, b1, b2, be, mm;
    float   ms;

    int         i, foundIndex = -1;

    TMPreset    *p = [[TMPreset alloc] init];

    if (14 == sscanf(geom, "%*s %u %d %d %d : %d %d %d %d : %d %d %d %d : %d %f",
                     &tl, &tt, &tr, &tb, &sl, &st, &sr, &sb, &b0, &b1, &b2, &be, &mm, &ms)) {
        // Try to find a matching preset
        for (i=0;i<[presetsArray count];i++) {
            (void)[p initWithDictionary:[presetsArray objectAtIndex:i]];

            if (    [p matchesTabletLeft:tl top:tt right:tr bottom:tb]
                &&  [p matchesScreenLeft:sl top:st right:sr bottom:sb]
                &&  [p matchesButtonTip:b0 switch1:b1 switch2:b2 eraser:be]
                &&  [p matchesMouseMode:mm scaling:ms]) {
                foundIndex = i;
                break;
            }
        }

        // If none was found, check for the name "Custom"
        if (foundIndex == -1) {
            for (i=0;i<[presetsArray count];i++) {
                if ( NSOrderedSame == [[[presetsArray objectAtIndex:i] objectForKey:@keyName] compare:[thePane localizedString:kCustom] options:NSCaseInsensitiveSearch] ) {
                    foundIndex = i;
                    break;
                }
            }
        }

        if (foundIndex == -1) {
            [ self addPresetNamed:[thePane localizedString:kCustom] ];
            foundIndex = (int)[presetsArray count] - 1;
        }

        if (foundIndex != activePresetIndex) {
            [ popupPresets selectItemAtIndex:foundIndex ];
            [ self activatePresetIndex:foundIndex ];
        }
    }

#if !ARC_ENABLED
    [ p release ];
#endif
}

- (void)updateControlsForActivePreset {
    TMPreset *p = activePreset;

    [ buttonConstrain setState:[p constrained] ];
    [ self reflectConstrainSetting:[p constrained] ];

    [ checkMouseMode setState:[p mouseMode] ];
    [ sliderScaling setFloatValue:[p mouseScaling] ];

    [ chooserScreen setFromPresetWidth:[p screenWidth] height:[p screenHeight] left:[p screenLeft] top:[p screenTop] right:[p screenRight] bottom:[p screenBottom] ];
    [ chooserTablet setFromPresetWidth:[p tabletRangeX] height:[p tabletRangeY] left:[p tabletLeft] top:[p tabletTop] right:[p tabletRight] bottom:[p tabletBottom] ];

    [ popupStylusTip selectItemWithTag:[p buttonTip] ];
    [ popupSwitch1 selectItemWithTag:[p buttonSwitch1] ];
    [ popupSwitch2 selectItemWithTag:[p buttonSwitch2] ];
    [ popupEraser selectItemWithTag:[p buttonEraser] ];
}

- (void)geometryReceived:(char*)geom {
    int     tl, tt, tr, tb, sl, st, sr, sb, b0, b1, b2, be, mm;
    float   ms;
    if (14 == sscanf(geom, "%*s %u %d %d %d : %d %d %d %d : %d %d %d %d : %d %f",
                     &tl, &tt, &tr, &tb, &sl, &st, &sr, &sb, &b0, &b1, &b2, &be, &mm, &ms)) {
        [ self activatePresetMatching:geom ];

        [ activePreset setConstrained:NO ];

        if (tr > tl && tb > tt)
            [ self setTabletAreaLeft:tl top:tt right:tr bottom:tb ];

        [ self setScreenWidth:[ chooserScreen maxWidth ] height:[ chooserScreen maxHeight ] ];
        [ self setScreenAreaLeft:sl top:st right:sr bottom:sb ];
        [ self setMappingForTip:b0 side1:b1 side2:b2 eraser:be ];
        [ self setMouseMode:mm andScaling:ms ];

        if (tr <= tl || tb <= tt)
            [ self sendPresetToDaemon ];

        [ self synchronizePresetInArray ];
        [ self updateControlsForActivePreset ];
    }
}

- (void)initControls {
    //NSString        *imagePath = [[thePane bundle] pathForImageResource:@"button.jpg" ];
    //NSImage         *img1 = [[NSImage alloc] initWithContentsOfFile:imagePath];
    //NSImage         *img2 = [ theController retainedHighlightedImageForImage:img1 ];

    NSButtonCell    *cell = [buttonConstrain cell];
    //[cell setImage:img1];
    //[cell setAlternateImage:img2];
    [cell setShowsStateBy:NSChangeBackgroundCellMask];
    [cell setHighlightsBy:NSPushInCellMask];
    //[ buttonConstrain setTitle:[ thePane localizedString:@"< constrain <" ] ];

    //
    // Set the initial dimensions for the Screen Chooser
    //
    [ chooserScreen calibrate:YES ];

    //
    // At this point in the initialization the tablet
    // probably hasn't even been queried so use a sane
    // default. Soon we'll load the preset. Once a tablet
    // is found TMController will read its current
    // settings and update the w/h in the control for the
    // tablet's max coordinates.
    // (important to make sure the order remains the same)
    //
    [ chooserTablet setMaxWidth:k12inches1270ppi height:k12inches1270ppi ];
    [ chooserTablet setAreaLeft:0 top:0 right:k12inches1270ppi-1 bottom:k12inches1270ppi-1 ];

    // Update controls based on the preset
    [ self updateControlsForActivePreset ];
}


/*
 When the screen changes recalculate the overall area
 and the current values will be set proportional to
 the original.
 */
- (void)screenChanged {
    // Recalibrate the Screen Chooser (but don't reset)
    [ chooserScreen calibrate:NO ];
}

/*
 For one reason or another the tablet scale can
 change intermittently.
 */
- (void)updateTabletScaleX:(unsigned)x y:(unsigned)y {
    [ chooserTablet updateMaxWidth:x height:y ];
}


#pragma mark -

- (void)setTabletRangeX:(unsigned)x y:(unsigned)y {
    [ activePreset setTabletRangeX:x y:y ];
}

- (void)setScreenWidth:(unsigned)w height:(unsigned)h {
    [ activePreset setScreenWidth:w height:h ];
}

- (void)setTabletAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
    [ activePreset setTabletAreaLeft:l top:t right:r bottom:b ];
}

- (void)setScreenAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
    [ activePreset setScreenAreaLeft:l top:t right:r bottom:b ];
}

- (void)setMappingForTip:(int)tip side1:(int)s1 side2:(int)s2 eraser:(int)e {
    [ activePreset setMappingForTip:tip side1:s1 side2:s2 eraser:e ];
}

- (void)setMouseMode:(BOOL)enabled andScaling:(float)s {
    [ activePreset setMouseMode:enabled ];
    [ activePreset setMouseScaling:s ];
}

- (void)setTabletConstraint:(NSSize)size {
    [ chooserTablet constrain:size ];
}

- (void)updatePresetFromControls {
    [ activePreset setScreenWidth:[chooserScreen maxWidth] height:[chooserScreen maxHeight] ];
    [ activePreset setTabletRangeX:[chooserTablet maxWidth] y:[chooserTablet maxHeight] ];
    [ activePreset setScreenAreaLeft:[chooserScreen left] top:[chooserScreen top] right:[chooserScreen right] bottom:[chooserScreen bottom] ];
    [ activePreset setTabletAreaLeft:[chooserTablet left] top:[chooserTablet top] right:[chooserTablet right] bottom:[chooserTablet bottom] ];
    [ activePreset setMappingForTip:(int)[[popupStylusTip selectedItem] tag]
                              side1:(int)[[popupSwitch1 selectedItem] tag]
                              side2:(int)[[popupSwitch2 selectedItem] tag]
                             eraser:(int)[[popupEraser selectedItem] tag] ];

    [ activePreset setMouseMode:[checkMouseMode state]==NSOnState ];
    [ activePreset setMouseScaling:[sliderScaling floatValue] ];
    [ activePreset setConstrained:[buttonConstrain state]==NSOnState ];

    [ self synchronizePresetInArray ];
    [ theController updateAutoStartSoon ];
}

- (void)reflectConstrainSetting:(BOOL)b {
    if (b)
        [ chooserTablet constrain:[ chooserScreen activeArea ] ];
    else
        [ chooserTablet disableConstrain ];
}

- (void)synchronizePresetInArray {
    [ presetsArray replaceObjectAtIndex:activePresetIndex withObject:[activePreset dictionary] ];
}

#pragma mark - Actions

- (IBAction)selectedPreset:(id)sender {
    activePresetIndex = (int)[sender indexOfSelectedItem];
    [ self activatePresetIndex:activePresetIndex ];
    [ self sendPresetToDaemon ];
}

- (IBAction)addPreset:(id)sender {
    [ editAdd setStringValue:[ thePane localizedString:@"Untitled Preset" ] ];
    [ editAdd selectText:self  ];

    [ NSApp beginSheet:sheetAdd
        modalForWindow:[[thePane mainView] window]
         modalDelegate:self
        didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
           contextInfo:nil
     ];
}

- (IBAction)deletePreset:(id)sender {
    [ textDelete setStringValue:
     [ NSString stringWithFormat:
      [ thePane localizedString:@"Are you sure you want to delete the preset \"%@\" ?" ],
      [activePreset name]
      ]
     ];

    [ NSApp beginSheet:sheetDelete
        modalForWindow:[[thePane mainView] window]
         modalDelegate:self
        didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
           contextInfo:nil
     ];
}

- (IBAction)renamePreset:(id)sender {
    [ editRename setStringValue:[activePreset name] ];
    [ editRename selectText:self  ];

    [ NSApp beginSheet:sheetRename
        modalForWindow:[[thePane mainView] window]
         modalDelegate:self
        didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
           contextInfo:nil
     ];
}

- (IBAction)cancelSheet:(id)sender {
    [ NSApp endSheet:[ sender window ] ];
}

- (IBAction)doAddPreset:(id)sender {
    if (![[editAdd stringValue] isEqualToString:@""]) {
        [ NSApp endSheet:[ sender window ] ];
        [ self addPresetNamed:[editAdd stringValue] ];
    }
}

-(void)addPresetNamed:(NSString*)n {
    TMPreset *newPreset = [[TMPreset alloc] init];
    (void)[ newPreset initWithDictionary:[activePreset dictionary] ];
    [ newPreset setName:n ];
    [ presetsArray addObject:[newPreset dictionary] ];
#if !ARC_ENABLED
    [ newPreset release ];
#endif
    activePresetIndex = (int)[ presetsArray count ] - 1;
    (void)[ activePreset initWithDictionary:[presetsArray objectAtIndex:activePresetIndex] ];
    [ self updatePresetsMenu ];
}

- (IBAction)doDeletePreset:(id)sender {
    [ NSApp endSheet:[ sender window ] ];

    [ presetsArray removeObjectAtIndex:activePresetIndex ];

    if (activePresetIndex > 0 && activePresetIndex >= [ presetsArray count ])
        activePresetIndex--;

    [ self updatePresetsMenu ];
    [ self activatePresetIndex:activePresetIndex ];
    [ self sendPresetToDaemon ];
}

- (IBAction)doRenamePreset:(id)sender {
    if (![[editRename stringValue] isEqualToString:@""]) {
        [ NSApp endSheet:[ sender window ] ];
        [ activePreset setName:[editRename stringValue] ];
        [ presetsArray replaceObjectAtIndex:activePresetIndex withObject:[activePreset dictionary] ];
        [ self updatePresetsMenu ];
    }
}

- (void)didEndSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
    [ sheet orderOut:self ];
}

- (IBAction)presetChanged:(id)sender {
    [ self updatePresetFromControls ];

    if ([ theController isDaemonLoaded:NO ])
        [ self sendPresetToDaemon ];
}

- (IBAction)toggleConstrain:(id)sender {
    [ self reflectConstrainSetting:[buttonConstrain state]==NSOnState ];
    [ self updatePresetFromControls ];
}

- (IBAction)toggleMouseMode:(id)sender {
    [ activePreset setMouseMode:[sender state]==NSOnState ];
    [ self sendMouseModeToDaemon ];
    [ self synchronizePresetInArray ];
    [ theController updateAutoStartSoon ];
}

- (IBAction)mouseScalingChanged:(id)sender {
    [ activePreset setMouseScaling:[sender floatValue] ];
    [ self sendMouseModeToDaemon ];
    [ self synchronizePresetInArray ];
    [ theController updateAutoStartSoon ];
}

#pragma mark -

- (TMPreset*)activePreset {
    return activePreset;
}

- (NSDictionary*)dictionary {
    static NSDictionary *dict = NULL;

    if (dict) {
#if !ARC_ENABLED
        [ dict release ];
#else
        dict = nil;
#endif
    }

    NSArray *keys   = [NSArray arrayWithObjects:keyPresetList, keySelectedPreset, nil];
    NSArray *values = [NSArray arrayWithObjects:presetsArray, NSINT(activePresetIndex), nil];
    dict = [[NSDictionary alloc] initWithObjects:values forKeys:keys];

    return dict;
}

- (BOOL)tabletIsConstrained { return [ chooserTablet isConstrained ]; }

@end
