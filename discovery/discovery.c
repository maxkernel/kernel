#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <aul/mainloop.h>
#include <aul/net.h>

#include <kernel.h>
#include <discovery.h>


#define PACKET_LEN		25

bool enable_discovery = false;
char hostname[50];

bool discovery_newclient(mainloop_t * loop, int fd, fdcond_t cond, void * data)
{
	char buf[PACKET_LEN];
	memset(buf, 0, sizeof(buf));

	struct sockaddr_in remote;
	socklen_t remote_len = sizeof(remote);
	ZERO(remote);

	ssize_t bytesread = recvfrom(fd, buf, PACKET_LEN-1, 0, (struct sockaddr *)&remote, &remote_len);
	if (bytesread > 0)
	{
		bool doreply = (enable_discovery && strcmp(buf, "discover") == 0) || (strprefix(buf, "name=") && strcmp(buf+5, hostname) == 0);
		if (doreply)
		{
			string_t reply = string_new("name=%s\nid=%s\nstarted=%" PRIu64 "\nnow=%" PRIu64 "\nmodel=%s\nversion=%s\nprovider=%s\nprovider_url=%s\n", hostname, kernel_id(), (kernel_timestamp()-kernel_elapsed())/(int64_t)MICROS_PER_SECOND, kernel_timestamp()/(int64_t)MICROS_PER_SECOND, max_model(), VERSION, PROVIDER, PROVIDER_URL);

			if (syscall_exists("service_getstreamconfig", "s:v"))
			{
				const char * config = NULL;
				if (SYSCALL("service_getstreamconfig", &config))
				{
					string_append(&reply, "%s", config);
				}
			}

			sendto(fd, reply.string, reply.length+1, MSG_DONTWAIT, (struct sockaddr *)&remote, remote_len);
		}
	}

	return true;
}

bool module_init()
{
	gethostname(hostname, sizeof(hostname));

	exception_t * e = NULL;
	int sock = udp_server(DISCOVERY_PORT, &e);

	if (sock == -1 || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create UDP Discovery socket on port %d: %s", DISCOVERY_PORT, (e == NULL)? "Unknown error" : e->message);
		exception_free(e);
		return false;
	}

	mainloop_addwatch(NULL, sock, FD_READ, discovery_newclient, NULL);
	return true;
}

module_name("Discovery");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides discovery/auto-discovery of rovers on the network.");
module_oninitialize(module_init);

config_param(enable_discovery, 'b', "Enables other computers to auto-discover this robot automatically (setting to false hides robot on network)");
