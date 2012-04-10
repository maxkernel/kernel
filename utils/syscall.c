#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

#include <max.h>
#include <serialize.h>

static struct
{
	char * host;
} args = { 0 };

static struct argp_option arg_opts[] = {
	{ "host",	'h',	"HOST",		0,		"Execute syscall on specified host", 0 },
	{ 0 }
};

static error_t parse_args(int key, char * arg, struct argp_state * state);
static struct argp argp = { arg_opts, parse_args, 0, 0 };

static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	switch (key)
	{
		case 'h':   args.host = strdup(arg);   break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

int main(int argc, char ** argv)
{
	int argi;
	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &argi, 0);


	if (argi == argc)
	{
		fprintf(stderr, "Nothing to do.\n");
		return EXIT_FAILURE;
	}

	exception_t * e = NULL;
	bool success = false;

	maxhandle_t hand;
	return_t ret;
	max_initialize(&hand);

	success = max_connect(&hand, (args.host == NULL)? HOST_LOCAL : args.host, &e);
	if (!success)
	{
		fprintf(stderr, "<error> Error during init connection: %s\n", e->message);
		return EXIT_FAILURE;
	}
	
	const char * syscall_name = argv[argi++];

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
	size_t numparams = method_numparams(syscall_params);

	if (numparams != (argc - argi))
	{
		fprintf(stderr, "<error> Parameter mismatch. Expected %zu got %d\n", numparams, argc - argi);
		return EXIT_FAILURE;
	}

	void * syscall_args[numparams];
	const char * param;
	size_t index = 0;
	
	foreach_methodparam(syscall_params, param)
	{
		switch (*param)
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				bool * v = malloc(sizeof(bool));
				*v = parse_bool(argv[argi++], &e);
				syscall_args[index] = v;
				break;
			}

			case T_INTEGER:
			{
				int * v = malloc(sizeof(int));
				*v = parse_int(argv[argi++], &e);
				syscall_args[index] = v;
				break;
			}

			case T_DOUBLE:
			{
				double * v = malloc(sizeof(double));
				*v = parse_double(argv[argi++], &e);
				syscall_args[index] = v;
				break;
			}

			case T_CHAR:
			{
				char * v = malloc(sizeof(char));
				*v = argv[argi++][0];
				syscall_args[index] = v;
				break;
			}

			case T_STRING:
			{
				char ** v = malloc(sizeof(char *));
				*v = argv[argi++];
				syscall_args[index] = v;
				break;
			}

			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			{
				fprintf(stderr, "<error> Unsupported buffer or array type for argument #%zu\n", index);
				return EXIT_FAILURE;
			}

			default:
			{
				fprintf(stderr, "<error> Unknown parameter type (%c) for argument #%zu\n", syscall_params[index], index);
				return EXIT_FAILURE;
			}
		}
		
		if (exception_check(&e))
		{
			fprintf(stderr, "<error> Could not parse parameter type (%c) for argument #%zu. What the hell is '%s'?\n", syscall_params[index], index, argv[argi]);
			return EXIT_FAILURE;
		}

		index += 1;
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
			fprintf(stderr, "<error> Unknown return type (%c)\n", ret.sig);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
