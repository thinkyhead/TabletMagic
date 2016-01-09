/**
 * TMPreset.m
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

#import "TMPreset.h"
#import "TMController.h"
#import "TabletMagicPref.h"
#import "Constants.h"

@implementation TMPreset

- (id)init {
	if ((self = [super init])) {
		name = nil;
		[ self initProperties ];
	}

	return self;
}

- (id)initWithDictionary:(NSDictionary*)dict {
	[ self initProperties ];

	[ self setName:[dict objectForKey:@keyName] ];

	tabletRangeX	= [ [ dict objectForKey:@keyTabletRangeX ] intValue ];
	tabletRangeY	= [ [ dict objectForKey:@keyTabletRangeY ] intValue ];

	tabletLeft		= [ [ dict objectForKey:@keyTabletLeft ] intValue ];
	tabletTop		= [ [ dict objectForKey:@keyTabletTop ] intValue ];
	tabletRight		= [ [ dict objectForKey:@keyTabletRight ] intValue ];
	tabletBottom	= [ [ dict objectForKey:@keyTabletBottom ] intValue ];

	screenWidth		= [ [ dict objectForKey:@keyScreenWidth ] intValue ];
	screenHeight	= [ [ dict objectForKey:@keyScreenHeight ] intValue ];

	screenLeft		= [ [ dict objectForKey:@keyScreenLeft ] intValue ];
	screenTop		= [ [ dict objectForKey:@keyScreenTop ] intValue ];
	screenRight		= [ [ dict objectForKey:@keyScreenRight ] intValue ];
	screenBottom	= [ [ dict objectForKey:@keyScreenBottom ] intValue ];

	buttonTip		= [ [ dict objectForKey:@keyStylusTip ] intValue ];
	buttonSwitch1	= [ [ dict objectForKey:@keySwitch1 ] intValue ];
	buttonSwitch2	= [ [ dict objectForKey:@keySwitch2 ] intValue ];
	buttonEraser	= [ [ dict objectForKey:@keyEraser ] intValue ];

	mouseMode		= [ [ dict objectForKey:@keyMouseMode ] boolValue ];
	mouseScaling	= [ [ dict objectForKey:@keyMouseScaling ] floatValue ];

	constrained		= [ [ dict objectForKey:@keyConstrained ] boolValue ];

	return self;
}

- (void)setTabletRangeX:(int)x y:(int)y {
	tabletRangeX = x;
	tabletRangeY = y;
}

- (void)setScreenWidth:(int)w height:(int)h {
	screenWidth = w;
	screenHeight = h;
}

- (void)setTabletAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
	tabletLeft = l;
	tabletTop = t;
	tabletRight = r;
	tabletBottom = b;
}

- (void)setScreenAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b {
	screenLeft = l;
	screenTop = t;
	screenRight = r;
	screenBottom = b;
}

- (void)setMappingForTip:(int)tip side1:(int)s1 side2:(int)s2 eraser:(int)e {
	buttonTip = tip;
	buttonSwitch1 = s1;
	buttonSwitch2 = s2;
	buttonEraser = e;
}

#pragma mark -

-(BOOL)matchesTabletLeft:(int)tl top:(int)tt right:(int)tr bottom:(int)tb {
	return	tabletLeft == tl
		&&	tabletTop == tt
		&&	tabletRight == tr
		&&	tabletBottom == tb;
}

-(BOOL)matchesScreenLeft:(int)sl top:(int)st right:(int)sr bottom:(int)sb {
	return	screenLeft == sl
		&&	screenTop == st
		&&	screenRight == sr
		&&	screenBottom == sb;
}

-(BOOL)matchesButtonTip:(int)b0 switch1:(int)b1 switch2:(int)b2 eraser:(int)be {
	return	buttonTip = b0
		&&	buttonSwitch1 == b1
		&&	buttonSwitch2 == b2
		&&	buttonEraser == be;
}

-(BOOL)matchesMouseMode:(BOOL)mm scaling:(float)ms {
	float md = mouseScaling - ms;
	return	mouseMode==mm && md>-0.02 && md<0.02;
}

#pragma mark -

- (void)initProperties {
	[ self setName:[ thePane localizedString:@"Untitled Preset" ] ];

	tabletRangeX	= k12inches1270ppi;
	tabletRangeY	= k12inches1270ppi;
	tabletLeft		= 0;
	tabletTop		= 0;
	tabletRight		= k12inches1270ppi-1;
	tabletBottom	= k12inches1270ppi-1;

	screenWidth		= 1600;
	screenHeight	= 1200;
	screenLeft		= 0;
	screenTop		= 0;
	screenRight		= 1599;
	screenBottom	= 1199;

	buttonTip		= kSystemButton1;
	buttonSwitch1	= kSystemButton1;
	buttonSwitch2	= kSystemButton2;
	buttonEraser	= kSystemEraser;

	mouseMode		= NO;
	mouseScaling	= 1.0;

	constrained		= NO;
}

- (char*)daemonMessage {
	static char message[200];
	sprintf(message, "geom %d %d %d %d : %d %d %d %d : %d %d %d %d : %d %0.2f",
			tabletLeft, tabletTop, tabletRight, tabletBottom,
			screenLeft, screenTop, screenRight, screenBottom,
			buttonTip, buttonSwitch1, buttonSwitch2, buttonEraser,
			mouseMode, mouseScaling
		);

	return message;
}

- (char*)mouseModeMessage {
	static char message[200];
	sprintf(message, "mmode %d %.4f", mouseMode, mouseScaling);
	return message;
}

- (NSDictionary*)dictionary {
	static NSDictionary *dict = nil;

	if (dict) [ dict release ];

	NSArray * keys   = [NSArray arrayWithObjects:
		@keyName,
		@keyScreenWidth, @keyScreenHeight,
		@keyScreenLeft, @keyScreenTop, @keyScreenRight, @keyScreenBottom,
		@keyTabletRangeX, @keyTabletRangeY,
		@keyTabletLeft, @keyTabletTop, @keyTabletRight, @keyTabletBottom,
		@keyStylusTip, @keySwitch1, @keySwitch2, @keyEraser,
		@keyMouseMode, @keyMouseScaling,
		@keyConstrained,
		nil];

	NSArray * values = [NSArray arrayWithObjects:
		name,
		NSINT(screenWidth), NSINT(screenHeight),
		NSINT(screenLeft), NSINT(screenTop), NSINT(screenRight), NSINT(screenBottom),
		NSINT(tabletRangeX), NSINT(tabletRangeY),
		NSINT(tabletLeft), NSINT(tabletTop), NSINT(tabletRight), NSINT(tabletBottom),
		NSINT(buttonTip), NSINT(buttonSwitch1), NSINT(buttonSwitch2), NSINT(buttonEraser),
		NSBOOL(mouseMode), NSFLOAT(mouseScaling),
		NSBOOL(constrained),
		nil];

	dict = [[NSDictionary alloc] initWithObjects:values forKeys:keys];

	return dict;
}

#pragma mark -

- (NSString*)name	{ return name; }
- (int)tabletRangeX	{ return tabletRangeX; }
- (int)tabletRangeY { return tabletRangeY; }
- (int)tabletLeft	{ return tabletLeft; }
- (int)tabletTop	{ return tabletTop; }
- (int)tabletRight	{ return tabletRight; }
- (int)tabletBottom	{ return tabletBottom; }
- (int)screenWidth	{ return screenWidth; }
- (int)screenHeight	{ return screenHeight; }
- (int)screenLeft	{ return screenLeft; }
- (int)screenTop	{ return screenTop; }
- (int)screenRight	{ return screenRight; }
- (int)screenBottom	{ return screenBottom; }
- (int)buttonTip	{ return buttonTip; }
- (int)buttonSwitch1 { return buttonSwitch1; }
- (int)buttonSwitch2 { return buttonSwitch2; }
- (int)buttonEraser	{ return buttonEraser; }
- (BOOL)mouseMode	{ return mouseMode; }
- (float)mouseScaling { return mouseScaling; }
- (BOOL)constrained	{ return constrained; }

#pragma mark -

- (void)setName:(NSString *)n {
	[ name release ];
	name = [ n copy ];
}

- (void)setTabletRangeX:(int)n	{ tabletRangeX = n; }
- (void)setTabletRangeY:(int)n	{ tabletRangeY = n; }
- (void)setTabletLeft:(int)n	{ tabletLeft = n; }
- (void)setTabletTop:(int)n		{ tabletTop = n; }
- (void)setTabletRight:(int)n	{ tabletRight = n; }
- (void)setTabletBottom:(int)n	{ tabletBottom = n; }
- (void)setScreenWidth:(int)n	{ screenWidth = n; }
- (void)setScreenHeight:(int)n	{ screenHeight = n; }
- (void)setScreenLeft:(int)n	{ screenLeft = n; }
- (void)setScreenTop:(int)n		{ screenTop = n; }
- (void)setScreenRight:(int)n	{ screenRight = n; }
- (void)setScreenBottom:(int)n	{ screenBottom = n; }
- (void)setButtonTip:(int)n		{ buttonTip = n; }
- (void)setButtonSwitch1:(int)n	{ buttonSwitch1 = n; }
- (void)setButtonSwitch2:(int)n	{ buttonSwitch2 = n; }
- (void)setButtonEraser:(int)n	{ buttonEraser = n; }
- (void)setMouseMode:(BOOL)n	{ mouseMode = n; }
- (void)setMouseScaling:(float)n { mouseScaling = n; }
- (void)setConstrained:(BOOL)n	{ constrained = n; }

@end
