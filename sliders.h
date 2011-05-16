//
//  sliders.h
//  
//
//  Created by Robert Quattlebaum on 5/15/11.
//

#ifndef __SLIDERS_H__
#define __SLIDERS_H__ 1

#if !defined(__BEGIN_DECLS) || !defined(__END_DECLS)
#if defined(__cplusplus)
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#define SLIDERS_MIN_VALUE		(sliders_value_t)(0x8000)
#define SLIDERS_MAX_VALUE		(sliders_value_t)(0x7FFF)

#define SLIDERS_COUNT			(30)

__BEGIN_DECLS

enum {
	SLIDERS_STATUS_OK = 0,
	SLIDERS_STATUS_FAILURE = 1,
	SLIDERS_STATUS_ERRNO = 2,
};

typedef int sliders_status_t;

typedef int16_t sliders_value_t;

struct sliders_s;
typedef struct sliders_s *sliders_t;

typedef void (*sliders_changed_callback_t)(void* context,sliders_t sliders,int slider_index,sliders_value_t value);

sliders_t sliders_create(const char* dev_path, int baud_rate);
void sliders_release(sliders_t x);

sliders_status_t sliders_process(sliders_t self);

void sliders_set_callback(sliders_t self,sliders_changed_callback_t callback, void* context);

int sliders_get_fd(sliders_t self);

sliders_value_t sliders_get_value(sliders_t self,int slider);

__END_DECLS


#endif // #ifndef __SLIDERS_H__
