#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <pablio.h>

#include <getopt.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

/** Modal model, which is loaded from an .sy format text file.
        @author Kees van den Doel (kvdoel@cs.ubc.ca)
*/

/** Mode frequencies in Hertz. */
float *f;

/**  Angular decay rates in Hertz. */
float *d;

/** Gains. a[p][k] is gain at point p for mode k. */
float *a;

/** Number of modes available. */
int nf;

/** Number of modes used. */
int nfUsed;

/** Number of points. */
int np;

/** Multiplies all frequencies. */
float fscale;

/** Multiplies all dampings. */
float dscale;

/** Multiplies all gains. */
float ascale;

/** Allocated arrays.
    @param nf number of modes.
    @param np number of locations.
*/

// globals
FILE *gnuplot = NULL;
static struct termios Otty, Ntty;

void
allocateArrays(int nf, int np)
{
	f = (float*)malloc(sizeof(float)*nf);
	d = (float*)malloc(sizeof(float)*nf);
	a = (float*)malloc(sizeof(float)*nf*np);
	fscale = dscale = ascale = 1.f;
};

void
readModes(FILE * fp)
{
	float dval;
	int ival;
	char *s;

	s = (char *)malloc(256);

	fscanf(fp, "%s\n", s);
	// s is now: "nactive_freq:"    
	fscanf(fp, "%d\n", &nfUsed);
	//printf(":%s:\n",s);
	fscanf(fp, "%s\n", s);		// n_freq:
	fscanf(fp, "%d\n", &nf);
	fscanf(fp, "%s\n", s);		// n_points:
	fscanf(fp, "%d\n", &np);
	fscanf(fp, "%s\n", s);		// f_scale:
	fscanf(fp, "%f\n", &fscale);
	fscanf(fp, "%s\n", s);		// d_scale:
	fscanf(fp, "%f\n", &dscale);
	fscanf(fp, "%s\n", s);		// a_scale:
	fscanf(fp, "%f\n", &ascale);

	allocateArrays(nf, np);

	fscanf(fp, "%s\n", s);		// frequencies:
	for (int i = 0; i < nf; i++) {
		fscanf(fp, "%f\n", &f[i]);
	}
	fscanf(fp, "%s\n", s);		// dampings:
	for (int i = 0; i < nf; i++) {
		fscanf(fp, "%f\n", &d[i]);
	}
	fscanf(fp, "%s\n", s);		// amplitudes[point][freq]:
	for (int p = 0; p < np; p++) {
		for (int i = 0; i < nf; i++) {
			fscanf(fp, "%f\n", &a[p * nf + i]);
		}
	}
	fscanf(fp, "%s\n", s);		// END
};

/** Read the modes file in .sy format.
    @param fn File name with modal data in .sy format.
*/
void
readModesFile(char *fn)
{
	FILE *fp = fopen(fn, "r");
	readModes(fp);
	fclose(fp);
};

/** Vibration model of object, capable of playing sound.
    @author Kees van den Doel (kvdoel@cs.ubc.ca)
*/

/** Sampling rate in Hertz. */
float srate = 44100.f;

/** State of filters. */
float *yt_1, *yt_2;

/** The transfer function of a reson filter is H(z) = 1/(1-twoRCosTheta/z + R2/z*z). */
float *R2;

/** The transfer function of a reson filter is H(z) = 1/(1-twoRCosTheta/z + R2/z*z). */
float *twoRCosTheta;

/** Cached values. */
float *c_i;

/** Reson filter gain vector. */
float *ampR;

/** Compute gains of contact
*/
void
computeLocation()
{
	for (int i = 0; i < nf; i++) {
		ampR[i] = ascale * c_i[i] * a[i];
	}
}

/** Set state to non-vibrating.
*/
void
clearHistory()
{
	for (int i = 0; i < nf; i++) {
		yt_1[i] = yt_2[i] = 0;
	}
}

void
dampResonators(double damping)
{
	for (int i = 0; i < nf; i++) {
		yt_1[i] *= damping;
		yt_2[i] *= damping;
	}
}

/** Allocate data.
    @param nf number of modes.
    @param np number of locations.
*/
void
allocateData(int nf, int np)
{
	R2 = (float*)malloc(sizeof(float)*nf);
	twoRCosTheta = (float*)malloc(sizeof(float)*nf);
	yt_1 = (float*)malloc(sizeof(float)*nf);
	yt_2 = (float*)malloc(sizeof(float)*nf);
	c_i = (float*)malloc(sizeof(float)*nf);
	ampR = (float*)malloc(sizeof(float)*nf);
	clearHistory();
}

/** Compute the reson coefficients from the modal model parameters.
    Cache values for location computation.
*/
void
computeResonCoeff()
{
	for (int i = 0; i < nf; i++) {
		float tmp_r = (float)(exp(-dscale * d[i] / srate));
		R2[i] = tmp_r * tmp_r;
		twoRCosTheta[i] =
			(float)(2. * cos(2. * M_PI * fscale * f[i] / srate) * tmp_r);
		c_i[i] = (float)(sin(2. * M_PI * fscale * f[i] / srate) * tmp_r);
	}
}

/** Compute the filter coefficients used for real-time rendering
    from the modal model parameters.
*/
void
computeFilter()
{
	computeResonCoeff();
	computeLocation();
}

/** Compute response through bank of modal filters.
    @param output provided output buffer.
*/
double
computeSoundBuffer(float *output, float *force, int nsamples, bool doPlot,
				   int numResonators)
{
	static unsigned plotCount = 0;
	double total = 0.0;

	for (int k = 0; k < nsamples; k++) {
		output[k] = 0;
	}
	int nf = nfUsed;

	if (doPlot) {
		//if((plotCount == 0) && (i == 0)) { // XXX this one is if we moved this inside the loop to only plot for f0
		if (plotCount == 0) {
			fprintf(gnuplot, "set style data lines\n");
			//fprintf(gnuplot, "set xrange[0:1000]\n");
			fprintf(gnuplot, "set yrange[-1:1]\n");
			fprintf(gnuplot, "unset key\n");

			fprintf(gnuplot, "plot \"-\"\n");
		}
	}

	for (int i = 0; i < numResonators; i++) {
		float tmp_twoRCosTheta = twoRCosTheta[i];
		float tmp_R2 = R2[i];
		float tmp_a = ampR[i];
		float tmp_yt_1 = yt_1[i];
		float tmp_yt_2 = yt_2[i];
		for (int k = 0; k < nsamples; k++) {
			float ynew = tmp_twoRCosTheta * tmp_yt_1 - tmp_R2 * tmp_yt_2
				+ tmp_a * force[k];
			tmp_yt_2 = tmp_yt_1;
			tmp_yt_1 = ynew;
			output[k] += ynew;

			if (i == 0)			// only total f0
				total += fabs(ynew);

#ifdef PLOTEACH
			if ((doPlot) && (i == 0)) {
				if (!(plotCount % 20))	// try plotting every 10th pt
					fprintf(gnuplot, "%d %f\n", plotCount, ynew);

				if (plotCount++ > 1000) {
					fprintf(gnuplot, "e\n");
					plotCount = 0;
				}

			}
#endif							/* PLOTEACH */
		}
		yt_1[i] = tmp_yt_1;
		yt_2[i] = tmp_yt_2;
	}

	if (gnuplot) {
		for (int k = 0; k < nsamples; k += 10) {
			fprintf(gnuplot, "%d %f\n", plotCount, output[k]);

			if (plotCount++ > 500) {
				fprintf(gnuplot, "e\n");
				plotCount = 0;
			}
		}
	}

	return (total);
}

void
cleanup()
{
	tcsetattr(0, TCSANOW, &Otty);
	exit(0);
}

#define min(x, y) (x<y)?x:y

int
main(int argc, char *argv[])
{
	// int bufferSize = 4096;
	int bufferSize = 10;

	bool doInput = true;
	bool doImpulse = false;
	bool doPlot = false;
	bool doInteraction = true;
	unsigned plotCount = 0;

	int numResonators;
	enum { resonators, tuneFreq, tuneDamp, tuneAmp } mode = resonators;

	int c;
	while ((c = getopt(argc, argv, "ig")) != -1) {
		switch (c) {
		case 'i':
			doImpulse = true;
			doInput = false;	// mutually exclusive for now...
			break;

		case 'g':
			doPlot = true;
			break;

		case '?':
			fprintf(stderr, "%s: Unknown argument %c\n", argv[0], optopt);
			cleanup();

		default:
			break;
		}
	}

	printf("argc:%d optind:%d\n", argc, optind);
	if (argc != (optind + 1)) {
		fprintf(stderr, "%s: [-i -g] resonant filename\n", argv[0]);
		cleanup();
	}

	printf("%s, %s\n", doPlot ? "PLOT" : "!plot", doInput ? "INPUT" : "!input");

	if (doPlot) {
		gnuplot = popen("gnuplot", "w");
		setvbuf(gnuplot, NULL, _IONBF, 0);	// use unbuffered writes
	}

	PABLIO_Stream *inStream, *outStream;
	if (doInput)
		OpenAudioStream(&inStream, srate, paFloat32, PABLIO_READ | PABLIO_MONO);
	OpenAudioStream(&outStream, srate, paFloat32, PABLIO_WRITE | PABLIO_MONO);

	readModesFile(argv[optind]);
	numResonators = nf;

	if (doInteraction) {
		int status;

		tcgetattr(0, &Otty);
		Ntty = Otty;

		Ntty.c_lflag = 0;		// line settings (no echo)
		Ntty.c_lflag = 0;		// line settings (no echo)

		Ntty.c_cc[VMIN] = 0;	// minimum time to wait
		Ntty.c_cc[VTIME] = 0;	// minimum characters to wait for
		status = tcsetattr(0, TCSANOW, &Ntty);
		if (status)
			fprintf(stderr, "tcsetattr returned %d\n", status);
	}

	if (doImpulse)
		bufferSize = 4096;
	else
		bufferSize = 10;

	float *buffer = (float *)malloc(bufferSize * sizeof(float));

	printf("nactive_freq: %d\n", nfUsed);
	printf("n_freq: %d\n", nf);
	printf("n_points: %d\n", np);
	printf("f_scale: %f\n", fscale);
	printf("d_scale: %f\n", dscale);
	printf("a_scale: %f\n", ascale);
	printf("frequencies:\n");
	for (int i = 0; i < nf; i++) {
		printf("%f\n", f[i]);
	}
	printf("dampings:\n");
	for (int i = 0; i < nf; i++) {
		printf("%f\n", d[i]);
	}
	printf("amplitudes[point][freq]:\n");
	for (int p = 0; p < np; p++) {
		for (int i = 0; i < nf; i++) {
			printf("%f\n", a[p * nf + i]);
		}
	}

	allocateData(nf, np);
	computeFilter();

	float dur = .0002f;			// 2 ms
	int nsamples = (int)(srate * dur);
	printf("nsamples: %d\n", nsamples);
	float *cosForce = (float*)malloc(sizeof(float)*bufferSize);
	memset(cosForce, 0, bufferSize * sizeof(float));
	for (int i = 0; i < nsamples; i++) {
		cosForce[i] =
			(float)(.5 * (1. - cos(2 * M_PI * (i + 1) / (1 + nsamples))));
	}

	int x;
	bool done = false;
	while (!done) {
		char car;
		double total;
		int num = 0;

		if (doInput)
			ReadAudioStream(inStream, cosForce, bufferSize);

		car = fgetc(stdin);
		if (car != -1) {
			switch (car) {
			case ' ':
				dampResonators(0.3);
				break;

			case '0':
				numResonators = nf;
				break;

			case 'r':
				mode = resonators;
				break;
			case 'f':
				mode = tuneFreq;
				printf("tuneFreq\n");
				break;
			case 'd':
				mode = tuneDamp;
				printf("tuneDamp\n");
				break;
			case 'a':
				mode = tuneAmp;
				printf("tuneAmp\n");
				break;

			case 'i':
				for (int i = 0; i < nsamples; i++)
					cosForce[i] =
						(float)(.5 *
								(1. -
								 cos(2 * M_PI * (i + 1) / (1 + nsamples))));
				break;

			case 'q':
				printf("calling cleanup\n");
				cleanup();
				break;
			default:
				if ((car == '+') || (car == '=') || (car == '-')) {
					double delta = 0.1 * (car == '-') ? -1.0 : 1.0;

					// update params
					switch (mode) {
					case tuneFreq:
						f[num] += delta;
						break;
					case tuneDamp:
						d[num] += delta;
						break;
					case tuneAmp:
						a[num] += delta;
						break;
					}

					computeFilter();	// rebuild filters (XXX eventually narrow it to rebuild what we need)
				}

				if ((car >= '1') && (car <= '9')) {
					num = car - '0';

					numResonators = min(nf, num);
					printf("numResonators = %d\n", numResonators);
				}
			}
		} else {
			clearerr(stdin);
		}
		total =
			computeSoundBuffer(buffer, cosForce, bufferSize, doPlot,
							   numResonators);
		if (doImpulse && (total < 0.0001)) {
			done = true;
			printf("total = %f\n", total);
		}
		memset(cosForce, 0, nsamples * sizeof(float));
		for (int i = 0; i < bufferSize; i++) {
			buffer[i] /= nf;
			// printf("%f\n",buffer[i]);
			// XXX Realy should saturate...
			if (buffer[i] > 1.0)
				buffer[i] = 1.0;
			if (buffer[i] < -1.0)
				buffer[i] = -1.0;
		}
		WriteAudioStream(outStream, buffer, bufferSize);

	}

	CloseAudioStream(outStream);

	return 0;
}
