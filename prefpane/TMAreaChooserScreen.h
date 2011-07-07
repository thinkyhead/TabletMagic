/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMAreaChooserScreen.h ($Id: TMAreaChooserScreen.h,v 1.12 2009/02/09 06:00:04 slurslee Exp $)
*/

#import "TMAreaChooser.h"

@interface TMAreaChooserScreen : TMAreaChooser {
	float minX, minY, maxX, maxY;
}

- (void)calibrate:(BOOL)bReset;
- (void)drawSubsections:(NSRect)rect;

@end
