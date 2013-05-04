/*
gub211.c
Copyright (c) 2013 RyanC <code@ryanc.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the
      names of contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
user space utility to grab the device connected to a IOGEAR GUB211 auto
sharing usb switch. The device shows up with VID=0x2101, PID=0x0231. Since
that vendor id belongs to actionstar, I would expect this program to also
work with ActionStar's SW-221A and SW-241A, but I haven't tested it.

This will either need to run as root or you will need to do udev magic to
set the permissions to allow access to the user you're going to run it as.
*/

#include <stdio.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define GUB211_VENDORID  0x2101
#define GUB211_PRODUCTID 0x0231

/* compile with gcc -Wall -O2 gub211.c -lusb-1.0 -o gub211 */

int main() {
#ifdef GUB211_DOLIST
	libusb_device **devs;
	ssize_t cnt;
#endif
	libusb_context *ctx = NULL;
	libusb_device_handle *handle;
	int r;
	r = libusb_init(&ctx);
	if (r < 0) {
		fprintf(stderr, "Error initializing libusb: %d\n", r);
		return 1;
	}
	libusb_set_debug(ctx, 3);
#ifdef GUB211_DOLIST
	cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0) {
		fprintf(stderr, "Error getting device list: %zd\n", cnt);
		return 1;
	}
#endif
	handle = libusb_open_device_with_vid_pid(ctx, GUB211_VENDORID, GUB211_PRODUCTID);

#ifdef GUB211_DOLIST
	libusb_free_device_list(devs, cnt);
#endif
	if (libusb_kernel_driver_active(handle, 0) == 1) {
		if ((r = libusb_detach_kernel_driver(handle, 0)) < 0) {
			fprintf(stderr, "Detaching kernel driver from interface 0 failed: %d\n", r);
			return 1;
		}
	}
	if (libusb_kernel_driver_active(handle, 1) == 1) {
		if ((r = libusb_detach_kernel_driver(handle, 1)) < 0) {
			fprintf(stderr, "Detaching kernel driver from interface 1 failed: %d\n", r);
			return 1;
		}
	}
	if ((r = libusb_claim_interface(handle, 0)) < 0) {
		fprintf(stderr, "Claiming interface 0 failed: %d\n", r);
		return 1;
	}
	if ((r = libusb_claim_interface(handle, 1)) < 0) {
		fprintf(stderr, "Claiming interface 1 failed: %d\n", r);
		return 1;
	}
	/* magic numbers obtained by sniffing the usb bus */
	if ((r = libusb_control_transfer(handle, 0x21, 0x09, 0x0203, 0x0001, (unsigned char *)"\x03\x5d\x42\x00\x00", 5, 100)) != 5) {
		fprintf(stderr, "Control transfer failed: %d\n", r);
		return 1;
	}
	if ((r = libusb_release_interface(handle, 0)) < 0) {
		fprintf(stderr, "Releasing interface 0 failed: %d\n", r);
		return 2;
	}
	if ((r = libusb_release_interface(handle, 1)) < 0) {
		fprintf(stderr, "Releasing interface 1 failed: %d\n", r);
		return 2;
	}
	libusb_close(handle);
	libusb_exit(ctx);
	return 0;
}
