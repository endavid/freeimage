// ==========================================================
// Sun rasterfile Loader
//
// Design and implementation by 
// - Herv� Drolon (drolon@infonie.fr)
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

#include "FreeImage.h"
#include "Utilities.h"

// ----------------------------------------------------------
//   Constants + headers
// ----------------------------------------------------------

#ifdef WIN32
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

typedef struct tagSUNHEADER {
	DWORD magic;		// Magic number
	DWORD width;		// Image width in pixels
	DWORD height;		// Image height in pixels
	DWORD depth;		// Depth (1, 8, 24 or 32 bits) of each pixel
	DWORD length;		// Image length (in bytes)
	DWORD type;			// Format of file (see RT_* below)
	DWORD maptype;		// Type of colormap (see RMT_* below)
	DWORD maplength;	// Length of colormap (in bytes)
} SUNHEADER;

#ifdef WIN32
#pragma pack(pop)
#else
#pragma pack()
#endif

// ----------------------------------------------------------

// Following the header is the colormap, for maplength bytes (unless maplength is zero),
// then the image. Each row of the image is rounded to 2 bytes.

#define RAS_MAGIC 0x59A66A95 // Magic number for Sun rasterfiles

// Sun supported type's

#define RT_OLD			0	// Old format (raw image in 68000 byte order)
#define RT_STANDARD		1	// Raw image in 68000 byte order
#define RT_BYTE_ENCODED	2	// Run-length encoding of bytes 
#define RT_FORMAT_RGB	3	// XRGB or RGB instead of XBGR or BGR
#define RT_FORMAT_TIFF	4	// TIFF <-> standard rasterfile
#define RT_FORMAT_IFF	5	// IFF (TAAC format) <-> standard rasterfile

#define RT_EXPERIMENTAL 0xffff	// Reserved for testing

// These are the possible colormap types.
// if it's in RGB format, the map is made up of three byte arrays
// (red, green, then blue) that are each 1/3 of the colormap length.

#define RMT_NONE		0	// maplength is expected to be 0
#define RMT_EQUAL_RGB	1	// red[maplength/3], green[maplength/3], blue[maplength/3]
#define RMT_RAW			2	// Raw colormap
#define RESC			128 // Run-length encoding escape character

// ----- NOTES -----
// Each line of the image is rounded out to a multiple of 16 bits.
// This corresponds to the rounding convention used by the memory pixrect
// package (/usr/include/pixrect/memvar.h) of the SunWindows system.
// The ras_encoding field (always set to 0 by Sun's supported software)
// was renamed to ras_length in release 2.0.  As a result, rasterfiles
// of type 0 generated by the old software claim to have 0 length; for
// compatibility, code reading rasterfiles must be prepared to compute the
// TRUE length from the width, height, and depth fields.

// ==========================================================
// Internal functions
// ==========================================================

static void
ReadData(FreeImageIO *io, fi_handle handle, BYTE *buf, DWORD length, BOOL rle) {
	// Read either Run-Length Encoded or normal image data

	static BYTE repchar, remaining= 0;

	if (rle) {
		// Run-length encoded read

		while(length--) {
			if (remaining) {
				remaining--;
				*(buf++)= repchar;
			} else {
				io->read_proc(&repchar, 1, 1, handle);

				if (repchar == RESC) {
					io->read_proc(&remaining, 1, 1, handle);

					if (remaining == 0) {
						*(buf++)= RESC;
					} else {
						io->read_proc(&repchar, 1, 1, handle);

						*(buf++)= repchar;
					}
				} else {
					*(buf++)= repchar;
				}
			}
		}
	} else {
		// Normal read
	
		io->read_proc(buf, length, 1, handle);
	}
}

// ==========================================================
// Plugin Interface
// ==========================================================

static int s_format_id;

// ==========================================================
// Plugin Implementation
// ==========================================================

static const char * DLL_CALLCONV
Format() {
	return "RAS";
}

static const char * DLL_CALLCONV
Description() {
	return "Sun Raster Image";
}

static const char * DLL_CALLCONV
Extension() {
	return "ras";
}

static const char * DLL_CALLCONV
RegExpr() {
	return NULL;
}

static const char * DLL_CALLCONV
MimeType() {
	return "image/x-cmu-raster";
}

static BOOL DLL_CALLCONV
Validate(FreeImageIO *io, fi_handle handle) {
	BYTE ras_signature[] = { 0x59, 0xA6, 0x6A, 0x95 };
	BYTE signature[4] = { 0, 0, 0, 0 };

	io->read_proc(signature, 1, sizeof(ras_signature), handle);

	return (memcmp(ras_signature, signature, sizeof(ras_signature)) == 0);
}

static BOOL DLL_CALLCONV
SupportsExportDepth(int depth) {
	return FALSE;
}

static BOOL DLL_CALLCONV 
SupportsExportType(FREE_IMAGE_TYPE type) {
	return FALSE;
}

// ----------------------------------------------------------

static FIBITMAP * DLL_CALLCONV
Load(FreeImageIO *io, fi_handle handle, int page, int flags, void *data) {
	SUNHEADER header;	// Sun file header
	WORD linelength;	// Length of raster line in bytes
	WORD fill;			// Number of fill bytes per raster line
	BOOL rle;			// TRUE if RLE file
	BOOL isRGB;			// TRUE if file type is RT_FORMAT_RGB
	BYTE fillchar;

	FIBITMAP *dib = NULL;
	BYTE *bits;			// Pointer to dib data
	WORD x, y;

	if (handle) {
		try {
			// Read SUN raster header

			io->read_proc(&header, sizeof(SUNHEADER), 1, handle);

#ifndef FREEIMAGE_BIGENDIAN
			// SUN rasterfiles are big endian only

			SwapLong(&header.magic);
			SwapLong(&header.width);
			SwapLong(&header.height);
			SwapLong(&header.depth);
			SwapLong(&header.length);
			SwapLong(&header.type);
			SwapLong(&header.maptype);
			SwapLong(&header.maplength);
#endif

			// Verify SUN identifier

			if (header.magic != RAS_MAGIC)
				throw "Invalid magic number";

			// Allocate a new DIB

			switch(header.depth) {
				case 1:
				case 8:
					dib = FreeImage_Allocate(header.width, header.height, header.depth);
					break;

				case 24:
					dib = FreeImage_Allocate(header.width, header.height, header.depth, FIRGB_RED_MASK, FIRGB_GREEN_MASK, FIRGB_BLUE_MASK);
					break;

				case 32:
					dib = FreeImage_Allocate(header.width, header.height, header.depth, FIRGBA_RED_MASK, FIRGBA_GREEN_MASK, FIRGBA_BLUE_MASK);
					break;
			}

			if (dib == NULL)
				throw "DIB allocation failed";

			// Check the file format

			rle = FALSE;
			isRGB = FALSE;

			switch(header.type) {
				case RT_OLD:
				case RT_STANDARD:
				case RT_FORMAT_TIFF: // I don't even know what these format are...
				case RT_FORMAT_IFF: //The TIFF and IFF format types indicate that the raster
					//file was originally converted from either of these file formats.
					//so lets at least try to process them as RT_STANDARD
					break;

				case RT_BYTE_ENCODED:
					rle = TRUE;
					break;

				case RT_FORMAT_RGB:
					isRGB = TRUE;
					break;

				default:
					throw "Unsupported Sun rasterfile";
			}

			// set up the colormap if needed

			switch(header.maptype) {
				case RMT_NONE :
				{				
					if (header.depth < 24) {
						// Create linear color ramp

						RGBQUAD *pal = FreeImage_GetPalette(dib);

						int numcolors = 1 << header.depth;

						for (int i = 0; i < numcolors; i++) {
							pal[i].rgbRed	= (255 * i) / (numcolors - 1);
							pal[i].rgbGreen = (255 * i) / (numcolors - 1);
							pal[i].rgbBlue	= (255 * i) / (numcolors - 1);
						}
					}

					break;
				}

				case RMT_EQUAL_RGB:
				{
					BYTE *r, *g, *b;

					// Read SUN raster colormap

					int numcolors = 1 << header.depth;

					r = (BYTE*)malloc(3 * numcolors * sizeof(BYTE));
					g = r + numcolors;
					b = g + numcolors;

					RGBQUAD *pal = FreeImage_GetPalette(dib);

					io->read_proc(r, 3 * numcolors, 1, handle);

					for (int i = 0; i < numcolors; i++) {
						pal[i].rgbRed	= r[i];
						pal[i].rgbGreen = g[i];
						pal[i].rgbBlue	= b[i];
					}

					free(r);
					break;
				}

				case RMT_RAW:
				{
					BYTE *colormap;

					// Read (skip) SUN raster colormap.

					colormap = (BYTE *)malloc(header.maplength * sizeof(BYTE));

					io->read_proc(colormap, header.maplength, 1, handle);

					free(colormap);
					break;
				}
			}

			// Calculate the line + pitch
			// Each row is multiple of 16 bits (2 bytes).

			if (header.depth == 1)
				linelength = (WORD)((header.width / 8) + (header.width % 8 ? 1 : 0));
			else
				linelength = (WORD)header.width;

			fill = (linelength % 2) ? 1 : 0;

			int pitch = FreeImage_GetPitch(dib);

			// Read the image data
			
			switch(header.depth) {
				case 1:
				case 8:
				{
					bits = FreeImage_GetBits(dib) + (header.height - 1) * pitch;

					for (y = 0; y < header.height; y++) {
						ReadData(io, handle, bits, linelength, rle);

						bits -= pitch;

						if (fill)
							ReadData(io, handle, &fillchar, fill, rle);
					}

					break;
				}

				case 24:
				{
					BYTE *buf, *bp;

					buf = (BYTE*)malloc(header.width * 3);

					for (y = 0; y < header.height; y++) {
						bits = FreeImage_GetBits(dib) + (header.height - 1 - y) * pitch;

						ReadData(io, handle, buf, header.width * 3, rle);

						bp = buf;

						if (isRGB) {
							for (x = 0; x < header.width; x++) {
								bits[FI_RGBA_RED] = *(bp++);	// red
								bits[FI_RGBA_GREEN] = *(bp++);	// green
								bits[FI_RGBA_BLUE] = *(bp++);	// blue

								bits += 3;
							}
						} else {
							for (x = 0; x < header.width; x++) {
								bits[FI_RGBA_RED] = *(bp + 2);	// red
								bits[FI_RGBA_GREEN] = *(bp + 1);// green
								bits[FI_RGBA_BLUE] = *bp;       // blue

								bits += 3; bp += 3;
							}
						}

						if (fill)
							ReadData(io, handle, &fillchar, fill, rle);
					}

					free(buf);
					break;
				}

				case 32:
				{
					BYTE *buf, *bp;

					buf = (BYTE*)malloc(header.width * 4);

					for (y = 0; y < header.height; y++) {
						bits = FreeImage_GetBits(dib) + (header.height - 1 - y) * pitch;

						ReadData(io, handle, buf, header.width * 4, rle);

						bp = buf;

						if (isRGB) {
							for (x = 0; x < header.width; x++) {
								bits[FI_RGBA_ALPHA] = *(bp++);	// alpha
								bits[FI_RGBA_RED] = *(bp++);	// red
								bits[FI_RGBA_GREEN] = *(bp++);	// green
								bits[FI_RGBA_BLUE] = *(bp++);	// blue

								bits += 4;
							}
						}
						else {
							for (x = 0; x < header.width; x++) {
								bits[FI_RGBA_RED] = *(bp + 3);	// red
								bits[FI_RGBA_GREEN] = *(bp + 2); // green
								bits[FI_RGBA_BLUE] = *(bp + 1);	// blue
								bits[FI_RGBA_ALPHA] = *bp;		// alpha

								bits += 4;
								bp += 4;
							}
						}

						if (fill)
							ReadData(io, handle, &fillchar, fill, rle);
					}

					free(buf);
					break;
				}
			}
			
			return dib;

		} catch (const char *text) {
			FreeImage_OutputMessageProc(s_format_id, text);
	
			return NULL;
		}

	}

	return NULL;
}

// ==========================================================
//   Init
// ==========================================================

void DLL_CALLCONV
InitRAS(Plugin *plugin, int format_id) {
	s_format_id = format_id;

	plugin->format_proc = Format;
	plugin->description_proc = Description;
	plugin->extension_proc = Extension;
	plugin->regexpr_proc = RegExpr;
	plugin->open_proc = NULL;
	plugin->close_proc = NULL;
	plugin->pagecount_proc = NULL;
	plugin->pagecapability_proc = NULL;
	plugin->load_proc = Load;
	plugin->save_proc = NULL;
	plugin->validate_proc = Validate;
	plugin->mime_proc = MimeType;
	plugin->supports_export_bpp_proc = SupportsExportDepth;
	plugin->supports_export_type_proc = SupportsExportType;
	plugin->supports_icc_profiles_proc = NULL;
}
