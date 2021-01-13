#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include "rk_aiq_user_api_sysctl.h"
#include "common/mediactl/mediactl.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define DBG(...) do { if(!silent) printf("DBG: " __VA_ARGS__);} while(0)
#define ERR(...) do { fprintf(stderr, "ERR: " __VA_ARGS__); } while (0)


/* Private v4l2 event */
#define CIFISP_V4L2_EVENT_STREAM_START  \
                    (V4L2_EVENT_PRIVATE_START + 1)
#define CIFISP_V4L2_EVENT_STREAM_STOP   \
                    (V4L2_EVENT_PRIVATE_START + 2)

#define RKAIQ_FILE_PATH_LEN                       64
#define RKAIQ_CAMS_NUM_MAX                        2
#define RKAIQ_FLASH_NUM_MAX                       2

rk_aiq_sys_ctx_t* aiq_ctx = NULL;
static int silent = 0;
static int width = 2688;
static int height = 1520;
static const char *mdev_path = NULL;

struct rkaiq_media_info {
    char sd_isp_path[RKAIQ_FILE_PATH_LEN];
    char vd_params_path[RKAIQ_FILE_PATH_LEN];
    char vd_stats_path[RKAIQ_FILE_PATH_LEN];

    struct {
            char sd_sensor_path[RKAIQ_FILE_PATH_LEN];
            char sd_lens_path[RKAIQ_FILE_PATH_LEN];
            char sd_flash_path[RKAIQ_FLASH_NUM_MAX][RKAIQ_FILE_PATH_LEN];
            bool link_enabled;
            char sensor_entity_name[32];
    } cams[RKAIQ_FLASH_NUM_MAX];
};

static struct rkaiq_media_info media_info;


static void errno_exit(const char *s)
{
    ERR("%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}


static int rkaiq_get_devname(struct media_device *device, const char *name, char *dev_name)
{
	const char *devname;
	struct media_entity *entity =  NULL;

	entity = media_get_entity_by_name(device, name, strlen(name));
	if (!entity)
		return -1;

	devname = media_entity_get_devname(entity);

	if (!devname) {
		fprintf(stderr, "can't find %s device path!", name);
		return -1;
	}

	strncpy(dev_name, devname, RKAIQ_FILE_PATH_LEN);

	DBG("get %s devname: %s\n", name, dev_name);

	return 0;
}
	
static int rkaiq_enumrate_modules(struct media_device *device, struct rkaiq_media_info *media_info)
{
	uint32_t nents, i;
	const char* dev_name = NULL;
	int active_sensor = -1;

	nents = media_get_entities_count (device);
	for (i = 0; i < nents; ++i) {
		int module_idx = -1;
		struct media_entity *e;
		const struct media_entity_desc *ef;
		const struct media_link *link;

		e = media_get_entity(device, i);
		ef = media_entity_get_info(e);

		if (ef->type != MEDIA_ENT_T_V4L2_SUBDEV_SENSOR &&
			ef->type != MEDIA_ENT_T_V4L2_SUBDEV_FLASH &&
			ef->type != MEDIA_ENT_T_V4L2_SUBDEV_LENS)
			continue;

		if (ef->name[0] != 'm' && ef->name[3] != '_') {
			fprintf(stderr, "sensor/lens/flash entity name format is incorrect,"
							"pls check driver version !\n");
			return -1;
		}

		/* Retrive the sensor index from sensor name,
		  * which is indicated by two characters after 'm',
		  *	 e.g.  m00_b_ov13850 1-0010
		  *			^^, 00 is the module index
		  */
		module_idx = atoi(ef->name + 1);
		if (module_idx >= RKAIQ_CAMS_NUM_MAX) {
			fprintf(stderr, "sensors more than two not supported, %s\n",
					ef->name);
			continue;
		}

		dev_name = media_entity_get_devname (e);

		switch (ef->type) {
		case MEDIA_ENT_T_V4L2_SUBDEV_SENSOR:
			strncpy(media_info->cams[module_idx].sd_sensor_path,
					dev_name, RKAIQ_FILE_PATH_LEN);

			link = media_entity_get_link(e, 0);
			if (link && (link->flags & MEDIA_LNK_FL_ENABLED)) {
				media_info->cams[module_idx].link_enabled = true;
				active_sensor = module_idx;
				strcpy(media_info->cams[module_idx].sensor_entity_name, ef->name);
			}
			break;
		case MEDIA_ENT_T_V4L2_SUBDEV_FLASH:
			// TODO, support multiple flashes attached to one module
			strncpy(media_info->cams[module_idx].sd_flash_path[0],
					dev_name, RKAIQ_FILE_PATH_LEN);
			break;
		case MEDIA_ENT_T_V4L2_SUBDEV_LENS:
			strncpy(media_info->cams[module_idx].sd_lens_path,
					dev_name, RKAIQ_FILE_PATH_LEN);
			break;
		default:
			break;
		}
	}

	if (active_sensor < 0) {
		fprintf(stderr, "Not sensor link is enabled, does sensor probe correctly?\n");
		return -1;
	}

	return 0;
}
	
int rkaiq_get_media_info(struct rkaiq_media_info *media_info, const char *mdev_path) 
{
	struct media_device *device = NULL;
	int ret;

	device = media_device_new (mdev_path);
	if (!device)
		return -ENOMEM;
	/* Enumerate entities, pads and links. */
	ret = media_device_enumerate (device);
	if (ret)
		return ret;
	if (!ret) {
		/* Try rkisp */
		ret = rkaiq_get_devname(device, "rkisp-isp-subdev",
								media_info->sd_isp_path);
		ret |= rkaiq_get_devname(device, "rkisp-input-params",
								media_info->vd_params_path);
		ret |= rkaiq_get_devname(device, "rkisp-statistics",
								media_info->vd_stats_path);
	}
	if (ret) {
		media_device_unref (device);
		return ret;
	}

	ret = rkaiq_enumrate_modules(device, media_info);
	media_device_unref (device);

	return ret;
}


static void init_engine(void)
{
    int index;

    for (index=0; index<RKAIQ_CAMS_NUM_MAX; index++)
        if(media_info.cams[index].link_enabled)
            break;

	aiq_ctx = rk_aiq_uapi_sysctl_init(media_info.cams[index].sensor_entity_name, NULL, NULL, NULL);

    if (rk_aiq_uapi_sysctl_prepare(aiq_ctx, width, height, RK_AIQ_WORKING_MODE_ISP_HDR2)) {
        ERR("rkaiq engine prepare failed !\n");
        exit(-1);
    }
}

static void start_engine(void)
{
    DBG("device manager start\n");
    rk_aiq_uapi_sysctl_start(aiq_ctx );
    if (aiq_ctx == NULL) {
        ERR("rkisp_init engine failed\n");
        exit(-1);
    } else {
        DBG("rkisp_init engine succeed\n");
    }
}

static void stop_engine(void)
{
    rk_aiq_uapi_sysctl_stop(aiq_ctx, false);
}

static void deinit_engine(void)
{
    rk_aiq_uapi_sysctl_deinit(aiq_ctx);
}

// blocked func
static int wait_stream_event(int fd, unsigned int event_type, int time_out_ms)
{
    int ret;
    struct v4l2_event event;

    CLEAR(event);

    do {
	/*
	 * xioctl instead of poll.
	 * Since poll() cannot wait for input before stream on,
	 * it will return an error directly. So, use ioctl to
	 * dequeue event and block until sucess.
	 */
	ret = xioctl(fd, VIDIOC_DQEVENT, &event);
	if (ret == 0 && event.type == event_type) {
		return 0;
	}
    } while (true);

    return -1;

}

static int subscrible_stream_event(int fd, bool subs)
{
    struct v4l2_event_subscription sub;
    int ret = 0;

    CLEAR(sub);
    sub.type = CIFISP_V4L2_EVENT_STREAM_START;
    ret = xioctl(fd,
                 subs ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT,
                 &sub);
    if (ret) {
        ERR("can't subscribe %s start event!\n", media_info.vd_params_path);
        exit(EXIT_FAILURE);
    }

    CLEAR(sub);
    sub.type = CIFISP_V4L2_EVENT_STREAM_STOP;
    ret = xioctl(fd,
                 subs ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT,
                 &sub);
    if (ret) {
        ERR("can't subscribe %s stop event!\n", media_info.vd_params_path);
    }

    DBG("subscribe events from %s success !\n", media_info.vd_params_path);

    return 0;
}

void parse_args(int argc, char **argv)
{
   int c;
   int digit_optind = 0;

   while (1) {
       int this_option_optind = optind ? optind : 1;
       int option_index = 0;
       static struct option long_options[] = {
           {"mmedia",    required_argument, 0, 'm' },
           {"silent",    no_argument,       0, 's' },
           {"help",      no_argument,       0, 'h' },
           {0,           0,                 0,  0  }
       };

       c = getopt_long(argc, argv, "m:sh", long_options, &option_index);
       if (c == -1)
           break;

       switch (c) {
       case 'm':
           mdev_path = optarg;
           break;
       case 'w':
           width = atoi(optarg);
           break;
       case 'h':
           height = atoi(optarg);
           break;
       case 's':
           silent = 1;
           break;
       case '?':
           ERR("Usage: %s to start 3A engine\n"
               "         --mmedia,  optional, mapped media device node\n"
               "         --silent,  optional, subpress debug log\n",
               argv[0]);
           exit(-1);

       default:
           ERR("?? getopt returned character code %c ??\n", c);
       }
   }

   if (!mdev_path) {
        ERR("argument --mmedia is required\n");
        exit(-1);
	}
}

int main(int argc, char **argv)
{
    int ret = 0;
    int isp_fd;
    unsigned int stream_event = -1;

    /* Line buffered so that printf can flash every line if redirected to
     * no-interactive device.
     */
    setlinebuf(stdout);

    parse_args(argc, argv);
    for (;;) {
        /* Refresh media info so that sensor link status updated */
        if (rkaiq_get_media_info(&media_info, mdev_path))
            errno_exit("Bad media topology\n");

        isp_fd = open(media_info.vd_params_path, O_RDWR);
        if (isp_fd < 0) {
            ERR("open %s failed %s\n", media_info.vd_params_path,
                strerror(errno));
            exit(-1);
        }
        subscrible_stream_event(isp_fd, true);
        init_engine();

        {
            DBG("wait stream start event...\n");
            wait_stream_event(isp_fd, CIFISP_V4L2_EVENT_STREAM_START, -1);
            DBG("wait stream start event success ...\n");

            start_engine();

            DBG("wait stream stop event...\n");
            wait_stream_event(isp_fd, CIFISP_V4L2_EVENT_STREAM_STOP, -1);
            DBG("wait stream stop event success ...\n");

            stop_engine();
        }

        deinit_engine();
        subscrible_stream_event(isp_fd, false);
        close(isp_fd);

        DBG("----------------------------------------------\n\n");
    }

    return 0;
}
