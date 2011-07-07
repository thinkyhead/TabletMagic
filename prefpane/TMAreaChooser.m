/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMAreaChooser.m ($Id: TMAreaChooser.m,v 1.12 2009/02/09 06:00:04 slurslee Exp $)
*/

#import "TMAreaChooser.h"
#import "TMPresetsController.h"
#import "TabletMagicPref.h"
#import "Constants.h"

#define HandleSize	4
#define HSize2		(HandleSize*2+1)


@implementation TMAreaChooser

//
// Language Notes:
//
// initWithFrame as an initializer returns an instance of TMAreaChooser.
// Note that it doesn't call alloc. Presumably the root class calls the
// alloc method and initializers just call their superclass counterpart.
//

- (id)initWithFrame:(NSRect)frameRect {
	if ((self = [super initWithFrame:frameRect]) != nil) {
		// Add initialization code here
		originalFrame = [ self frame ];
		isConstrained = NO;
		constrainW = constrainH = 1.0f;
	}
	return self;
}

#pragma mark -
- (void)constrain:(NSSize)size {
	if (size.width < size.height) {
		constrainW = size.width / size.height;
		constrainH = 1.0f;
	}
	else {
		constrainW = 1.0;
		constrainH = size.height / size.width;
	}

	isConstrained = YES;
	[ self setAreaLeft:left top:top right:right bottom:bottom ];
	[ buttonAll setTitle:[ thePane localizedString:@"max" ] ];
}

- (void)disableConstrain {
	isConstrained = NO;
	constrainW = constrainH = 1.0;
	[ self setNeedsDisplay:YES ];
	[ buttonAll setTitle:[ thePane localizedString:@"all" ] ];
}

- (void)setAreaWithRect:(NSRect)rect {
	[ self setAreaLeft:rect.origin.x top:rect.origin.y right:rect.origin.x+rect.size.width bottom:rect.origin.y+rect.size.height ];
}

// Set the area with spatial coordinates
- (void)setAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
//	NSLog(@"setAreaLeft %f %f %f %f", l, t, r, b);

	NSRect newRect = [ self sanityCheckArea:NSMakeRect(l, t, r - l, b - t) ];
	l = newRect.origin.x;
	t = newRect.origin.y;
	r = newRect.origin.x + newRect.size.width;
	b = newRect.origin.y + newRect.size.height;

	if (isConstrained) {
		float w = r - l, h = b - t;
		if (constrainH == 1.0f)
			w = h * constrainW;
		else
			h = w * constrainH;

		switch(dragType) {
			case DRAG_TOPLEFT:
				l = r - w;
				t = b - h;
				break;
			case DRAG_TOPRIGHT:
				r = l + w;
				t = b - h;
				break;
			case DRAG_BOTTOMLEFT:
				l = r - w;
				b = t + h;
				break;
			case DRAG_BOTTOMRIGHT:
				r = l + w;
				b = t + h;
				break;
			default: {
				float nl = (l + r - w) / 2.0;
				float nt = (t + b - h) / 2.0;
				float nr = (l + r + w) / 2.0;
				float nb = (t + b + h) / 2.0;
				l = nl;
				t = nt;
				r = nr;
				b = nb;
			}
		}
	}

	newRect = [ self sanityCheckArea:NSMakeRect(l, t, r - l, b - t) ];
	left = newRect.origin.x;
	top = newRect.origin.y;
	right = left + newRect.size.width;
	bottom = top + newRect.size.height;

	[ self updateView:YES andText:YES ];
}

- (void)setAreaFromEditFields {
	float l = [ textLeft floatValue ];
	float t = [ textTop floatValue ];
	float r = [ textRight floatValue ];
	float b = [ textBottom floatValue ];

	if (l < 0.0f || l >= maxWidth)	l = left;
	if (t < 0.0f || t >= maxHeight)	t = top;
	if (r < 0.0f || r >= maxWidth)	r = right;
	if (b < 0.0f || b >= maxHeight)	b = bottom;

	if (floorf(l) != floorf(left) || floorf(t) != floorf(top) || floorf(r) != floorf(right) || floorf(b) != floorf(bottom)) {
		[ self setAreaLeft:l top:t right:r bottom:b ];
		[ self presetChanged ];
	}
}

//
// setMaxWidth:height:
// Set the width and height represented by this control
//
// If the width and height are the same then the whole control
//	is used as the active region. Otherwise a proportional area
//	of the control is made active.
//
- (void)setMaxWidth:(unsigned)w height:(unsigned)h; {
	[ textSizeHeading setStringValue:[ NSString stringWithFormat:@"%d x %d", w, h ] ];

	maxWidth = (float)w;
	maxHeight = (float)h;

	float ratioW, ratioH;

	if (maxWidth > maxHeight) {
		ratioW = 1.0;
		ratioH = maxHeight / maxWidth;
	} else if (maxWidth < maxHeight) {
		ratioW = maxWidth / maxHeight;
		ratioH = 1.0;
	}
	else
		ratioW = ratioH = 1.0;

	// The active width and height in NSView units
	float activeWidth = floorf(originalFrame.size.width * ratioW + 0.5f);
	float activeHeight = floorf(originalFrame.size.height * ratioH + 0.5f);

	// Number of units represented by each pixel
	// These will tend to be very close due to the proportional stuff
	scaleFactorW = maxWidth / activeWidth;
	scaleFactorH = maxHeight / activeHeight;

	if ([ thePane systemVersion ] >= 0x1030)
		[ self setHidden:YES ];

	// Make the control size match the proportions
	[ self setFrame:NSMakeRect(
		originalFrame.origin.x + (originalFrame.size.width - activeWidth) / 2.0f,
		originalFrame.origin.y + (originalFrame.size.height - activeHeight) / 2.0f,
		activeWidth, activeHeight) ];

	if ([ thePane systemVersion ] >= 0x1030)
		[ self setHidden:NO ];

	[ self setNeedsDisplay:YES ];
}


/*
	When the resolution changes the maxWidth and height
	need to be updated, but also the 
*/
- (void)updateMaxWidth:(unsigned)w height:(unsigned)h {
//	NSLog(@"updateMaxWidth %f %f", w, h);

	float	divW = (float)w / maxWidth,
			divH = (float)h / maxHeight;

	if (divW != 1.0 || divH != 1.0) {
		[ self setMaxWidth:w height:h ];
		[ self setAreaLeft:left*divW top:top*divH right:right*divW bottom:bottom*divH ];
	}

}


/*
	When you set this control from a preset, the preset data
	may be based on a different max width and height. So use
	this to make sure the values are properly translated from
	the original coordinate system to the current one.
*/
- (void)setFromPresetWidth:(float)w height:(float)h left:(float)l top:(float)t right:(float)r bottom:(float)b {
	float	divW = maxWidth / w,
			divH = maxHeight / h;

//	NSLog(@"setFromPresetWidth (%f %f vs %f %f) : %f %f %f %f", w, h, maxWidth, maxHeight, l, t, r, b);

	[ self setAreaLeft:l*divW top:t*divH right:r*divW bottom:b*divH ];
}

- (void)resetToAll {
	float l = 0, t = 0, r = maxWidth - 1, b = maxHeight - 1;

	if (isConstrained) {
		if (constrainW == 1.0) {
			float m = top + bottom;
			float q = maxWidth * constrainH;
			t = (m - q) / 2.0f;
			b = (m + q) / 2.0f;
		}
		else {
			float m = left + right;
			float q = maxHeight * constrainW;
			l = (m - q) / 2.0f;
			r = (m + q) / 2.0f;
		}
	}

	[ self setAreaLeft:l top:t right:r bottom:b ];
}

- (void)presetChanged {
	[ (TMPresetsController*)controller presetChanged:self ];
}

#pragma mark -
#pragma mark Class Overrides

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent { return YES; }

- (void)drawSubsections:(NSRect)rect {}

//
// drawRect:rect
// The draw method.
//
- (void)drawRect:(NSRect)rect {
	[ NSColor setIgnoresAlpha:NO ];

	setFillColor([NSColor controlBackgroundColor]);
	NSRectFill(rect);

	NSPoint lt = { left, top };
	NSPoint rb = { right, bottom };
	lt = [ self viewPointFromRepPoint:lt ];
	rb = [ self viewPointFromRepPoint:rb ];
	float l = floorf(lt.x+0.5f), t = floorf(lt.y+0.5f), r = floorf(rb.x+0.5f), b = floorf(rb.y+0.5f);

	NSRect	innerRect = { { l, rect.size.height - b }, { r - l, b - t } };
	NSRect	tlRect = { { l - HandleSize, rect.size.height - t - HandleSize }, { HSize2, HSize2 } };
	NSRect	trRect = { { r - HandleSize - 1, rect.size.height - t - HandleSize }, { HSize2, HSize2 } };
	NSRect	blRect = { { l - HandleSize, rect.size.height - b - HandleSize }, { HSize2, HSize2 } };
	NSRect	brRect = { { r - HandleSize - 1, rect.size.height - b - HandleSize }, { HSize2, HSize2 } };

	if (dragType == DRAG_WHOLE)
		setFillColor([NSColor colorWithCalibratedRed:0.92 green:0.92 blue:0.98 alpha:1.0]);
	else
		setFillColor([NSColor colorWithCalibratedRed:0.96 green:0.96 blue:0.99 alpha:1.0]);

	NSRectFill(innerRect);

	setFillColor([NSColor colorWithCalibratedRed:0.8 green:0.8 blue:0.90 alpha:1.0]);
	NSFrameRect(innerRect);

	[ self drawHandlePart:tlRect asActive:dragType == DRAG_TOPLEFT || dragType == DRAG_WHOLE ];
	[ self drawHandlePart:trRect asActive:dragType == DRAG_TOPRIGHT || dragType == DRAG_WHOLE ];
	[ self drawHandlePart:blRect asActive:dragType == DRAG_BOTTOMLEFT || dragType == DRAG_WHOLE ];
	[ self drawHandlePart:brRect asActive:dragType == DRAG_BOTTOMRIGHT || dragType == DRAG_WHOLE ];

	[ self drawSubsections:rect ];

	setFillColor([NSColor blackColor]);
	NSFrameRectWithWidth(rect, 1.0f);
}

- (void)mouseDown:(NSEvent *)theEvent {
	NSPoint lpos = [ self convertPoint:[ theEvent locationInWindow ] fromView:nil ];

	// Convert the view point to its represented point
	NSRect bounds = [ self bounds ];
	lpos.y = bounds.size.height - lpos.y;
	NSPoint realpos = [ self repPointFromViewPoint:lpos ];

	int c = DRAG_NONE;

	float hw = HandleSize * scaleFactorW, hh = HandleSize * scaleFactorH;

	if (realpos.x > left-hw && realpos.x <= left+hw) {
		if (realpos.y > top-hh && realpos.y <= top+hh)
			c = DRAG_TOPLEFT;
		else if (realpos.y > bottom-hh && realpos.y <= bottom+hh)
			c = DRAG_BOTTOMLEFT;
	}
	else if (realpos.x > right-hw && realpos.x <= right+hw) {
		if (realpos.y > top-hh && realpos.y <= top+hh)
			c = DRAG_TOPRIGHT;
		else if (realpos.y > bottom-hh && realpos.y <= bottom+hh)
			c = DRAG_BOTTOMRIGHT;
	}

	if (c == DRAG_NONE && realpos.x > left && realpos.x < right && realpos.y > top && realpos.y < bottom)
		c = DRAG_WHOLE;

	dragStart = realpos;
	dragType = c;

	[ self updateView:YES andText:YES ];

//	return (dragType != DRAG_NONE);  
}

- (void)mouseDragged:(NSEvent *)theEvent {
	if (dragType == DRAG_NONE)
		return;

	NSPoint lpos = [ self convertPoint:[ theEvent locationInWindow ] fromView:nil ];

	// Convert the view point to its represented point
	NSRect bounds = [ self bounds ];
	lpos.y = bounds.size.height - lpos.y;
	NSPoint realpos = [ self repPointFromViewPoint:lpos ];

	float dx = realpos.x - dragStart.x, dy = realpos.y - dragStart.y;
	dragStart = realpos;

	float l = left, t = top, r = right, b = bottom;

	//
	// Dragging the whole box
	//
	if (dragType == DRAG_WHOLE) {
		// Move all corners by the same amount
		l += dx;
		r += dx;
		t += dy;
		b += dy;
    
		// And constrain the outer bounds
		if (l < 0) {
			r += (0 - l);
			l = 0;
		} else if (r > maxWidth-1) {
			l += (maxWidth-1 - r);
			r = maxWidth-1;
		}
    
		if (t < 0) {
			b += (0 - t);
			t = 0;
		} else if (b > maxHeight-1) {
			t += (maxHeight-1 - b);
			b = maxHeight-1;
		}

//		Draw
    
	}
	else {

		float mx = 15 * scaleFactorW, my = 15 * scaleFactorH;

		// Corner right or left
		if (dragType == DRAG_TOPLEFT || dragType == DRAG_BOTTOMLEFT) {
			l += dx;
			if (l < 0)				l = 0;
			else if (l > r - mx)	l = r - mx;
		}
		else {
			r += dx;
			if (r > maxWidth-1)		r = maxWidth-1;
			else if (r < l + mx)	r = l + mx;
		}
		
		// Corner bottom or top
		if (dragType == DRAG_TOPLEFT || dragType == DRAG_TOPRIGHT) {
			t += dy;
			if (t < 0)				t = 0;
			else if (t > b - my)	t = b - my;
		}
		else {
			b += dy;
			if (b > maxHeight-1)	b = maxHeight-1;
			else if (b < t + my)	b = t + my;
		}

//	UpdateRepFromPix
//	ConstrainCorner corn
//	UpdatePixFromRep
    
	}

	[ self setAreaLeft:l top:t right:r bottom:b ];
}

- (void)mouseUp:(NSEvent *)theEvent {
	if (dragType != DRAG_NONE) {
		dragType = DRAG_NONE;
		[ self updateView:YES andText:YES ];
		[ self presetChanged ];
	}
}

#pragma mark -
// NOTE: the Y part should be inverted before calling this
- (NSPoint)repPointFromViewPoint:(NSPoint)viewPoint {
	NSPoint repPoint = { viewPoint.x * scaleFactorW, viewPoint.y * scaleFactorH };
	return repPoint;
}

// NOTE: The returned Y value is inverted
- (NSPoint)viewPointFromRepPoint:(NSPoint)repPoint {
	NSPoint viewPoint = { repPoint.x / scaleFactorW, repPoint.y / scaleFactorH };
	return viewPoint;
}

- (NSRect)sanityCheckArea:(NSRect)areaRect {
	float	l = areaRect.origin.x,
			t = areaRect.origin.y,
			r = l + areaRect.size.width,
			b = t + areaRect.size.height;

	if (r < l) {
		float q = l;
		l = r; r = q;
	}

	if (b < t) {
		float q = t;
		t = b; b = q;
	}

	// Sanity check all values
	if (l < 0.0f)		r -= l, l = 0.0f;
	if (l > maxWidth)	r -= (l - maxWidth + 1.0f), l = maxWidth - 1.0f;
	if (t < 0.0f)		b -= t, t = 0.0f;
	if (t >= maxHeight)	b -= (t - maxHeight + 1.0f), t = maxHeight - 1.0f;
	if (r < 0.0f)		l -= r, r = 0.0f;
	if (r >= maxWidth)	l -= (r - maxWidth + 1.0f), r = maxWidth - 1.0f;
	if (b < 0.0f)		t -= b, b = 0.0f;
	if (b >= maxHeight)	t -= (b - maxHeight + 1.0f), b = maxHeight - 1.0f;

	if (l < 0.0f)		l = 0.0f;
	if (l >= maxWidth)	l = maxWidth - 1.0f;
	if (t < 0.0f)		t = 0.0f;
	if (t >= maxHeight)	t = maxHeight - 1.0f;
	if (r < 0.0f)		r = 0.0f;
	if (r >= maxWidth)	r = maxWidth - 1.0f;
	if (b < 0.0f)		b = 0.0f;
	if (b >= maxHeight)	b = maxHeight - 1.0f;

	return NSMakeRect(l, t, r - l, b - t);
}

- (void)updateView:(BOOL)doView andText:(BOOL)doText {
	if (doView)
		[ self setNeedsDisplay:YES ];

	if (doText)
		[ self updateTextFields ];
}

- (void)updateTextFields {
	[ textLeft setIntValue:(int)floorf(left) ];
	[ textTop setIntValue:(int)floorf(top) ];
	[ textRight setIntValue:(int)floorf(right) ];
	[ textBottom setIntValue:(int)floorf(bottom) ];
}

- (void)drawHandlePart:(NSRect)rect asActive:(BOOL)active {
	NSColor *fillColor;

	if (active) {
		fillColor = isConstrained
				? [NSColor colorWithCalibratedRed:0.6 green:0.6 blue:0.9 alpha:1.0]
				: [NSColor colorWithCalibratedRed:0.4 green:0.8 blue:0.4 alpha:1.0];
	}
	else {
		fillColor = isConstrained
				? [NSColor colorWithCalibratedRed:0.8 green:0.8 blue:1.0 alpha:1.0]
				: [NSColor colorWithCalibratedRed:0.8 green:1.0 blue:0.8 alpha:1.0];
	}

	setFillColor(fillColor);

	NSBezierPath *bez = [ NSBezierPath bezierPathWithOvalInRect:rect ];
	[ bez fill ];

	setFillColor([NSColor colorWithCalibratedRed:0.4 green:0.7 blue:0.8 alpha:1.0]);
	[ bez stroke ];
}

#pragma mark -
#pragma mark Accessors
- (unsigned)left		{ return (unsigned)left; }
- (unsigned)top			{ return (unsigned)top; }
- (unsigned)right		{ return (unsigned)right; }
- (unsigned)bottom		{ return (unsigned)bottom; }
- (unsigned)maxWidth	{ return (unsigned)maxWidth; }
- (unsigned)maxHeight	{ return (unsigned)maxHeight; }
- (BOOL)isConstrained	{ return isConstrained; }

- (NSSize)representedSize	{ return NSMakeSize( maxWidth, maxHeight ); }
- (NSSize)activeArea	{ return NSMakeSize(right - left, bottom - top); }
- (NSRect)activeRect	{ return NSMakeRect(left, top, right - left, bottom - top); }

// Actions
#pragma mark -
#pragma mark Actions

- (IBAction)setToMaxSize:(id)sender {
	[ self resetToAll ];
	[ self presetChanged ];
}

- (IBAction)setToLess:(id)sender {
	float w = right - left - 20 * scaleFactorW;
	float h = bottom - top - 20 * scaleFactorH;
	if (w < 15 * scaleFactorW)	w = 15 * scaleFactorW;
	if (h < 15 * scaleFactorH)	h = 15 * scaleFactorH;

	NSRect oldRect = [ self activeRect ];
	NSRect newRect = NSInsetRect(oldRect, (oldRect.size.width-w)/2.0f, (oldRect.size.height-h)/2.0f);
	[ self setAreaWithRect:[ self sanityCheckArea:newRect ] ];
	[ self presetChanged ];
}

- (IBAction)setToMore:(id)sender {
	[ self setAreaWithRect:[ self sanityCheckArea:NSInsetRect([ self activeRect ], -10*scaleFactorW, -10*scaleFactorH) ] ];
	[ self presetChanged ];
}

- (IBAction)boundsChanged:(id)sender {
	[ self setAreaFromEditFields ];
}

@end
