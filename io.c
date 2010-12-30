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
#define IO_FIXED_SIZE(type)	(type == T_BOOLEAN || type == T_INTEGER || type == T_DOUBLE || type == T_CHAR)

extern mutex_t * io_lock;

static void link_copy(boutput_inst_t * out, binput_inst_t * in);
static void link_strcopy(boutput_inst_t * out, binput_inst_t * in);
static void link_bufcopy(boutput_inst_t * out, binput_inst_t * in);
static void link_i2dcopy(boutput_inst_t * out, binput_inst_t * in);
static void link_d2icopy(boutput_inst_t * out, binput_inst_t * in);
static void link_c2icopy(boutput_inst_t * out, binput_inst_t * in);

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
		case T_BOOLEAN:		return sizeof(bool);
		case T_INTEGER:		return sizeof(int);
		case T_DOUBLE:		return sizeof(double);
		case T_CHAR:		return sizeof(char);
	}

	return 0;
}

static void * io_malloc(bio_t * type)
{
	size_t size = io_size(type);
	if (size == 0)
	{
		LOGK(LOG_FATAL, "Unknown IO type '%c'", type->sig);
	}

	return g_malloc(size);
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
			if (in->sig == T_DOUBLE)
			{
				//integer to double is okay
				return link_i2dcopy;
			}

			break;
		}

		case T_DOUBLE:
		{
			if (in->sig == T_INTEGER)
			{
				//warn on double to integer
				LOGK(LOG_WARN, "Possible loss of precision in link from %s.%s -> %s.%s (double to integer)", out->block->name, out->name, in->block->name, in->name);
				return link_d2icopy;
			}

			break;
		}

		case T_CHAR:
		{
			if (in->sig == T_INTEGER)
			{
				//warn on char to integer
				LOGK(LOG_WARN, "Possible misuse of type in link from %s.%s -> %s.%s (char to integer)", out->block->name, out->name, in->block->name, in->name);
				return link_c2icopy;
			}
		}
	}

	LOGK(LOG_ERR, "Could not find a link function to convert output '%c' to input '%c' in link from %s.%s -> %s.%s", out->sig, in->sig, out->block->name, out->name, in->block->name, in->name);
	return NULL;
}

static void io_constructor(block_t * blk, void * data, void ** args)
{
	if (blk->new == NULL)
	{
		return;
	}

	LOGK(LOG_DEBUG, "Calling constructor %s on block %s with sig '%s'", blk->new_name, blk->name, blk->new_sig);

	ffi_cif cif;
	ffi_type ** atypes = g_malloc0(sizeof(ffi_type *) * strlen(blk->new_sig));

	char * sig = (char *)blk->new_sig;
	gint i=0;
	while (*sig != '\0')
	{
		switch (*sig)
		{
			#define __io_constructor_elem(t1, t2) \
				case t1: \
					atypes[i] = &t2; \
					break;

			// BOOL is 1 byte
			__io_constructor_elem(T_BOOLEAN, ffi_type_uint8)
			__io_constructor_elem(T_INTEGER, ffi_type_sint)
			__io_constructor_elem(T_DOUBLE, ffi_type_double)
			__io_constructor_elem(T_CHAR, ffi_type_uchar)
			__io_constructor_elem(T_STRING, ffi_type_pointer)
			__io_constructor_elem(T_VOID, ffi_type_void);

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
				atypes[i] = &ffi_type_pointer;
				break;

			default:
				LOGK(LOG_FATAL, "Unknown constructor signature type '%c'", *sig);
				//will exit
		}

		i++;
		sig++;
	}

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, i, &ffi_type_pointer, atypes);
	ffi_call(&cif, (void *)blk->new, data, args);

	g_free(atypes);
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

	String str = string_new("%s:%s " IO_BLOCK_NAME, blk->module->kobject.obj_name, blk->name);
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

		list_add(&blk_inst->inputs_inst, &in_inst->block_inst_list);
	}

	list_foreach(pos, &blk->outputs)
	{
		bio_t * out = list_entry(pos, bio_t, block_list);
		boutput_inst_t * out_inst = malloc0(sizeof(boutput_inst_t));
		out_inst->block_inst = blk_inst;
		out_inst->output = out;
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
			in->data = io_malloc(in->input);
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

/*
block_inst_t * io_getblockinst(const char * blkid)
{
	block_inst_t * blk_inst = (block_inst_t *)kobj_lookup(blkid);
	if (blk_inst == NULL)
	{
		LOGK(LOG_DEBUG, "Block instance id %s does not exist", blkid);
		return NULL;
	}

	if (blk_inst->class_type != C_RUNNABLE || blk_inst->runtype != RT_BLOCKINST)
	{
		return NULL;
	}

	return blk_inst;
}
*/

bool io_route(boutput_inst_t * out, binput_inst_t * in)
{
	if (out == NULL || in == NULL)
	{
		LOGK(LOG_FATAL, "Could not route. One of the inputs/outputs is null!");
		//will exit
	}

	LOGK(LOG_DEBUG, "Routing %s:%s.%s -> %s:%s.%s", out->block_inst->block->module->kobject.obj_name, out->block_inst->block->name, out->output->name, in->block_inst->block->module->kobject.obj_name, in->block_inst->block->name, in->input->name);

	if (in->src_inst != NULL)
	{
		LOGK(LOG_ERR, "Input %s in block %s in module %s has been double targeted (two outputs to one input)", in->input->name, in->block_inst->block->name, in->block_inst->block->module->path);
		return false;
	}

	blk_link_f link_func = io_linkfunc(out->output, in->input);
	if (link_func == NULL)
	{
		return false;
	}

	in->copy_func = link_func;
	in->src_inst = out;

	list_add(&out->links, &in->boutput_inst_list);

	return true;
}

void io_beforeblock(block_inst_t * block)
{
	mutex_lock(io_lock);

	list_t * pos;
	list_foreach(pos, &block->inputs_inst)
	{
		binput_inst_t * in_inst = list_entry(pos, binput_inst_t, block_inst_list);
		if (in_inst->src_inst != NULL && in_inst->src_inst->copybuf != NULL)
		{
			in_inst->copy_func(in_inst->src_inst, in_inst);
		}
	}

	mutex_unlock(io_lock);
}

void io_afterblock(block_inst_t * block)
{
	mutex_lock(io_lock);

	list_t * pos;
	list_foreach(pos, &block->outputs_inst)
	{
		boutput_inst_t * out_inst = list_entry(pos, boutput_inst_t, block_inst_list);

		//swap pointers
		void * temp = out_inst->copybuf;
		out_inst->copybuf = out_inst->data;
		out_inst->data = temp;
	}

	mutex_unlock(io_lock);
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

	if (in->data == NULL)
	{
		return NULL;
	}

	if (IO_FIXED_SIZE(in->input->sig))
	{
		return in->data;
	}
	else
	{
		return &in->data;
	}
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

void io_dooutput(block_inst_t * blk, const char * name, const void * value, bool docopy)
{
	list_t * pos;
	boutput_inst_t * out = NULL;
	list_foreach(pos, &blk->outputs_inst)
	{
		boutput_inst_t * test = list_entry(pos, boutput_inst_t, block_inst_list);
		if (strcmp(name, test->output->name) == 0)
		{
			out = test;
			break;
		}
	}

	if (out == NULL)
	{
		LOGK(LOG_WARN, "Output %s in block %s does not exist!", name, blk->block->name);
		return;
	}

	if (IO_FIXED_SIZE(out->output->sig))
	{
		if (out->data == NULL)
		{
			out->data = io_malloc(out->output);
		}

		memcpy(out->data, value, io_size(out->output));
	}
	else
	{
		switch (out->output->sig)
		{
			case T_STRING:
				g_free(out->data);
				out->data = docopy? g_strdup(*(char **)value) : *(char **)value;
				break;

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
				buffer_free(out->data);
				out->data = docopy? buffer_copy(*(buffer_t *)value) : *(buffer_t *)value;
				break;

			default:
				LOGK(LOG_ERR, "Unknown type. Cannot copy '%c' into %s.%s.%s", out->output->sig, out->output->block->module->path, out->output->block->name, name);
				return;
		}
	}
}

void io_output(const char * name, const void * value, bool docopy)
{
	block_inst_t * blk = io_currentblockinst();
	if (blk == NULL)
	{
		LOGK(LOG_WARN, "Could not set block instance output, invalid operating context!");
		return;
	}

	io_dooutput(blk, name, value, docopy);
}


/*---------- LINK FUNCTIONS -------------------*/

static void link_copy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data == NULL)
	{
		in->data = io_malloc(in->input);
	}

	memcpy(in->data, out->copybuf, io_size(in->input));
}

static void link_strcopy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data != NULL)
	{
		g_free(in->data);
	}

	in->data = g_strdup(out->copybuf);
}

static void link_bufcopy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data != NULL && buffer_size(out->copybuf) == buffer_size(in->data))
	{
		//buffers are the same size, just memcpy
		memcpy(in->data, out->copybuf, buffer_size(in->data));
	}
	else
	{
		//this is a very inefficient method!
		buffer_free(in->data);
		in->data = buffer_copy(out->copybuf);
	}
}

static void link_i2dcopy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data == NULL)
	{
		in->data = io_malloc(in->input);
	}

	*(double *)in->data = (double) *(int *)out->copybuf;
}

static void link_d2icopy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data == NULL)
	{
		in->data = io_malloc(in->input);
	}

	*(int *)in->data = (int) *(double *)out->copybuf;
}

static void link_c2icopy(boutput_inst_t * out, binput_inst_t * in)
{
	if (in->data == NULL)
	{
		in->data = io_malloc(in->input);
	}

	*(int *)in->data = (int) *(char *)out->copybuf;
}
