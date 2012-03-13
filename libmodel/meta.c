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
				case meta_init:
				case meta_destroy:
				case meta_preactivate:
				case meta_postactivate:
				case meta_calmodechange:
				case meta_calpreview:
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
						case meta_init:				name = "meta_init";				meta_location = (void **)&meta->init;			break;
						case meta_destroy:			name = "meta_destroy";			meta_location = (void **)&meta->destroy;		break;
						case meta_preactivate:		name = "meta_preactivate";		meta_location = (void **)&meta->preact;			break;
						case meta_postactivate:		name = "meta_postactivate";		meta_location = (void **)&meta->postact;		break;
						case meta_calmodechange:	name = "meta_calmodechange";	meta_location = (void **)&meta->cal_modechange;	break;
						case meta_calpreview:		name = "meta_calpreview";		meta_location = (void **)&meta->cal_preview;	break;
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
				case meta_blockupdate:
				case meta_blockdestroy:
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
						case meta_blockupdate:
						case meta_blockdestroy:		name = "meta_callback";		list = (void **)&meta->block_callbacks;	length = META_MAX_BLOCKS * 2;		break;
						case meta_blockinput:
						case meta_blockoutput:		name = "meta_io";			list = (void **)&meta->block_ios;		length = META_MAX_BLOCKS * META_MAX_BLOCKIOS; break;
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
	return newmeta;
}

void meta_free(meta_t * meta)
{
	// Sanity check
	if (meta == NULL)
	{
		return;
	}

	free(meta);
}


bool meta_getblock(const meta_t * meta, const char * blockname, char * const * constructor_sig, size_t * ios_length)
{
	return false;
}

bool meta_getblockios(const meta_t * meta, const char * blockname, char * const * names, const metaiotype_t * types, const char * sigs, size_t length)
{
	return false;
}
