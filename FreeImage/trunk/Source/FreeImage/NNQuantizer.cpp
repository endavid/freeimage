// NeuQuant Neural-Net Quantization Algorithm
// ------------------------------------------
//
// Copyright (c) 1994 Anthony Dekker
//
// NEUQUANT Neural-Net quantization algorithm by Anthony Dekker, 1994.
// See "Kohonen neural networks for optimal colour quantization"
// in "Network: Computation in Neural Systems" Vol. 5 (1994) pp 351-367.
// for a discussion of the algorithm.
//
// Any party obtaining a copy of these files from the author, directly or
// indirectly, is granted, free of charge, a full and unrestricted irrevocable,
// world-wide, paid up, royalty-free, nonexclusive right and license to deal
// in this software and documentation files (the "Software"), including without
// limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons who receive
// copies from any such party to do so, with the only requirement being
// that this copyright notice remain intact.

///////////////////////////////////////////////////////////////////////
// History
// -------
// January 2001: Adaptation of the Neural-Net Quantization Algorithm
//               for the FreeImage 2 library
//               Author: Herv� Drolon (drolon@infonie.fr)
// March 2004:   Adaptation for the FreeImage 3 library (port to big endian processors)
//               Author: Herv� Drolon (drolon@infonie.fr)
///////////////////////////////////////////////////////////////////////

#include "FreeImage.h"

const int netsize = 256;			// number of colours used


// For 256 colours, fixed arrays need 8kb, plus space for the image
// ----------------------------------------------------------------


// Four primes near 500 - assume no image has a length so large
// that it is divisible by all four primes
// ==========================================================

#define prime1		499
#define prime2		491
#define prime3		487
#define prime4		503

// Prototypes 
// ==========================================================

static void initnet(BYTE *thepic, DWORD len, int sample);	
static void unbiasnet();	// can edit this function to do output of colour map
static void inxbuild();
static int inxsearch(int b, int g, int r);
static int contest(int b, int g, int r);
static void altersingle(int alpha, int i, int b, int g, int r);
static void alterneigh(int rad, int i, int b, int g, int r);
static void learn();

// Network Definitions
// ==========================================================

#define maxnetpos		(netsize-1)
#define netbiasshift	4			// bias for colour values
#define ncycles			100			// no. of learning cycles

// defs for freq and bias

#define intbiasshift    16			// bias for fractions
#define intbias		(((int) 1)<<intbiasshift)
#define gammashift  	10			// gamma = 1024
#define gamma   	(((int) 1)<<gammashift)
#define betashift  	10
#define beta		(intbias>>betashift)	// beta = 1 / 1024
#define betagamma	(intbias<<(gammashift-betashift))

// defs for decreasing radius factor

#define initrad			(netsize>>3)	// for 256 cols, radius starts
#define radiusbiasshift	6				// at 32.0 biased by 6 bits
#define radiusbias	(((int) 1)<<radiusbiasshift)
#define initradius	(initrad*radiusbias)	// and decreases by a 
#define radiusdec	30						// factor of 1/30 each cycle

// defs for decreasing alpha factor

#define alphabiasshift	10			// alpha starts at 1.0
#define initalpha	(((int) 1)<<alphabiasshift)

// radbias and alpharadbias used for radpower calculation

#define radbiasshift	8
#define radbias			(((int) 1)<<radbiasshift)
#define alpharadbshift  (alphabiasshift+radbiasshift)
#define alpharadbias    (((int) 1)<<alpharadbshift)

// Types and Global Variables
// ==========================================================
   
static BYTE *thepicture;	// the input image itself
static DWORD lengthcount;	// lengthcount = H*W*3

static int samplefac;				// sampling factor 1..30


typedef int pixel[4];				// BGRc
static pixel network[netsize];		// the network itself

static int netindex[256];			// for network lookup - really 256

static int bias [netsize];			// bias and freq arrays for learning
static int freq [netsize];
static int radpower[initrad];		// radpower for precomputation

// ==========================================================

///////////////////////////////////////////////////////////////////////////
// Initialise network in range (0,0,0) to (255,255,255) and set parameters
// -----------------------------------------------------------------------

static void initnet(BYTE *thepic, DWORD len, int sample)	
{
	int i, *p;
	
	thepicture = thepic;
	lengthcount = len;
	samplefac = sample;
	
	for (i = 0; i < netsize; i++) {
		p = network[i];
		p[FI_RGBA_BLUE] = p[FI_RGBA_GREEN] = p[FI_RGBA_RED] = (i << (netbiasshift+8))/netsize;
		freq[i] = intbias/netsize;	/* 1/netsize */
		bias[i] = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////////////	
// Unbias network to give byte values 0..255 and record position i to prepare for sort
// ------------------------------------------------------------------------------------

static void unbiasnet()
{
	int i, j, temp;

	for (i = 0; i < netsize; i++) {
		for (j = 0; j < 3; j++) {
			// OLD CODE: network[i][j] >>= netbiasshift; 
			// Fix based on bug report by Juergen Weigert jw@suse.de
			temp = (network[i][j] + (1 << (netbiasshift - 1))) >> netbiasshift;
			if (temp > 255) temp = 255;
			network[i][j] = temp;
		}
		network[i][3] = i;			// record colour no 
	}
}

//////////////////////////////////////////////////////////////////////////////////
// Insertion sort of network and building of netindex[0..255] (to do after unbias)
// -------------------------------------------------------------------------------

static void inxbuild()
{
	int i,j,smallpos,smallval;
	int *p,*q;
	int previouscol,startpos;

	previouscol = 0;
	startpos = 0;
	for (i = 0; i < netsize; i++) {
		p = network[i];
		smallpos = i;
		smallval = p[FI_RGBA_GREEN];			// index on g
		// find smallest in i..netsize-1
		for (j = i+1; j < netsize; j++) {
			q = network[j];
			if (q[FI_RGBA_GREEN] < smallval) {	// index on g
				smallpos = j;
				smallval = q[FI_RGBA_GREEN];	// index on g
			}
		}
		q = network[smallpos];
		// swap p (i) and q (smallpos) entries
		if (i != smallpos) {
			j = q[FI_RGBA_BLUE];  q[FI_RGBA_BLUE]  = p[FI_RGBA_BLUE];  p[FI_RGBA_BLUE]  = j;
			j = q[FI_RGBA_GREEN]; q[FI_RGBA_GREEN] = p[FI_RGBA_GREEN]; p[FI_RGBA_GREEN] = j;
			j = q[FI_RGBA_RED];   q[FI_RGBA_RED]   = p[FI_RGBA_RED];   p[FI_RGBA_RED]   = j;
			j = q[3];   q[3] = p[3];   p[3] = j;
		}
		// smallval entry is now in position i
		if (smallval != previouscol) {
			netindex[previouscol] = (startpos+i)>>1;
			for (j = previouscol+1; j < smallval; j++)
				netindex[j] = i;
			previouscol = smallval;
			startpos = i;
		}
	}
	netindex[previouscol] = (startpos+maxnetpos)>>1;
	for (j = previouscol+1; j < 256; j++)
		netindex[j] = maxnetpos; // really 256
}

///////////////////////////////////////////////////////////////////////////////
// Search for BGR values 0..255 (after net is unbiased) and return colour index
// ----------------------------------------------------------------------------

static int inxsearch(int b, int g, int r)
{
	int i, j, dist, a, bestd;
	int *p;
	int best;

	bestd = 1000;		// biggest possible dist is 256*3
	best = -1;
	i = netindex[g];	// index on g
	j = i-1;			// start at netindex[g] and work outwards

	while ((i < netsize) || (j >= 0)) {
		if (i < netsize) {
			p = network[i];
			dist = p[FI_RGBA_GREEN] - g;				// inx key
			if (dist >= bestd)
				i = netsize;	// stop iter
			else {
				i++;
				if (dist < 0)
					dist = -dist;
				a = p[FI_RGBA_BLUE] - b;
				if (a < 0)
					a = -a;
				dist += a;
				if (dist < bestd) {
					a = p[FI_RGBA_RED] - r;
					if (a<0)
						a = -a;
					dist += a;
					if (dist < bestd) {
						bestd = dist;
						best = p[3];
					}
				}
			}
		}
		if (j >= 0) {
			p = network[j];
			dist = g - p[FI_RGBA_GREEN];			// inx key - reverse dif
			if (dist >= bestd)
				j = -1;	// stop iter
			else {
				j--;
				if (dist < 0)
					dist = -dist;
				a = p[FI_RGBA_BLUE] - b;
				if (a<0)
					a = -a;
				dist += a;
				if (dist < bestd) {
					a = p[FI_RGBA_RED] - r;
					if (a<0)
						a = -a;
					dist += a;
					if (dist < bestd) {
						bestd = dist;
						best = p[3];
					}
				}
			}
		}
	}
	return best;
}

///////////////////////////////
// Search for biased BGR values
// ----------------------------

static int contest(int b, int g, int r)
{
	// finds closest neuron (min dist) and updates freq
	// finds best neuron (min dist-bias) and returns position
	// for frequently chosen neurons, freq[i] is high and bias[i] is negative
	// bias[i] = gamma*((1/netsize)-freq[i])

	int i,dist,a,biasdist,betafreq;
	int bestpos,bestbiaspos,bestd,bestbiasd;
	int *p,*f, *n;

	bestd = ~(((int) 1)<<31);
	bestbiasd = bestd;
	bestpos = -1;
	bestbiaspos = bestpos;
	p = bias;
	f = freq;

	for (i = 0; i < netsize; i++) {
		n = network[i];
		dist = n[FI_RGBA_BLUE] - b;
		if (dist < 0)
			dist = -dist;
		a = n[FI_RGBA_GREEN] - g;
		if (a < 0)
			a = -a;
		dist += a;
		a = n[FI_RGBA_RED] - r;
		if (a < 0)
			a = -a;
		dist += a;
		if (dist < bestd) {
			bestd = dist;
			bestpos = i;
		}
		biasdist = dist - ((*p)>>(intbiasshift-netbiasshift));
		if (biasdist < bestbiasd) {
			bestbiasd = biasdist;
			bestbiaspos = i;
		}
		betafreq = (*f >> betashift);
		*f++ -= betafreq;
		*p++ += (betafreq << gammashift);
	}
	freq[bestpos] += beta;
	bias[bestpos] -= betagamma;
	return bestbiaspos;
}

///////////////////////////////////////////////////////
// Move neuron i towards biased (b,g,r) by factor alpha
// ---------------------------------------------------- 

static void altersingle(int alpha, int i, int b, int g, int r)
{
	int *n;

	n = network[i];				// alter hit neuron
	n[FI_RGBA_BLUE]	 -= (alpha * (n[FI_RGBA_BLUE]  - b)) / initalpha;
	n[FI_RGBA_GREEN] -= (alpha * (n[FI_RGBA_GREEN] - g)) / initalpha;
	n[FI_RGBA_RED]   -= (alpha * (n[FI_RGBA_RED]   - r)) / initalpha;
}

////////////////////////////////////////////////////////////////////////////////////
// Move adjacent neurons by precomputed alpha*(1-((i-j)^2/[r]^2)) in radpower[|i-j|]
// ---------------------------------------------------------------------------------

static void alterneigh(int rad, int i, int b, int g, int r)
{
	int j, k, lo, hi, a;
	int *p, *q;

	lo = i - rad;   if (lo < -1) lo = -1;
	hi = i + rad;   if (hi > netsize) hi = netsize;

	j = i+1;
	k = i-1;
	q = radpower;
	while ((j < hi) || (k > lo)) {
		a = (*(++q));
		if (j < hi) {
			p = network[j];
			p[FI_RGBA_BLUE]  -= (a * (p[FI_RGBA_BLUE] - b)) / alpharadbias;
			p[FI_RGBA_GREEN] -= (a * (p[FI_RGBA_GREEN] - g)) / alpharadbias;
			p[FI_RGBA_RED]   -= (a * (p[FI_RGBA_RED] - r)) / alpharadbias;
			j++;
		}
		if (k > lo) {
			p = network[k];
			p[FI_RGBA_BLUE]  -= (a * (p[FI_RGBA_BLUE] - b)) / alpharadbias;
			p[FI_RGBA_GREEN] -= (a * (p[FI_RGBA_GREEN] - g)) / alpharadbias;
			p[FI_RGBA_RED]   -= (a * (p[FI_RGBA_RED] - r)) / alpharadbias;
			k--;
		}
	}
}

/////////////////////
// Main Learning Loop
// ------------------

static void learn()
{
	int i, j, b, g, r;
	int radius, rad, alpha, step, delta, samplepixels;
	int alphadec; // biased by 10 bits
	BYTE *p;
	BYTE *lim;

	alphadec = 30 + ((samplefac - 1) / 3);
	p = thepicture;
	lim = thepicture + lengthcount;
	samplepixels = lengthcount / (3*samplefac);
	delta = samplepixels / ncycles;
	if(delta == 0) {
		// avoid 'divide by zero' error
		delta = 1;
	}
	alpha = initalpha;
	radius = initradius;
	
	rad = radius >> radiusbiasshift;
	if (rad <= 1) rad = 0;
	for (i = 0; i < rad; i++) 
		radpower[i] = alpha*(((rad*rad - i*i)*radbias)/(rad*rad));
	
	if ((lengthcount % prime1) != 0)
		step = 3*prime1;
	else {
		if ((lengthcount % prime2) != 0)
			step = 3*prime2;
		else {
			if ((lengthcount % prime3) != 0) 
				step = 3*prime3;
			else
				step = 3*prime4;
		}
	}
	
	i = 0;
	while (i < samplepixels) {
		b = p[FI_RGBA_BLUE] << netbiasshift;
		g = p[FI_RGBA_GREEN] << netbiasshift;
		r = p[FI_RGBA_RED] << netbiasshift;
		j = contest(b,g,r);

		altersingle(alpha,j,b,g,r);
		if (rad) alterneigh(rad,j,b,g,r);   // alter neighbours 

		p += step;
		if (p >= lim) p -= lengthcount;
	
		i++;
		if (i % delta == 0) {	
			alpha -= alpha / alphadec;
			radius -= radius / radiusdec;
			rad = radius >> radiusbiasshift;
			if (rad <= 1) rad = 0;
			for (j = 0; j < rad; j++) 
				radpower[j] = alpha * (((rad*rad - j*j) * radbias) / (rad*rad));
		}
	}
}

///////////////////////////
// FreeImage implementation
// ------------------------

// Input parameters: 
// - void* dib: DIB 24-bit to be quantized
// - int sampling: a sampling factor in range 1..30
//                 1 => slower, 30 => faster. Default value is 15
// Return value: 
// - NULL  if the DIB is not valid or if it's not a 24-bit DIB
// - the quantized 8-bit (color palette) DIB otherwise

FIBITMAP *
NNQuantizer(FIBITMAP *dib, int sampling)
{
	LONG width, height;
	BYTE *dib_bits;			// the input image itself
	DWORD length;

	if ((!dib) || (FreeImage_GetBPP(dib) != 24)) {
		return NULL;
	}

	// 1) Select a sampling factor in range 1..30 (input parameter 'sampling')
	//    1 => slower, 30 => faster. Default value is 15


	// 2) Get DIB parameters
	
	width = FreeImage_GetWidth(dib);	// DIB width
	height = FreeImage_GetHeight(dib);	// DIB height
	dib_bits = FreeImage_GetBits(dib);	// pointer to DIB pixels
	length = 3L * width * height;		// image size in bytes

	// For small images, adjust the sampling factor to avoid a 'divide by zero' error later 
	// (see delta in learn() routine)
	int adjust = (width * height) / ncycles;
	if(sampling >= adjust)
		sampling = 1;

	// 3) Initialize the network and apply the learning algorithm

	initnet(dib_bits, length, sampling);
	learn();
	unbiasnet();

	// 4) Allocate a new 8-bit DIB

	FIBITMAP *new_dib = FreeImage_Allocate(width, height, 8);

	if (new_dib == NULL)
		return NULL;

	// 5) Write the quantized palette

	RGBQUAD *new_pal = FreeImage_GetPalette(new_dib);

    for (int j = 0; j < netsize; j++) {
		new_pal[j].rgbBlue  = (BYTE)network[j][FI_RGBA_BLUE];
		new_pal[j].rgbGreen = (BYTE)network[j][FI_RGBA_GREEN];
		new_pal[j].rgbRed	= (BYTE)network[j][FI_RGBA_RED];
	}

	inxbuild();

	// 6) Write output image using inxsearch(b,g,r)

	for (WORD rows = 0; rows < height; rows++) {
		BYTE *new_bits = FreeImage_GetScanLine(new_dib, rows);			
		BYTE *bits = FreeImage_GetScanLine(dib, rows);

		for (WORD cols = 0; cols < FreeImage_GetWidth(dib); cols++) {
			new_bits[cols] = (BYTE)inxsearch(bits[FI_RGBA_BLUE], bits[FI_RGBA_GREEN], bits[FI_RGBA_RED]);

			bits += 3;
		}
	}

	return (FIBITMAP*) new_dib;
}
