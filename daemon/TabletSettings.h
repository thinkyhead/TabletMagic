/**
 * TabletSettings.h
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

#ifndef __TABLETSETTINGS_H__
#define __TABLETSETTINGS_H__

//
// TabletSettings
// The internal settings of a Wacom tablet
// as reported by the tablet
//
class TabletSettings {

public:
    static int index;
    char    *bankname;
    UInt8   mem_slot;

    UInt8   command_set;
    UInt8   baud_rate;
    UInt8   parity;
    UInt8   data_bits;
    UInt8   stop_bits;
    UInt8   cts;
    UInt8   dsr;
    UInt8   transfer_mode;
    UInt8   output_format;
    UInt8   coordsys;
    UInt8   transfer_rate;
    UInt8   resolution;
    UInt8   origin;
    UInt8   oor_data;
    UInt8   terminator;
    UInt8   pnp;
    UInt8   sensitivity;
    UInt8   read_height;
    UInt8   mdm;
    UInt8   tilt;
    UInt8   mm_comm;
    UInt8   orientation;
    UInt8   cursor_data;
    UInt8   remote_mode;

    int     increment;
    int     interval;
    int     xrez, yrez;
    SInt32  xscale, yscale;
    UInt16  screen_width, screen_height;
    int     packet_size;

public:
    TabletSettings();
    ~TabletSettings();

    bool    Import(const char *state);
    void    InitForCalComp();
    void    InitForTabletPC(Boolean use38400=false);
    void    InitForSD();
    void    InitForPL();
    void    InitForPenPartner();
    void    InitForIntuos();
    char*   Description();
    char*   SettingsString(bool notail=false);
};

#endif
