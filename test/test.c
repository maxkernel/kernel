#include <string.h>
#include <time.h>

#include <kernel.h>
#include <buffer.h>
#include <map.h>


DEF_BLOCK(myblock, myblock_new, "");
BLK_ONUPDATE(myblock, myblock_update);
BLK_INPUT(myblock, myblock_in, "d");
BLK_OUTPUT(myblock, myblock_out, "d");

void * myblock_new()
{
	return NULL;
}

void myblock_update(void * data)
{



	const double * foo = INPUT(myblock_in);

	if (foo != NULL)
	{
		LOG(LOG_INFO, "Myblock Update (%f)", *foo);
	}
	else
	{
		LOG(LOG_INFO, "Myblock Update (NULL)");
	}


}

