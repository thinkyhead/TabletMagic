/*
	TabletMagicDaemon
	Thinkyhead Software

	SerialDaemon.h ($Id: SerialDaemon.h,v 1.19 2009/08/05 20:59:07 slurslee Exp $)

	This program is a component of TabletMagic. See the
	accompanying documentation for more details about the
	TabletMagic project.

	LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "TabletSettings.h"
#include "TMSerialPort.h"

//
// Wacom.h is a very sparse header provided by Wacom.
// It is included here for completeness.
//
#include "Wacom.h"

#include <sys/param.h>
#include <termios.h>

#pragma mark -

#define kNumRetries				3


//! Command-line options
typedef struct init_arguments {
	bool	quiet;		//!< suppress diagnostic messages
	bool	command;	//!< run in command mode
	bool	dodaemon;	//!< run as a daemon
	bool	forcepc;	//!< always assume ISD-V4 at 19200
	bool	baud38400;	//!< initially try 38400
	bool	startoff;	//!< start up in disabled mode
	bool	quit;		//!< quit after testing the connection
	bool	logging;	//!< redirect output to a log file
	char	*port;		//!< the serial port to connect to (null = Automatic)
	char	*init;		//!< initial setup string to send to the tablet
	char	*digi;		//!< digitizer string, if any
	int		rate;		//!< initial baud rate (default 9600)

	bool	mouse;		//!< operate in mouse mode
	float	scaling;	//!< initial mouse scaling (default 1.0)

	int		priority;	//!< process priority (-20 to 20)

	int		tab_left;	//!< initial tablet left boundary
	int		tab_top;	//!< initial tablet top boundary
	int		tab_right;	//!< initial tablet right boundary
	int		tab_bottom;	//!< initial tablet bottom boundary

	int		scr_left;	//!< initial screen left boundary
	int		scr_top;	//!< initial screen top boundary
	int		scr_right;	//!< initial screen right boundary
	int		scr_bottom;	//!< initial screen bottom boundary
} init_arguments;

typedef struct {
	CGPoint	scrPos, oldPos;				// Screen position (tracked by the tablet object)
	struct { SInt32	x, y; } point;		// Tablet-level X / Y coordinates
	struct { SInt32	x, y; } old;		// Old coordinates used to calculate mouse mode
	struct { SInt32	x, y; } motion;		// Tablet-level X / Y motion
	struct { SInt16 x, y; } tilt;		// Current tilt, scaled for NX Event usage
	UInt16		raw_pressure;			//!< Previous raw pressure (for SD II-S ASCII)
	UInt16		pressure;				//!< Current pressure, scaled for NX Event usage
	bool		button_click;			//!< a button is clicked
	UInt16		button_mask;			//!< Bits set here for each button
	bool		button[kButtonMax];		//!< Booleans for each button
	bool		off_tablet;				//!< nothing is near or clicked
	bool		pen_near;				//!< pen or eraser is near or clicked
	bool		eraser_flag;			//!< eraser is near or clicked
	UInt16		menu_button;			//!< Last menu button pressed (clear after handling)

	// Intuos
	UInt16		tool;					//!< Intuos supports several tools
	int			toolid;					//!< Tool ID passed on to system for apps to recognize
	long		serialno;				//!< Serial number of the selected tool
	SInt16		rotation;				//!< Rotation from the 4D mouse
	SInt16		wheel;					//!< The 2D Mouse has a wheel
	SInt16		throttle;				//!< The mouse has a throttle (-1023 to 1023)

	NXTabletProximityData   proximity;  //!< Proximity data description

} StylusState;


#pragma mark -
class WacomTablet {

private:
	bool	no_tablet_events;			//!< True if the OSX system doesn't support tablet events
	bool	tablet_on;					//!< If true, events are generated from tablet data
	bool	doing_modal;				//!< True when awaiting a modal command reply
	int		bank_last_requested;		//!< For ArtZ tablets, the settings bank last queried

	bool	send_stream;				//!< If true then pass tablet events to the pref pane
	char	stream_packet[500];			//!< The most recent packet processed
	char	*stream_event;				//!< The most recent event
	char	stream_size;				//!< The most recent packet size

	CFRunLoopTimerRef	streamTimer;			//!< Seconds counter to support rate counting
	CFMessagePortRef	local_message_port;		//!< To receive messages from the pref pane

#if ASYNCHRONOUS_MESSAGING
	CFMessagePortRef	remote_message_port;	//!< To send messages to the pref pane
#else
	bool				messageQueueEnabled;	//!< Whether this queue is enabled
	CFMutableArrayRef	outgoing_message_queue;	//!< Messages bound for the pref pane
#endif

	char	model_number[64];			//!< The model number, as received from ~#
	char	rom_version[64];			//!< The ROM version, as received from ~#
	float	base_version;				//!< The base version of the ROM, as received from ~#
	SeriesIndex series_index;			//!< The type of tablet, based on 
	bool	can_parse_ud_setup;			//!< Flag for UD parsing ability (not SD or TPC)

	/*! Internal tablet settings

		[0] is the active setting
		[1] and [2] are the ArtZ memory bank settings
	*/
	TabletSettings  settings[3];

	StylusState		stylus;				//!< The state of the (single) stylus
	StylusState		oldStylus;			//!< A mirrored state used to track changes
	bool			buttonState[kSystemClickTypes];		//!< The state of all the system-level buttons
	bool			oldButtonState[kSystemClickTypes];	//!< The previous state of all system-level buttons

	TMSerialPort	serialPort;			//!< The serial port instance associated with this tablet

	char			phrase[100];		//!< A single "phrase" of the tablet stream, with room to spare
	int				phrase_count;		//!< length of the phrase so far
	int				comma_count;		//!< comma counting for model replies of certain tablets
	char			buffer[1024];		//!< Buffer for the raw stream, with room to spare
	char			modalbuffer[1024];	//!< Buffer for the raw stream when awaiting modal replies
	char			out_message[200];	//!< A buffer for composing messages sent to the pref pane

	int				commandSent;		//!< Flag if we are waiting for a command result

	bool			mouse_mode;			//!< If set, the tablet behaves like a mouse
	float			mouse_scaling;		//!< Mouse motion relative to tablet motion

	bool			test_mode;			//!< If set, quit after initial connection
	bool			quiet_mode;			//!< If set, don't print any messages

	bool			in_packet;			//!< Set when a packet start bit is detected

	CGRect			tabletMapping;		//!< The active area of the tablet
	CGRect			screenMapping;		//!< The corresponding active area of the screen

	mach_port_t		io_master_port;		//!< The master port for HID events
	io_connect_t	gEventDriver;		//!< The connection by which HID events are sent

public:
	WacomTablet(init_arguments);
	~WacomTablet();

	void			InitializeForPort(char *port_name);
	void			InitStylus();
	void			ResetStylus();
	bool			FindTabletOnPort(char *port_name=NULL);
	bool			InitializeTablet(int try_tablet_model=kModelUnknown);
	bool			SendCommandToTablet(const char *command);
	int				SendRequestToTablet(const char *command);

	void			SendUDSetupString(char *setup, int bank=0, bool insist=true);
	void			RequestUDSettings(int bank=0);
	char*			GetUDSettings(int bank=0);
	bool			GetUDSettingsOrFail(int bank=0);

	bool			SendScaleToTablet(int h, int v);

	bool			IsActive()						{ return serialPort.IsActive(); }
	inline bool		AwaitingModalReply()			{ return doing_modal; }

	void			SetTestMode(bool b=true)		{ test_mode = b; }
	void			SetQuietMode(bool b=true)		{ quiet_mode = b; }
	void			SetMouseMode(bool b=true)		{ mouse_mode = b; stylus.oldPos = stylus.scrPos; }
	void			SetMouseScaling(float s=1.0)	{ mouse_scaling = s; }
	void			SetScreenMapping(SInt16 x1, SInt16 y1, SInt16 x2, SInt16 y2);
	void			InitTabletBounds(SInt32 x1, SInt32 y1, SInt32 x2, SInt32 y2);
	void			UpdateTabletScale(SInt32 h, SInt32 v, bool tellprefs=false);

	void			RequestTabletID()				{ SendCommandToTablet(WAC_TabletID); }
	void			RequestMaxCoordinates()			{ SendCommandToTablet(WAC_TabletSize); }

	char*			RequestTabletIDModal()			{ SendRequestToTablet(WAC_TabletID); return modalbuffer; }
	char*			RequestMaxCoordinatesModal()	{ SendRequestToTablet(WAC_TabletSize); return modalbuffer; }

	void			RunEventLoop();
	static void		TabletTimerCallback( CFRunLoopTimerRef timer, void *info );
	static void		EventTimerCallback( CFRunLoopTimerRef timer, void *info );

	void			SetStreamLogging(bool do_stream);
	static void		StreamTimerCallback( CFRunLoopTimerRef timer, void *info );

	void			ProcessSerialStream();
	void			ProcessPacket(char *pkt, int size);
	void			ProcessCommandReply(char *response);
	void			ProcessTabletPCCommandReply(char *packet);

	void			ProcessWacomIIS_ASCII(char *pkt, int size);
	void			ProcessWacomIIS_Binary(char *pkt, int size);
	void			ProcessWacomIV_Base(char *pkt, int size);
	void			ProcessWacomIV_13(char *pkt, int size);
	void			ProcessWacomIV_14(char *pkt, int size);
	void			ProcessWacomV(char *pkt, int size);
	void			ProcessGraphire(char *pkt, int size);
	void			ProcessTabletPC(char *pkt, int size);
	void			ProcessFinepoint(char *pkt, int size);
	void			ProcessFujitsuPSeries(char *pkt);

	void			PostChangeEvents();
	void			PostNXEvent(int eventType, SInt16 eventSubType, UInt8 otherButton=0);

	void			ApplySettings(int i);

	inline int		Flush()							{ return serialPort.Flush(); }

	kern_return_t   OpenHIDService();
	kern_return_t   CloseHIDService();

	void			RegisterForNotifications();
	void			DisableNotifications();
	static void		PowerCallBack(void *x, io_service_t y, natural_t messageType, void *messageArgument);
	static void		ResolutionChangeCallback( CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo );
	void			ScreenChanged();

	void			CreateLocalMessagePort();

#if ASYNCHRONOUS_MESSAGING
	CFMessagePortRef GetRemoteMessagePort();
	static void		invalidation_callback(CFMessagePortRef ms, void *info);
	void			HandleInvalidation(CFMessagePortRef ms);
#else
	void			AddMessageToQueue(CFDataRef message);
	void			FlushMessageQueue();
	char*			PopMessageQueue();
	inline void		EnableMessageQueue()	{ FlushMessageQueue(); messageQueueEnabled = true; }
	inline void		DisableMessageQueue()	{ messageQueueEnabled = false; FlushMessageQueue(); }
#endif

	static CFDataRef	message_callback(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info);
	CFDataRef		HandleMessage(CFMessagePortRef local, SInt32 msgid, CFDataRef data);
	void			SendMessage(const char *message);
	void			SendMessage(CFDataRef message);

	void			SendMessageInfo(int bank=0);
	void			SendMessageScale();
	void			SendMessageModel();
	void			SendMessageProtocol();

	char*			GetMessageInfo(int bank=0);
	char*			GetMessageScale();
	char*			GetMessageModel();
	char*			GetMessageProtocol();
	char*			GetMessageGeometry();
	char*			GetMessageStream();
	char*			GetMessageSerialPort();

	// Commands - As sent by the PreferencePane
	void			SetProcessing(bool ena)	{ tablet_on = ena; }
};

