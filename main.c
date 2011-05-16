//
//  main.c
//  
//
//  Created by Robert Quattlebaum on 5/15/11.
//

#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include "sliders.h"
#include <poll.h>
#include <signal.h>

void sliders_changed_callback(void* context,sliders_t sliders,int slider_index,sliders_value_t value) {
	printf("%d: %d\n",slider_index,value);
}

bool gDidGetInterrupt;

void
received_interrupt(int unused) {
	gDidGetInterrupt = true;
	signal(SIGINT,NULL);
	fprintf(stderr,"Received Interrupt...\n");
}

int
main(void) {
	int ret = 1;

	sliders_t sliders;

	signal(SIGINT,&received_interrupt);

	sliders = sliders_create("/dev/tty.usbserial-pplug01", 0);

	if(!sliders) {
		fprintf(stderr,"Unable to make sliders object\n");
		goto bail;
	}

	sliders_set_callback(sliders,&sliders_changed_callback,NULL);

	// Run Loop
#if 0
	while(!gDidGetInterrupt) {
		if(sliders_process(sliders)!=SLIDERS_STATUS_OK)
			break;
	}
#else
	// poll() based runloop
	while(!gDidGetInterrupt) {
		struct pollfd polltable[] = {
			{ sliders_get_fd(sliders), POLLIN, 0 },
		};

		if(poll(polltable,sizeof(polltable)/sizeof(*polltable),1000)<0)
			break;

		if(polltable[0].revents) {
			if(sliders_process(sliders)!=SLIDERS_STATUS_OK) {
				break;
			}
		}
	}
#endif

	ret = 0;

bail:
	sliders_release(sliders);
	return 0;
}
