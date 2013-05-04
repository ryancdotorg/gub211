#!/usr/bin/env python

import usb.core
import usb.util

'''
user space utility to grab the device connected to a IOGEAR GUB211 auto
sharing usb switch. The device shows up with VID=0x2101, PID=0x0231. Since
that vendor id belongs to actionstar, I would expect this program to also
work with ActionStar's SW-221A and SW-241A, but I haven't tested it.

This will either need to run as root or you will need to do udev magic to
set the permissions to allow access to the user you're going to run it as.
'''

# Find the device.
dev = usb.core.find(idVendor=0x2101, idProduct=0x0231)

# Detatch kernel drivers if needed.
if dev.is_kernel_driver_active(0):
    dev.detach_kernel_driver(0)

if dev.is_kernel_driver_active(1):
    dev.detach_kernel_driver(1)

# Copypasta from the pyusb 1.0 tutorial, with the mistakes fixed. :p
dev.set_configuration()

cfg = dev.get_active_configuration()
interface_number = cfg[(0,0)].bInterfaceNumber
alternate_setting = usb.control.get_interface(dev, interface_number)

intf = usb.util.find_descriptor(
    cfg,
    bInterfaceNumber = interface_number,
    bAlternateSetting = alternate_setting
)


# Claim the interface so we can talk to it
usb.util.claim_interface(dev, intf)

# Send the magic 'switch to me' command, found by sniffing the USB bus.
dev.ctrl_transfer(0x21, 0x09, 0x0203, 0x0001, "\x03\x5d\x42\x00\x00")
