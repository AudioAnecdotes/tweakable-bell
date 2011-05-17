//
//  sliders.c
//  
//
//  Created by Robert Quattlebaum on 5/15/11.
//

#include "sliders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#define SLIDERS_BUFFER_SIZE		(1024)

struct sliders_s {
	int fd;
	enum {
		STATE_NORMAL = 0,
	} state;
	int buffer_size;
	char buffer[SLIDERS_BUFFER_SIZE + 1];
	sliders_value_t values[SLIDERS_COUNT];
	sliders_changed_callback_t callback;
	void *callback_context;
	struct termios original_termios;

};

sliders_t
sliders_create(const char *dev_path, int baud_rate)
{
	sliders_t ret = NULL;
	struct termios t;

	if (!dev_path)
		goto bail;

	if (!baud_rate)
		baud_rate = 115200;

	ret = calloc(sizeof(*ret), 1);

	if (!ret) {
		perror("sliders_create:calloc");
		goto bail;
	}
	// We need O_NONBLOCK here because otherwise
	// the call to open() itself might block. Let's prevent that.
	ret->fd = open(dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (ret->fd < 0) {
		perror(dev_path);
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}
	// Turn off O_NONBLOCK.
	fcntl(ret->fd, F_SETFL, fcntl(ret->fd, F_GETFL) & ~O_NONBLOCK);

	if (tcgetattr(ret->fd, &t) < 0) {
		perror("sliders_create:tcgetattr");
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}

	ret->original_termios = t;

	cfmakeraw(&t);

	if (cfsetspeed(&t, baud_rate) < 0) {
		perror("sliders_create:cfsetspeed");
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}

	t.c_cflag = CLOCAL | CREAD | CS8;
	t.c_iflag = IGNPAR | IGNBRK;
	t.c_oflag = 0;

	if (tcsetattr(ret->fd, TCSANOW, &t) < 0) {
		perror("sliders_create:tcsetattr");
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}

	if (write(ret->fd, "Rlp", 3) < 3) {
		perror("sliders_create:write");
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}

	usleep(1000 * 500);

	if (write(ret->fd, "d", 3) < 3) {
		perror("sliders_create:write");
		sliders_release(ret);
		ret = NULL;
		goto bail;
	}

bail:
	return ret;
}

void
sliders_release(sliders_t x)
{
	if (x) {
		if (x->fd > 0) {
			write(x->fd, "Rd", 3);
			usleep(1000);
			tcsetattr(x->fd, TCSANOW, &x->original_termios);
			close(x->fd);
		}
		free(x);
	}
}

sliders_status_t
sliders_process(sliders_t self)
{
	sliders_status_t ret = SLIDERS_STATUS_OK;
	ssize_t bytes_read;
	int i;

	bytes_read = read(self->fd,
		self->buffer + self->buffer_size,
		SLIDERS_BUFFER_SIZE - self->buffer_size);

	if (bytes_read < 0) {
		ret = SLIDERS_STATUS_ERRNO;
		perror("sliders_process");
		goto bail;
	} else if (!bytes_read) {
		goto bail;
	}

	self->buffer_size += bytes_read;

	for (i = 0; i < self->buffer_size; i++) {
		char c = self->buffer[i];
		if (c == 'F') {
			// Slave failure update.
			// Two characters.
			if (self->buffer_size - 1 <= i)
				break;
			fprintf(stderr, "sliders_process: Slave '%c' failure\n",
				self->buffer[i + 1]);
			//write(self->fd,"R",1);
			i++;
		} else if (isdigit(c) || ((c >= 'a') && (c <= 'f'))) {
			int slider_index;
			// Slider update.
			// Seven Characters.
			if (self->buffer_size - 6 <= i)
				break;
			self->buffer[i + 2] = 0;
			slider_index = strtol(self->buffer + i, NULL, 16);

			if (slider_index >= SLIDERS_COUNT) {
				fprintf(stderr, "sliders_process: Bad slider index %d\n",
					slider_index);
			} else {
				self->buffer[i + 7] = 0;
				self->values[slider_index] =
					strtol(self->buffer + i + 3, NULL, 16);
			}

			if (self->callback) {
				(*self->callback) (self->callback_context,
					self, slider_index, self->values[slider_index]
					);
			}

			i += 6;
		}
	}

	if (i < self->buffer_size) {
		// Not all of the data was read out of the buffer.
		// We need to move the unread data to the start of
		// the buffer and update the buffer size.
		memmove(self->buffer, self->buffer + i, self->buffer_size - i);
		self->buffer_size -= i;
	} else {
		// All of the data was read, just update the buffer size.
		self->buffer_size = 0;
	}

bail:
	return ret;
}

void
sliders_set_callback(sliders_t self,
	sliders_changed_callback_t callback, void *context)
{
	self->callback = callback;
	self->callback_context = context;
}

int
sliders_get_fd(sliders_t self)
{
	return self->fd;
}

sliders_value_t
sliders_get_value(sliders_t self, int slider)
{
	return self->values[slider];
}
