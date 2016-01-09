/**
 * TMSerialPort.cpp
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

#include "TMSerialPort.h"
#include "TabletSettings.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#if LOG_STREAM_TO_FILE
extern FILE* logfile;
#endif

TMSerialPort::TMSerialPort() {
    output = NULL;
    fd = kSerialError;          // No serial port yet
    (void)SetDefaultParameters();
}

TMSerialPort::~TMSerialPort() {
    Close();
}

bool TMSerialPort::SetDefaultParameters() {
    return SetParameters(B9600, CS8, 0, 1, false, false);
}

bool TMSerialPort::SetParameters(TabletSettings *sett) {
    speed_t     rate[] = { B2400, B4800, B9600, B19200, B38400 };
    int         parity[] = { 0, 0, 1, 2 };

    return SetParameters(rate[sett->baud_rate], (sett->data_bits ? CS8 : CS7), parity[sett->parity], (sett->stop_bits ? 2 : 1), sett->cts, sett->dsr);
}

bool TMSerialPort::SetParameters(speed_t speed, tcflag_t databits, int parity, int stopbits, bool cts, bool dsr) {
    openSpeed       = speed;
    openDataBits    = databits;
    openParity      = parity;
    openStopBits    = stopbits;
    openCTS         = cts;
    openDSR         = dsr;

    return ApplySettings();
}

int TMSerialPort::ReInit(TabletSettings *sett) {
    Close();
    (void)SetParameters(sett);
    return Open();
}

int TMSerialPort::Open(char *filepath) {
    int             handshake;
    struct termios  attribs;

    if (filepath != NULL)
        strcpy(deviceFilePath, filepath);

    strcpy(shortName, deviceFilePath + (strstr(deviceFilePath, "/dev/cu.") ? 8 : (strstr(deviceFilePath, "/dev/") ? 5 : 0)));

    // Open the serial port read/write, with no controlling terminal,
    // and don't wait for a connection.
    // The O_NONBLOCK flag also causes subsequent I/O on the device to
    // be non-blocking.
    // See open(2) ("man 2 open") for details.

    fd = open(deviceFilePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == kSerialError) {
        if (output != NULL)
            fprintf(output, "Error opening serial port %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        goto error;
    }

    // Note that open() follows POSIX semantics: multiple open() calls to
    // the same file will succeed unless the TIOCEXCL ioctl is issued.
    // This will prevent additional opens except by root-owned processes.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(fd, TIOCEXCL) == kSerialError) {
        if (output != NULL)
            fprintf(output, "Error setting TIOCEXCL on %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        goto error;
    }

    // Now that the device is open, clear the O_NONBLOCK flag so
    // subsequent I/O will block.
    // See fcntl(2) ("man 2 fcntl") for details.

    if (fcntl(fd, F_SETFL, 0) == kSerialError) {
        if (output != NULL)
            fprintf(output, "Error clearing O_NONBLOCK %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        goto error;
    }

    // Get the current options and save them so we can restore the
    // default settings later.
    if (tcgetattr(fd, &originalAttribs) == kSerialError) {
        if (output != NULL)
            fprintf(output, "Error getting tty attributes %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        goto error;
    }

    // The serial port attributes such as timeouts and baud rate are set by
    // modifying the termios structure and then calling tcsetattr to
    // cause the changes to take effect. Note that the
    // changes will not take effect without the tcsetattr() call.
    // See tcsetattr(4) ("man 4 tcsetattr") for details.

    attribs = originalAttribs;

    // Print the current input and output baud rates.
    // See tcsetattr(4) ("man 4 tcsetattr") for details.

    //  if (output != NULL) {
    //      fprintf(output, "Current input baud rate is %d\n", (int) cfgetispeed(&attribs));
    //      fprintf(output, "Current output baud rate is %d\n", (int) cfgetospeed(&attribs));
    //  }

    // Set raw input (non-canonical) mode, with reads blocking until either
    // a single character has been received or a one second timeout expires.
    // See tcsetattr(4) ("man 4 tcsetattr") and termios(4) ("man 4 termios")
    // for details.

    cfmakeraw(&attribs);
    attribs.c_cc[VMIN] = 1;         // 1 byte minimum
    attribs.c_cc[VTIME] = 10;       // 1 second

    // The Baud Rate
    cfsetspeed(&attribs, openSpeed);            // Set the baud rate

    // Data Bits - 7 or 8
    attribs.c_cflag &= ~CSIZE;                  // Clear the data size bits
    attribs.c_cflag |= openDataBits;            // Set the data size

    // Parity N-O-E
    switch(openParity) {
        case 0:
            attribs.c_cflag &= ~PARENB;
            break;
        case 1:
            attribs.c_cflag |= (PARENB|PARODD);
            break;
        case 2:
            attribs.c_cflag |= PARENB;
            attribs.c_cflag &= ~PARODD;
            break;
    }

    // Stop Bits 1 or 2
    if (openStopBits == 2)
        attribs.c_cflag |= CSTOPB;
    else
        attribs.c_cflag &= ~CSTOPB;

    // Cause the new attribs to take effect immediately.
    if (tcsetattr(fd, TCSANOW, &attribs) == kSerialError) {
        if (output != NULL)
            fprintf(output, "Error setting tty attributes %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        goto error;
    }

    // Print the new input and output baud rates.
    //  if (output != NULL) {
    //      fprintf(output, "Input baud rate changed to %d\n", (int) cfgetispeed(&attribs));
    //      fprintf(output, "Output baud rate changed to %d\n", (int) cfgetospeed(&attribs));
    //  }

    // Assert Data Terminal Ready (DTR)
    // To set the port handshake lines, use the following ioctls.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.
    if (ioctl(fd, TIOCSDTR) == kSerialError && output != NULL)
        fprintf(output, "Error asserting DTR %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);

    // Clear Data Terminal Ready (DTR)
    if (ioctl(fd, TIOCCDTR) == kSerialError && output != NULL)
        fprintf(output, "Error clearing DTR %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);

    // Set the serial port lines depending on the bits set in handshake.
    handshake = TIOCM_DTR | TIOCM_RTS;

    if (openCTS)
        handshake |= TIOCM_CTS;

    if (openDSR)
        handshake |= TIOCM_DSR;

    if (ioctl(fd, TIOCMSET, &handshake) == kSerialError && output != NULL)
        fprintf(output, "Error setting handshake lines %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);

    // Store the state of the serial port lines in handshake.
    // To read the state of the port's lines, use the following ioctl.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.
    if (ioctl(fd, TIOCMGET, &handshake) == kSerialError && output != NULL)
        fprintf(output, "Error getting handshake lines %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);

    //  if (output != NULL)
    //      fprintf(output, "Handshake lines currently set to %d\n", handshake);

    // Success:
    return fd;

    // Failure:
error:
    if (fd != kSerialError) {
        close(fd);
        fd = kSerialError;
    }

    return fd;
}

//
// ApplySettings
//
bool TMSerialPort::ApplySettings() {
    bool result = true;

    if (IsOpen()) {
        result = false;
        struct termios  attribs;

        if (tcgetattr(fd, &attribs) != kSerialError) {
            cfsetspeed(&attribs, openSpeed);

            // Data Bits - 7 or 8
            attribs.c_cflag &= ~CSIZE;                  // Clear the data size bits
            attribs.c_cflag |= openDataBits;            // Set the data size

            // Parity N-O-E
            switch(openParity) {
                case 0:
                    attribs.c_cflag &= ~PARENB;
                    break;
                case 1:
                    attribs.c_cflag |= (PARENB|PARODD);
                    break;
                case 2:
                    attribs.c_cflag |= PARENB;
                    attribs.c_cflag &= ~PARODD;
                    break;
            }

            // Stop Bits 1 or 2
            if (openStopBits == 2)
                attribs.c_cflag |= CSTOPB;
            else
                attribs.c_cflag &= ~CSTOPB;

            if (tcsetattr(fd, TCSANOW|TCSAFLUSH, &attribs) != kSerialError) {
                result = true;
            }
            else {
                if (output != NULL)
                    fprintf(output, "Error setting tty attributes %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
            }
        }
        else {
            if (output != NULL)
                fprintf(output, "Error getting tty attributes %s - %s(%d).\n", deviceFilePath, strerror(errno), errno);
        }
    }
    return result;
}

//
// CloseSerialPort()
//
// Close the serial port.
// We close the serial port when switching ports or quitting.
//
void TMSerialPort::Close() {
    if (fd != kSerialError) {
        // Block until all written output has been sent from the device.
        // Note that this call is simply passed on to the serial device driver.
        // See tcsendbreak(3) ("man 3 tcsendbreak") for details.
        if (tcdrain(fd) == kSerialError && output != NULL)
            fprintf(output, "Error waiting for drain - %s(%d).\n", strerror(errno), errno);

        // It is good practice to reset a serial port back to the state in
        // which you found it. This is why we saved the original termios struct
        // The constant TCSANOW (defined in termios.h) indicates that
        // the change should take effect immediately.
        if (tcsetattr(fd, TCSANOW, &originalAttribs) == kSerialError && output != NULL)
            fprintf(output, "Error resetting tty attributes - %s(%d).\n", strerror(errno), errno);

        close(fd);
        fd = kSerialError;
    }
}

//
// Select(timeout)
//
// Timeout must be between 1 and 999999.
// (Default: 4000us, 1/250th of a second)
//
// Returns n > 0 on success
//
int TMSerialPort::Select(__darwin_suseconds_t usec) {
    if (!IsOpen()) return -1;

    fd_set          inputList;      // A file descriptor set
    struct timeval  timeout;        // A one second timeout

    FD_ZERO(&inputList);
    FD_SET(fd, &inputList);
    timeout.tv_sec  = 0;
    timeout.tv_usec = usec;

    // look for ready ports up to and including ours
    int n = select(fd + 1, &inputList, NULL, NULL, &timeout);

    if (n < 0) {
        // n < 0 when select fails
        if (output != NULL)
            fprintf(output, "[ERR ] TMSerialPort::Select() failed!\n");
    }
    else if (n == 0) {
        // n == 0 if there are no ready ports
    }
    else if (!FD_ISSET(fd, &inputList)) {
        // FD_ISSET looks for our port in the list
        n = 0;
    }

    return n;
}


//
// Read(buffer, maxlen)
//
int TMSerialPort::Read(char *buffer, int maxlen) {
    return (int)read(fd, buffer, maxlen);
}


//
// Flush()
//
int TMSerialPort::Flush() {
    if (!IsOpen()) return -1;

    char            ch[16];
    fd_set          fdsRead;
    struct timeval  timeout;

    if (tcflush(fd, TCIFLUSH) == 0)
        return 0;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while (1) {
        FD_ZERO(&fdsRead);
        FD_SET(fd, &fdsRead);
        if (select(FD_SETSIZE, &fdsRead, NULL, NULL, &timeout) <= 0)
            break;
        read(fd, &ch, sizeof(ch));
    }

    return 0;
}


//
// ReadLine(buffer, maxlen [,timeout])
//
int TMSerialPort::ReadLine(char *buffer, int maxlen, __darwin_suseconds_t usec) {
    int     numBytes = 0;
    char    *bufPtr = buffer;
    bool    gotLine = false, in_packet = false;

    // Loop while bytes await
    while (!gotLine && Select(usec) > 0) {
        // Loop through bytes received
        while (!gotLine && BytesOnPort()) {
            char temp[1];
            numBytes = Read(temp, 1);

            if (numBytes > 0) {
                char c = temp[0];

                if ((c & 0x80) != 0) {
                    in_packet = true;
#if LOG_STREAM_TO_FILE
                    fprintf(logfile, "\n");
#endif
                }
                else if (c == '~') {
                    in_packet = false;
                    bufPtr = buffer;
#if LOG_STREAM_TO_FILE
                    fprintf(logfile, "\n");
#endif
                }

#if LOG_STREAM_TO_FILE
                short b = ((short)c) & 0x00FF;
                fprintf(logfile, "-%02X", b);
#endif

                if (!in_packet) {
                    *bufPtr = c;
                    if (c == '\n' || c == '\r')
                        gotLine = true;
                    else
                        bufPtr++;
                }
            }
            else {
                gotLine = true;
                if (output != NULL) {
                    if (numBytes == 0)
                        fprintf(output, "[ERR ] No line waiting on the serial port\n");
                    else if (numBytes == kSerialError)
                        fprintf(output, "[ERR ] (%d) %s\n", errno, strerror(errno));
                }
            }

        }

    }

    *bufPtr = '\0';

#if LOG_STREAM_TO_FILE
    fprintf(logfile, "\n(%s)\n", buffer);
#endif

    return (int)strlen(buffer);
}

int TMSerialPort::Write(char *buffer, int length) {
    return (int)write(fd, buffer, length);
}

int TMSerialPort::WriteString(char *string) {
    return (int)Write(string, (int)strlen(string));
}

//
// BytesOnPort
// How many bytes are waiting on the serial port?
//
int TMSerialPort::BytesOnPort() {
    int bytes;
    ioctl(fd, FIONREAD, &bytes);
    return bytes;
}

#pragma mark -

//
// BeginPortScan(RS232Only?)
//
// Prepare a list of ports (i.e., that could have a tablet attached)
// The result is stored in the tablet object's serialPortIterator data member
// for use by FindTabletOnPort.
//
// Once again, thanks be to Apple for the samples.
//
kern_return_t TMSerialPort::BeginPortScan(bool RS232Only) {
    EndPortScan();

    kern_return_t           kernResult;
    mach_port_t             masterPort;
    CFMutableDictionaryRef  classesToMatch;

    //
    // Get the masterPort, whatever that is
    //
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (KERN_SUCCESS != kernResult) {
        if (output != NULL)
            fprintf(output, "IOMasterPort returned %d\n", kernResult);
        goto exit;
    }

    //
    // Serial devices are instances of class IOSerialBSDClient.
    // Here I search for ports with RS232Type because with my
    // Keyspan USB-Serial adapter the first port shows up
    //
    classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
    if (classesToMatch == NULL) {
        if (output != NULL)
            fprintf(output, "No BSD Serial Service?\n");
    }
    else
        CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey), RS232Only ? CFSTR(kIOSerialBSDRS232Type) : CFSTR(kIOSerialBSDAllTypes));

    //
    // Find the next matching service
    //
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, &serialPortIterator);
    if (KERN_SUCCESS != kernResult) {
        if (output != NULL)
            fprintf(output, "IOServiceGetMatchingServices returned %d\n", kernResult);
        goto exit;
    }

exit:
    return kernResult;
}

void TMSerialPort::EndPortScan() {
    if (serialPortIterator != 0) {
        IOObjectRelease(serialPortIterator);
        serialPortIterator = 0;
    }
}

bool TMSerialPort::HasValidIterator() {
    return (serialPortIterator != 0) && IOIteratorIsValid(serialPortIterator);
}

//
// GetNextPortPath
// Get the path in /dev that represents the next eligible port.
//
// Muchas gracias to Apple for this code.
//
kern_return_t TMSerialPort::GetNextPortPath() {
    io_object_t     portService;
    kern_return_t   kernResult = KERN_FAILURE;

    clearstr(deviceFilePath);

    // Keep iterating until successful
    while ((portService = IOIteratorNext(serialPortIterator))) {
        CFTypeRef   deviceFilePathAsCFString;

        // Get the callout device's path (/dev/cu.xxxxx).
        deviceFilePathAsCFString = IORegistryEntryCreateCFProperty(
                                                                   portService,
                                                                   CFSTR(kIOCalloutDeviceKey),
                                                                   kCFAllocatorDefault,
                                                                   0);

        if (deviceFilePathAsCFString) {
            Boolean result;

            // Convert the path from CFString to C string
            result = CFStringGetCString((CFStringRef)deviceFilePathAsCFString,
                                        deviceFilePath,
                                        sizeof(deviceFilePath),
                                        kCFStringEncodingASCII);

            CFRelease(deviceFilePathAsCFString);

            if (result) {
                kernResult = KERN_SUCCESS;
                break;
            }
        }

        // Release the io_service_t now that we are done with it.
        (void) IOObjectRelease(portService);
    }

    return kernResult;
}

//
// OpenNextMatchingPort
//
//  This looks for the next selected port matching a given name
//  and tries to open it.
//
//  Result:
//      true    successfully opened a matching port
//      false   ran out of openable ports
//
bool TMSerialPort::OpenNextMatchingPort(char *portname) {
    bool success = false;

    // Loop until success, the iterator dies, or we run out of paths
    while (!success && HasValidIterator() && GetNextPortPath() != KERN_FAILURE) {
        // Try the port if it matches the argument
        if ( portname == NULL || portname[0] == '\0' || NULL != strstr(deviceFilePath, portname) ) {
            // Try to open the serial port
            if (Open() != kSerialError)
                success = true;

            // Report the result
            if (output != NULL)
                fprintf(output, "\n[PORT] %s: %s\n", shortName, success ? "OPENED" : "OPEN ERROR");
        }
    }

    return success;
}

