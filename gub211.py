#!/usr/bin/env python3

from sys import exit, argv, stdin, stderr, stdout, version_info
from functools import partial
eprint = partial(print, file=stderr)

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

def _ctrl_transfer(dev, *args):
    try:
        # detatch kernel driver
        for n in (0, 1):
            if dev.is_kernel_driver_active(n):
                dev.detach_kernel_driver(n)

        # copypasta from the pyusb 1.0 tutorial, with the mistakes fixed
        dev.set_configuration()

        cfg = dev.get_active_configuration()
        if_num = cfg[(0,0)].bInterfaceNumber

        intf = usb.util.find_descriptor(
            cfg,
            bInterfaceNumber = if_num,
            bAlternateSetting = usb.control.get_interface(dev, if_num),
        )

        # claim the interface so we can talk to it
        usb.util.claim_interface(dev, intf)

        # send command
        dev.ctrl_transfer(*args)

        # release
        usb.util.release_interface(dev, intf)
        usb.util.dispose_resources(dev)

        return True
    except:
        dev.reset()

        return False

def switch(dev):
    # send the magic 'switch to me' command, found by sniffing the USB bus
    # bmRequestType 0b0_01_00001 (dir: host to device, type: class, recip: interface)
    # bRequest 0x09 HID_SET_REPORT?
    # wValue 0x0203
    # wIndex 0x0001
    _ctrl_transfer(dev, 0x21, 0x09, 0x0203, 0x0001, "\x03\x5d\x42\x00\x00")

def find():
    return list(usb.core.find(find_all=True, idVendor=0x2101, idProduct=0x0231))

def path(dev):
    return f'{dev.bus}:{"/".join(map(str, dev.port_numbers))}'

def _match(path):
    bus, _, ports = path.partition(':')
    if not (bus and ports):
        return None, None

    return int(bus), tuple(map(int, ports.split('/')))

def main(arg=None):
    devs = find()

    if arg == 'list':
        for dev in devs:
            print('device at:', path(dev))
    elif not len(devs):
        eprint('no devices found')
        return -1
    elif arg == 'only':
        if len(devs) != 1:
            eprint('multiple devices found')
            return -1
        switch(devs[0])
    elif arg == 'first' or arg is None:
        switch(devs[0])
    else:
        bus, ports = _match(arg)
        for dev in devs:
            if dev.bus == bus and dev.port_numbers == ports:
                switch(dev)
                return 0

        eprint('no matching device')
        return -1

    return 0

if __name__ == '__main__':
    args = argv[1:]
    exit(main(*args))
