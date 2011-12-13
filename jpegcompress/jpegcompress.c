#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <ctype.h>
#include <jpeglib.h>
#include <jerror.h>

#include <kernel.h>
#include <buffer.h>

MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("Jpeg compression module");

DEF_BLOCK(compressor, jpeg_new, "si");
BLK_ONUPDATE(compressor, jpeg_update);
BLK_ONDESTROY(compressor, jpeg_free);
BLK_INPUT(compressor, width, "i");
BLK_INPUT(compressor, height, "i");
BLK_INPUT(compressor, frame, "x");
BLK_OUTPUT(compressor, frame, "x");


#define BUFFER_STEP			10000

/*-------------------- JPEG STRUCT & MANAGE FUNCS -------------*/
static void jc_yuv422(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to);
static void jc_yuv420(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to);


typedef void (*convert_f)(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to);

typedef struct
{
	int width, height, quality;

	struct jpeg_compress_struct * cinfo;
	struct jpeg_error_mgr * jerr;
	struct jpeg_destination_mgr * dest;
	JSAMPLE * row;
	jmp_buf jmp;

	JOCTET * buffer;
	size_t buffer_length;

	convert_f converter;
} jpeg_t;

static convert_f jpeg_getconverter(char * fmt)
{
	size_t len = strlen(fmt), i = 0;
	for (; i<len; i++)
	{
		fmt[i] = (char)tolower(fmt[i]);
	}

	if (strcmp(fmt, "yuv420") == 0)
	{
		return jc_yuv420;
	}
	else if (strcmp(fmt, "yuyv") == 0 || strcmp(fmt, "yuv422") == 0)
	{
		return jc_yuv422;
	}

	return NULL;
}

void * jpeg_new(char * format, int quality)
{
	jpeg_t * jpeg = malloc0(sizeof(jpeg_t));
	jpeg->quality = quality;
	jpeg->converter = jpeg_getconverter(format);

	if (jpeg->converter == NULL)
	{
		LOG(LOG_ERR, "libJPEG error: Invalid input format: %s", format);
		return NULL;
	}

	return jpeg;
}

static void jpeg_freecompress(jpeg_t * jpeg)
{
	if (jpeg->cinfo != NULL)
	{
		jpeg_abort_compress(jpeg->cinfo);
		jpeg_destroy_compress(jpeg->cinfo);
	}

	free(jpeg->cinfo);
	free(jpeg->jerr);
	free(jpeg->dest);

	jpeg->cinfo = NULL;
	jpeg->jerr = NULL;
	jpeg->dest = NULL;
}

void jpeg_free(void * data)
{
	if (data == NULL)
	{
		return;
	}

	jpeg_t * jpeg = data;
	jpeg_freecompress(jpeg);
	free(jpeg->buffer);
	free(jpeg);
}

/*--------------------------- LIBJPEG ERROR HANDLERS ---------------------*/
static void jpeg_error_exit(j_common_ptr cinfo)
{
	jpeg_t * jpeg = cinfo->client_data;
	cinfo->err->output_message(cinfo);
	longjmp(jpeg->jmp, 1);
}

static void jpeg_output_message(j_common_ptr cinfo)
{
	LOG(LOG_ERR, "libJPEG Error: %s", cinfo->err->jpeg_message_table[cinfo->err->msg_code]);
}

/*-------------------------- LIBJPEG DESTINATION MANAGER HANDLERS --------------*/
void jpeg_destinit(j_compress_ptr cinfo)
{
	jpeg_t * jpeg = cinfo->client_data;

	if (jpeg->buffer == NULL)
	{
		jpeg->buffer = malloc(BUFFER_STEP);
		jpeg->buffer_length = BUFFER_STEP;
	}

	cinfo->dest->next_output_byte = jpeg->buffer;
	cinfo->dest->free_in_buffer = jpeg->buffer_length;
}

void jpeg_destterm(j_compress_ptr cinfo)
{
	jpeg_t * jpeg = cinfo->client_data;
	jpeg->buffer_length = cinfo->dest->next_output_byte - jpeg->buffer;
}

boolean jpeg_destempty(j_compress_ptr cinfo)
{
	//the buffer is empty, malloc more (using BUFFER_STEP) and set the pointers correctly
	jpeg_t * jpeg = cinfo->client_data;

	size_t oldlen = jpeg->buffer_length;
	size_t newlen = jpeg->buffer_length + BUFFER_STEP;

	jpeg->buffer = realloc(jpeg->buffer, newlen);
	jpeg->buffer_length = newlen;

	jpeg->dest->next_output_byte = jpeg->buffer+(newlen-oldlen);
	jpeg->dest->free_in_buffer = newlen-oldlen;

	return true;
}

/*-------------------------- THE BEEF OF THE MODULE ------------------------*/
static void jc_yuv422(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to)
{
	size_t rowindex = row * width * 2;
	size_t toindex = 0;

	while (toindex < width*3)
	{
		unsigned char data[4];
		buffer_setpos(from, rowindex);			// TODO - optimize this!!
		buffer_read(from, data, sizeof(data));

		to[toindex++] = data[0];
		to[toindex++] = data[1];
		to[toindex++] = data[3];

		to[toindex++] = data[2];
		to[toindex++] = data[1];
		to[toindex++] = data[3];

		rowindex += 4;
	}
}

static void jc_yuv420(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to)
{
	size_t yindex = row * width;
	size_t uindex = (width * height) + (row * width)/4;
	size_t vindex = uindex + (width * height)/4;

	size_t toindex = 0;
	while (toindex < width*3)
	{
		unsigned char data[4];
		buffer_setpos(from, yindex);	buffer_read(from, &data[0], 2);	// TODO - optimize this!!
		buffer_setpos(from, uindex);	buffer_read(from, &data[2], 1);
		buffer_setpos(from, vindex);	buffer_read(from, &data[3], 1);

		to[toindex++] = data[0];
		to[toindex++] = data[2];
		to[toindex++] = data[3];

		to[toindex++] = data[1];
		to[toindex++] = data[2];
		to[toindex++] = data[3];

		yindex += 2;
		uindex++;
		vindex++;
	}
}

void jpeg_update(void * object)
{
	const int * width = INPUT(width);
	const int * height = INPUT(height);
	const buffer_t * frame = INPUT(frame);

	if (object == NULL || width == NULL || height == NULL || frame == NULL)
	{
		//required input parameters not there!
		return;
	}

	if (*width == 0 || *height == 0)
	{
		//invalid input dimensions
		return;
	}

	jpeg_t * jpeg = object;
	struct jpeg_compress_struct * cinfo = jpeg->cinfo;
	struct jpeg_error_mgr * jerr = jpeg->jerr;
	struct jpeg_destination_mgr * dest = jpeg->dest;

	if (*width != jpeg->width || *height != jpeg->height)
	{
		jpeg_freecompress(jpeg);
		jpeg->width = *width;
		jpeg->height = *height;

		free(jpeg->row);
		jpeg->row = NULL;
	}

	if (jpeg->row == NULL)
	{
		jpeg->row = malloc(jpeg->width * 3);
	}

	if (jpeg->cinfo == NULL)
	{
		//init the compress struct

		jpeg->cinfo = cinfo = malloc0(sizeof(struct jpeg_compress_struct));
		jpeg->jerr = jerr = malloc0(sizeof(struct jpeg_error_mgr));
		jpeg->dest = dest = malloc0(sizeof(struct jpeg_destination_mgr));
		cinfo->client_data = jpeg;

		cinfo->err = jpeg_std_error(jerr);
		jerr->error_exit = jpeg_error_exit;
		jerr->output_message = jpeg_output_message;

		jpeg_create_compress(cinfo);

		cinfo->dest = dest;
		dest->init_destination = jpeg_destinit;
		dest->term_destination = jpeg_destterm;
		dest->empty_output_buffer = jpeg_destempty;

		cinfo->image_width = jpeg->width;
		cinfo->image_height = jpeg->height;
		cinfo->input_components = 3;
		cinfo->in_color_space = JCS_YCbCr;

		jpeg_set_defaults(cinfo);
		jpeg_set_quality(cinfo, jpeg->quality, true);
	}

	if (setjmp(jpeg->jmp))
	{
		//an error happened!
		LOG(LOG_WARN, "An exception occurred during JPEG compression, aborting frame");

		//reset all struct variables
		jpeg_freecompress(jpeg);
	}
	else
	{
		//start the compression
		jpeg_start_compress(cinfo, true);

		JSAMPROW rowdata[1] = {jpeg->row};
		int rownum = 0;

		for (; rownum < jpeg->height; rownum++)
		{
			jpeg->converter(rownum, jpeg->width, jpeg->height, *frame, jpeg->row);
			jpeg_write_scanlines(cinfo, rowdata, 1);
		}

		jpeg_finish_compress(cinfo);

		buffer_t out = buffer_new();
		buffer_write(out, jpeg->buffer, jpeg->buffer_length);
		OUTPUT(frame, &out);
		buffer_free(out);
	}
}
