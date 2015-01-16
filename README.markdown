TabletMagic Links
-----------------
- [TabletMagic Home](http://www.thinkyhead.com/tabletmagic)
- [Thinkyhead Software](http://www.thinkyhead.com/)
- [TabletMagic for TabletPC](http://www.insanelymac.com/forum/topic/43948-tabletmagic-for-tabletpcs/)
- [USB Serial Adapters](http://www.thinkyhead.com/tabletmagic/adapters)

What Is TabletMagic?
--------------------
_TabletMagic_ is an OS X driver for obsolete serial Wacom tablets. The minimum system requirement is Mac OS X 10.4. A USB to serial adapter will also be required.

TabletMagic also works as a driver for TabletPC digitizers based on Wacom serial hardware. TabletPCs with "ISD-V4" or "Fujitsu P-series" protocol are currently supported. [This Page](http://www.insanelymac.com/forum/topic/43948-tabletmagic-for-tabletpcs/) contains more information and help for TabletPC users.

Installed Components
--------------------
- "TabletMagicDaemon" is the actual device driver that communicates with the tablet and produces Mac system events. The daemon is a relatively simple C++ project. There's a class to represent the tablet, one for the serial port interface, and a small class to encapsulate UD-style tablet parameters. The intra-application messaging interface is part of the tablet class, but this will be placed in its own class pretty soon.

- "LaunchHelper" is a simple C program that the TabletMagic preference pane uses to perform any actions that require escalated privileges. The preference pane asks for an admin password on first-run and tells LaunchHelper to suid itself. From then on no password is required.

- The "TabletMagic" preference pane is an Objective-C / Cocoa plugin that provides a user interface to start, stop, and configure TabletMagic. It is currently localized in English, French, and Italian.

Notes
-----
Some kinds of drivers –USB for example– need to run in the kernel, but TabletMagic doesn't require a kernel extension. The daemon can freely run in user space without any of the other components present.

TabletMagic uses CFMessagePort for messaging between the daemon and preference pane. However, the prefpane and daemon run in different "bootstrap domains" when the daemon is auto-started. Although the daemon can receive messages from the preference pane as soon as they are sent, the preference pane must use synchronous messaging and poll for any messages sent by the daemon. Presumably this could be worked around with Unix domain sockets, but I've had no luck so far in that approach.

The code is probably fine in terms of efficiency, but it could use an overhaul in terms of standards, encapsulation, and doxygen comments.

Troubleshooting
---------------
For users with _USB Serial Adapters_ the most common problem is the driver, so make sure to use the latest drivers available for your hardware variety. USB Serial Adapters are pretty generic, so usually the chip maker's reference driver will work even if the branded driver doesn't. See the [TabletMagic Serial Adapters Page](http://www.thinkyhead.com/tabletmagic/adapters) for more links and information.
