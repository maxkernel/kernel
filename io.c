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
	blockinst_t * inst = obj;

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
	ffi_function_t * ffi = function_build(blk->new, fsig.string, &err);
	if (ffi == NULL)
	{
		const char * path = NULL;
		meta_getinfo(blk->module->backing, &path, NULL, NULL, NULL, NULL);

		LOGK(LOG_FATAL, "Could not call constructor on block %s.%s: Code %d %s", path, blk->name, err->code, err->message);
		// Will exit
	}

	function_call(ffi, data, args);
	function_free(ffi);
}

static blockinst_t * io_currentblockinst()
{
	runnable_t * run = exec_getcurrent();
	if (strsuffix(run->kobject.object_name, IO_BLOCK_NAME))
	{
		return (blockinst_t *)run;
	}

	return NULL;
}

static void io_doinst(void * object)
{
	blockinst_t * blockinst = object;

	if (blockinst != NULL)
	{
		io_beforeblock(blockinst);
		blockinst->block->onupdate(blockinst->userdata);
		io_afterblock(blockinst);
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

blockinst_t * io_newblockinst(block_t * blk, void ** args)
{
	LOGK(LOG_DEBUG, "Creating new block instance %s in module %s", blk->name, blk->module->kobject.object_name);

	string_t str = string_new("%s:%s " IO_BLOCK_NAME, blk->module->kobject.object_name, blk->name);
	blockinst_t * blockinst = exec_new(string_copy(&str), io_instinfo, io_instdestroy, io_doinst, NULL, sizeof(blockinst_t));
	list_add(&blk->module->blockinsts, &blockinst->module_list);

	blockinst->block = blk;
	blockinst->ondestroy = blk->ondestroy;
	LIST_INIT(&blockinst->inputs_inst);
	LIST_INIT(&blockinst->outputs_inst);

	list_t * pos;

	list_foreach(pos, &blk->inputs)
	{
		bio_t * in = list_entry(pos, bio_t, block_list);
		binput_inst_t * ininst = malloc0(sizeof(binput_inst_t));
		ininst->blockinst = blockinst;
		ininst->input = in;
		ininst->stage3 = io_malloc(in);
		ininst->stage3_isnull = true;

		list_add(&blockinst->inputs_inst, &ininst->blockinst_list);
	}

	list_foreach(pos, &blk->outputs)
	{
		bio_t * out = list_entry(pos, bio_t, block_list);
		boutput_inst_t * outinst = malloc0(sizeof(boutput_inst_t));
		outinst->blockinst = blockinst;
		outinst->output = out;
		outinst->stage1 = io_malloc(out);
		outinst->stage1_isnull = true;
		outinst->stage2 = io_malloc(out);
		outinst->stage2_isnull = true;
		LIST_INIT(&outinst->links);

		list_add(&blockinst->outputs_inst, &outinst->blockinst_list);
	}

	io_constructor(blk, &blockinst->userdata, args);

	return blockinst;
}

void io_newcomplete(blockinst_t * blockinst)
{
	//check to make sure block has been properly created and routed
	list_t * pos;

	//check inputs
	list_foreach(pos, &blockinst->inputs_inst)
	{
		binput_inst_t * in = list_entry(pos, binput_inst_t, blockinst_list);
		if (in->src_inst == NULL)
		{
			const char * path = NULL;
			meta_getinfo(in->blockinst->block->module->backing, &path, NULL, NULL, NULL, NULL);
			LOGK(LOG_WARN, "Unconnected input %s in block %s in module %s", in->input->name, in->blockinst->block->name, path);
		}
	}

	//check outputs
	list_foreach(pos, &blockinst->outputs_inst)
	{
		boutput_inst_t * out = list_entry(pos, boutput_inst_t, blockinst_list);
		if (list_isempty(&out->links))
		{
			const char * path = NULL;
			meta_getinfo(out->blockinst->block->module->backing, &path, NULL, NULL, NULL, NULL);
			LOGK(LOG_WARN, "Unconnected output %s in block %s in module %s", out->output->name, out->blockinst->block->name, path);
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

	LOGK(LOG_DEBUG, "Routing %s:%s.%s -> %s:%s.%s", out->blockinst->block->module->kobject.object_name, out->blockinst->block->name, out->output->name, in->blockinst->block->module->kobject.object_name, in->blockinst->block->name, in->input->name);

	if (in->src_inst != NULL)
	{
		const char * path = NULL;
		meta_getinfo(in->blockinst->block->module->backing, &path, NULL, NULL, NULL, NULL);
		LOGK(LOG_ERR, "Input %s in block %s in module %s has been double targeted (two outputs to one input)", in->input->name, in->blockinst->block->name, path);
		return false;
	}

	io_checklink(out->output, in->input);
	in->src_inst = out;
	list_add(&out->links, &in->boutput_inst_list);

	return true;
}

void io_beforeblock(blockinst_t * blockinst)
{
	mutex_lock(&io_lock);
	{
		list_t * pos;
		list_foreach(pos, &blockinst->inputs_inst)
		{
			binput_inst_t * ininst = list_entry(pos, binput_inst_t, blockinst_list);
			if (ininst->src_inst == NULL)
			{
				// Nothing to be done for this input (it's unconnected)
				continue;
			}

			// Copy stage2 to stage3
			io_linkfunc(ininst->src_inst->output, ininst->input)(ininst->src_inst->stage2, ininst->src_inst->stage2_isnull, io_size(ininst->src_inst->output), ininst->stage3, ininst->stage3_isnull, io_size(ininst->input));

			// Update the isnull flags
			ininst->stage3_isnull = ininst->src_inst->stage2_isnull;
		}
	}
	mutex_unlock(&io_lock);
}

void io_afterblock(blockinst_t * blockinst)
{
	mutex_lock(&io_lock);
	{
		list_t * pos;
		list_foreach(pos, &blockinst->outputs_inst)
		{
			boutput_inst_t * outinst = list_entry(pos, boutput_inst_t, blockinst_list);

			// Copy stage1 to stage2
			io_linkfunc(outinst->output, outinst->output)(outinst->stage1, outinst->stage1_isnull, io_size(outinst->output), outinst->stage2, outinst->stage2_isnull, io_size(outinst->output));

			// Update the isnull flags
			outinst->stage2_isnull = outinst->stage1_isnull;
		}
	}
	mutex_unlock(&io_lock);
}

const void * io_doinput(blockinst_t * blockinst, const char * name)
{
	list_t * pos;
	binput_inst_t * in = NULL;
	list_foreach(pos, &blockinst->inputs_inst)
	{
		binput_inst_t * test = list_entry(pos, binput_inst_t, blockinst_list);
		if (strcmp(name, test->input->name) == 0)
		{
			in = test;
			break;
		}
	}

	if (in == NULL)
	{
		LOGK(LOG_WARN, "Input '%s' in block %s does not exist!", name, blockinst->block->name);
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
	blockinst_t * blockinst = io_currentblockinst();
	if (blockinst == NULL)
	{
		LOGK(LOG_WARN, "Could not get block instance input, invalid operating context!");
		return NULL;
	}

	return io_doinput(blockinst, name);
}

void io_dooutput(blockinst_t * blockinst, const char * name, const void * value)
{
	list_t * pos;
	boutput_inst_t * out = NULL;
	list_foreach(pos, &blockinst->outputs_inst)
	{
		boutput_inst_t * test = list_entry(pos, boutput_inst_t, blockinst_list);
		if (strcmp(name, test->output->name) == 0)
		{
			out = test;
			break;
		}
	}

	if (out == NULL)
	{
		LOGK(LOG_WARN, "Output %s in block %s does not exist!", name, blockinst->block->name);
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
	blockinst_t * blockinst = io_currentblockinst();
	if (blockinst == NULL)
	{
		LOGK(LOG_WARN, "Could not set block instance output, invalid operating context!");
		return;
	}

	io_dooutput(blockinst, name, value);
}


binput_inst_t * io_getbinput(const blockinst_t * blockinst, const char * name)
{
	list_t * pos;
	list_foreach(pos, &blockinst->inputs_inst)
	{
		binput_inst_t * ininst = list_entry(pos, binput_inst_t, blockinst_list);
		if (strcmp(name, ininst->input->name) == 0)
		{
			return ininst;
		}
	}

	return NULL;
}

boutput_inst_t * io_getboutput(const blockinst_t * blockinst, const char * name)
{
	list_t * pos;
	list_foreach(pos, &blockinst->outputs_inst)
	{
		boutput_inst_t * outinst = list_entry(pos, boutput_inst_t, blockinst_list);
		if (strcmp(name, outinst->output->name) == 0)
		{
			return outinst;
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
