#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bell.h"

struct aa_bell_s {
	int		bufferSize;

	/** Mode frequencies in Hertz. */
	float * f;

	/**  Angular decay rates in Hertz. */
	float * d;

	/** Gains. a[p][k] is gain at point p for mode k. */
	float * a;

	/** Number of modes available. */
	int		nf;

	/** Number of modes used. */
	int		nfUsed;

	/** Number of points. */
	int		np;

	/** Multiplies all frequencies. */
	float	fscale;

	/** Multiplies all dampings. */
	float	dscale;

	/** Multiplies all gains. */
	float	ascale;


	float * cosForce;


	/** Sampling rate in Hertz. */
	float	srate;

	/** State of filters. */
	float * yt_1, *yt_2;

	/** The transfer function of a reson filter is H(z) = 1/(1-twoRCosTheta/z + R2/z*z). */
	float * R2;

	/** The transfer function of a reson filter is H(z) = 1/(1-twoRCosTheta/z + R2/z*z). */
	float * twoRCosTheta;

	/** Cached values. */
	float * c_i;

	/** Reson filter gain vector. */
	float * ampR;
};

void
aa_bell_dump(
	aa_bell_t self, FILE* outfile
) {
	fprintf(outfile, "nactive_freq: %d\n", self->nfUsed);
	fprintf(outfile, "n_freq: %d\n", self->nf);
	fprintf(outfile, "n_points: %d\n", self->np);
	fprintf(outfile, "f_scale: %f\n", self->fscale);
	fprintf(outfile, "d_scale: %f\n", self->dscale);
	fprintf(outfile, "a_scale: %f\n", self->ascale);
	fprintf(outfile, "frequencies and dampings:\n");
	for(int i = 0; i < self->nf; i++) {
		fprintf(outfile, "\t%d: \t%f \t%f\n", i, self->f[i], self->d[i]);
	}
	fprintf(outfile, "amplitudes[point][freq]:\n");
	for(int i = 0; i < self->nf; i++) {
		fprintf(outfile, "\t%d:", i);
		for(int p = 0; p < self->np; p++) {
			fprintf(outfile, " \t%f", self->a[p * self->nf + i]);
		}
		fprintf(outfile, "\n");
	}

	fprintf(outfile, "R2 yt_1 yt_2 twoRCosTheta c_i ampR:\n");
	for(int i = 0; i < self->nf; i++) {
		fprintf(outfile,
			"\t%d: %f %f %f %f %f %f\n",
			i,
			self->R2[i],
			self->yt_1[i],
			self->yt_2[i],
			self->twoRCosTheta[i],
			self->c_i[i],
			self->ampR[i]);
	}

	fprintf(outfile, "Force:\n");
	for(int i = 0; i < self->bufferSize; i++) {
		fprintf(outfile, "\t%d: %f\n", i, self->cosForce[i]);
	}
	fprintf(outfile, "-------\n");
}

aa_bell_t
aa_bell_create(
	int nf, int np, int bufferSize, int srate
) {
	aa_bell_t ret = NULL;

	ret = calloc(sizeof(*ret), 1);

	if(!ret)
		goto bail;


	ret->fscale = ret->dscale = ret->ascale = 1.0f;
	ret->nfUsed = ret->nf = nf;
	ret->np = np;
	if(srate)
		ret->srate = srate;
	else
		ret->srate = AA_BELL_DEFAULT_SRATE;
	ret->bufferSize = bufferSize;

	ret->f = (float*)malloc(sizeof(float) * nf);

	if(!ret->f) {
		aa_bell_release(ret);
		ret = NULL;
		goto bail;
	}

	ret->d = (float*)calloc(sizeof(float), nf);

	if(!ret->d) {
		aa_bell_release(ret);
		ret = NULL;
		goto bail;
	}

	ret->a = (float*)calloc(sizeof(float), nf * np);

	if(!ret->a) {
		aa_bell_release(ret);
		ret = NULL;
		goto bail;
	}

	ret->cosForce = (float*)calloc(sizeof(float), bufferSize);

	ret->R2 = (float*)calloc(sizeof(float), nf);
	ret->twoRCosTheta = (float*)calloc(sizeof(float), nf);
	ret->yt_1 = (float*)calloc(sizeof(float), nf);
	ret->yt_2 = (float*)calloc(sizeof(float), nf);
	ret->c_i = (float*)calloc(sizeof(float), nf);
	ret->ampR = (float*)calloc(sizeof(float), nf);


bail:
	return ret;
}

void
aa_bell_release(aa_bell_t self) {
	free(self->f);
	free(self->d);
	free(self->a);
	free(self->R2);
	free(self->twoRCosTheta);
	free(self->yt_1);
	free(self->yt_2);
	free(self->c_i);
	free(self->ampR);
	free(self->cosForce);
	free(self);
}

aa_bell_t
aa_bell_create_from_file(
	const char *path, int bufferSize, int srate
) {
	aa_bell_t ret = NULL;
	float dval;
	int ival;
	int nfUsed, nf, np;
	char s[256];
	FILE* fp;

	fp = fopen(path, "r");

	if(!fp)
		goto bail;

	fscanf(fp, "%s\n", s);
	// s is now: "nactive_freq:"
	fscanf(fp, "%d\n", &nfUsed);
	//printf(":%s:\n",s);
	fscanf(fp, "%s\n", s);      // n_freq:
	fscanf(fp, "%d\n", &nf);
	fscanf(fp, "%s\n", s);      // n_points:
	fscanf(fp, "%d\n", &np);

	ret = aa_bell_create(nf, np, bufferSize, srate);

	if(!ret)
		goto bail;

	fscanf(fp, "%s\n", s);      // f_scale:
	fscanf(fp, "%f\n", &ret->fscale);
	fscanf(fp, "%s\n", s);      // d_scale:
	fscanf(fp, "%f\n", &ret->dscale);
	fscanf(fp, "%s\n", s);      // a_scale:
	fscanf(fp, "%f\n", &ret->ascale);

	fscanf(fp, "%s\n", s);      // frequencies:
	for(int i = 0; i < nf; i++) {
		fscanf(fp, "%f\n", &ret->f[i]);
	}
	fscanf(fp, "%s\n", s);      // dampings:
	for(int i = 0; i < nf; i++) {
		fscanf(fp, "%f\n", &ret->d[i]);
	}
	fscanf(fp, "%s\n", s);      // amplitudes[point][freq]:
	for(int p = 0; p < np; p++) {
		for(int i = 0; i < nf; i++) {
			fscanf(fp, "%f\n", &ret->a[p * nf + i]);
		}
	}

	aa_bell_compute_filter(ret);

bail:
	if(fp)
		fclose(fp);

	return ret;
}


/** Compute gains of contact
 */
void
aa_bell_compute_location(
	aa_bell_t self, int i
) {
	self->ampR[i] = self->ascale * self->c_i[i] * self->a[i];
}

/** Set state to non-vibrating.
 */
void
aa_bell_clear_history(aa_bell_t self) {
	for(int i = 0; i < self->nf; i++) {
		self->yt_1[i] = self->yt_2[i] = 0;
	}
}

void
aa_bell_damp_resonators(
	aa_bell_t self, double damping
) {
	for(int i = 0; i < self->nf; i++) {
		self->yt_1[i] *= damping;
		self->yt_2[i] *= damping;
	}
}

/** Compute the reson coefficients from the modal model parameters.
    Cache values for location computation.
 */
void
aa_bell_compute_reson_coeff(
	aa_bell_t self, int i
) {
	float tmp_r = (float)(exp(-self->dscale * self->d[i] / self->srate));

	self->R2[i] = tmp_r * tmp_r;
	self->twoRCosTheta[i] =
	    (float)(2. *
	    cos(2. * M_PI * self->fscale * self->f[i] / self->srate) * tmp_r);
	self->c_i[i] =
	    (float)(sin(2. * M_PI * self->fscale * self->f[i] /
			self->srate) * tmp_r);
}

/** Compute the filter coefficients used for real-time rendering
    from the modal model parameters.
 */
void
aa_bell_compute_filter(aa_bell_t self) {
	for(int i = 0; i < self->nf; i++) {
		aa_bell_compute_reson_coeff(self, i);
		aa_bell_compute_location(self, i);
	}
}

double
aa_bell_compute_sound_buffer(
	aa_bell_t	self,
	float *		output
) {
	double total = 0.0;
	int numResonators = self->nfUsed;
	int nsamples = self->bufferSize;

	memset((void*)output, 0, sizeof(float) * nsamples);

	for(int i = 0; i < numResonators; i++) {
		float tmp_twoRCosTheta = self->twoRCosTheta[i];
		float tmp_R2 = self->R2[i];
		float tmp_a = self->ampR[i];
		float tmp_yt_1 = self->yt_1[i];
		float tmp_yt_2 = self->yt_2[i];

		for(int k = 0; k < nsamples; k++) {
			float ynew = tmp_twoRCosTheta * tmp_yt_1 - tmp_R2 * tmp_yt_2
			    + tmp_a * self->cosForce[k];
			tmp_yt_2 = tmp_yt_1;
			tmp_yt_1 = ynew;
			output[k] += ynew;


			if(i == 0)          // only total f0
				total += fabs(ynew);
		}
		self->yt_1[i] = tmp_yt_1;
		self->yt_2[i] = tmp_yt_2;
	}

	memset((void*)self->cosForce, 0, sizeof(float) * self->bufferSize);

	for(int i = 0; i < self->bufferSize; i++) {
//		output[i] /= numResonators;
		// printf("%f\n",buffer[i]);
		// XXX Realy should saturate...
		if(output[i] > 1.0)
			output[i] = 1.0;
		if(output[i] < -1.0)
			output[i] = -1.0;
	}

	return total;
}

void
aa_bell_add_energy(
	aa_bell_t self, float energy, float dur
) {
	int nsamples = (int)(self->srate * dur);

	if(nsamples > self->bufferSize)
		nsamples = self->bufferSize;
	if(nsamples <= 1) {
		self->cosForce[0] = energy;
	} else {
		for(int i = 0; i < nsamples; i++) {
			self->cosForce[i] +=
			    (float)(energy *
			        (1. - cos(2 * M_PI * (i + 1) / (1 + nsamples))));
		}
	}
}


void
aa_bell_set_mode_freq(
	aa_bell_t self, int res_index, float val
) {
	if(res_index < self->nf)
		self->f[res_index] = val;
}

void aa_bell_set_angular_decay(
	aa_bell_t self, int res_index, float val
) {
	if(res_index < self->nf)
		self->d[res_index] = val;
}

void aa_bell_set_gain(
	aa_bell_t self, int point, int mode, float val
) {
	if((mode < self->nf) && (point < self->np))
		self->a[mode + point * self->nf] = val;
}

int aa_bell_get_mode_count(aa_bell_t self) {
	return self->nf;
}

int aa_bell_get_point_count(aa_bell_t self) {
	return self->np;
}

int aa_bell_get_used_mode_count(aa_bell_t self) {
	return self->nfUsed;
}

void aa_bell_set_used_mode_count(
	aa_bell_t self, int nfUsed
) {
	self->nfUsed = nfUsed;
}

float* aa_bell_get_cos_force_ptr(aa_bell_t self) {
	return self->cosForce;
}
