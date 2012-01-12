#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <max.h>
#include <serialize.h>

int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Nothing to do.\n");
		return EXIT_FAILURE;
	}

	exception_t * e = NULL;
	bool success = false;

	maxhandle_t hand;
	return_t ret;
	max_initialize(&hand);

	success = max_connectlocal(&hand, &e);
	if (!success)
	{
		fprintf(stderr, "<error> Error during init connection: %s\n", e->message);
		return EXIT_FAILURE;
	}
	
	const char * syscall_name = argv[1];

	success = max_syscall(&hand, &e, &ret, "syscall_signature", "s:s", syscall_name);
	if (!success)
	{
		fprintf(stderr, "<error> Error during syscall lookup: %s\n", e->message);
		return 0;
	}

	if (ret.type != T_RETURN || ret.sig != T_STRING)
	{
		fprintf(stderr, "<error> Invalid reply during syscall lookup\n");
		return EXIT_FAILURE;
	}

	if (strlen(ret.data.t_string) == 0)
	{
		fprintf(stderr, "<error> Syscall %s doesn't exist\n", syscall_name);
		return EXIT_FAILURE;
	}

	const char * syscall_sig = strdup(ret.data.t_string);
	const char * syscall_params = method_params(syscall_sig);
	const char syscall_return = method_returntype(syscall_sig);

	if (strlen(syscall_params) != (argc - 2))
	{
		fprintf(stderr, "<error> Parameter mismatch. Expected %zu got %d\n", strlen(syscall_params), argc - 2);
		return EXIT_FAILURE;
	}

	void * syscall_args[strlen(syscall_params)];
	for (int index=0; index<strlen(syscall_params); index++)
	{
		switch (syscall_params[index])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				bool * v = malloc(sizeof(bool));
				*v = atoi(argv[index + 2]);
				syscall_args[index] = v;
				break;
			}

			case T_INTEGER:
			{
				int * v = malloc(sizeof(int));
				*v = atoi(argv[index + 2]);
				syscall_args[index] = v;
				break;
			}

			case T_DOUBLE:
			{
				double * v = malloc(sizeof(double));
				*v = strtod(argv[index + 2], NULL);
				syscall_args[index] = v;
				break;
			}

			case T_CHAR:
			{
				char * v = malloc(sizeof(char));
				*v = argv[index + 2][0];
				syscall_args[index] = v;
				break;
			}

			case T_STRING:
			{
				char ** v = malloc(sizeof(char *));
				*v = argv[index + 2];
				syscall_args[index] = v;
				break;
			}

			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			{
				fprintf(stderr, "<error> Unsupported buffer or array type for #%d\n", index);
				return EXIT_FAILURE;
			}

			default:
			{
				fprintf(stderr, "<error> Unknown parameter type '%c' for #%d\n", syscall_params[index], index);
				return EXIT_FAILURE;
			}
		}
	}

	success = max_asyscall(&hand, &e, &ret, syscall_name, syscall_sig, syscall_args);
	if (!success)
	{
		fprintf(stderr, "<error> Could not complete syscall: %s\n", e->message);
		return EXIT_FAILURE;
	}

	if (ret.type != T_RETURN || ret.sig != syscall_return)
	{
		fprintf(stderr, "<error> Bad return from syscall\n");
		return EXIT_FAILURE;
	}

	switch (syscall_return)
	{
		case T_VOID:
		{
			fprintf(stdout, "<void>\n");
			break;
		}

		case T_BOOLEAN:
		{
			fprintf(stdout, "<bool> %s\n", ret.data.t_boolean? "true" : "false");
			break;
		}

		case T_INTEGER:
		{
			fprintf(stdout, "<int> %d\n", ret.data.t_integer);
			break;
		}

		case T_DOUBLE:
		{
			fprintf(stdout, "<double> %f\n", ret.data.t_double);
			break;
		}

		case T_CHAR:
		{
			fprintf(stdout, "<char> '%c'\n", ret.data.t_char);
			break;
		}

		case T_STRING:
		{
			fprintf(stdout, "<string> \"%s\"\n", ret.data.t_string);
			break;
		}

		default:
		{
			fprintf(stderr, "<error> Unknown return type '%c'\n", ret.sig);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
