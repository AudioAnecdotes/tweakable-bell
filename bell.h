//
//  bell.h
//

#ifndef __AA_BELL_H__
#define __AA_BELL_H__ 1

#if !defined(__BEGIN_DECLS) || !defined(__END_DECLS)
#if defined(__cplusplus)
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS \
	}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

#define AA_BELL_DEFAULT_SRATE       (44100.0f)

struct aa_bell_s;
typedef struct aa_bell_s *aa_bell_t;

aa_bell_t aa_bell_create(
	int mode_count, int point_count, int bufferSize, int srate);

aa_bell_t aa_bell_create_from_file(
	const char *path, int bufferSize, int srate);
void aa_bell_release(aa_bell_t x);

void aa_bell_set_mode_freq(
	aa_bell_t self, int res_index, float val);
void aa_bell_set_angular_decay(
	aa_bell_t self, int res_index, float val);
void aa_bell_set_gain(
	aa_bell_t self, int point, int mode, float val);
int aa_bell_get_mode_count(aa_bell_t self);
int aa_bell_get_point_count(aa_bell_t self);
int aa_bell_get_used_mode_count(aa_bell_t self);
void aa_bell_set_used_mode_count(
	aa_bell_t self, int nfUsed);
float* aa_bell_get_cos_force_ptr(aa_bell_t self);

void aa_bell_compute_location(
	aa_bell_t self, int i);
void aa_bell_clear_history(aa_bell_t self);
void aa_bell_damp_resonators(
	aa_bell_t self, double damping);
void aa_bell_compute_reson_coeff(
	aa_bell_t self, int i);
void aa_bell_add_energy(
	aa_bell_t self, float energy, float dur);

void aa_bell_compute_filter(aa_bell_t self);

double aa_bell_compute_sound_buffer(
	aa_bell_t self, float *output);

void aa_bell_clear_history(aa_bell_t self);

void aa_bell_dump(
	aa_bell_t self, FILE* outfile);

__END_DECLS
#endif                          // #ifndef __AA_BELL_H__
