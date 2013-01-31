/*
	TabletMagicPrefPane
	Thinkyhead Software

	TMScratchpad.m
*/

#import "TMScratchpad.h"
#import "Constants.h"

@implementation TMScratchpad

- (id)initWithFrame:(NSRect)frameRect {
	if ((self = [super initWithFrame:frameRect]) != nil) {
		// Add initialization code here
		smoothing = YES;

		// Requires 10.6 or later...
		[NSEvent addLocalMonitorForEventsMatchingMask: NSTabletProximityMask
											   handler: ^(NSEvent* theEvent) {
												   [self _tabletProximity: theEvent];
												   return theEvent;
											   }
		 ];

		[NSEvent addGlobalMonitorForEventsMatchingMask: NSTabletProximityMask
											  handler: ^(NSEvent* theEvent) {
												  [self _tabletProximity: theEvent];
											  }
		 ];
		

	}
	return self;
}

- (void)awakeFromNib {
	// Must inform the window that we want mouse moves after all object
	// are created and linked.
	// Let our internal routine make the API call so that everything
	// stays in sych. Change the value in the init routine to change
	// the default behavior
	// Mouse moves must be captured if you want to recieve Proximity Events
/*
	[[self window] setAcceptsMouseMovedEvents:YES];
   
	//Must register to be notified when device come in and out of Prox
	[[NSNotificationCenter defaultCenter] addObserver:self
			selector:@selector(handleProximity:)
			name:kProximityNotification
			object:nil];
*/
}

//
// The proximity notification is based on the Proximity Event.
// (see CarbonEvents.h). The proximity notification will give you detailed
// information about the device that was either just placed on, or just
// taken off of the tablet.
// 
// In this sample code, the Proximity notification is used to determine if
// the pen TIP or ERASER is being used. This information is not provided in
// the embedded tablet event.
//
// Also, on the Intuos line of tablets, each transducer has a unique ID,
// even when different transducers are of the same type. We get that
// information here so we can keep track of the Color assigned to each
// transducer.
//
- (void) handleProximity:(NSNotification *)proxNotice {
/*
	NSDictionary *proxDict = [proxNotice userInfo];
	UInt8 enterProximity;
	UInt8 pointerType;
	UInt16 deviceID;

	[[proxDict objectForKey:kEnterProximity] getValue:&enterProximity];

	if (enterProximity != 0) { //Enter Proximity
		[[proxDict objectForKey:kPointerType] getValue:&pointerType];
		erasing = (pointerType == NX_TABLET_POINTER_ERASER);

		[[proxDict objectForKey:kDeviceID] getValue:&deviceID];

		if ([knownDevices setCurrentDeviceByID: deviceID] == NO) {
			//must be a new device
			Transducer *newDevice = [[Transducer alloc]
				initWithIdent: deviceID
				color: [NSColor blackColor]];

			[knownDevices addDevice:newDevice];
			[newDevice release];
			[knownDevices setCurrentDeviceByID: deviceID];
		}

		[[NSNotificationCenter defaultCenter]
			postNotificationName:WTViewUpdatedNotification
			object: self];
	}
*/
}

- (void)drawRect:(NSRect)rect {
	[[eraserColor color] setFill];
	NSRectFill(rect);

	[[NSColor blackColor] setFill];
	NSFrameRectWithWidth(rect, 1.0f);
}

BOOL tablet_eraser = NO;

- (void)_tabletProximity:(NSEvent *)theEvent {
	NSPointingDeviceType device_type = [theEvent pointingDeviceType];
	tablet_eraser = device_type == NSEraserPointingDevice;
}

- (void)mouseDown:(NSEvent *)theEvent {
	if ([theEvent subtype] == NSTabletPointEventSubtype) {

		startPoint = [ self convertPoint:[theEvent locationInWindow] fromView:nil ];
		startPressure = [theEvent pressure];

		[self drawBlobAtPoint:startPoint withPressure:[theEvent pressure] erasing:tablet_eraser];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent {
	if ([theEvent subtype] == NSTabletPointEventSubtype) {

		NSPoint newPoint = [ self convertPoint:[theEvent locationInWindow] fromView:nil ];
		float newPressure = [theEvent pressure];
		if (smoothing) {
			float	dx = newPoint.x - startPoint.x,
					dy = newPoint.y - startPoint.y,
					dist = sqrt(dx*dx+dy*dy),
					fx = dx / dist, fy = dy / dist;
			int distint = floorf(dist);
			float r = newPressure * [(tablet_eraser ? eraserFlow : penFlow) floatValue ];
			if (distint > 1 && distint >= r) {
				NSPoint point = startPoint;
				float press = startPressure, pstep = (newPressure - startPressure) / (float)distint;
				// the original point was already drawn (mouseDown) so...
				// draw from the 2nd to the next-to-last
				for (int i=distint - 1; i--;) {
					point.x += fx;
					point.y += fy;
					press += pstep;
					[ self drawBlobAtPoint:point withPressure:press erasing:tablet_eraser ];
				}
				
//				NSAffineTransform *transform = [NSAffineTransform transform];
//				[transform translateXBy: 10.0 yBy: 10.0];
//				[bezierPath transformUsingAffineTransform: transform];
			}
		}

		[ self drawBlobAtPoint:newPoint withPressure:newPressure erasing:tablet_eraser ];
		startPoint = newPoint;
		startPressure = newPressure;
	}
}

- (void)mouseUp:(NSEvent *)theEvent {
	tablet_eraser = NO;
}

- (void)drawBlobAtPoint:(NSPoint)point withPressure:(float)pressure erasing:(BOOL)is_eraser {
	[ self lockFocus ];
	[ NSBezierPath clipRect:NSInsetRect([self bounds], 1.0, 1.0) ];

	[[(is_eraser ? eraserColor : penColor) color] setFill];
	float r = pressure * [(is_eraser ? eraserFlow : penFlow) floatValue ];
	NSBezierPath *bez = [ NSBezierPath bezierPathWithOvalInRect:NSMakeRect(point.x-r, point.y-r, r*2.0f, r*2.0f) ];
	[ bez fill ];

	[ self unlockFocus ];
}

- (IBAction)clear:(id)sender {
	[ self setNeedsDisplay:YES ];
}

- (IBAction)toggleSmoothing:(id)sender {
	smoothing = ([sender state] == NSOnState);
}

@end
