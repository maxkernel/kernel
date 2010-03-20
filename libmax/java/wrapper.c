#include <glib.h>

#include "org_senseta_MaxRobot.h"
#include "max.h"

static GPtrArray * handles = NULL;

JNIEXPORT jint JNICALL Java_org_senseta_MaxRobot_connect_1local(JNIEnv * env, jclass clazz)
{
	if (handles == NULL)
	{
		handles = g_ptr_array_new();
	}
	
	int i = handles->len;
	
	maxhandle_t * hand = g_malloc0(sizeof(maxhandle_t));
	g_ptr_array_add(handles, (gpointer) hand);
	
	max_local(hand);
	return (jint)i;
}


JNIEXPORT jobject JNICALL Java_org_senseta_MaxRobot_syscall(JNIEnv * env, jclass clazz, jint handle, jstring name, jstring sig, jobjectArray args)
{
	return NULL;
}
