#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <maxmeta.h>

// TODO - remove before commit
#ifndef USE_BFD
#define USE_BFD
#endif

#if defined(USE_BFD)
#include <bfd.h>

bool meta_parseelf(meta_t * meta, const char * path, exception_t ** err)
{
	LABELS(end);

	// Sanity check
	if (exception_check(err))
	{
		return false;
	}

	memset(meta, 0, sizeof(meta_t));
	bool rval = false;

	bfd * abfd = bfd_openr(path, NULL);
	if (abfd == NULL)
	{
		exception_set(err, EACCES, "Error reading meta information: %s", bfd_errmsg(bfd_get_error()));
		return false;
	}

	{
		if (!bfd_check_format(abfd, bfd_object))
		{
			exception_set(err, EINVAL, "Wrong module format %s: %s", path, bfd_errmsg(bfd_get_error()));
			goto end;
		}

		asection * sect = bfd_get_section_by_name(abfd, ".meta");
		if (sect == NULL)
		{
			// TODO - should we allow this? If the section doesn't exist, we might want to error ?!?
			// Section doesn't exist, nothing to do
			rval = true;
			goto end;
		}

		size_t section_length = bfd_get_section_size(sect);
		size_t section_index = 0;

		size_t buffer_length = meta_sizemax;
		size_t buffer_index = 0;
		uint8_t buffer[buffer_length];

		while (section_index < section_length)
		{
			size_t read_length = MIN(buffer_length - buffer_index, section_length - section_index);
			if (!bfd_get_section_contents(abfd, sect, buffer + buffer_index, section_index, read_length))
			{
				exception_set(err, EINVAL, "Could not read meta section: %s", bfd_errmsg(bfd_get_error()));
				goto end;
			}

			buffer_index += read_length;
			section_index += read_length;

			// Process as many items in this buffer as possible
			metahead_t * head = (metahead_t *)buffer;
			while (buffer_index > 0 && head->size <= buffer_index)
			{
				size_t struct_length = 0;

				switch (head->type)
				{
					case m_name:
					case m_author:
					{
						const char * switch_name = NULL;
						char * name = NULL;

						switch (head->type)
						{
							case m_name:
							{
								switch_name = "name";
								name = &meta->name[0];
								break;
							}

							case m_author:
							{
								switch_name = "author";
								name = &meta->author[0];
								break;
							}

							default:
							{
								exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
								goto end;
							}
						}

						if (name[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate %s entries!", switch_name);
							goto end;
						}

						struct_length = sizeof(meta_annotate_t);
						meta_annotate_t * annotate = (meta_annotate_t *)head;
						strncpy(name, annotate->name, META_SIZE_ANNOTATE-1);
						break;
					}

					case m_version:
					{
						if (meta->version != 0.0)
						{
							exception_set(err, EINVAL, "Duplicate version entries!");
							goto end;
						}

						struct_length = sizeof(meta_version_t);
						meta_version_t * version = (meta_version_t *)head;
						meta->version = version->version;
						break;
					}

					case m_description:
					{
						if (meta->description[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate description entries!");
							goto end;
						}

						struct_length = sizeof(meta_description_t);
						meta_description_t * desc = (meta_description_t *)head;
						strncpy(meta->description, desc->description, META_SIZE_LONGDESCRIPTION-1);
						break;
					}

					case m_dependency:
					{
						metasize_t i = 0;
						for (; i < META_MAX_DEPENDENCIES; i++)
						{
							if (meta->dependencies[i][0] == '\0')
							{
								struct_length = sizeof(meta_dependency_t);
								meta_dependency_t * dep = (meta_dependency_t *)head;
								strncpy(meta->dependencies[i], dep->dependency, META_SIZE_DEPENDENCY-1);
								break;
							}
						}

						if (i == META_MAX_DEPENDENCIES)
						{
							exception_set(err, ENOMEM, "Too many dependencies! (Out of memory)");
							goto end;
						}

						meta->dependencies_length = i+1;
						break;
					}

					case m_init:
					{
						if (meta->init_name[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate initialization entries!");
							goto end;
						}

						struct_length = sizeof(meta_callback_bv_t);
						meta_callback_bv_t * cb = (meta_callback_bv_t *)head;
						if (strncmp(cb->callback.function_signature, "b:v", META_SIZE_SIGNATURE) != 0)
						{
							exception_set(err, EINVAL, "Invalid initialization function signature!");
							goto end;
						}

						strncpy(meta->init_name, cb->callback.function_name, META_SIZE_FUNCTION-1);
						meta->init = cb->function;
						break;
					}

					case m_destroy:
					case m_preactivate:
					case m_postactivate:
					{
						const char * switch_name = NULL;
						char * name = NULL;
						meta_callback_vv_f * function = NULL;

						switch (head->type)
						{
							case m_destroy:
							{
								switch_name = "destroy";
								name = &meta->destroy_name[0];
								function = &meta->destroy;
								break;
							}

							case m_preactivate:
							{
								switch_name = "preactivate";
								name = &meta->preact_name[0];
								function = &meta->preact;
								break;
							}

							case m_postactivate:
							{
								switch_name = "postactivate";
								name = &meta->postact_name[0];
								function = &meta->postact;
								break;
							}

							default:
							{
								exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
								goto end;
							}
						}

						if (name[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate %s entries!", switch_name);
							goto end;
						}

						struct_length = sizeof(meta_callback_vv_t);
						meta_callback_vv_t * cb = (meta_callback_vv_t *)head;
						if (strncmp(cb->callback.function_signature, "v:v", META_SIZE_SIGNATURE) != 0)
						{
							exception_set(err, EINVAL, "Invalid %s function signature!", switch_name);
							goto end;
						}

						strncpy(name, cb->callback.function_name, META_SIZE_FUNCTION-1);
						*function = cb->function;
						break;
					}

					case m_syscall:
					{
						metasize_t i = 0;
						for (; i < META_MAX_SYSCALLS; i++)
						{
							if (meta->syscalls[i].syscall_name[0] == '\0')
							{
								struct_length = sizeof(meta_callback_t);
								meta_callback_t * cb = (meta_callback_t *)head;
								strncpy(meta->syscalls[i].syscall_name, cb->callback.function_name, META_SIZE_FUNCTION-1);
								strncpy(meta->syscalls[i].syscall_signature, cb->callback.function_signature, META_SIZE_SIGNATURE-1);
								strncpy(meta->syscalls[i].syscall_description, cb->callback.function_description, META_SIZE_SHORTDESCRIPTION-1);
								meta->syscalls[i].function = cb->function;
								break;
							}
						}

						if (i == META_MAX_DEPENDENCIES)
						{
							exception_set(err, ENOMEM, "Too many dependencies! (Out of memory)");
							goto end;
						}

						meta->syscalls_length = i+1;
						break;
					}

					case m_configparam:
					case m_calparam:
					{
						const char * switch_name = NULL;
						char * elems = NULL;
						size_t elem_size = 0;
						metasize_t elems_max = 0;
						metasize_t * length;

						switch (head->type)
						{
							case m_configparam:
							{
								switch_name = "config param";
								elems = (char *)&meta->configparams[0];
								elem_size = sizeof(meta->configparams[0]);
								elems_max = META_MAX_CONFIGPARAMS;
								length = &meta->configparams_length;
								break;
							}

							case m_calparam:
							{
								switch_name = "cal param";
								elems = (char *)&meta->calparams[0];
								elem_size = sizeof(meta->calparams[0]);
								elems_max = META_MAX_CALPARAMS;
								length = &meta->calparams_length;
								break;
							}

							default:
							{
								exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
								goto end;
							}
						}

						metasize_t i = 0;
						for (; i < elems_max; i++)
						{
							if (elems[i * elem_size] == '\0')
							{
								char * name = NULL;
								char * sig = NULL;
								char * desc = NULL;
								meta_variable_m * var = NULL;

								switch (head->type)
								{
									case m_configparam:
									{
										name = &meta->configparams[i].config_name[0];
										sig = &meta->configparams[i].config_signature;
										desc = &meta->configparams[i].config_description[0];
										var = &meta->configparams[i].variable;
										break;
									}

									case m_calparam:
									{
										name = &meta->calparams[i].cal_name[0];
										sig = &meta->calparams[i].cal_signature;
										desc = &meta->calparams[i].cal_description[0];
										var = &meta->calparams[i].variable;
										break;
									}

									default:
									{
										exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
										goto end;
									}
								}

								struct_length = sizeof(meta_variable_t);
								meta_variable_t * cb = (meta_variable_t *)head;
								strncpy(name, cb->variable_name, META_SIZE_VARIABLE-1);
								*sig = cb->variable_signature;
								strncpy(desc, cb->variable_description, META_SIZE_SHORTDESCRIPTION-1);
								*var = cb->variable;

								break;
							}
						}

						if (i == elems_max)
						{
							exception_set(err, ENOMEM, "Too many %ss! (Out of memory)", switch_name);
							goto end;
						}

						*length = i+1;
						break;
					}

					case m_calmodechange:
					{
						if (meta->cal_modechange_name[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate calmodechange entries!");
							goto end;
						}

						struct_length = sizeof(meta_callback_vi_t);
						meta_callback_vi_t * cb = (meta_callback_vi_t *)head;
						if (strncmp(cb->callback.function_signature, "v:i", META_SIZE_SIGNATURE) != 0)
						{
							exception_set(err, EINVAL, "Invalid calmodechange function signature!");
							goto end;
						}

						strncpy(meta->cal_modechange_name, cb->callback.function_name, META_SIZE_FUNCTION-1);
						meta->cal_modechange = cb->function;
						break;
					}

					case m_calpreview:
					{
						if (meta->cal_preview_name[0] != '\0')
						{
							exception_set(err, EINVAL, "Duplicate calpreview entries!");
							goto end;
						}

						struct_length = sizeof(meta_callback_bscpp_t);
						meta_callback_bscpp_t * cb = (meta_callback_bscpp_t *)head;
						if (strncmp(cb->callback.function_signature, "b:scpp", META_SIZE_SIGNATURE) != 0)
						{
							exception_set(err, EINVAL, "Invalid calpreview function signature!");
							goto end;
						}

						strncpy(meta->cal_preview_name, cb->callback.function_name, META_SIZE_FUNCTION-1);
						meta->cal_preview = cb->function;
						break;
					}

					case m_block:
					{
						metasize_t i = 0;
						for (; i < META_MAX_BLOCKS; i++)
						{
							if (meta->blocks[i].block_name[0] == '\0')
							{
								struct_length = sizeof(meta_block_t);
								meta_block_t * block = (meta_block_t *)head;
								strncpy(meta->blocks[i].block_name, block->block_name, META_SIZE_BLOCKNAME-1);
								strncpy(meta->blocks[i].block_description, block->block_description, META_SIZE_LONGDESCRIPTION-1);
								strncpy(meta->blocks[i].constructor_name, block->constructor_name, META_SIZE_FUNCTION-1);
								strncpy(meta->blocks[i].constructor_signature, block->constructor_signature, META_SIZE_SIGNATURE-1);
								strncpy(meta->blocks[i].constructor_description, block->constructor_description, META_SIZE_SHORTDESCRIPTION-1);
								meta->blocks[i].constructor = block->constructor;
								break;
							}
						}

						if (i == META_MAX_BLOCKS)
						{
							exception_set(err, ENOMEM, "Too many blocks! (Out of memory)");
							goto end;
						}

						meta->blocks_length = i+1;
						break;
					}

					case m_blockupdate:
					case m_blockdestroy:
					{
						const char * switch_name = NULL;

						switch (head->type)
						{
							case m_blockupdate:
							{
								switch_name = "block update";
								break;
							}

							case m_blockdestroy:
							{
								switch_name = "block destroy";
								break;
							}

							default:
							{
								exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
								goto end;
							}
						}

						struct_length = sizeof(meta_blockcallback_t);
						meta_blockcallback_t * cb = (meta_blockcallback_t *)head;
						if (strncmp(cb->callback.function_signature, "v:p", META_SIZE_SIGNATURE) != 0)
						{
							exception_set(err, EINVAL, "Invalid %s function signature!", switch_name);
							goto end;
						}

						metasize_t i = 0;
						for (; i < META_MAX_BLOCKS; i++)
						{
							if (strncmp(meta->blocks[i].block_name, cb->block_name, META_SIZE_BLOCKNAME) == 0)
							{
								char * name = NULL;
								meta_callback_vp_f * function;

								switch (head->type)
								{
									case m_blockupdate:
									{
										name = &meta->blocks[i].update_name[0];
										function = &meta->blocks[i].update;
										break;
									}

									case m_blockdestroy:
									{
										name = &meta->blocks[i].destroy_name[0];
										function = &meta->blocks[i].destroy;
										break;
									}

									default:
									{
										exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
										goto end;
									}
								}

								if (name[0] != '\0')
								{
									exception_set(err, EINVAL, "Duplicate %s entries!", switch_name);
									goto end;
								}

								strncpy(name, cb->callback.function_name, META_SIZE_FUNCTION-1);
								*function = cb->function;
								break;
							}
						}

						if (i == META_MAX_BLOCKS)
						{
							exception_set(err, ENOMEM, "Block %s hasn't been defined for %s!", cb->block_name, switch_name);
							goto end;
						}

						break;
					}

					case m_blockinput:
					case m_blockoutput:
					{
						const char * switch_name = NULL;
						metaiotype_t type;

						switch (head->type)
						{
							case m_blockinput:
							{
								switch_name = "block input";
								type = m_input;
								break;
							}

							case m_blockoutput:
							{
								switch_name = "block output";
								type = m_output;
								break;
							}

							default:
							{
								exception_set(err, EFAULT, "Unhandled switch for item %x!", head->type);
								goto end;
							}
						}

						struct_length = sizeof(meta_blockio_t);
						meta_blockio_t * io = (meta_blockio_t *)head;

						metasize_t i = 0;
						for (; i < META_MAX_BLOCKS; i++)
						{
							if (strncmp(meta->blocks[i].block_name, io->block_name, META_SIZE_BLOCKNAME) == 0)
							{
								metasize_t j = 0;
								for (; j < META_MAX_BLOCKIOS; j++)
								{
									if (meta->blocks[i].ios[j].io_name[0] == '\0')
									{
										strncpy(meta->blocks[i].ios[j].io_name, io->io_name, META_SIZE_BLOCKIONAME-1);
										meta->blocks[i].ios[j].io_type = type;
										meta->blocks[i].ios[j].io_signature = io->io_signature;
										strncpy(meta->blocks[i].ios[j].io_description, io->io_description, META_SIZE_SHORTDESCRIPTION-1);
										break;
									}
								}

								if (j == META_MAX_BLOCKIOS)
								{
									exception_set(err, ENOMEM, "Too many ios for block %s! Out of memory)", io->block_name);
									goto end;
								}

								meta->blocks[i].ios_length = j+1;
								break;
							}
						}

						if (i == META_MAX_BLOCKS)
						{
							exception_set(err, ENOMEM, "Block %s hasn't been defined! Can't add a %s!", io->block_name, switch_name);
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

				// Move the processed struct out of the buffer
				memmove(&buffer[0], &buffer[struct_length], buffer_index - struct_length);
				buffer_index -= struct_length;

				// Zero out the upper part of the buffer
				memset(&buffer[buffer_index], 0, buffer_length - buffer_index);
			}
		}

		if (buffer_index > 0)
		{
			// We haven't parsed entire region
			exception_set(err, EINVAL, "Could not parse meta entire region!");
			goto end;
		}

		// Horray! We parsed everything
		rval = true;
	}

end:
	bfd_close(abfd);
	return rval;
}

#endif
