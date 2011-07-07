/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMScratchpad.h ($Id: TMScratchpad.h,v 1.12 2009/02/09 06:00:04 slurslee Exp $)
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

@end
