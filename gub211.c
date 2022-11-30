/*
gub211.c
Copyright © 2013-2022 Ryan Castellucci

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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#define GUB211_VENDORID  0x2101
#define GUB211_PRODUCTID 0x0231
#define MAX_PORT_DEPTH 7
#define MAX_PATH_STRLEN (5+MAX_PORT_DEPTH*4)

/* compile with gcc -Wall -O2 gub211.c -lusb-1.0 -o gub211 */

int ctrl_transfer(
libusb_device_handle *handle,
uint8_t bmRequestType, uint8_t bRequest,
uint16_t wValue, uint16_t wIndex,
unsigned char *data, uint16_t wLength,
unsigned int timeout) {
	int r;
	for (int i = 0; i < 2; ++i) {
		if (libusb_kernel_driver_active(handle, i)) {
			if ((r = libusb_detach_kernel_driver(handle, i)) != LIBUSB_SUCCESS) {
				fprintf(stderr, "Detaching kernel driver from interface %d failed: %s\n", i, libusb_error_name(r));
				return r;
			}
		}

		if ((r = libusb_claim_interface(handle, i)) != LIBUSB_SUCCESS) {
			fprintf(stderr, "Claiming interface %d failed: %s\n", i, libusb_error_name(r));
			return r;
		}
	}

	if ((r = libusb_control_transfer(handle, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout)) != wLength) {
		fprintf(stderr, "Control transfer failed: %s\n", libusb_error_name(r));
		return r >= 0 ? LIBUSB_ERROR_OTHER : r;
	}

	for (int i = 0; i < 2; ++i) {
		if ((r = libusb_release_interface(handle, i)) != LIBUSB_SUCCESS) {
			fprintf(stderr, "Releasing interface %d failed: %s\n", i, libusb_error_name(r));
			return r;
		}
	}

	return LIBUSB_SUCCESS;
}

int switch_to(libusb_device_handle *handle) {
	return ctrl_transfer(handle, 0x21, 0x09, 0x0203, 0x0001, (unsigned char *)"\x03\x5d\x42\x00\x00", 5, 100);
}

ssize_t bnprintf(char **d, size_t *n, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	// length excluding null byte
	ssize_t len = vsnprintf(*d, *n, format, ap);
	va_end(ap);
	if (len < 0 || (size_t)len >= *n) return -1;
	*n -= len;
	*d += len;
	return len;
}

int path_str(char *str, size_t len, libusb_device *dev) {
	int r;
	char *ptr = str;
	uint8_t bus = libusb_get_bus_number(dev);
	uint8_t port_numbers[MAX_PORT_DEPTH+1];

	uint8_t n_ports;
	if ((r = libusb_get_port_numbers(dev, port_numbers, MAX_PORT_DEPTH)) < 0) {
		fprintf(stderr, "Error getting port numbers: %s\n", libusb_error_name(r));
		return r;
	}
	n_ports = (uint8_t)r;

	if (bnprintf(&ptr, &len, "%u:", bus) < 0) {
		return LIBUSB_ERROR_OTHER;
	}

	for (int j = 0; j < n_ports; ++j) {
		if (bnprintf(&ptr, &len, "%s%u", j ? "/" : "", port_numbers[j]) < 0) {
			return LIBUSB_ERROR_OTHER;
		}
	}

	return LIBUSB_SUCCESS;
}

int matches_vid_pid(libusb_device *dev, uint16_t vid, uint16_t pid) {
	int r;
	struct libusb_device_descriptor desc[1] = {0};
	if ((r = libusb_get_device_descriptor(dev, desc)) != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error getting device descriptor: %s\n", libusb_error_name(r));
		return r;
	}

	return desc->idVendor == vid && desc->idProduct == pid ? 1 : 0;
}

int print_list(ssize_t cnt, libusb_device **devs) {
	int r;
	char path[MAX_PATH_STRLEN];

	for (ssize_t i = 0; i < cnt; ++i) {
		if ((r = matches_vid_pid(devs[i], GUB211_VENDORID, GUB211_PRODUCTID)) == 1) {
			if ((r = path_str(path, MAX_PATH_STRLEN, devs[i])) != LIBUSB_SUCCESS) {
				fprintf(stderr, "Failed to get path: %d\n", r);
				return r;
			}
			printf("device at: %s\n", path);
		} else if (r < 0) {
			return r;
		}
	}

	return LIBUSB_SUCCESS;
}

int main(int argc, char *argv[]) {
	int r;
	ssize_t cnt;

	libusb_device **devs = NULL;
	libusb_context *ctx = NULL;
	libusb_device_handle *handle = NULL;

	if ((r = libusb_init(&ctx)) != LIBUSB_SUCCESS) {
		fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(r));
		return 1;
	}

	libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 3);

	if ((cnt = libusb_get_device_list(ctx, &devs)) < 0) {
		fprintf(stderr, "Error getting device list: %s\n", libusb_error_name(cnt));
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "list") == 0) {
		if ((r = print_list(cnt, devs)) != LIBUSB_SUCCESS) {
			goto cleanup_failure;
		}
	} else if (argc == 1 || (argc == 2 && strcmp(argv[1], "first") == 0)) {
		for (ssize_t i = 0; i < cnt; ++i) {
			if ((r = matches_vid_pid(devs[i], GUB211_VENDORID, GUB211_PRODUCTID)) == 1) {
				if ((r = libusb_open(devs[i], &handle)) != LIBUSB_SUCCESS) {
					fprintf(stderr, "Could not open device: %s\n",  libusb_error_name(r));
					goto cleanup_failure;
				}
				if ((r = switch_to(handle)) != LIBUSB_SUCCESS) { goto cleanup_failure; }
				goto cleanup_success;
			} else if (r < 0) {
				goto cleanup_failure;
			}
		}
		fprintf(stderr, "no devices found\n");
		goto cleanup_failure;
	} else if (argc == 2 && strcmp(argv[1], "only") == 0) {
		ssize_t only = -1;
		for (ssize_t i = 0; i < cnt; ++i) {
			if ((r = matches_vid_pid(devs[i], GUB211_VENDORID, GUB211_PRODUCTID)) == 1) {
				if (only == -1) {
					only = i;
				} else {
					fprintf(stderr, "multiple devices found\n");
					goto cleanup_failure;
				}
			} else if (r < 0) {
				goto cleanup_failure;
			}
		}
		if (only == -1) {
			fprintf(stderr, "no devices found\n");
			goto cleanup_failure;
		}
		if ((r = libusb_open(devs[only], &handle)) != LIBUSB_SUCCESS) {
			fprintf(stderr, "Could not open device: %s\n",  libusb_error_name(r));
			goto cleanup_failure;
		}
		if ((r = switch_to(handle)) != LIBUSB_SUCCESS) { goto cleanup_failure; }
		goto cleanup_success;
	} else if (argc == 2) {
		char path[MAX_PATH_STRLEN];
		for (ssize_t i = 0; i < cnt; ++i) {
			if ((r = matches_vid_pid(devs[i], GUB211_VENDORID, GUB211_PRODUCTID)) == 1) {
				if ((r = path_str(path, MAX_PATH_STRLEN, devs[i])) != LIBUSB_SUCCESS) {
					fprintf(stderr, "Failed to get path: %d\n", r);
				} else if (strcmp(path, argv[1]) == 0) {
					if ((r = libusb_open(devs[i], &handle)) != LIBUSB_SUCCESS) {
						fprintf(stderr, "Could not open device: %s\n",  libusb_error_name(r));
						goto cleanup_failure;
					}
					if ((r = switch_to(handle)) != LIBUSB_SUCCESS) { goto cleanup_failure; }
					goto cleanup_success;
				}
			} else if (r < 0) {
				goto cleanup_failure;
			}
		}
		fprintf(stderr, "no matching device\n");
		goto cleanup_failure;
	}

cleanup_success:
	r = EXIT_SUCCESS;
	goto cleanup_done;
cleanup_failure:
	r = EXIT_FAILURE;
cleanup_done:
	if (devs != NULL && cnt > 0) { libusb_free_device_list(devs, cnt); }
	if (handle != NULL) { libusb_close(handle); }
	libusb_exit(ctx);
	return r;
}
