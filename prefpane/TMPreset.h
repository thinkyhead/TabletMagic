/**
 * TMPReset.h
 *
 * TabletMagicPrefPane
 * Thinkyhead Software
 */

@interface TMPreset : NSObject {
    NSString    *name;
    int         tabletLeft, tabletTop, tabletRight, tabletBottom;
    int         tabletRangeX, tabletRangeY;
    int         screenLeft, screenTop, screenRight, screenBottom;
    int         screenWidth, screenHeight;
    int         buttonTip, buttonSwitch1, buttonSwitch2, buttonEraser;
    BOOL        mouseMode;
    float       mouseScaling;
    BOOL        constrained;
}

- (void)setTabletRangeX:(int)x y:(int)y;
- (void)setScreenWidth:(int)x height:(int)y;
- (void)setTabletAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)setScreenAreaLeft:(float)l top:(float)t right:(float)r bottom:(float)b;
- (void)setMappingForTip:(int)tip side1:(int)s1 side2:(int)s2 eraser:(int)e;

- (void)initProperties;
- (char*)daemonMessage;
- (char*)mouseModeMessage;

// Preset as a dictionary
- (id)initWithDictionary:(NSDictionary*)dict;
- (NSDictionary*) dictionary;

// Matching clauses
-(BOOL)matchesTabletLeft:(int)tl top:(int)tt right:(int)tr bottom:(int)tb;
-(BOOL)matchesScreenLeft:(int)sl top:(int)st right:(int)sr bottom:(int)sb;
-(BOOL)matchesButtonTip:(int)b0 switch1:(int)b1 switch2:(int)b2 eraser:(int)be;
-(BOOL)matchesMouseMode:(BOOL)mm scaling:(float)ms;

// Accessors
- (NSString*)name;
- (int)tabletRangeX;
- (int)tabletRangeY;
- (int)tabletLeft;
- (int)tabletTop;
- (int)tabletRight;
- (int)tabletBottom;
- (int)screenWidth;
- (int)screenHeight;
- (int)screenLeft;
- (int)screenTop;
- (int)screenRight;
- (int)screenBottom;
- (int)buttonTip;
- (int)buttonSwitch1;
- (int)buttonSwitch2;
- (int)buttonEraser;
- (BOOL)mouseMode;
- (float)mouseScaling;
- (BOOL)constrained;

// Setters
- (void)setName:(NSString *)name;
- (void)setTabletRangeX:(int)n;
- (void)setTabletRangeY:(int)n;
- (void)setTabletLeft:(int)n;
- (void)setTabletTop:(int)n;
- (void)setTabletRight:(int)n;
- (void)setTabletBottom:(int)n;
- (void)setScreenWidth:(int)n;
- (void)setScreenHeight:(int)n;
- (void)setScreenLeft:(int)n;
- (void)setScreenTop:(int)n;
- (void)setScreenRight:(int)n;
- (void)setScreenBottom:(int)n;
- (void)setButtonTip:(int)n;
- (void)setButtonSwitch1:(int)n;
- (void)setButtonSwitch2:(int)n;
- (void)setButtonEraser:(int)n;
- (void)setMouseMode:(BOOL)n;
- (void)setMouseScaling:(float)n;
- (void)setConstrained:(BOOL)n;

@end
