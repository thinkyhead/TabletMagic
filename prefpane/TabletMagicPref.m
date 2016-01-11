/**
 * TabletMagicPref.m
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

#import "TabletMagicPref.h"
#import "TMController.h"

TabletMagicPref *thePane;

@implementation TabletMagicPref

- (void) mainViewDidLoad {
    thePane = self;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_10
    // This replacement is available from 10.10 onward
    NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
    systemVersion.majorVersion = ver.majorVersion;
    systemVersion.minorVersion = ver.minorVersion;
    systemVersion.patchVersion = ver.patchVersion;
#else
    // This method may be available in 10.9
    // or by extending NSProcessInfo with a category
    if ([NSProcessInfo respondsToSelector:@selector(operatingSystemVersion)]) {
        NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
        systemVersion.majorVersion = ver.majorVersion;
        systemVersion.minorVersion = ver.minorVersion;
        systemVersion.patchVersion = ver.patchVersion;
    }
    else {
        // Deprecated since 10.8
        SInt32 val;
        Gestalt(gestaltSystemVersionMajor, &val); systemVersion.majorVersion = val;
        Gestalt(gestaltSystemVersionMinor, &val); systemVersion.minorVersion = val;
        Gestalt(gestaltSystemVersionBugFix, &val); systemVersion.patchVersion = val;
    }
#endif

    has_tablet_events = [ self systemVersionAtLeastMajor:10 minor:3 ];

    // Notify us if the preference pane is closed
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(paneTerminating)
                                                 name:NSApplicationWillTerminateNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(screenChanged)
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

- (TMOperatingSystemVersion*) systemVersion {
    return &systemVersion;
}

- (BOOL) systemVersionAtLeastMajor:(long)maj minor:(long)min {
    return systemVersion.majorVersion >= maj || (systemVersion.majorVersion == maj && systemVersion.minorVersion >= min);
}

- (BOOL) systemVersionBeforeMajor:(long)maj minor:(long)min {
    return systemVersion.majorVersion < maj || (systemVersion.majorVersion == maj && systemVersion.minorVersion < min);
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
