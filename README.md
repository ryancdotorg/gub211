# Description

This is a libusb-1.0 based user space utility to grab the device connected
to a IOGEAR GUB211 auto sharing usb switch in Linux. The device shows up with
VID=0x2101, PID=0x0231. Since that vendor id belongs to actionstar, I would
expect this program to also work with ActionStar's SW-221A and SW-241A, but
I haven't tested it. Monoprice's "USB 2.0 2 to 1 Auto Printer Sharing Switch"
with product id 5151 has also been tested and works.

This will either need to run as root (you could make it suid) or you will need
to use udev rules to set the permissions. The included udev rules allow access
to all users, but can be modified (examples included) to allow only a specific
user or group.

# Install

Compile with:

    gcc -Wall -O2 gub211.c -lusb-1.0 -o gub211

then copy the gub211 binary. Copy the 99-gub211.rules to /etc/udev/rules.d and
edit if desired.

# Usage

List devices: `gub211 list`

Switch to the first device found: `gub211` or `gub211 first`

Switch to the only device: `gub211 only`

Switch to specific device: `gub211 PATH`
