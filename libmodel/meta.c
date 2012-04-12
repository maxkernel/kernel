#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aul/base64.h>
#include <maxmodel/meta.h>


#if defined(USE_BFD)
#include <bfd.h>

meta_t * meta_parseelf(const char * path, exception_t ** err)
{
	labels(end);

	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (strlen(path) >= META_SIZE_PATH)
		{
			exception_set(err, ENOMEM, "Path length too long (META_SIZE_PATH = %d)", META_SIZE_PATH);
			return NULL;
		}
	}


	bfd * abfd = bfd_openr(path, NULL);
	if (abfd == NULL)
	{
		exception_set(err, EACCES, "Error reading meta information: %s", bfd_errmsg(bfd_get_error()));
		return NULL;
	}

	bool status = false;
	meta_t * meta = malloc(sizeof(meta_t));
	memset(meta, 0, sizeof(meta_t));

	meta->meta_version = __meta_struct_version;
	strncpy(meta->path, path, META_SIZE_PATH);

	{
		if (!bfd_check_format(abfd, bfd_object))
		{
			exception_set(err, EINVAL, "Wrong module format %s: %s", path, bfd_errmsg(bfd_get_error()));
			goto end;
		}

		// Get meta section
		asection * sect = bfd_get_section_by_name(abfd, ".meta");
		if (sect == NULL)
		{
			exception_set(err, EINVAL, "Meta section for file %s doesn't exist! (Invalid module object)", path);
			goto end;
		}

		// Get the size of the meta section
		meta->section_size = bfd_get_section_size(sect);
		if (meta->section_size < sizeof(meta_begin_t))
		{
			exception_set(err, EINVAL, "Meta section too small!");
			goto end;
		}

		// Read in the meta section
		meta->buffer = malloc(meta->section_size);
		if (!bfd_get_section_contents(abfd, sect, meta->buffer, 0, meta->section_size))
		{
			exception_set(err, EINVAL, "Could not read meta section: %s", bfd_errmsg(bfd_get_error()));
			goto end;
		}

		// Verify the first few bytes of the meta section (which should be a meta_begin_t struct)
		{
			meta_begin_t * begin = (meta_begin_t *)meta->buffer;
			if (begin->head.size != sizeof(meta_begin_t) || begin->head.type != meta_begin || begin->special != __meta_special)
			{
				exception_set(err, EINVAL, "Unknown start of meta section! [Meta section must begin with meta_name(...)]");
				goto end;
			}
		}

		// Now parse through the meta section
		size_t buffer_index = 0;
		size_t struct_index = 0;
		while (buffer_index < meta->section_size && struct_index < META_MAX_STRUCTS)
		{
			metahead_t * head = (metahead_t *)&meta->buffer[buffer_index];
			meta->buffer_indexes[struct_index] = head;
			meta->buffer_layout[struct_index] = head->type;


			switch (head->type)
			{
				case meta_begin:
				case meta_name:
				case meta_version:
				case meta_author:
				case meta_description:
				case meta_oninit:
				case meta_ondestroy:
				case meta_onpreact:
				case meta_onpostact:
				//case meta_calonmodechange:
				//case meta_calonpreview:
				{
					const char * name = NULL;
					void ** meta_location = NULL;

					switch (head->type)
					{
						case meta_begin:			name = "meta_begin";			meta_location = (void **)&meta->begin;			break;
						case meta_name:				name = "meta_name";				meta_location = (void **)&meta->name;			break;
						case meta_version:			name = "meta_version";			meta_location = (void **)&meta->version;		break;
						case meta_author:			name = "meta_author";			meta_location = (void **)&meta->author;			break;
						case meta_description:		name = "meta_description";		meta_location = (void **)&meta->description;	break;
						case meta_oninit:			name = "meta_init";				meta_location = (void **)&meta->init;			break;
						case meta_ondestroy:		name = "meta_destroy";			meta_location = (void **)&meta->destroy;		break;
						case meta_onpreact:			name = "meta_preactivate";		meta_location = (void **)&meta->preact;			break;
						case meta_onpostact:		name = "meta_postactivate";		meta_location = (void **)&meta->postact;		break;
						//case meta_calonmodechange:	name = "meta_calmodechange";	meta_location = (void **)&meta->cal_modechange;	break;
						//case meta_calonpreview:		name = "meta_calpreview";		meta_location = (void **)&meta->cal_preview;	break;
						default:
						{
							exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
							goto end;
						}
					}

					if (*meta_location != NULL)
					{
						exception_set(err, EINVAL, "Duplicate %s entries!", name);
						goto end;
					}

					*meta_location = (void *)head;
					break;
				}

				case meta_dependency:
				case meta_syscall:
				case meta_config:
				//case meta_calparam:
				case meta_block:
				case meta_blockonupdate:
				case meta_blockondestroy:
				case meta_blockinput:
				case meta_blockoutput:
				{
					const char * name = NULL;
					void ** list = NULL;
					size_t length = 0;

					switch (head->type)
					{
						case meta_dependency:		name = "meta_dependency";	list = (void **)&meta->dependencies;	length = META_MAX_DEPENDENCIES;		break;
						case meta_syscall:			name = "meta_syscall";		list = (void **)&meta->syscalls;		length = META_MAX_SYSCALLS;			break;
						case meta_config:		name = "meta_configparam";	list = (void **)&meta->configs;			length = META_MAX_CONFIGS;		break;
						//case meta_calparam:			name = "meta_calparam";		list = (void **)&meta->cal_params;		length = META_MAX_CALPARAMS;		break;
						case meta_block:			name = "meta_block";		list = (void **)&meta->blocks;			length = META_MAX_BLOCKS;			break;
						case meta_blockonupdate:
						case meta_blockondestroy:	name = "meta_callback";		list = (void **)&meta->block_callbacks;	length = META_MAX_BLOCKCBS;			break;
						case meta_blockinput:
						case meta_blockoutput:		name = "meta_io";			list = (void **)&meta->block_ios;		length = META_MAX_BLOCKIOS; 		break;
						default:
						{
							exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
							goto end;
						}
					}

					size_t i = 0;
					for (; i < length; i++)
					{
						if (list[i] == NULL)
						{
							list[i] = (void *)head;
							break;
						}
					}

					if (i == length)
					{
						exception_set(err, ENOMEM, "Out of %s memory! (size = %d)", name, length);
						goto end;
					}

					break;
				}

				default:
				{
					exception_set(err, EINVAL, "Unknown meta entry: %x", head->type);
					goto end;
				}
			}


			struct_index += 1;
			buffer_index += head->size;
		}

		if (buffer_index != meta->section_size)
		{
			exception_set(err, EINVAL, "Unexpected end of meta section.");
			goto end;
		}
	}

	// Horray! We've parsed it all without errors!
	status = true;

end:
	bfd_close(abfd);
	if (!status)
	{
		// We're here in error, free meta and return null
		if (meta->buffer)
		{
			free(meta->buffer);
		}
		free(meta);
		return NULL;
	}

	return meta;
}

#endif

#ifdef USE_DL
#include <dlfcn.h>

__meta_begin_callback __meta_init = NULL;

bool meta_loadmodule(meta_t * meta, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return false;
		}

		if (meta == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if (meta->path[0] == '\0')
		{
			exception_set(err, EINVAL, "Meta path not set!");
			return false;
		}
	}

	if (meta->dlobject != NULL || meta->resolved)
	{
		exception_set(err, EALREADY, "Module %s has already been loaded", meta->path);
		return false;
	}

	meta->dlobject = dlopen(meta->path, RTLD_NOLOAD);
	if (meta->dlobject != NULL)
	{
		exception_set(err, EALREADY, "Module %s has already been loaded without meta reflect!", meta->path);
		return false;
	}

	exception_t * e = NULL;
	void __do_meta_init(const meta_begin_t * begin)
	{
		labels(end_init);

		// Sanity check
		{
			if (begin == NULL)
			{
				exception_set(&e, EINVAL, "Bad meta address!");
				goto end_init;
			}
		}

		if (begin->head.type != meta_begin || begin->special != __meta_special)
		{
			exception_set(&e, EINVAL, "Bad beginning of meta section! (corrupt?)");
			goto end_init;
		}

		const char * buffer = (const char *)begin;
		for (size_t i = 0; meta->buffer_layout[i] != '\0' && i < META_MAX_SYSCALLS; i++)
		{
			const metahead_t * head = (const metahead_t *)&buffer[(off_t)meta->buffer_indexes[i] - (off_t)meta->buffer];
			if (head->type != meta->buffer_layout[i])
			{
				exception_set(&e, EINVAL, "Meta section mismatch for element %zu. Expected %zx, is %zx", i, meta->buffer_layout[i], head->type);
				goto end_init;
			}

			switch (head->type)
			{
				case meta_name:
				{
					if (meta->name == NULL)
					{
						exception_set(&e, EINVAL, "Meta name not set!");
						goto end_init;
					}

					const meta_annotate_t * name = (const meta_annotate_t *)head;
					if (strcmp(meta->name->name, name->name) != 0)
					{
						exception_set(&e, EINVAL, "Meta name mismatch! Expected %s, is %s", meta->name->name, name->name);
						goto end_init;
					}

					break;
				}

				case meta_version:
				{
					if (meta->version == NULL)
					{
						exception_set(&e, EINVAL, "Meta version not set!");
						goto end_init;
					}

					const meta_version_t * version = (const meta_version_t *)head;
					if (meta->version->version != version->version)
					{
						string_t expect_version = version_tostring(meta->version->version);
						string_t is_version = version_tostring(version->version);
						exception_set(&e, EINVAL, "Meta version mismatch! Expected %s, is %s", expect_version.string, is_version.string);
						goto end_init;
					}

					break;
				}

				default: break;
			}
		}

		memcpy(meta->buffer, begin, meta->section_size);
		meta->resolved = true;

end_init:
		__meta_init = NULL;
	}

	__meta_init = __do_meta_init;
	meta->dlobject = dlopen(meta->path, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);

	if (exception_check(&e))
	{
		exception_set(err, e->code, "Module %s load error: %s", meta->path, e->message);
		exception_free(e);
		return false;
	}

	char * error = dlerror();
	if (error != NULL)
	{
		exception_set(err, EFAULT, "Could not load module. DL error: %s", error);
		return false;
	}

	if (meta->dlobject == NULL || !meta->resolved)
	{
		exception_set(err, EFAULT, "Could not resolve module memory addresses in module %s! (Unknown fault, cb failure)", meta->path);
		return false;
	}

	return true;
}

#endif

meta_t * meta_parsebase64(const char * from, size_t length, exception_t ** err)
{
	// TODO - finish me
	// TODO - make sure to check the meta_version against __meta_struct_version

	return NULL;
}

size_t meta_encodebase64(meta_t * meta, const char * to, size_t length)
{
	// TODO - finish me
	//return base64_encode((void *)meta, sizeof(meta_t), to, length);

	return 0;
}


meta_t * meta_copy(const meta_t * meta)
{
	meta_t * newmeta = malloc(sizeof(meta_t));
	memcpy(newmeta, meta, sizeof(meta_t));

	newmeta->buffer = malloc(newmeta->section_size);
	memcpy(newmeta->buffer, meta->buffer, newmeta->section_size);

	ssize_t offset = ((ssize_t)newmeta->buffer - (ssize_t)meta->buffer);
	#define fixptr(p)		(p) = ((p) == NULL)? NULL : (void *)((ssize_t)(p) + (offset))

	// Fix the pointers to point to the new buffer
	{
		for (size_t i = 0; i < META_MAX_STRUCTS; i++)
		{
			fixptr(newmeta->buffer_indexes[i]);
		}
		fixptr(newmeta->begin);
		fixptr(newmeta->name);
		fixptr(newmeta->version);
		fixptr(newmeta->author);
		fixptr(newmeta->description);
		for (size_t i = 0; i < META_MAX_DEPENDENCIES; i++)
		{
			fixptr(newmeta->dependencies[i]);
		}
		fixptr(newmeta->init);
		fixptr(newmeta->destroy);
		fixptr(newmeta->preact);
		fixptr(newmeta->postact);
		for (size_t i = 0; i < META_MAX_SYSCALLS; i++)
		{
			fixptr(newmeta->syscalls[i]);
		}
		for (size_t i = 0; i < META_MAX_CONFIGS; i++)
		{
			fixptr(newmeta->configs[i]);
		}
		/*
		fixptr(newmeta->cal_modechange);
		fixptr(newmeta->cal_preview);
		for (size_t i = 0; i < META_MAX_CALPARAMS; i++)
		{
			fixptr(newmeta->cal_params[i]);
		}
		*/
		for (size_t i = 0; i < META_MAX_BLOCKS; i++)
		{
			fixptr(newmeta->blocks[i]);
		}
		for (size_t i = 0; i < META_MAX_BLOCKCBS; i++)
		{
			fixptr(newmeta->block_callbacks[i]);
		}
		for (size_t i = 0; i < META_MAX_BLOCKIOS; i++)
		{
			fixptr(newmeta->block_ios[i]);
		}
	}

	return newmeta;
}

void meta_destroy(meta_t * meta)
{
	// Sanity check
	if (meta == NULL)
	{
		return;
	}

	if (meta->buffer != NULL)	free(meta->buffer);
	free(meta);
}


void meta_getinfo(const meta_t * meta, const char ** path, const char ** name, const version_t ** version, const char ** author, const char ** desc)
{
	// Sanity check
	{
		if (meta == NULL)
		{
			return;
		}
	}

	if (path != NULL)
	{
		*path = meta->path;
	}

	if (name != NULL && meta->name != NULL)
	{
		*name = meta->name->name;
	}

	if (version != NULL && meta->version != NULL)
	{
		*version = &meta->version->version;
	}

	if (author != NULL && meta->author != NULL)
	{
		*author = meta->author->name;
	}

	if (desc != NULL && meta->description != NULL)
	{
		*desc = meta->description->description;
	}
}

void meta_getactivators(const meta_t * meta, meta_initializer * init, meta_destroyer * destroy, meta_activator * preact, meta_activator * postact)
{
	// Sanity check
	{
		if (meta == NULL)
		{
			return;
		}
	}

	if (init != NULL && meta->init != NULL)
	{
		*init = meta->init->function;
	}

	if (destroy != NULL && meta->destroy != NULL)
	{
		*destroy = meta->destroy->function;
	}

	if (preact != NULL && meta->preact != NULL)
	{
		*preact = meta->preact->function;
	}

	if (postact != NULL && meta->postact != NULL)
	{
		*postact = meta->postact->function;
	}
}

bool meta_getblock(const meta_t * meta, const char * blockname, char const ** constructor_sig, size_t * ios_length, const char ** desc)
{
	// Sanity check
	{
		if (meta == NULL || blockname == NULL)
		{
			return false;
		}

		if (strlen(blockname) >= META_SIZE_BLOCKNAME)
		{
			return false;
		}
	}

	meta_block_t * block = NULL;
	meta_foreach(block, meta->blocks, META_MAX_BLOCKS)
	{
		if (strcmp(block->block_name, blockname) == 0)
		{
			if (constructor_sig != NULL)
			{
				*constructor_sig = block->constructor_signature;
			}

			if (ios_length != NULL)
			{
				*ios_length = 0;

				meta_blockio_t * blockio = NULL;
				meta_foreach(blockio, meta->block_ios, META_MAX_BLOCKIOS)
				{
					if (strcmp(blockio->blockname, blockname) == 0)
					{
						*ios_length += 1;
					}
				}
			}

			if (desc != NULL)
			{
				*desc = block->block_description;
			}

			return true;
		}
	}

	return false;
}

bool meta_getblockios(const meta_t * meta, const char * blockname, char const ** names, metaiotype_t * types, char * sigs, const char ** descs, size_t length)
{
	// Sanity check
	{
		if (meta == NULL || blockname == NULL)
		{
			return false;
		}

		if (strlen(blockname) >= META_SIZE_BLOCKNAME)
		{
			return false;
		}
	}

	meta_blockio_t * blockio = NULL;
	size_t index = 0;
	meta_foreach(blockio, meta->block_ios, META_MAX_BLOCKIOS)
	{
		if (index == length)
		{
			break;
		}

		if (strcmp(blockio->blockname, blockname) == 0)
		{
			if (names != NULL)
			{
				names[index] = blockio->io_name;
			}

			if (types != NULL)
			{
				switch (blockio->head.type)
				{
					case meta_blockinput:	types[index] = meta_input;		break;
					case meta_blockoutput:	types[index] = meta_output;		break;
					default:				types[index] = meta_unknownio;	break;
				}
			}

			if (sigs != NULL)
			{
				sigs[index] = blockio->io_signature;
			}

			if (descs != NULL)
			{
				descs[index] = blockio->io_description;
			}

			index += 1;
		}
	}

	return true;
}

iterator_t meta_getdependencyitr(const meta_t * meta)
{
	const void * ditr_next(const void * object, void ** itrobject)
	{
		const meta_t * meta = object;
		meta_dependency_t ** dep = (meta_dependency_t **)*itrobject;
		*itrobject = (*dep == NULL)? (void *)&meta->dependencies[0] : (void *)&dep[1];

		return *dep;
	}

	return iterator_new("meta_dependency", ditr_next, NULL, meta, (void *)&meta->dependencies[0]);
}

bool meta_dependencynext(iterator_t itr, const char ** dependency)
{
	const meta_dependency_t * nextdependency = iterator_next(itr, "meta_dependency");
	if (nextdependency != NULL)
	{
		if (dependency != NULL)		*dependency = nextdependency->dependency;
		return true;
	}

	return false;
}

iterator_t meta_getconfigitr(const meta_t * meta)
{
	const void * citr_next(const void * object, void ** itrobject)
	{
		const meta_t * meta = object;
		meta_variable_t ** cfg = (meta_variable_t **)*itrobject;
		*itrobject = (*cfg == NULL)? (void *)&meta->configs[0] : (void *)&cfg[1];

		return *cfg;
	}

	return iterator_new("meta_config", citr_next, NULL, meta, (void *)&meta->configs[0]);
}

bool meta_confignext(iterator_t itr, const meta_variable_t ** variable)
{
	const meta_variable_t * nextconfig = iterator_next(itr, "meta_config");
	if (nextconfig != NULL)
	{
		if (variable != NULL)		*variable = nextconfig;
		return true;
	}

	return false;
}

bool meta_findconfig(const meta_t * meta, const char * configname, const meta_variable_t ** config)
{
	// Sanity check
	{
		if (meta == NULL || configname == NULL)
		{
			return false;
		}

		if (strlen(configname) >= META_SIZE_VARIABLE)
		{
			return false;
		}
	}

	meta_variable_t * variable = NULL;
	meta_foreach(variable, meta->configs, META_MAX_CONFIGS)
	{
		if (strcmp(variable->variable_name, configname) == 0)
		{
			if (config != NULL)		*config = variable;
			return true;
		}
	}

	return false;
}


void meta_getvariable(const meta_variable_t * variable, const char ** name, char * sig, const char ** desc, const void ** value)
{
	// Sanity check
	{
		if (variable == NULL)
		{
			return;
		}
	}

	if (name != NULL)		*name = variable->variable_name;
	if (sig != NULL)		*sig = variable->variable_signature;
	if (desc != NULL)		*desc = variable->variable_description;
	if (value != NULL)		*value = variable->variable;
}
