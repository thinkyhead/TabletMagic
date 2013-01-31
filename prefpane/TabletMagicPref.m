/*
	TabletMagicPrefPane
	Thinkyhead Software

	TabletMagicPrefPanePref.m
*/

#import "TabletMagicPref.h"
#import "TMController.h"

TabletMagicPref *thePane;

@implementation TabletMagicPref

- (void) mainViewDidLoad {
	thePane = self;

	Gestalt(gestaltSystemVersion, &systemVersion);
	has_tablet_events = (systemVersion >= 0x1030);

	// Notify us if the preference pane is closed
	[[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(paneTerminating:)
            name:NSApplicationWillTerminateNotification
            object:nil];

	[[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(screenChanged:)
            name:NSApplicationDidChangeScreenParametersNotification
            object:nil];

	[ theController mainViewDidLoad ];
}

- (void) willUnselect {
	[ theController paneWillUnselect ];
}

- (void) didSelect {
	[ theController paneDidSelect ];
}

- (void) paneTerminating:(NSNotification*)aNotification {
	[ theController paneTerminating ];
}

- (void) screenChanged:(NSNotification*)aNotification {
	[ theController screenChanged ];
}

- (long) systemVersion {
	return systemVersion;
}

- (BOOL) canUseTabletEvents {
	return has_tablet_events;
}

- (NSBundle*) bundle {
	return [NSBundle bundleForClass:[self class]];
}

- (NSString*) localizedString:(NSString*)string {
	return NSLocalizedStringFromTableInBundle(string, nil, [self bundle], @"Convenience method");
}

@end
