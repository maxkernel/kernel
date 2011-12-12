#include <string.h>
#include <ctype.h>
#include <ffi.h>
#include <glib.h>

#include <aul/mutex.h>

#include "kernel.h"
#include "kernel-priv.h"
#include "buffer.h"
#include "array.h"

#define IO_BLOCK_NAME		"block instance"

extern mutex_t io_lock;

static void link_noop(const void * out, void * in, size_t outsize, size_t insize);
static void link_copy(const void * out, void * in, size_t outsize, size_t insize);
static void link_strcopy(const void * out, void * in, size_t outsize, size_t insize);
static void link_bufcopy(const void * out, void * in, size_t outsize, size_t insize);
static void link_i2dcopy(const void * out, void * in, size_t outsize, size_t insize);
static void link_d2icopy(const void * out, void * in, size_t outsize, size_t insize);
static void link_c2icopy(const void * out, void * in, size_t outsize, size_t insize);

/*------------------ STATIC FUNCTIONS -----------------------*/

static char * io_instinfo(void * obj)
{
	char * str = "[PLACEHOLDER INFO BLOCK]";
	return strdup(str);
}

static void io_instdestroy(void * obj)
{
	//TODO - finish this
	block_inst_t * inst = obj;

	if (inst->ondestroy != NULL)
	{
		inst->ondestroy(inst->userdata);
	}
}

static size_t io_size(bio_t * type)
{
	switch (type->sig)
	{
		case T_BOOLEAN:			return sizeof(bool);
		case T_INTEGER:			return sizeof(int);
		case T_DOUBLE:			return sizeof(double);
		case T_CHAR:			return sizeof(char);
		case T_STRING:			return (sizeof(char) * AUL_STRING_MAXLEN) + sizeof(char *);
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:			return sizeof(int);
		default:				return 0;
	}
}

static void * io_malloc(bio_t * type)
{
	size_t size = io_size(type);
	if (size == 0)
	{
		LOGK(LOG_FATAL, "Unknown IO type '%c'", type->sig);
	}

	return malloc0(size);
}

static void io_checklink(bio_t * out, bio_t * in)
{
	if (out->sig != in->sig)
	{
		if (out->sig == T_DOUBLE && in->sig == T_INTEGER)
		{
			LOGK(LOG_WARN, "Possible loss of precision in link from %s.%s -> %s.%s (double to integer)", out->block->name, out->name, in->block->name, in->name);
		}
		else if (out->sig == T_CHAR && in->sig == T_INTEGER)
		{
			LOGK(LOG_WARN, "Possible misuse of type in link from %s.%s -> %s.%s (char to integer)", out->block->name, out->name, in->block->name, in->name);
		}
		else if (out->sig == T_INTEGER && in->sig == T_DOUBLE)
		{
			// This is okay
		}
		else
		{
			LOGK(LOG_ERR, "Could not find a link function to convert output '%c' to input '%c' in link from %s.%s -> %s.%s", out->sig, in->sig, out->block->name, out->name, in->block->name, in->name);
		}
	}
}

static blk_link_f io_linkfunc(bio_t * out, bio_t * in)
{
	if (out->sig == in->sig)
	{
		switch (out->sig)
		{
			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
				return link_copy;

			case T_STRING:
				return link_strcopy;

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
				return link_bufcopy;
		}
	}

	switch (out->sig)
	{
		case T_INTEGER:
		{
			if (in->sig == T_DOUBLE)	return link_i2dcopy;
			break;
		}

		case T_DOUBLE:
		{
			if (in->sig == T_INTEGER)	return link_d2icopy;
			break;
		}

		case T_CHAR:
		{
			if (in->sig == T_INTEGER)	return link_c2icopy;
			break;
		}
	}

	return link_noop;
}

static void io_constructor(block_t * blk, void * data, void ** args)
{
	if (blk->new == NULL)
	{
		return;
	}

	LOGK(LOG_DEBUG, "Calling constructor %s on block %s with sig '%s'", blk->new_name, blk->name, blk->new_sig);

	ffi_cif cif;
	ffi_type * atypes[PARAMS_MAX];

	const char * sig = (const char *)blk->new_sig;
	unsigned int i = 0;

	while (*sig != '\0')
	{
		switch (*sig)
		{
			case T_BOOLEAN:
				atypes[i++] = &ffi_type_uint8;	// bool is 1 byte
				break;

			case T_INTEGER:
				atypes[i++] = &ffi_type_sint;
				break;

			case T_DOUBLE:
				atypes[i++] = &ffi_type_double;
				break;

			case T_CHAR:
				atypes[i++] = &ffi_type_uchar;
				break;

			case T_STRING:
				atypes[i++] = &ffi_type_pointer;
				break;

			case T_VOID:
				atypes[i++] = &ffi_type_void;
				break;

			default:
				LOGK(LOG_FATAL, "Unknown/invalid constructor signature type '%c'", *sig);
				//will exit
		}

		sig++;
	}

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, i, &ffi_type_pointer, atypes);
	ffi_call(&cif, (void *)blk->new, data, args);
}

static block_inst_t * io_currentblockinst()
{
	runnable_t * run = exec_getcurrent();
	if (strsuffix(run->kobject.obj_name, IO_BLOCK_NAME))
	{
		return (block_inst_t *)run;
	}

	return NULL;
}

static void io_doinst(void * object)
{
	block_inst_t * blk_inst = object;

	if (blk_inst != NULL)
	{
		io_beforeblock(blk_inst);
		blk_inst->block->onupdate(blk_inst->userdata);
		io_afterblock(blk_inst);
	}
}

/* --------------- LIBRARY ---------------------------------*/

char * io_blockinfo(void * obj)
{
	char * str = "[PLACEHOLDER BLOCK INFO]";
	return strdup(str);
}

void io_blockfree(void * obj)
{
	block_t * block = obj;

	FREES(block->name);
	FREES(block->desc);

	FREES(block->new_name);
	FREES(block->new_sig);
	FREES(block->onupdate_name);
	FREES(block->ondestroy_name);

	//TODO - finish me (free inputs/outputs/links)
}

block_inst_t * io_newblock(block_t * blk, void ** args)
{
	LOGK(LOG_DEBUG, "Creating new block %s in module %s", blk->name, blk->module->kobject.obj_name);

	string_t str = string_new("%s:%s " IO_BLOCK_NAME, blk->module->kobject.obj_name, blk->name);
	block_inst_t * blk_inst = exec_new(string_copy(&str), io_instinfo, io_instdestroy, io_doinst, NULL, sizeof(block_inst_t));
	list_add(&blk->module->block_inst, &blk_inst->module_list);

	blk_inst->block = blk;
	blk_inst->ondestroy = blk->ondestroy;
	LIST_INIT(&blk_inst->inputs_inst);
	LIST_INIT(&blk_inst->outputs_inst);

	list_t * pos;

	list_foreach(pos, &blk->inputs)
	{
		bio_t * in = list_entry(pos, bio_t, block_list);
		binput_inst_t * in_inst = malloc0(sizeof(binput_inst_t));
		in_inst->block_inst = blk_inst;
		in_inst->input = in;
		in_inst->data = io_malloc(in);
		in_inst->src_valid = false;
		in_inst->src_modified = false;
		in_inst->input_valid = false;
		in_inst->input_modified = false;

		list_add(&blk_inst->inputs_inst, &in_inst->block_inst_list);
	}

	list_foreach(pos, &blk->outputs)
	{
		bio_t * out = list_entry(pos, bio_t, block_list);
		boutput_inst_t * out_inst = malloc0(sizeof(boutput_inst_t));
		out_inst->block_inst = blk_inst;
		out_inst->output = out;
		out_inst->data = io_malloc(out);
		out_inst->copybuf = io_malloc(out);
		out_inst->output_valid = false;
		out_inst->output_modified = false;
		LIST_INIT(&out_inst->links);

		list_add(&blk_inst->outputs_inst, &out_inst->block_inst_list);
	}

	io_constructor(blk, &blk_inst->userdata, args);

	return blk_inst;
}

void io_newcomplete(block_inst_t * inst)
{
	//check to make sure block has been properly created and routed
	list_t * pos;

	//check inputs
	list_foreach(pos, &inst->inputs_inst)
	{
		binput_inst_t * in = list_entry(pos, binput_inst_t, block_inst_list);
		if (in->src_inst == NULL)
		{
			LOGK(LOG_WARN, "Unconnected input %s in block %s in module %s", in->input->name, in->block_inst->block->name, in->block_inst->block->module->path);
		}
	}

	//check outputs
	list_foreach(pos, &inst->outputs_inst)
	{
		boutput_inst_t * out = list_entry(pos, boutput_inst_t, block_inst_list);
		if (list_isempty(&out->links))
		{
			LOGK(LOG_WARN, "Unconnected output %s in block %s in module %s", out->output->name, out->block_inst->block->name, out->block_inst->block->module->path);
		}
	}
}

bool io_route(boutput_inst_t * out, binput_inst_t * in)
{
	if (out == NULL || in == NULL)
	{
		LOGK(LOG_ERR, "Could not route. One of the inputs/outputs is null!");
		return false;
	}

	LOGK(LOG_DEBUG, "Routing %s:%s.%s -> %s:%s.%s", out->block_inst->block->module->kobject.obj_name, out->block_inst->block->name, out->output->name, in->block_inst->block->module->kobject.obj_name, in->block_inst->block->name, in->input->name);

	if (in->src_inst != NULL)
	{
		LOGK(LOG_ERR, "Input %s in block %s in module %s has been double targeted (two outputs to one input)", in->input->name, in->block_inst->block->name, in->block_inst->block->module->path);
		return false;
	}

	io_checklink(out->output, in->input);
	in->src_inst = out;
	list_add(&out->links, &in->boutput_inst_list);

	return true;
}

void io_beforeblock(block_inst_t * block)
{
	mutex_lock(&io_lock);
	{
		list_t * pos;
		list_foreach(pos, &block->inputs_inst)
		{
			binput_inst_t * in_inst = list_entry(pos, binput_inst_t, block_inst_list);
			if (in_inst->src_inst != NULL && in_inst->src_modified)
			{
				// The source has been modified, copy it over
				if (in_inst->src_valid)
				{
					// The source is valid, do a regular copy
					io_linkfunc(in_inst->src_inst->output, in_inst->input)(in_inst->src_inst->copybuf, in_inst->data, io_size(in_inst->src_inst->output), io_size(in_inst->input));
				}
				else
				{
					// The source is not valid, set the output * to NULL
					io_linkfunc(in_inst->src_inst->output, in_inst->input)(NULL, in_inst->data, io_size(in_inst->src_inst->output), io_size(in_inst->input));
				}

				// Update the valid and modified flags
				in_inst->input_valid = in_inst->src_valid;
				in_inst->input_modified = in_inst->src_modified;
				in_inst->src_modified = false;
			}
		}
	}
	mutex_unlock(&io_lock);
}

void io_afterblock(block_inst_t * block)
{
	mutex_lock(&io_lock);
	{
		list_t * pos;
		list_foreach(pos, &block->outputs_inst)
		{
			boutput_inst_t * out_inst = list_entry(pos, boutput_inst_t, block_inst_list);
			if (out_inst->output_modified)
			{
				// The output has been modified, copy it to the copybuf and tell all the inputs

				if (out_inst->output_valid)
				{
					// The output is valid, do a regular copy
					io_linkfunc(out_inst->output, out_inst->output)(out_inst->data, out_inst->copybuf, io_size(out_inst->output), io_size(out_inst->output));
				}
				else
				{
					// The output is not valid, call the copy function with a NULL ptr
					io_linkfunc(out_inst->output, out_inst->output)(NULL, out_inst->copybuf, io_size(out_inst->output), io_size(out_inst->output));
				}

				// Tell all the inputs
				list_t * pos2;
				list_foreach(pos2, &out_inst->links)
				{
					binput_inst_t * in_inst = list_entry(pos2, binput_inst_t, boutput_inst_list);
					in_inst->src_valid = out_inst->output_valid;
					in_inst->src_modified = out_inst->output_modified;
				}

				// Update the flags
				out_inst->output_modified = false;
			}
		}
	}
	mutex_unlock(&io_lock);
}

const void * io_doinput(block_inst_t * blk, const char * name)
{
	list_t * pos;
	binput_inst_t * in = NULL;
	list_foreach(pos, &blk->inputs_inst)
	{
		binput_inst_t * test = list_entry(pos, binput_inst_t, block_inst_list);
		if (strcmp(name, test->input->name) == 0)
		{
			in = test;
			break;
		}
	}

	if (in == NULL)
	{
		LOGK(LOG_WARN, "Input '%s' in block %s does not exist!", name, blk->block->name);
		return NULL;
	}

	if (!in->input_valid)
	{
		// Input is not valid, just return NULL
		return NULL;
	}

	return in->data;
}

const void * io_input(const char * name)
{
	block_inst_t * blk = io_currentblockinst();
	if (blk == NULL)
	{
		LOGK(LOG_WARN, "Could not get block instance input, invalid operating context!");
		return NULL;
	}

	return io_doinput(blk, name);
}

void io_dooutput(block_inst_t * blk, const char * name, const void * value)
{
	list_t * pos;
	boutput_inst_t * out_inst = NULL;
	list_foreach(pos, &blk->outputs_inst)
	{
		boutput_inst_t * test = list_entry(pos, boutput_inst_t, block_inst_list);
		if (strcmp(name, test->output->name) == 0)
		{
			out_inst = test;
			break;
		}
	}

	if (out_inst == NULL)
	{
		LOGK(LOG_WARN, "Output %s in block %s does not exist!", name, blk->block->name);
		return;
	}

	io_linkfunc(out_inst->output, out_inst->output)(value, out_inst->data, io_size(out_inst->output), io_size(out_inst->output));

	out_inst->output_valid = value != NULL;
	out_inst->output_modified = true;
}

void io_output(const char * name, const void * value)
{
	block_inst_t * blk = io_currentblockinst();
	if (blk == NULL)
	{
		LOGK(LOG_WARN, "Could not set block instance output, invalid operating context!");
		return;
	}

	io_dooutput(blk, name, value);
}


/*---------- LINK FUNCTIONS -------------------*/
// TODO - standardize outputting NULL values
static void link_noop(const void * out, void * in, size_t outsize, size_t insize)
{
	// This is a noop function, used when there is an incompatible out -> in
}

static void link_copy(const void * out, void * in, size_t outsize, size_t insize)
{
	if (UNLIKELY( out == NULL ))	memset(in, 0, insize);
	else							memcpy(in, out, insize);
}

static void link_strcopy(const void * out, void * in, size_t outsize, size_t insize)
{
	*(char **)in = in + sizeof(char *);

	if (UNLIKELY( out == NULL ))	(*(char **)in)[0] = '\0';
	else							memcpy(*(char **)in, *(const char **)out, strlen(*(const char **)out)+1);
}

static void link_bufcopy(const void * out, void * in, size_t outsize, size_t insize)
{
	if (*(buffer_t *)in != 0)
	{
		buffer_free(*(buffer_t *)in);
	}

	if (UNLIKELY( out == NULL))		*(buffer_t *)in = 0;
	else							*(buffer_t *)in = buffer_dup(*(const buffer_t *)out);
}

static void link_i2dcopy(const void * out, void * in, size_t outsize, size_t insize)
{
	if (UNLIKELY( out == NULL ))	*(double *)in = 0.0;
	else							*(double *)in = (double) *(const int *)out;
}

static void link_d2icopy(const void * out, void * in, size_t outsize, size_t insize)
{
	if (UNLIKELY( out == NULL ))	*(int *)in = 0;
	else							*(int *)in = (int) *(const double *)out;
}

static void link_c2icopy(const void * out, void * in, size_t outsize, size_t insize)
{
	if (UNLIKELY( out == NULL ))	*(int *)in = 0;
	else							*(int *)in = (int) *(const char *)out;
}
