#include <string.h>
#include <ctype.h>
#include <glib.h>

#include <aul/mutex.h>

#include <buffer.h>
#include <array.h>
#include <kernel.h>
#include <kernel-priv.h>


#define IO_BLOCK_NAME		"block instance"

extern mutex_t io_lock;

static void link_noop(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_copy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_strcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_bufcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_i2dcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_d2icopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);
static void link_c2icopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize);

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
		case T_STRING:			return sizeof(char) * AUL_STRING_MAXLEN;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:			return sizeof(buffer_t);
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

	string_t fsig = string_new("%c:%s", T_POINTER, blk->new_sig);

	exception_t * err = NULL;
	ffi_t * ffi = function_build(blk->new, fsig.string, &err);
	if (ffi == NULL)
	{
		LOGK(LOG_FATAL, "Could not call constructor on block %s.%s: Code %d %s", blk->module->path, blk->name, err->code, err->message);
		// Will exit
	}

	function_call(ffi, data, args);
	function_free(ffi);
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
		in_inst->stage3 = io_malloc(in);
		in_inst->stage3_isnull = true;

		list_add(&blk_inst->inputs_inst, &in_inst->block_inst_list);
	}

	list_foreach(pos, &blk->outputs)
	{
		bio_t * out = list_entry(pos, bio_t, block_list);
		boutput_inst_t * out_inst = malloc0(sizeof(boutput_inst_t));
		out_inst->block_inst = blk_inst;
		out_inst->output = out;
		out_inst->stage1 = io_malloc(out);
		out_inst->stage1_isnull = true;
		out_inst->stage2 = io_malloc(out);
		out_inst->stage2_isnull = true;
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
			if (in_inst->src_inst == NULL)
			{
				// Nothing to be done for this input (it's unconnected)
				continue;
			}

			// Copy stage2 to stage3
			io_linkfunc(in_inst->src_inst->output, in_inst->input)(in_inst->src_inst->stage2, in_inst->src_inst->stage2_isnull, io_size(in_inst->src_inst->output), in_inst->stage3, in_inst->stage3_isnull, io_size(in_inst->input));

			// Update the isnull flags
			in_inst->stage3_isnull = in_inst->src_inst->stage2_isnull;
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

			// Copy stage1 to stage2
			io_linkfunc(out_inst->output, out_inst->output)(out_inst->stage1, out_inst->stage1_isnull, io_size(out_inst->output), out_inst->stage2, out_inst->stage2_isnull, io_size(out_inst->output));

			// Update the isnull flags
			out_inst->stage2_isnull = out_inst->stage1_isnull;
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

	if (in->stage3_isnull)
	{
		return NULL;
	}

	void * value = in->stage3;

	// Correct for type-specific io
	switch (in->input->sig)
	{
		case T_STRING:
		{
			value = &in->stage3;
			break;
		}
	}

	return value;
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

	// Correct for type-specific io
	switch (out->output->sig)
	{
		case T_STRING:
		{
			if (value != NULL)
			{
				value = *(char **)value;
			}

			break;
		}
	}

	io_linkfunc(out->output, out->output)(value, value == NULL, io_size(out->output), out->stage1, out->stage1_isnull, io_size(out->output));
	out->stage1_isnull = value == NULL;
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


binput_inst_t * io_getbinput(const block_inst_t * block_inst, const char * name)
{
	list_t * pos;
	list_foreach(pos, &block_inst->inputs_inst)
	{
		binput_inst_t * in_inst = list_entry(pos, binput_inst_t, block_inst_list);
		if (strcmp(name, in_inst->input->name) == 0)
		{
			return in_inst;
		}
	}

	return NULL;
}

boutput_inst_t * io_getboutput(const block_inst_t * block_inst, const char * name)
{
	list_t * pos;
	list_foreach(pos, &block_inst->outputs_inst)
	{
		boutput_inst_t * out_inst = list_entry(pos, boutput_inst_t, block_inst_list);
		if (strcmp(name, out_inst->output->name) == 0)
		{
			return out_inst;
		}
	}

	return NULL;
}


/*---------- LINK FUNCTIONS -------------------*/
// TODO - standardize outputting NULL values
static void link_noop(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	// This is a noop function, used when there is an incompatible out -> in
}

static void link_copy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	if (!out_isnull)
	{
		memcpy(in, out, insize);
	}
}

static void link_strcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	if (!out_isnull)
	{
		strncpy(in, out, AUL_STRING_MAXLEN-1);
	}
}

static void link_bufcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	const buffer_t * outbuf = out;
	buffer_t * inbuf = in;

	if (!in_isnull)
	{
		buffer_free(*inbuf);
	}

	if (!out_isnull)
	{
		*inbuf = buffer_dup(*outbuf);
	}
}

static void link_i2dcopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	if (!out_isnull)
	{
		*(double *)in = (double) *(const int *)out;
	}
}

static void link_d2icopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	if (!out_isnull)
	{
		*(int *)in = (int) *(const double *)out;
	}
}

static void link_c2icopy(const void * out, bool out_isnull, size_t outsize, void * in, bool in_isnull, size_t insize)
{
	if (!out_isnull)
	{
		*(int *)in = (int) *(const char *)out;
	}
}
