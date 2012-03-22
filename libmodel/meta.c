#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aul/base64.h>
#include <maxmeta.h>


#if defined(USE_BFD)
#include <bfd.h>

meta_t * meta_parseelf(const char * path, exception_t ** err)
{
	LABELS(end);

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
				case meta_calonmodechange:
				case meta_calonpreview:
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
						case meta_calonmodechange:	name = "meta_calmodechange";	meta_location = (void **)&meta->cal_modechange;	break;
						case meta_calonpreview:		name = "meta_calpreview";		meta_location = (void **)&meta->cal_preview;	break;
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
				case meta_configparam:
				case meta_calparam:
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
						case meta_configparam:		name = "meta_configparam";	list = (void **)&meta->config_params;	length = META_MAX_CONFIGPARAMS;		break;
						case meta_calparam:			name = "meta_calparam";		list = (void **)&meta->cal_params;		length = META_MAX_CALPARAMS;		break;
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
		LABELS(end_init);

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
		exception_set(err, EFAULT, "Could not load module %s. DL error: %s", meta->path, error);
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


meta_t * meta_copy(meta_t * meta)
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
		for (size_t i = 0; i < META_MAX_CONFIGPARAMS; i++)
		{
			fixptr(newmeta->config_params[i]);
		}
		fixptr(newmeta->cal_modechange);
		fixptr(newmeta->cal_preview);
		for (size_t i = 0; i < META_MAX_CALPARAMS; i++)
		{
			fixptr(newmeta->cal_params[i]);
		}
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


bool meta_getconfigparam(const meta_t * meta, const char * configname, char * sig, const char ** desc)
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
	meta_foreach(variable, meta->config_params, META_MAX_CONFIGPARAMS)
	{
		if (strcmp(variable->variable_name, configname) == 0)
		{
			if (sig != NULL)
			{
				*sig = variable->variable_signature;
			}

			if (desc != NULL)
			{
				*desc = variable->variable_description;
			}

			return true;
		}
	}

	return false;
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
