// ==========================================================
// PNG Loader and Writer
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Herve Drolon (drolon@infonie.fr)
// - Detlev Vendt (detlev.vendt@brillit.de)
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

#include <stdlib.h>
#include <memory.h> 

// ----------------------------------------------------------

#define PNG_ASSEMBLER_CODE_SUPPORTED
#define PNG_BYTES_TO_CHECK 8

// ----------------------------------------------------------

#include "../LibPNG/png.h"

// ----------------------------------------------------------

static FreeImageIO *s_io;
static fi_handle s_handle;

/////////////////////////////////////////////////////////////////////////////
// libpng interface 
// 

static void
_ReadProc(struct png_struct_def *, unsigned char *data, png_size_t size) {
	s_io->read_proc(data, size, 1, s_handle);
}

static void
_WriteProc(struct png_struct_def *, unsigned char *data, png_size_t size) {
	s_io->write_proc(data, size, 1, s_handle);
}

static void
_FlushProc(png_structp png_ptr) {
	// empty flush implementation
}

static void
error_handler(struct png_struct_def *, const char *error) {
	throw error;
}

// in FreeImage warnings disabled

static void
warning_handler(struct png_struct_def *, const char *warning) {
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
	return "PNG";
}

static const char * DLL_CALLCONV
Description() {
	return "Portable Network Graphics";
}

static const char * DLL_CALLCONV
Extension() {
	return "png";
}

static const char * DLL_CALLCONV
RegExpr() {
	return "^.PNG\r";
}

static const char * DLL_CALLCONV
MimeType() {
	return "image/png";
}

static BOOL DLL_CALLCONV
Validate(FreeImageIO *io, fi_handle handle) {
	BYTE png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	BYTE signature[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	io->read_proc(&signature, 1, 8, handle);

	return (memcmp(png_signature, signature, 8) == 0);
}

static BOOL DLL_CALLCONV
SupportsExportDepth(int depth) {
	return (
			(depth == 1) ||
			(depth == 4) ||
			(depth == 8) ||
			(depth == 24) ||
			(depth == 32)
		);
}

static BOOL DLL_CALLCONV 
SupportsExportType(FREE_IMAGE_TYPE type) {
	return (type == FIT_BITMAP) ? TRUE : FALSE;
}

static BOOL DLL_CALLCONV
SupportsICCProfiles() {
	return TRUE;
}

// ----------------------------------------------------------

static FIBITMAP * DLL_CALLCONV
Load(FreeImageIO *io, fi_handle handle, int page, int flags, void *data) {
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	png_colorp png_palette;
	int bpp, color_type, palette_entries;

	FIBITMAP *dib = NULL;
	RGBQUAD *palette = NULL;	// pointer to dib palette
	png_bytepp  row_pointers = NULL;
	int i;

	s_io = io;
	s_handle = handle;

	if (handle) {
		try {		
			// check to see if the file is in fact a PNG file

			unsigned char png_check[PNG_BYTES_TO_CHECK];

			io->read_proc(png_check, PNG_BYTES_TO_CHECK, 1, handle);

			if (png_sig_cmp(png_check, (png_size_t)0, PNG_BYTES_TO_CHECK) != 0)
				return NULL;	// Bad signature
			
			// create the chunk manage structure

			png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)error_handler, error_handler, warning_handler);

			if (!png_ptr)
				return NULL;			

			// create the info structure

		    info_ptr = png_create_info_struct(png_ptr);

			if (!info_ptr) {
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
				return NULL;
			}

			// init the IO

			png_set_read_fn(png_ptr, info_ptr, _ReadProc);

			if (setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
				return NULL;
			}

			// Because we have already read the signature...

			png_set_sig_bytes(png_ptr, PNG_BYTES_TO_CHECK);

			// read the IHDR chunk

			png_read_info(png_ptr, info_ptr);
			png_get_IHDR(png_ptr, info_ptr, &width, &height, &bpp, &color_type, NULL, NULL, NULL);

			// DIB's don't support >8 bits per sample
			// => tell libpng to strip 16 bit/color files down to 8 bits/color

			if (bpp == 16) {
				png_set_strip_16(png_ptr);
				bpp = 8;
			}

			// Set some additional flags

			switch(color_type) {
				case PNG_COLOR_TYPE_RGB:
				case PNG_COLOR_TYPE_RGB_ALPHA:
#ifndef FREEIMAGE_BIGENDIAN
					// Flip the RGB pixels to BGR (or RGBA to BGRA)

					png_set_bgr(png_ptr);
#endif
					break;

				case PNG_COLOR_TYPE_PALETTE:
					// Expand palette images to the full 8 bits from 2 or 4 bits/pixel

					if ((bpp == 2) || (bpp == 4)) {
						png_set_packing(png_ptr);
						bpp = 8;
					}					

					break;

				case PNG_COLOR_TYPE_GRAY:
				case PNG_COLOR_TYPE_GRAY_ALPHA:
					// Expand grayscale images to the full 8 bits from 2 or 4 bits/pixel

					if ((bpp == 2) || (bpp == 4)) {
						png_set_expand(png_ptr);
						bpp = 8;
					}

					break;

				default:
					throw "PNG format not supported";
			}

			// Get the background color to draw transparent and alpha images over.
			// Note that even if the PNG file supplies a background, you are not required to
			// use it - you should use the (solid) application background if it has one.

			png_color_16p image_background = NULL;
			RGBQUAD rgbBkColor;

			if (color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
				if (png_get_bKGD(png_ptr, info_ptr, &image_background)) {
					rgbBkColor.rgbRed      = image_background->red;
					rgbBkColor.rgbGreen    = image_background->green;
					rgbBkColor.rgbBlue     = image_background->blue;
					rgbBkColor.rgbReserved = 0;
				}
			}

			// if this image has transparency, store the trns values

			png_bytep trans               = NULL;
			int num_trans                 = 0;
			png_color_16p trans_values    = NULL;
			png_uint_32 transparent_value = png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trans_values);

			// unlike the example in the libpng documentation, we have *no* idea where
			// this file may have come from--so if it doesn't have a file gamma, don't
			// do any correction ("do no harm")

			double gamma = 0;
			double screen_gamma = 2.2;

			if (png_get_gAMA(png_ptr, info_ptr, &gamma) && ( flags & PNG_IGNOREGAMMA ) != PNG_IGNOREGAMMA)
				png_set_gamma(png_ptr, screen_gamma, gamma);

			// All transformations have been registered; now update info_ptr data

			png_read_update_info(png_ptr, info_ptr);

			// Create a DIB and write the bitmap header
			// set up the DIB palette, if needed

			switch (color_type) {
				case PNG_COLOR_TYPE_RGB:
					png_set_invert_alpha(png_ptr);

					dib = FreeImage_Allocate(width, height, 24, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
					break;

				case PNG_COLOR_TYPE_RGB_ALPHA :
					dib = FreeImage_Allocate(width, height, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
					break;

				case PNG_COLOR_TYPE_PALETTE :
					dib = FreeImage_Allocate(width, height, bpp);

					png_get_PLTE(png_ptr,info_ptr, &png_palette,&palette_entries);

					palette = FreeImage_GetPalette(dib);

					// store the palette

					for (i = 0; i < palette_entries; i++) {
						palette[i].rgbRed   = png_palette[i].red;
						palette[i].rgbGreen = png_palette[i].green;
						palette[i].rgbBlue  = png_palette[i].blue;
					}

					// store the transparency table

					if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
						FreeImage_SetTransparencyTable(dib, (BYTE *)trans, num_trans);					

					break;

				case PNG_COLOR_TYPE_GRAY:
				case PNG_COLOR_TYPE_GRAY_ALPHA:
					dib = FreeImage_Allocate(width, height, bpp);

					palette = FreeImage_GetPalette(dib);
					palette_entries = 1 << bpp;

					for (i = 0; i < palette_entries; i++) {
						palette[i].rgbRed   =
						palette[i].rgbGreen =
						palette[i].rgbBlue  = (i * 255) / (palette_entries - 1);
					}

					// store the transparency table

					if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
						FreeImage_SetTransparencyTable(dib, (BYTE *)trans, num_trans);					

					break;
			}

			// store the background color 
			if(image_background) {
				FreeImage_SetBackgroundColor(dib, &rgbBkColor);
			}

			// get physical resolution

			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_pHYs)) {
				png_uint_32 res_x, res_y;
				
				// We'll overload this var and use 0 to mean no phys data,
				// since if it's not in meters we can't use it anyway

				int res_unit_type = 0;

				png_get_pHYs(png_ptr,info_ptr,&res_x,&res_y,&res_unit_type);

				if (res_unit_type == 1) {
					BITMAPINFOHEADER *bih = FreeImage_GetInfoHeader(dib);

					bih->biXPelsPerMeter = res_x;
					bih->biYPelsPerMeter = res_y;
				}
			}

			// get possible ICC profile

			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_iCCP)) {
				png_charp profile_name = NULL;
				png_charp profile_data = NULL;
				png_uint_32 profile_length = 0;
				int  compression_type;

				png_get_iCCP(png_ptr, info_ptr, &profile_name, &compression_type, &profile_data, &profile_length);

				// copy ICC profile data (must be done after FreeImage_Allocate)

				FreeImage_CreateICCProfile(dib, profile_data, profile_length);
			}


			// set the individual row_pointers to point at the correct offsets

			row_pointers = (png_bytepp)malloc(height * sizeof(png_bytep));

			if (!row_pointers) {
				if (palette)
					png_free(png_ptr, palette);				

				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

				FreeImage_Unload(dib);
				free(row_pointers);
				return NULL;
			}

			// read in the bitmap bits via the pointer table

			for (png_uint_32 k = 0; k < height; k++)				
				row_pointers[height - 1 - k] = FreeImage_GetScanLine(dib, k);			

			png_read_image(png_ptr, row_pointers);

			// check if the bitmap contains transparency, if so enable it in the header

			if (FreeImage_GetBPP(dib) == 32)
				if (FreeImage_GetColorType(dib) == FIC_RGBALPHA)
					FreeImage_SetTransparent(dib, TRUE);
				else
					FreeImage_SetTransparent(dib, FALSE);
				
			// cleanup

			if (row_pointers)
				free(row_pointers);

			png_read_end(png_ptr, info_ptr);

			if (png_ptr)
				png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

			return dib;
		} catch (const char *text) {
			if (png_ptr)
				png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
			
			if (row_pointers)
				free(row_pointers);

			if (dib)
				FreeImage_Unload(dib);			

			FreeImage_OutputMessageProc(s_format_id, text);
			
			return NULL;
		}
	}			

	return NULL;
}

static BOOL DLL_CALLCONV
Save(FreeImageIO *io, FIBITMAP *dib, fi_handle handle, int page, int flags, void *data) {
	png_structp png_ptr;
	png_infop info_ptr;
	png_colorp palette = NULL;
	png_uint_32 width, height;
	BOOL has_alpha_channel = FALSE;

	RGBQUAD *pal;	// pointer to dib palette
	int bit_depth;
	int palette_entries;
	int	interlace_type;

	s_io = io;
	s_handle = handle;

	if ((dib) && (handle)) {
		try {
			// create the chunk manage structure

			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)error_handler, error_handler, warning_handler);

			if (!png_ptr)  {
				return FALSE;
			}

			// Allocate/initialize the image information data.

			info_ptr = png_create_info_struct(png_ptr);

			if (!info_ptr)  {
				png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
				return FALSE;
			}

			// Set error handling.  REQUIRED if you aren't supplying your own
			// error handling functions in the png_create_write_struct() call.

			if (setjmp(png_jmpbuf(png_ptr)))  {
				// If we get here, we had a problem reading the file

				png_destroy_write_struct(&png_ptr, &info_ptr);

				return FALSE;
			}

			// init the IO

			png_set_write_fn(png_ptr, info_ptr, _WriteProc, _FlushProc);

			// set physical resolution

			BITMAPINFOHEADER *bih = FreeImage_GetInfoHeader(dib);
			png_uint_32 res_x = bih->biXPelsPerMeter;
			png_uint_32 res_y = bih->biYPelsPerMeter;

			if ((res_x > 0) && (res_y > 0))  {
				png_set_pHYs(png_ptr, info_ptr, res_x, res_y, 1);
			}
	
			// Set the image information here.  Width and height are up to 2^31,
			// bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
			// the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
			// PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
			// or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
			// PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
			// currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED

			interlace_type = PNG_INTERLACE_NONE;	// Default value
			width = FreeImage_GetWidth(dib);
			height = FreeImage_GetHeight(dib);
			bit_depth = FreeImage_GetBPP(dib);			

			if (bit_depth == 16) {
				png_destroy_write_struct(&png_ptr, &info_ptr);

				throw "Unsupported format";	// Note: this could be enhanced here...
			}

			bit_depth = (bit_depth > 8) ? 8 : bit_depth;

			switch (FreeImage_GetColorType(dib)) {
				case FIC_MINISWHITE:
					// Invert monochrome files to have 0 as black and 1 as white (no break here)
					png_set_invert_mono(png_ptr);

				case FIC_MINISBLACK:
					png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, 
						PNG_COLOR_TYPE_GRAY, interlace_type, 
						PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
					break;

				case FIC_PALETTE:
				{
					png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, 
						PNG_COLOR_TYPE_PALETTE, interlace_type, 
						PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

					// set the palette

					palette_entries = 1 << bit_depth;
					palette = (png_colorp)png_malloc(png_ptr, palette_entries * sizeof (png_color));
					pal = FreeImage_GetPalette(dib);

					for (int i = 0; i < palette_entries; i++) {
						palette[i].red   = pal[i].rgbRed;
						palette[i].green = pal[i].rgbGreen;
						palette[i].blue  = pal[i].rgbBlue;
					}
					
					png_set_PLTE(png_ptr, info_ptr, palette, palette_entries);

					// You must not free palette here, because png_set_PLTE only makes a link to
					// the palette that you malloced.  Wait until you are about to destroy
					// the png structure.

					break;
				}

				case FIC_RGBALPHA :
					has_alpha_channel = TRUE;

					png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, 
						PNG_COLOR_TYPE_RGBA, interlace_type, 
						PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

#ifndef FREEIMAGE_BIGENDIAN
					png_set_bgr(png_ptr); // flip BGR pixels to RGB
#endif
					break;
	
				case FIC_RGB:
					png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, 
						PNG_COLOR_TYPE_RGB, interlace_type, 
						PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

#ifndef FREEIMAGE_BIGENDIAN
					png_set_bgr(png_ptr); // flip BGR pixels to RGB
#endif
					break;
					
				case FIC_CMYK:
					break;
			}

			// write possible ICC profile

			FIICCPROFILE *iccProfile = FreeImage_GetICCProfile(dib);
			if (iccProfile->size && iccProfile->data) {
				png_set_iCCP(png_ptr, info_ptr, "Embedded Profile", 0, (png_charp)iccProfile->data, iccProfile->size);
			}

			// Optional gamma chunk is strongly suggested if you have any guess
			// as to the correct gamma of the image.
			// png_set_gAMA(png_ptr, info_ptr, gamma);

			// set the transparency table

			if ((bit_depth == 8) && (FreeImage_IsTransparent(dib)) && (FreeImage_GetTransparencyCount(dib) > 0))
				png_set_tRNS(png_ptr, info_ptr, FreeImage_GetTransparencyTable(dib), FreeImage_GetTransparencyCount(dib), NULL);

			// set the background color

			if(FreeImage_HasBackgroundColor(dib)) {
				png_color_16 image_background;
				RGBQUAD rgbBkColor;

				FreeImage_GetBackgroundColor(dib, &rgbBkColor);
				memset(&image_background, 0, sizeof(png_color_16));
				image_background.blue  = rgbBkColor.rgbBlue;
				image_background.green = rgbBkColor.rgbGreen;
				image_background.red   = rgbBkColor.rgbRed;
				image_background.index = rgbBkColor.rgbReserved;

				png_set_bKGD(png_ptr, info_ptr, &image_background);
			}
			
			// Write the file header information.

			png_write_info(png_ptr, info_ptr);

			// write out the image data

			if ((bit_depth == 32) && (!has_alpha_channel)) {
				BYTE *buffer = (BYTE *)malloc(width * 3);

				for (png_uint_32 k = 0; k < height; k++) {
					FreeImage_ConvertLine32To24(buffer, FreeImage_GetScanLine(dib, height - k - 1), width);

					png_write_row(png_ptr, buffer);
				}

				free(buffer);
			} else {
				for (png_uint_32 k = 0; k < height; k++) {
					png_write_row(png_ptr, FreeImage_GetScanLine(dib, height - k - 1));
				}
			}

			// It is REQUIRED to call this to finish writing the rest of the file
			// Bug with png_flush

			png_write_end(png_ptr, info_ptr);

			// clean up after the write, and free any memory allocated
			if (palette)
				png_free(png_ptr, palette);

			png_destroy_write_struct(&png_ptr, &info_ptr);

			return TRUE;
		} catch (const char *text) {
			FreeImage_OutputMessageProc(s_format_id, text);
		}
	}

	return FALSE;
}

// ==========================================================
//   Init
// ==========================================================

void DLL_CALLCONV
InitPNG(Plugin *plugin, int format_id) {
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
	plugin->save_proc = Save;
	plugin->validate_proc = Validate;
	plugin->mime_proc = MimeType;
	plugin->supports_export_bpp_proc = SupportsExportDepth;
	plugin->supports_export_type_proc = SupportsExportType;
	plugin->supports_icc_profiles_proc = SupportsICCProfiles;
}
