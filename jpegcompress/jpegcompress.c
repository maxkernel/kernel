#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
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

	buffer_t output_frame;
	JOCTET * buffer;
	size_t buffer_length;

	convert_f converter;
} jpeg_t;

static convert_f jpeg_getconverter(char * fmt)
{
	size_t len = strlen(fmt), i = 0;
	for (; i<len; i++)
	{
		fmt[i] = g_ascii_tolower(fmt[i]);
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
	jpeg_t * jpeg = g_malloc0(sizeof(jpeg_t));
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

	g_free(jpeg->cinfo);
	g_free(jpeg->jerr);
	g_free(jpeg->dest);

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
	g_free(jpeg->output_frame);
	g_free(jpeg);
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

	if (jpeg->output_frame == NULL)
	{
		jpeg->output_frame = g_malloc(BUFFER_STEP + sizeof(size_t));
		jpeg->buffer = jpeg->output_frame + sizeof(size_t);
		jpeg->buffer_length = BUFFER_STEP;
	}

	cinfo->dest->next_output_byte = jpeg->buffer;
	cinfo->dest->free_in_buffer = jpeg->buffer_length;
}

void jpeg_destterm(j_compress_ptr cinfo)
{
	jpeg_t * jpeg = cinfo->client_data;

	size_t imglen = cinfo->dest->next_output_byte - jpeg->buffer;
	*(size_t *)jpeg->output_frame = imglen + sizeof(size_t);
}

boolean jpeg_destempty(j_compress_ptr cinfo)
{
	//the buffer is empty, malloc more (using BUFFER_STEP) and set the pointers correctly
	jpeg_t * jpeg = cinfo->client_data;

	size_t oldlen = jpeg->buffer_length;
	size_t newlen = jpeg->buffer_length + BUFFER_STEP;

	jpeg->output_frame = g_realloc(jpeg->output_frame, newlen + sizeof(size_t));
	jpeg->buffer = jpeg->output_frame + sizeof(size_t);
	jpeg->buffer_length = newlen;

	jpeg->dest->next_output_byte = jpeg->buffer+(newlen-oldlen);
	jpeg->dest->free_in_buffer = newlen-oldlen;

	return true;
}

/*-------------------------- THE BEEF OF THE MODULE ------------------------*/
static void jc_yuv422(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to)
{
	unsigned char * frame = buffer_data(from);

	size_t rowindex = row * width * 2;
	size_t toindex = 0;

	while (toindex < width*3)
	{
		to[toindex++] = frame[rowindex];
		to[toindex++] = frame[rowindex+1];
		to[toindex++] = frame[rowindex+3];

		to[toindex++] = frame[rowindex+2];
		to[toindex++] = frame[rowindex+1];
		to[toindex++] = frame[rowindex+3];

		rowindex += 4;
	}
}

static void jc_yuv420(size_t row, JDIMENSION width, JDIMENSION height, buffer_t from, JSAMPLE * to)
{
	unsigned char * frame = buffer_data(from);

	size_t yindex = row * width;
	size_t uindex = (width * height) + (row * width)/4;
	size_t vindex = uindex + (width * height)/4;

	size_t toindex = 0;
	while (toindex < width*3)
	{
		to[toindex++] = frame[yindex];
		to[toindex++] = frame[uindex];
		to[toindex++] = frame[vindex];

		to[toindex++] = frame[yindex+1];
		to[toindex++] = frame[uindex];
		to[toindex++] = frame[vindex];

		yindex += 2;
		uindex++;
		vindex++;
	}
}

void jpeg_update(void * object)
{
	if (object == NULL || ISNULL(width) || ISNULL(height) || ISNULL(frame))
	{
		//required input parameters not there!
		return;
	}

	if (INPUTT(int, width) == 0 || INPUTT(int, height) == 0)
	{
		//invalid input dimensions
		return;
	}

	jpeg_t * jpeg = object;
	struct jpeg_compress_struct * cinfo = jpeg->cinfo;
	struct jpeg_error_mgr * jerr = jpeg->jerr;
	struct jpeg_destination_mgr * dest = jpeg->dest;

	size_t newwidth = INPUTT(int, width), newheight = INPUTT(int, height);

	if (newwidth != jpeg->width || newheight != jpeg->height)
	{
		jpeg_freecompress(jpeg);
		jpeg->width = newwidth;
		jpeg->height = newheight;

		g_free(jpeg->row);
		jpeg->row = NULL;
	}

	if (jpeg->row == NULL)
	{
		jpeg->row = g_malloc(jpeg->width * 3);
	}

	if (jpeg->cinfo == NULL)
	{
		//init the compress struct

		jpeg->cinfo = cinfo = g_malloc0(sizeof(struct jpeg_compress_struct));
		jpeg->jerr = jerr = g_malloc0(sizeof(struct jpeg_error_mgr));
		jpeg->dest = dest = g_malloc0(sizeof(struct jpeg_destination_mgr));
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
			jpeg->converter(rownum, jpeg->width, jpeg->height, INPUTT(buffer_t, frame), jpeg->row);
			jpeg_write_scanlines(cinfo, rowdata, 1);
		}

		jpeg_finish_compress(cinfo);

		OUTPUT(frame, &jpeg->output_frame);
	}
}
