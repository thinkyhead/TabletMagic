/**
 * TMAreaChooserScreen.m
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

#import "TMAreaChooserScreen.h"
#import "TMPresetsController.h"
#import "../common/Constants.h"

@implementation TMAreaChooserScreen

- (void)setAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
    [ super setAreaLeft:l top:t right:r bottom:b ];

    if ([ controller tabletIsConstrained ])
        [ controller setTabletConstraint:[ self activeArea ] ];
}

- (void)drawSubsections:(NSRect)rect {
    NSArray *screenArray = [ NSScreen screens ];
    int i, count = (int)[ screenArray count ];

    if (count > 1) {
        for (i=0; i<count; i++) {
            NSRect frame = [ [ screenArray objectAtIndex:i ] frame ];

            // Normalize screen coordinates
            float   l = frame.origin.x - minX,
            t = frame.origin.y - minY,
            r = l + frame.size.width - 1,
            b = t + frame.size.height - 1;

            // Convert coordinates to the drawing space
            NSPoint lt = { l, t };
            NSPoint rb = { r, b };
            lt = [ self viewPointFromRepPoint:lt ];
            rb = [ self viewPointFromRepPoint:rb ];
            l = floorf(lt.x+0.5f); t = floorf(lt.y+0.5f); r = floorf(rb.x+0.5f); b = floorf(rb.y+0.5f);

            NSRect  inRect = { { l, rect.size.height - b }, { r - l, b - t } };
            [[NSColor colorWithCalibratedRed:0.75 green:0.75 blue:1.0 alpha:1.0] setFill];
            NSFrameRectWithWidth(inRect, 1.0);
        }
    }
}

- (void)calibrate:(BOOL)bReset {
    // Get the bounds of all screens combined
    int i;
    NSRect frame;
    minX = 1e10; minY = 1e10; maxX = 0; maxY = 0;
    NSArray *screenArray = [ NSScreen screens ];
    for (i=0; i<[screenArray count]; i++) {
        frame = [ [ screenArray objectAtIndex:i ] frame ];
        // x = 0 - 1023     y = 768 - 1535      --    top screen    y is bottom-up
        // x = 0 - 1023     y =   0 -  767      -- bottom screen
        float   l = frame.origin.x,
                t = frame.origin.y,
                r = l + frame.size.width - 1,
                b = t + frame.size.height - 1;

        if (l < minX) minX = l;
        if (t < minY) minY = t;
        if (r > maxX) maxX = r;
        if (b > maxY) maxY = b;
    }

    if (bReset) {
        //      NSLog(@"Initializing chooser based on screen %f %f", maxX - minX + 1, maxY - minY + 1 );
        [ self setMaxWidth:maxX - minX + 1 height:maxY - minY + 1 ];
        [ self setAreaLeft:minX top:minY right:maxX bottom:maxY ];
    }
    else {
        //      NSLog(@"Recalibrating chooser based on screen");
        [ self updateMaxWidth:maxX - minX + 1 height:maxY - minY + 1 ];
    }
}

@end
