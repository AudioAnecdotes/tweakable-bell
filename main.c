//
//  main.c
//  
//
//  Created by Robert Quattlebaum on 5/15/11.
//

#include "main.h"
#include <stdio.h>
#include <sys/errno.h>
#include <stdbool.h>
#include "sliders.h"
#include <signal.h>

#include <poll.h>
#include <sys/select.h>

#ifndef MAX
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif

void
sliders_changed_callback(void *context, sliders_t sliders, int slider_index,
						 sliders_value_t value)
{
	printf("%d: %0.01f%%\n", slider_index,
		   100.0 * ((float)value -
					(float)SLIDERS_MIN_VALUE) / ((float)SLIDERS_MAX_VALUE -
												 (float)SLIDERS_MIN_VALUE));
}

bool gDidGetInterrupt;
int gInterruptFDs[2];

void
received_interrupt(int unused)
{
	gDidGetInterrupt = true;
	signal(SIGINT, NULL);
	fprintf(stderr, "Received Interrupt...\n");
	write(gInterruptFDs[1], " ", 1);
}

int
main(void)
{
	int ret = 1;

	sliders_t sliders;

	signal(SIGINT, &received_interrupt);

	// Create pipe'd file descriptors
	// for signalling when we want to interrupt.
	pipe(gInterruptFDs);

	sliders = sliders_create("/dev/tty.usbserial-pplug01", 0);

	if (!sliders) {
		fprintf(stderr, "Unable to make sliders object\n");
		goto bail;
	}

	sliders_set_callback(sliders, &sliders_changed_callback, NULL);

	// Run Loop
#if 0
	while (!gDidGetInterrupt) {
		if (sliders_process(sliders) != SLIDERS_STATUS_OK)
			break;
	}
#elif 1
	// select() based runloop
	while (!gDidGetInterrupt) {
		struct timeval timeout = {.tv_sec = 10 };
		int fd = sliders_get_fd(sliders);
		fd_set readfs, exceptfs;

		FD_ZERO(&readfs);
		FD_ZERO(&exceptfs);

		FD_SET(fd, &readfs);
		FD_SET(fd, &exceptfs);
		FD_SET(gInterruptFDs[0], &readfs);
		FD_SET(gInterruptFDs[0], &exceptfs);

		select(MAX(fd, gInterruptFDs[0]) + 1, &readfs, NULL, &exceptfs,
			   &timeout);

		if (FD_ISSET(gInterruptFDs[0], &readfs)) {
			break;
		} else if (FD_ISSET(fd, &readfs)) {
			if (sliders_process(sliders) != SLIDERS_STATUS_OK) {
				break;
			}
		} else if (FD_ISSET(fd, &exceptfs)) {
			fprintf(stderr, "select() got error on fd %d\n", fd);
			break;
		}
	}
#else
	// poll() based runloop
	while (!gDidGetInterrupt) {
		int fd = sliders_get_fd(sliders);

		struct pollfd polltable[] = {
			{fd, POLLIN, 0},
			{gInterruptFDs[0], POLLIN, 0},
		};

		int events =
			poll(polltable, sizeof(polltable) / sizeof(*polltable), 1000);

		if (events) {
			if (polltable[0].revents & POLLIN) {
				if (sliders_process(sliders) != SLIDERS_STATUS_OK) {
					break;
				}
			} else if (polltable[0].revents & POLLNVAL) {
				fprintf(stderr, "poll() returned POLLNVAL...?\n");
				break;
			}
		}
	}
#endif

	ret = 0;

bail:
	close(gInterruptFDs[0]);
	close(gInterruptFDs[1]);
	sliders_release(sliders);
	return 0;
}
