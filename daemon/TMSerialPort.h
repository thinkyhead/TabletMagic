/**
 * TMSerialPort.h
 *
 * TabletMagicDaemon
 * Thinkyhead Software
 *
 * This program is a component of TabletMagic. See the
 * accompanying documentation for more details about the
 * TabletMagic project.
 *
 * LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TMSERIALPORT_H__
#define __TMSERIALPORT_H__

class TabletSettings;

#include <sys/param.h>
#include <termios.h>

//===================================================================
//
//  TMSerialPort
//
//  This class models a serial port
//
//===================================================================

class TMSerialPort {

private:
	int				fd;
	char			deviceFilePath[MAXPATHLEN];
	char			shortName[MAXPATHLEN];
	struct termios  originalAttribs;
	io_iterator_t	serialPortIterator;

	speed_t			openSpeed;
	tcflag_t		openDataBits;
	int				openParity;
	int				openStopBits;
	bool			openCTS;
	bool			openDSR;

	FILE			*output;

public:
	TMSerialPort();
	~TMSerialPort();

	inline int		FileDevice()		{ return fd; }
	inline char*	DeviceFilePath()	{ return deviceFilePath; } 
	inline bool		IsOpen()			{ return fd != kSerialError; }
	inline bool		IsActive()			{ return IsOpen(); } 
	inline void		SetOutput(FILE *f)	{ output = f; }

	int				Open(char *filepath=NULL);
	void			Close();
	int				Flush();
	int				Select(__darwin_suseconds_t usec=4000);
	int				Read(char *buffer, int maxlen);
	int				ReadLine(char *buffer, int maxlen, __darwin_suseconds_t usec=4000);
	int				Write(char *buffer, int length);
	int				WriteString(char *string);
	int				BytesOnPort();

	int				ReInit(TabletSettings *sett);
	bool			SetParameters(TabletSettings *sett);
	bool			SetParameters(speed_t speed, tcflag_t databits, int parity, int stopbits, bool cts=false, bool dsr=false);
	bool			SetDefaultParameters();

	inline bool		SetSpeed(speed_t s)			{ openSpeed = s; return ApplySettings(); }
	inline bool		SetDataBits(tcflag_t d)		{ openDataBits = d; return ApplySettings(); }
	inline bool		SetParity(int p)			{ openParity = p; return ApplySettings(); }
	inline bool		SetStopBits(int s)			{ openStopBits = s; return ApplySettings(); }
	inline bool		SetCTS(bool b)				{ openCTS = b; return ApplySettings(); }
	inline bool		SetDSR(bool b)				{ openDSR = b; return ApplySettings(); }
	bool			ApplySettings();

	inline char*	Name()						{ return shortName; }
	inline speed_t	Speed()						{ return openSpeed; }
	inline tcflag_t	DataBits()					{ return openDataBits; }
	inline int		Parity()					{ return openParity; }
	inline int		StopBits()					{ return openStopBits; }
	inline bool		CTS()						{ return openCTS; }
	inline bool		DSR()						{ return openDSR; }

	bool			HasValidIterator();
	void			EndPortScan();
	kern_return_t	BeginPortScan(bool RS232Only=false);
	kern_return_t	GetNextPortPath();
	bool			OpenNextMatchingPort(char *portname=NULL);
};

#endif
