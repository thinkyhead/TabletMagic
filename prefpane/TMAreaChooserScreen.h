/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMAreaChooserScreen.h
*/

#import "TMAreaChooser.h"

@interface TMAreaChooserScreen : TMAreaChooser {
	float minX, minY, maxX, maxY;
}

- (void)calibrate:(BOOL)bReset;
- (void)drawSubsections:(NSRect)rect;

@end
