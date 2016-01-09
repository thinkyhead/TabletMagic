/**
 * TMScratchpad.h
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

@interface TMScratchpad : NSView {
	BOOL smoothing;
	NSPoint startPoint;
	float startPressure;
	IBOutlet NSColorWell *penColor;
	IBOutlet NSColorWell *eraserColor;
	IBOutlet NSSlider *penFlow;
	IBOutlet NSSlider *eraserFlow;
}

- (IBAction)clear:(id)sender;
- (IBAction)toggleSmoothing:(id)sender;

- (void)handleProximity:(NSNotification *)proxNotice;
- (void)drawBlobAtPoint:(NSPoint)point withPressure:(float)pressure erasing:(BOOL)is_eraser;

- (void)_tabletProximity:(NSEvent *)theEvent;

@end
