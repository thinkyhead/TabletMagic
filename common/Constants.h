/*
	TabletMagicDaemon
	Thinkyhead Software

	Constants.h
*/

#ifndef CONSTANTS_H
#define CONSTANTS_H

#define TABLETMAGIC_VERSION "2.0b19"

#define	ASYNCHRONOUS_MESSAGING	0
#define FORCE_TABLETPC			0

#define KNOWN_DIGIS {"WAC","FUJ","FPI"}

enum {
	kStylusTip			= 0,
	kStylusButton1,
	kStylusButton2,
	kStylusEraser,
	kStylusButtonTypes,

	kButtonLeft,			// Mouse button 1
	kButtonMiddle,			// Mouse button 3
	kButtonRight,			// Mouse button 2
	kButtonExtra,			// Mouse button 4
	kButtonSide,			// Mouse Button 5
	kButtonMax,

	kSystemNoButton		= 0,
	kSystemButton1,
	kSystemButton2,
	kSystemButton3,
	kSystemButton4,
	kSystemButton5,
	kSystemEraser,
	kSystemDoubleClick,
	kSystemSingleClick,
	kSystemControlClick,
	kSystemClickOrRelease,
	kSystemClickTypes,

	kOtherButton3		= 2,
	kOtherButton4,
	kOtherButton5,

	kBitStylusTip		= 1 << kStylusTip,
	kBitStylusButton1	= 1 << kStylusButton1,
	kBitStylusButton2	= 1 << kStylusButton2,
	kBitStylusEraser	= 1 << kStylusEraser,

	k12inches1270ppi	=	15240
};


#pragma mark -
//
// TabletPC Command Strings
//
#define TPC_StopTablet			"0"
#define TPC_Sample133pps		"1"
#define TPC_Sample80pps			"2"
#define TPC_Sample40pps			"3"
#define TPC_SurveyScanOn		"+"
#define TPC_SurveyScanOff		"-"
#define TPC_TabletID			"*"


#pragma mark -
//
// Wacom Tablet Command Strings
//
#define WAC_StartTablet			"ST\r"
#define WAC_StopTablet			"SP\r"
#define WAC_SelfTest			"TE\r"
#define WAC_TabletID			"~#\r"
#define WAC_TabletSize			"~C\r"
#define WAC_ReadSetting			"~R\r"
#define WAC_ReadSettingM1		"~R1\r"
#define WAC_ReadSettingM2		"~R2\r"

// Commands unavailable on CT
#define WAC_ResetBitpad2		"%%"
#define WAC_ResetMM1201			"&&"
#define WAC_ResetWacomII		"\r$"
#define WAC_ResetWacomIV		"\r#"
#define WAC_ResetDefaults		"RE\r"
#define WAC_TiltModeOn			"FM1\r"
#define WAC_TiltModeOff			"FM0\r"
#define WAC_MultiModeOn			"MU1\r"
#define WAC_MultiModeOff		"MU0\r"
#define WAC_SuppressIN2			"SU2\r"
#define WAC_PointMode			"PO\r"
#define WAC_SwitchStreamMode	"SW\r"
#define WAC_StreamMode			"SR\r"
#define WAC_DataContinuous		"AL1\r"
#define WAC_DataTrailing		"AL2\r"
#define WAC_DataNormal			"AL0\r"
#define WAC_OriginUL			"OC1\r"
#define WAC_OriginLL			"OC0\r"

// Numerical settings
#define WAC_IntervalOff			"IT0\r"
#define WAC_IncrementOff		"IN0\r"

// Wacom II-S Commands
#define WAC_PressureModeOn		"PH1\r"
#define WAC_PressureModeOff		"PH0\r"
#define WAC_ASCIIMode			"AS0\r"
#define WAC_BinaryMode			"AS1\r"
#define WAC_RelativeModeOn		"DE1\r"
#define WAC_RelativeModeOff		"DE0\r"
#define WAC_Rez1000ppi			"IC1\r"
#define WAC_Rez50ppmm			"IC0\r"

// Macro button commands
#define WAC_MacroAll			"~M0\r"
#define WAC_MacroNoSetup		"~M1\r"
#define WAC_MacroNoFunction		"~M2\r"
#define WAC_MacroNoPressure		"~M3\r"
#define WAC_MacroExtended		"~M4\r"

// Prefixes for longer commands
#define WAC_NewResolution		"NR"
#define WAC_Suppress			"SU"


#pragma mark -
//
// Wacom V Command Strings
//
#define WACV_MultiModeOff		"MT0\r"
#define WACV_MultiModeOn		"MT1\r"
#define WACV_Height				"HT1\r"
#define WACV_TabletID			"ID1\r"
#define WACV_SetBaud19200		"BA19\r"
#define WACV_SetBaud9600		"$\r"
#define WACV_SetBaud38400		"BA38\r"


#pragma mark -
//
// CalComp Tablet Command Strings
//
#define CAL_					"\x1B%"
#define CAL_StartTablet			CAL_ "IR\r"
#define CAL_StopTablet			CAL_ "H\r"
#define CAL_TabletID			CAL_ "__p\r"
#define CAL_Version             CAL_ "__V\r"
#define CAL_TabletSize			CAL_ "VS\r"
#define CAL_OriginUL			CAL_ "JUL\r"
#define CAL_OriginLL			CAL_ "JLL\r"
#define CAL_PressureModeOn		CAL_ "VA1\r"
#define CAL_PressureModeOff		CAL_ "VA0\r"
#define CAL_TilttoPressureOn	CAL_ "VA3\r"
#define CAL_TilttoPressureOff	CAL_ "VA2\r"
#define CAL_ASCIIMode			CAL_ "^17\r"
#define CAL_BinaryMode			CAL_ "^21\r"
#define CAL_22Mode              CAL_ "^22\r"
#define CAL_Rez1000ppi			CAL_ "JR1000,0\r"
#define CAL_Rez1270ppi			CAL_ "JR1270,0\r"
#define CAL_Rez2540ppi			CAL_ "JR2540,0\r"
#define CAL_Rez50ppmm			CAL_ "JM50,0\r"
#define CAL_ReadSetting			CAL_ "^17\r"
#define CAL_Reset               CAL_ "VR\r"
#define CAL_StreamMode			CAL_ "IR\r"
#define CAL_Rez1000ppi			CAL_ "JR1000,0\r"
#define CAL_DataRate1			CAL_ "W1\r"
#define CAL_DataRate50			CAL_ "W50\r"
#define CAL_DataRate125			CAL_ "W125\r"
#define CAL_LFEnable			CAL_ "L1\r"
#define CAL_LFDisable			CAL_ "L0\r"


#pragma mark - Main Preference Keys

#define keyTabletEnabled	@"tabletEnabled"
#define keySerialPort		@"serialPort"
#define keySelectedTab		@"selectedTab"
#define keyPenColor			@"penColor"
#define keyEraserColor		@"eraserColor"
#define keyPresets			@"presets"
#define keyIDonated			@"iDonatedV214"
#define keyTabletPC			@"tabletPC"
#define keyTabletPCScaleX	@"tabletPCScaleX"
#define keyTabletPCScaleY	@"tabletPCScaleY"
#define keyDidFixButtons	@"didFixButtons"


// These are remembered and used as a default state
// for either a running daemon or a non-running one
//
// TODO: Make sure the daemon initializes the serial port
// to the visible settings after these are set from prefs
//
#define keySerialBaudRate	@"serialBaudRate"
#define keySerialDataBits	@"serialDataBits"
#define keySerialParity		@"serialParity"
#define keySerialStopBits	@"serialStopBits"
#define keySerialCTS		@"serialCTS"
#define keySerialDSR		@"serialDSR"

#pragma mark - Preset Preference Keys

// Presets and their properties
#define keyPresetList		@"presetList"
#define keySelectedPreset	@"selectedPreset"

#define keyName			"name"
#define keyTabletRangeX	"tabletRangeX"
#define keyTabletRangeY	"tabletRangeY"
#define keyTabletLeft	"tabletLeft"
#define keyTabletTop	"tabletTop"
#define keyTabletRight	"tabletRight"
#define keyTabletBottom	"tabletBottom"
#define keyScreenWidth	"screenWidth"
#define keyScreenHeight	"screenHeight"
#define keyScreenLeft	"screenLeft"
#define keyScreenTop	"screenTop"
#define keyScreenRight	"screenRight"
#define keyScreenBottom	"screenBottom"
#define keyStylusTip	"stylusTip"
#define keySwitch1		"switch1"
#define keySwitch2		"switch2"
#define keyEraser		"eraser"
#define keyMouseMode	"mouseMode"
#define keyMouseScaling	"mouseScaling"
#define keyConstrained	"constrained"

#pragma mark -

// Tablet model letters for comparison
#define kTabletModelUnknown			"Unknown"
#define kTabletModelIntuos2			"XD"
#define kTabletModelIntuos			"GD"
#define kTabletModelGraphire3		"CTE"
#define kTabletModelGraphire2		"ETA"
#define kTabletModelGraphire		"ET"
#define kTabletModelCintiq			"PL"
#define kTabletModelCintiqPartner	"PTU"
#define kTabletModelArtZ			"UD"
#define kTabletModelArtPad			"KT"
#define kTabletModelPenPartner		"CT"
#define kTabletModelSDSeries		"SD"
#define kTabletModelTabletPC		"ISD"
#define kTabletModelCalComp         "Cal"

// Included for completeness
#define kTabletModelPLSeries		"PL"
#define kTabletModelUDSeries		"UD"

// Numeric index for each model
enum SeriesIndex {
	kModelUnknown		= 0,
	kModelIntuos2		,
	kModelIntuos		,
	kModelGraphire3		,
	kModelGraphire2		,
	kModelGraphire		,
	kModelCintiq		,
	kModelCintiqPartner ,
	kModelArtZ			,
	kModelArtPad		,
	kModelPenPartner	,
	kModelSDSeries		,
	kModelTabletPC		,
	kModelFujitsuP		,
    kModelCalComp		,

	// Included for completeness
	kModelPLSeries		,
	kModelUDSeries
	};

// Tablet setup bitfields
enum {
	kCommandSetBitpadII	= 0,
	kCommandSetMM1201	= 1,
	kCommandSetWacomIIS	= 2,
	kCommandSetWacomIV	= 3,
	kCommandSetWacomV	= 4,		// Synthetic - looks like BitPadII to prefs
	kCommandSetTabletPC	= 5,		// Synthetic - looks like MM1201 to prefs

	kBaudRate2400		= 0,
	kBaudRate4800		= 1,
	kBaudRate9600		= 2,
	kBaudRate19200		= 3,
	kBaudRate38400		= 4,		// Synthetic - looks like 2400 to prefs

	kParityNone			= 0,
	kParityNone2		= 1,
	kParityOdd			= 2,
	kParityEven			= 3,

	kDataBits7			= 0,
	kDataBits8			= 1,

	kStopBits1			= 0,
	kStopBits2			= 1,

	kCTSDisabled		= 0,
	kCTSEnabled			= 1,

	kDSRDisabled		= 0,
	kDSREnabled			= 1,

	kTransferModeSuppressed		= 0,
	kTransferModePoint			= 1,
	kTransferModeSwitchStream   = 2,
	kTransferModeStream			= 3,

	kOutputFormatBinary = 0,
	kOutputFormatASCII	= 1,

	kCoordSysAbsolute	= 0,
	kCoordSysRelative	= 1,

	kTransferRate50pps	= 0,
	kTransferRate67pps	= 1,
	kTransferRate100pps	= 2,
	kTransferRateMAX	= 3,
	kTransferRate200pps	= 4,		// Synthetic

	kResolution500lpi	= 0,
	kResolution508lpi	= 1,
	kResolution1000lpi	= 2,
	kResolution1270lpi	= 3,
	kResolution2540lpi	= 4,		// Synthetic

	kOriginUL			= 0,
	kOriginLL			= 1,

	kOORDisabled		= 0,
	kOOREnabled			= 1,

	kTerminatorCR		= 0,
	kTerminatorLF		= 1,
	kTerminatorCRLF		= 2,
	kTerminatorCRLF2	= 3,

	kPNPDisabled		= 0,
	kPNPEnabled			= 1,

	kSensitivityFirm	= 0,
	kSensitivitySoft	= 1,

	kReadHeight8mm		= 0,
	kReadHeight2mm		= 1,

	kMDMDisabled		= 0,
	kMDMEnabled			= 1,

	kTiltDisabled		= 0,
	kTiltEnabled		= 1,

	kMMCommandSet1201	= 0,
	kMMCommandSet961	= 1,

	kOrientationLandscape	= 0,
	kOrientationPortrait	= 1,

	kCursorData1234		= 0,
	kCursorData1248		= 1,

	kRemoteModeDisabled	= 0,
	kRemoteModeEnabled	= 1
};

//
// Tablet packet bit masks
//

	// Wacom V
#pragma mark - Wacom V

#define V_Mask1_ToolHi			0x7F
#define V_Mask2_ToolLo			0x7C

#define V_Mask2_Serial			0x03
#define V_Mask3_Serial			0x7F
#define V_Mask4_Serial			0x7F
#define V_Mask5_Serial			0x7F
#define V_Mask6_Serial			0x7F
#define V_Mask7_Serial			0x60

#define V_Mask1_X				0x7F
#define V_Mask2_X				0x7F
#define V_Mask3_X				0x60
#define V_Mask3_Y				0x1F
#define V_Mask4_Y				0x7F
#define V_Mask5_Y				0x78

#define V_Mask7_TiltX			0x3F
#define V_Mask7_TiltXBase		0x40
#define V_Mask8_TiltY			0x3F
#define V_Mask8_TiltYBase		0x40

#define V_Mask5_PressureHi		0x07
#define V_Mask6_PressureLo		0x7F

#define V_Mask0_Button1			0x02
#define V_Mask0_Button2			0x04

#define V_Mask5_WheelHi			0x07
#define V_Mask6_WheelLo			0x7F

#define V_Mask5_ThrottleHi		0x07
#define V_Mask6_ThrottleLo		0x7F
#define V_Mask8_ThrottleSign	0x08

#define V_Mask6_4dRotationHi	0x0F
#define V_Mask7_4dRotationLo	0x7F
#define V_Mask8_4dButtonsHi		0x70
#define V_Mask8_4dButtonsLo		0x07

#define V_Mask8_LensButtons		0x1F

#define V_Mask8_2dButtons		0x1C

#define V_Mask8_WheelUp			0x02
#define V_Mask8_WheelDown		0x01

	// Wacom IV
#pragma mark - Wacom IV

#define IV_Mask0_Engagement		0x60
#define IV_DisengagedOrMenu		0x20

#define IV_Mask0_Stylus			0x20

#define IV_Mask0_ButtonFlag		0x08

#define IV_Mask0_X				0x03
#define IV_Mask1_X				0x7F
#define IV_Mask2_X				0x7F

#define IV_Mask3_Buttons		0x78
#define IV_Mask3_Pressure0		0x04

#define IV_Mask3_Y				0x03
#define IV_Mask4_Y				0x7F
#define IV_Mask5_Y				0x7F

#define IV_Mask7_TiltX			0x3F
#define IV_Mask7_TiltXBase		0x40
#define IV_Mask8_TiltY			0x3F
#define IV_Mask8_TiltYBase		0x40

#define IV_Mask6_PressureLo		0x3F
#define IV_Mask6_PressureHi		0x40

	// Wacom II-S
#pragma mark - Wacom II-S

#define IIs_Mask0_Proximity		0x40
#define IIs_Mask0_Pressure		0x10
#define IIs_Mask0_Engaged		0x60
#define IIs_Disengaged			0x20

#define IIs_Mask0_X				0x03
#define IIs_Mask1_X				0x7F
#define IIs_Mask2_X				0x7F

#define IIs_Mask3_Y				0x03
#define IIs_Mask4_Y				0x7F
#define IIs_Mask5_Y				0x7F

#define IIs_Mask6_EraserOrTip	0x01
#define IIs_Mask6_Button1		0x02
#define IIs_Mask6_EraserOr2		0x04
#define IIs_Mask6_PressureLo	0x3F
#define IIs_Mask6_PressureHi	0x40
#define IIs_Mask6_ButtonFlag	0x20

	// CalComp
#pragma mark - CalComp
#define CAL_Mask0_Proximity		0x40
#define CAL_Mask0_Buttons		0x7C
#define CAL_Mask0_Pressure		0x10
#define CAL_Mask0_Engaged		0x60
#define CAL_Mask0_CursorOrPen   0x40
#define CAL_Mask0_Stylus        0x04
#define CAL_Mask0_Pen1          0x08
#define CAL_Mask0_Pen2          0x10
#define CAL_Mask0_Button1       0x04
#define CAL_Mask0_Button2       0x08
#define CAL_Mask0_Eraser		0x10
#define CAL_Disengaged			0x20

#define CAL_Mask0_X				0x03
#define CAL_Mask1_X				0x7F
#define CAL_Mask2_X				0x7F
#define CAL_Mask3_X				0x18

#define CAL_Mask3_Y				0x07
#define CAL_Mask4_Y				0x7F
#define CAL_Mask5_Y				0x7F

#define CAL_Mask6_EraserOrTip	0x01
#define CAL_Mask6_Button1		0x02
#define CAL_Mask6_Button2		0x04
#define CAL_Mask6_EraserOr2		0x08
#define CAL_Mask6_Pressure      0xFF
#define CAL_Mask6_ButtonFlag	0x20

	// Wacom SD420L
#pragma mark - SD Series

#define SD_Mask0_Pressure		0x10
#define SD_Mask6_PressureLo		0x3F
#define SD_Mask6_PressureHi		0x40

	// TabletPC
#pragma mark - TabletPC

#define TPC_Mask0_QueryData		0x40

#define TPC_Mask0_Proximity		0x20
#define TPC_Mask0_Eraser		0x04

#define TPC_Mask0_Touch			0x01
#define TPC_Mask0_Switch1		0x02
#define TPC_Mask0_Switch2		0x04

#define TPC_Mask6_PressureHi	0x01
#define TPC_Mask5_PressureLo	0x7F

#define TPC_Mask6_X				0x60
#define TPC_Mask2_X				0x7F
#define TPC_Mask1_X				0x7F

#define TPC_Mask6_Y				0x18
#define TPC_Mask4_Y				0x7F
#define TPC_Mask3_Y				0x7F

	// TabletPC Query Data
#pragma mark - TabletPC Query Data

#define TPC_QUERY_REPLY_SIZE	11

#define TPC_Query0_Data			0x3F

#define TPC_Query6_PressureHi	0x07
#define TPC_Query5_PressureLo	0x7F

#define TPC_Query6_MaxX			0x60
#define TPC_Query2_MaxX			0x7F
#define TPC_Query1_MaxX			0x7F

#define TPC_Query6_MaxY			0x18
#define TPC_Query4_MaxY			0x7F
#define TPC_Query3_MaxY			0x7F

#define TPC_Query9_FirmwareHi	0x7F
#define TPC_Query10_FirmwareLo	0x7F


#pragma mark -

#define NSBOOL(x)			[NSNumber numberWithBool:(x)]
#define NSINT(x)			[NSNumber numberWithInt:(x)]
#define NSFLOAT(x)			[NSNumber numberWithFloat:(x)]

#define BIT(x)				(1<<(x))
#define clearstr(x)			x[0]='\0'

#endif
