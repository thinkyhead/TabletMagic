#
# TabletPCEnabler.sh ($Id: TabletPCEnabler.sh,v 1.3 2008/07/31 16:28:24 slurslee Exp $)
#
# Used by TabletMagic to enable the serial port of a TabletPC
#

#
# Get the digitizer name from ioreg -l (or -lx)
#
DIGI=`ioreg -l | grep -A15 -E "\+-o (DIGI|WACM|COMA)" | grep -m1 \"name\" | sed -E "s/.*<\"?([^\">]+)\"?>/\\1/"`
if [ "$DIGI" == "" ]; then
	DIGI=`ioreg -l | grep -E "\"name\" = <\"(WAC|FUJ|574143|46554a)[0-9A-F]+\">" | sed -E "s/.*<\"?([^\">]+)\"?>/\\1/"`
fi

if [ "$DIGI" == "" ]; then
	#
	# return "notfound" value - no digitizer hardware!
	#
	echo "none"
	exit
fi

#
# Translate a hex coded string into ASCII
#
if [ ${#DIGI} -gt 12 ]; then
		DIGI=`echo $DIGI | xxd -r -p`
fi

#
# Enter the kernel extension directory
#
cd /System/Library/Extensions/Apple16X50Serial.kext/Contents/PlugIns/Apple16X50ACPI.kext/Contents/

#
# Make sure the proper key exists
#
OLDNAME=`defaults read $PWD/Info IOKitPersonalities | grep IONameMatch | sed -e "s/[ \\t]*IONameMatch = \([^;]*\);/\\1/"`

#
# Replace the IONameMatch value for the serial kext
#
sed -e "s/$OLDNAME/$DIGI/g" Info.plist >/tmp/digifix.tmp
mv -f /tmp/digifix.tmp /Users/Shared/Info.plist

#
# Fix the file's permissions!
#
chown root:wheel Info.plist
chmod 644 Info.plist

#
# Delete the kernel cache
#
rm /System/Library/Extensions.mkext

#
# return the digitizer string
#
echo $DIGI
