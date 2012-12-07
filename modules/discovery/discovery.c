#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <aul/mainloop.h>
#include <aul/net.h>

#include <kernel.h>
#include <discovery.h>


#define PACKET_LEN			25

#define HOSTNAME_DEFAULT	"max_robot"
#define HOSTNAME_LEN		50

static bool enable_discovery = false;
static char hostname[HOSTNAME_LEN];
static fdwatcher_t udp_watcher;

static bool discovery_newclient(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	unused(userdata);

	char buf[PACKET_LEN];
	memset(buf, 0, sizeof(buf));

	struct sockaddr_in remote;
	memset(&remote, 0, sizeof(struct sockaddr_in));
	socklen_t remote_len = sizeof(struct sockaddr_in);

	ssize_t bytesread = recvfrom(fd, buf, PACKET_LEN-1, 0, (struct sockaddr *)&remote, &remote_len);
	if (bytesread > 0)
	{
		bool doreply = (enable_discovery && strcmp(buf, "discover") == 0) || (strprefix(buf, "name=") && strcmp(buf+5, hostname) == 0);
		if (doreply)
		{
			string_t reply = string_new("name=%s\nid=%s\nstarted=%" PRIu64 "\nnow=%" PRIu64 "\nmodel=%s\nversion=%s\nprovider=%s\nprovider_url=%s", hostname, kernel_id(), (kernel_timestamp()-kernel_elapsed())/(int64_t)MICROS_PER_SECOND, kernel_timestamp()/(int64_t)MICROS_PER_SECOND, max_model(), version_tostring(kernel_version()).string, PROVIDER, PROVIDER_URL);

			// TODO IMPORTANT - get this syscall working
			if (syscall_exists("service_getstreamconfig", "s:v"))
			{
				const char * config = NULL;
				if (SYSCALL("service_getstreamconfig", &config))
				{
					string_append(&reply, "\n%s", config);
				}
			}

			sendto(fd, reply.string, reply.length+1, MSG_DONTWAIT, (struct sockaddr *)&remote, remote_len);
		}
	}

	return true;
}

static bool discovery_init()
{
	labels(after_udp);

	// Initialize the watchers
	{
		watcher_init(&udp_watcher);
	}

	// Get the hostname
	{
		memset(hostname, 0, HOSTNAME_LEN);
		if (gethostname(hostname, HOSTNAME_LEN - 1) < 0)
		{
			LOG(LOG_WARN, "Could not get computer hostname for discovery clients. Using default: %s", HOSTNAME_DEFAULT);
			strncpy(hostname, HOSTNAME_DEFAULT, HOSTNAME_LEN - 1);
		}
	}

	// Set up udp discovery server
	{
		exception_t * e = NULL;
		LOG(LOG_DEBUG, "Enabling auto-discovery over network");

		int sock = udp_server(DISCOVERY_PORT, &e);
		if (sock == -1 || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not create udp discovery socket on port %d: %s", DISCOVERY_PORT, exception_message(e));
			exception_free(e);
			goto after_udp;
		}

		watcher_newfd(&udp_watcher, sock, FD_READ, discovery_newclient, NULL);
		if (!mainloop_addwatcher(kernel_mainloop(), &udp_watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add udp discovery socket to mainloop: %s", exception_message(e));
			exception_free(e);

			watcher_close(&udp_watcher);
			goto after_udp;
		}

		LOG(LOG_DEBUG, "Awaiting discovery clients on udp port %d", DISCOVERY_PORT);
	}
after_udp:

	return true;
}

static void discovery_destroy()
{
	// Remove udp watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&udp_watcher, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove udp watcher: %s", exception_message(e));
			exception_free(e);
		}
	}
}

module_name("Discovery");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides discovery/auto-discovery of rovers on the network.");
module_oninitialize(discovery_init);
module_ondestroy(discovery_destroy);

module_config(enable_discovery, 'b', "Enables other computers to auto-discover this robot automatically (setting to false hides robot on network)");
