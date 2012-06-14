

#include <aul/mainloop.h>

#include <kernel.h>


static mainloop_t * serviceloop = NULL;


static bool service_init()
{
	LOG(LOG_DEBUG, "Initializing service subsystem");
/*
	serviceloop = mainloop_new("Service network loop");
	mainloop_addtimer(serviceloop, "Service timeout checker", SERVICE_TIMEOUT_US * MILLIS_PER_SECOND, service_checktimeout, NULL);
	if (!kthread_newthread("Service server", KTH_PRIO_LOW, service_runloop, service_stoploop, NULL, NULL))
	{
		LOG(LOG_ERR, "Could not start service server thread!");
		return false;
	}

	send_init();
	{
		size_t i=0;
		for (; i<SERVICE_SENDER_THREADS; i++)
		{
			if (!kthread_newthread("Service sender", KTH_PRIO_MEDIUM, send_startthread, send_stopthread, NULL, NULL))
			{
				LOG(LOG_ERR, "Could not start service sender thread!");
			}
		}
	}

	tcp_init();
	//udp_init();
*/
	return true;
}


module_name("Service");
module_version(0,8,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides a publish/subscribe read-only API to help modules stream sensors/status/other stuff to external clients");
//module_onpreactivate(module_preactivate);
module_oninitialize(service_init);
