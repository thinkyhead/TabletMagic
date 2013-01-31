/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMAreaChooser.h
*/

enum {
	DRAG_NONE,
	DRAG_TOPLEFT,
	DRAG_TOPRIGHT,
	DRAG_BOTTOMLEFT,
	DRAG_BOTTOMRIGHT,
	DRAG_WHOLE
};

@interface TMAreaChooser : NSView {
	id			controller;
	int			dragType;
	NSPoint		dragStart;
	NSRect		originalFrame;
	float		constrainW, constrainH;
	BOOL		isConstrained;

	float		maxWidth, maxHeight;
	float		scaleFactorW, scaleFactorH;
	float		left, top, right, bottom;

    IBOutlet NSButton *buttonAll;
    IBOutlet NSTextField *textBottom;
    IBOutlet NSTextField *textLeft;
    IBOutlet NSTextField *textRight;
    IBOutlet NSTextField *textSizeHeading;
    IBOutlet NSTextField *textTop;
}

// Actions
- (IBAction)boundsChanged:(id)sender;
- (IBAction)setToMaxSize:(id)sender;
- (IBAction)setToLess:(id)sender;
- (IBAction)setToMore:(id)sender;

// Methods
- (void)constrain:(NSSize)size;
- (void)disableConstrain;

- (void)drawSubsections:(NSRect)rect;

- (void)setAreaWithRect:(NSRect)rect;
- (void)setAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)setAreaFromEditFields;
- (void)setMaxWidth:(unsigned)w height:(unsigned)h;
- (void)updateMaxWidth:(unsigned)w height:(unsigned)h;
- (void)setFromPresetWidth:(float)w height:(float)h left:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)resetToAll;
- (void)presetChanged;

- (unsigned)left;
- (unsigned)top;
- (unsigned)right;
- (unsigned)bottom;
- (unsigned)maxWidth;
- (unsigned)maxHeight;
- (NSSize)representedSize;
- (NSSize)activeArea;
- (BOOL)isConstrained;
- (NSRect)sanityCheckArea:(NSRect)areaRect;
- (NSRect)activeRect;

- (void)updateView:(BOOL)doView andText:(BOOL)doText;
- (void)updateTextFields;

- (NSPoint)repPointFromViewPoint:(NSPoint)viewPoint;
- (NSPoint)viewPointFromRepPoint:(NSPoint)repPoint;

- (void)drawHandlePart:(NSRect)rect asActive:(BOOL)active;


@end
