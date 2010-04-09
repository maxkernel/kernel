#include <dc1394/dc1394.h>
#include <unistd.h>

#include <kernel.h>

#define CAM_MODE	DC1394_VIDEO_MODE_640x480_YUV422
#define CAM_POLICY	DC1394_CAPTURE_POLICY_POLL

static dc1394_t * handle = NULL;
struct camdata {
	dc1394camera_t * cam;
	dc1394video_frame_t * frame;
};

MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("Module for reading Videre Digital Cameras and outputing a [EDIT ME] stream");
MOD_PREACT(module_preactivate);

DEF_BLOCK(camera, camera_new, "i");
BLK_ONUPDATE(camera, camera_update);


#define VIDERE_LOCAL_PARAM_BASE               0xFF000UL
#define VIDERE_CAM_FW_LEVEL_OFFSET            (1*4)

static void camera_free(dc1394camera_t * cam)
{
	dc1394_video_set_transmission(cam, DC1394_OFF);
	dc1394_capture_stop(cam);
	dc1394_camera_free(cam);
}

static uint64_t camera_getguid(int num)
{
	dc1394camera_list_t * list = NULL;
	dc1394error_t err = dc1394_camera_enumerate(handle, &list);

	if (err != DC1394_SUCCESS)
		return 0;

	uint64_t guid = 0;
	if (num < list->num)
	{
		guid = list->ids[num].guid;
	}

	dc1394_camera_free_list(list);
	return guid;
}

static int camera_frame(dc1394camera_t * cam, dc1394video_frame_t ** frame)
{
	if (*frame != NULL)
	{
		//release previous frame
		int enqueue_ok = dc1394_capture_enqueue(cam, *frame) == DC1394_SUCCESS;
		if (!enqueue_ok)
		{
			LOG(LOG_ERR, "Could not enqueue last video frame");
			return -1;
		}

		*frame = NULL;
	}

	dc1394_capture_dequeue(cam, CAM_POLICY, frame);

	return 0;
}

void camera_update(struct camdata * data)
{
	if (data == NULL)
	{
		return;
	}

	int frameok = camera_frame(data->cam, &data->frame);

	if (frameok != 0)
	{
		LOG(LOG_WARN, "Could get capture camera frame");
	}
	else
	{
		LOG(LOG_INFO, "FRAME GRABBED");
	}
}

void * camera_new(int camnum)
{
	uint64_t guid = camera_getguid(camnum);
	if (guid == 0)
	{
		LOG(LOG_ERR, "dc1394 camera %d doesn't exist", camnum);
		return NULL;
	}

	dc1394camera_t * cam = dc1394_camera_new(handle, guid);
	if (cam == NULL)
	{
		LOG(LOG_ERR, "Could not open dc1394 camera %d", camnum);
		return NULL;
	}

	dc1394video_modes_t modes;
	int modes_ok = dc1394_video_get_supported_modes(cam, &modes) == DC1394_SUCCESS;
	if (!modes_ok)
	{
		LOG(LOG_ERR, "Could not get video modes from camera");
		camera_free(cam);
		return NULL;
	}

	{
		uint32_t i;
		for (i=0; i<modes.num; i++)
		{
			if (modes.modes[i] == CAM_MODE)
			{
				break;
			}
		}

		if (i == modes.num)
		{
			LOG(LOG_ERR, "Camera not compatible with camera mode %d", CAM_MODE);
			camera_free(cam);
			return NULL;
		}
	}


	//qval = getRegister(VIDERE_LOCAL_PARAM_BASE+VIDERE_CAM_FW_LEVEL_OFFSET);
	LOG(LOG_DEBUG, "Videre: getting local params for camera %d", camnum);

	uint32_t qval = 0;

	dc1394_get_control_register(cam, VIDERE_LOCAL_PARAM_BASE + VIDERE_CAM_FW_LEVEL_OFFSET, &qval); //getRegister(VIDERE_LOCAL_PARAM_BASE+VIDERE_CAM_FW_LEVEL_OFFSET);
    int major = (qval & 0x0000ff00)>>8;
    int minor = (qval & 0x000000ff);

    if ((qval >> 16) != 0 || major < 2 || minor > 10)
    {
    	LOG(LOG_WARN, "Videre: No local parameters");
    }
	else
	{
		//uint32_t firmware = qval & 0xffff;

		LOG(LOG_DEBUG, "Videre firmware: %02d.%02d", major, minor);
	}


	int mode_ok = dc1394_video_set_mode(cam, DC1394_VIDEO_MODE_640x480_YUV422) == DC1394_SUCCESS;
	if (!mode_ok)
	{
		LOG(LOG_ERR, "Could not set camera mode to %d", CAM_MODE);
		camera_free(cam);
		return NULL;
	}

	//int capture_ok = dc1394_capture_setup(cam, bufferSize, DC1394_CAPTURE_FLAGS_DEFAULT) == DC1394_SUCCESS;


	int tx_ok = dc1394_video_set_transmission(cam, DC1394_ON) == DC1394_SUCCESS;
	if (!tx_ok)
	{
		LOG(LOG_ERR, "Could not start camera transmission");
		camera_free(cam);
		return NULL;
	}

	//sleep for a bit and check pwr
	usleep(10000);
	dc1394switch_t pwr;
	dc1394_video_get_transmission(cam, &pwr);
	if (pwr != DC1394_ON)
	{
		LOG(LOG_WARN, "Could not get confirmation of video transmission");
	}

	struct camdata * data = g_malloc0(sizeof(struct camdata));
	data->cam = cam;

	return data;
}

void module_preactivate() {
	handle = dc1394_new();

	dc1394camera_t * cam0 = dc1394_camera_new(handle, camera_getguid(0));
	if (cam0 == NULL)
	{
		LOG(LOG_WARN, "Could not acquire camera bus reset");
		return;
	}

	dc1394_reset_bus(cam0);
	dc1394_camera_free(cam0);

	dc1394_free(handle);
	handle = dc1394_new();
}
