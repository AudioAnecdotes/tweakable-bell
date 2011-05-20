//
//  main.c
//
//
//  Created by Robert Quattlebaum on 5/15/11.
//

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <termios.h>

#include <pablio.h>

#include "bell.h"
#include "sliders.h"

#ifndef MAX
#define MAX(a, \
        b) ({ __typeof__(a)_a = (a); __typeof__(b)_b = (b); _a > \
              _b ? _a : _b; })
#endif

static bool gDidGetInterrupt;
static int gInterruptFDs[2];

void
sliders_changed_callback(
	void *context,
	sliders_t sliders, int slider_index, sliders_value_t v
) {
	aa_bell_t bell = (aa_bell_t)context;
	double value = ((float)v -
	        (float)SLIDERS_MIN_VALUE) / ((float)SLIDERS_MAX_VALUE -
	        (float)SLIDERS_MIN_VALUE);
	int type = slider_index / 3;
	int index = slider_index % 3;

	if(index < aa_bell_get_mode_count(bell)) {
		if(type == 0)
			aa_bell_set_mode_freq(bell, index, value * 5000);
		else if(type == 1)
			aa_bell_set_angular_decay(bell, index, (1.0 - value) * 50);
		else if(type == 2)
			aa_bell_set_gain(bell, 0, index, value * 10);

		aa_bell_compute_reson_coeff(bell, index);
		aa_bell_compute_location(bell, index);

		//printf("slider=%d index=%d type=%d value=%f\n",slider_index,index,type,value);
	}
}

void
received_interrupt(int unused) {
	gDidGetInterrupt = true;
	signal(SIGINT, NULL);
	fprintf(stderr, "Received Interrupt...\n");
	write(gInterruptFDs[1], " ", 1);
}

int
main(void) {
	int ret = 1;

	int srate = AA_BELL_DEFAULT_SRATE;
	int bufferSize = 10;
	struct termios Otty, Ntty;
	sliders_t sliders;
	aa_bell_t bell;
	PABLIO_Stream *outStream = NULL;

	signal(SIGINT, &received_interrupt);

	{
		int status;
		tcgetattr(0, &Otty);
		Ntty = Otty;

		Ntty.c_lflag = ISIG | ECHONL;   // line settings (no echo)

		Ntty.c_cc[VMIN] = 0;            // minimum time to wait
		Ntty.c_cc[VTIME] = 0;           // minimum characters to wait for
		status = tcsetattr(0, TCSANOW, &Ntty);
		if(status)
			fprintf(stderr, "tcsetattr returned %d\n", status);
	}

	// Create pipe'd file descriptors
	// for signalling when we want to interrupt.
	pipe(gInterruptFDs);

//	bell = aa_bell_create_from_file("glass.sy",bufferSize,srate);
//	bell = aa_bell_create_from_file("wok.sy",bufferSize,srate);
	bell = aa_bell_create(10, 1, bufferSize, srate);

	if(!bell) {
		fprintf(stderr, "Unable to make bell object\n");
		goto bail;
	}

	sliders = sliders_create("/dev/tty.usbserial-pplug01", 0);

	if(!sliders) {
		fprintf(stderr, "Unable to make sliders object\n");
		goto bail;
	} else {
		sliders_set_callback(sliders, &sliders_changed_callback, bell);
	}

	OpenAudioStream(&outStream,
		srate,
		paFloat32,
		PABLIO_WRITE | PABLIO_MONO);

	if(!outStream) {
		fprintf(stderr, "Unable to open output audio stream\n");
		goto bail;
	}

	aa_bell_add_energy(bell, 0.5, 0.002);

	// select() based runloop
	while(!gDidGetInterrupt) {
		struct timeval timeout = {
			.tv_usec	=
			    ((long long)bufferSize * 1000000ll /
			        (long long)srate) * 0.65
		};
		int fd = sliders_get_fd(sliders);
		fd_set readfs, exceptfs;
		float buffer[bufferSize];
		float total;

		FD_ZERO(&readfs);
		FD_ZERO(&exceptfs);

		FD_SET(fd, &readfs);
		FD_SET(fd, &exceptfs);
		FD_SET(0, &readfs);
		FD_SET(0, &exceptfs);
		FD_SET(gInterruptFDs[0], &readfs);
		FD_SET(gInterruptFDs[0], &exceptfs);

		select(MAX(fd, gInterruptFDs[0]) + 1, &readfs, NULL, &exceptfs,
			&timeout);

		if(FD_ISSET(gInterruptFDs[0], &readfs))
			break;

		if(FD_ISSET(0, &readfs)) {
			char c;
			read(0, &c, 1);
			if(c == ' ')
				aa_bell_add_energy(bell, 0.5, 0.002);
			else if(c == 'q')
				break;
			else if(c == 'd')
				aa_bell_dump(bell, stdout);
		}
		if(FD_ISSET(fd, &readfs))
			if(sliders_process(sliders) != SLIDERS_STATUS_OK)
				break;
		if(FD_ISSET(fd, &exceptfs)) {
			fprintf(stderr, "select() got error on fd %d\n", fd);
			break;
		}

		total = aa_bell_compute_sound_buffer(bell, buffer);

		WriteAudioStream(outStream, buffer, bufferSize);
	}

	ret = 0;

bail:
	close(gInterruptFDs[0]);
	close(gInterruptFDs[1]);
	if(bell)
		aa_bell_release(bell);
	if(sliders)
		sliders_release(sliders);
	if(outStream)
		CloseAudioStream(outStream);

	tcsetattr(0, TCSANOW, &Otty);
	return 0;
}
