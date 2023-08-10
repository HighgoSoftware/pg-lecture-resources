#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(get_sum);

Datum
get_sum(PG_FUNCTION_ARGS)
{
	bool isnull, isnull2;
	int a = 0, b = 0, sum = 0;

	isnull = PG_ARGISNULL(0);
	isnull2 = PG_ARGISNULL(1);
	if (isnull || isnull2)
		ereport( ERROR,
			( errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("the input must be two integers")));

	a = PG_GETARG_INT32(0);
	b = PG_GETARG_INT32(1);
	sum = a + b;

	PG_RETURN_INT32(sum);
}

