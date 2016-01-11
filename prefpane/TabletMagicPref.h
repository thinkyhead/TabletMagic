/**
 * TabletMagicPref.h
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

#import <PreferencePanes/PreferencePanes.h>

typedef struct {
    NSInteger majorVersion;
    NSInteger minorVersion;
    NSInteger patchVersion;
} TMOperatingSystemVersion;

@interface TabletMagicPref : NSPreferencePane {
    TMOperatingSystemVersion systemVersion;
    BOOL    has_tablet_events;
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

- (TMOperatingSystemVersion*) systemVersion;
- (BOOL) systemVersionAtLeastMajor:(long)maj minor:(long)min;
- (BOOL) systemVersionBeforeMajor:(long)maj minor:(long)min;
- (BOOL) canUseTabletEvents;
- (NSBundle*) bundle;
- (NSString*) localizedString:(NSString*)string;

@end

extern TabletMagicPref *thePane;
