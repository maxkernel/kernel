#include <matheval.h>
#include <glib.h>

#include "kernel.h"
#include "kernel-priv.h"

double math_eval(char * strexpr)
{
	void * expr = evaluator_create(strexpr);
	if (expr == NULL)
	{
		LOGK(LOG_WARN, "Could not evaluate expression '%s'. Parse error", strexpr);
		return 0.0;
	}

	double value = evaluator_evaluate(expr, 0, NULL, NULL);
	evaluator_destroy(expr);

	return value;
}
