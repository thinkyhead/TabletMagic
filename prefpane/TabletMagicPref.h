/**
 * TabletMagicPref.h
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

#import <PreferencePanes/PreferencePanes.h>

@interface TabletMagicPref : NSPreferencePane {
	SInt32	systemVersion;
	BOOL	has_tablet_events;
}

//
// mainViewDidLoad
// awakeFromNIB for Preference Panes - called when the pane is ready
//
- (void) mainViewDidLoad;
- (void) willUnselect;

//
// paneTerminating
// Receives a notification when the pane is quit
//
- (void) paneTerminating:(NSNotification*)aNotification;

//
// screenChanged
// Receives a notification whenever the screen configuration changes
//
- (void) screenChanged:(NSNotification*)aNotification;

- (long) systemVersion;
- (BOOL) canUseTabletEvents;
- (NSBundle*) bundle;
- (NSString*) localizedString:(NSString*)string;

@end

extern TabletMagicPref *thePane;
