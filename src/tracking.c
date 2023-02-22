#include <stdio.h>
#include <stdint.h>
#include <hidapi/hidapi.h>
#include <pthread.h>
#include "cglm/cglm.h"

#define AIR_VID 0x3318
#define AIR_PID 0x0424

// this is a guess
// ~3906000 tps, packets come every ~3906 ticks, 1000 Hz packets
#define TICK_LEN (1.0f / 3906000.0f)

static hid_device* device = 0;
static pthread_t thread = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static versor rotation = GLM_QUAT_IDENTITY_INIT;
static bool running = false;

typedef struct {
	uint32_t tick;
	int16_t ang_vel[3];
} air_sample;

static int
parse_report(const unsigned char* buffer, int size, air_sample* out_sample)
{
	if (size != 64) {
		printf("Invalid packet size");
		return -1;
	}

	buffer += 5;
	out_sample->tick = *(buffer++) | (*(buffer++) << 8) | (*(buffer++) << 16) | (*(buffer++) << 24);
	buffer += 10;
	out_sample->ang_vel[0] = *(buffer++) | (*(buffer++) << 8);
	buffer++;
	out_sample->ang_vel[1] = *(buffer++) | (*(buffer++) << 8);
	buffer++;
	out_sample->ang_vel[2] = *(buffer++) | (*(buffer++) << 8);

	return 0;
}

static void
process_ang_vel(const int16_t ang_vel[3], vec3 out_vec)
{
	// these scale and bias corrections are all rough guesses
	out_vec[0] = (float)(ang_vel[0] + 15) * -0.001f;
	out_vec[1] = (float)(ang_vel[2] - 6) * 0.001f;
	out_vec[2] = (float)(ang_vel[1] + 15) * 0.001f;
}

static hid_device*
open_device()
{
	struct hid_device_info* devs = hid_enumerate(AIR_VID, AIR_PID);
	struct hid_device_info* cur_dev = devs;
	hid_device* dev = NULL;

	while (devs) {
		if (cur_dev->interface_number == 3) {
			dev = hid_open_path(cur_dev->path);
			break;
		}

		cur_dev = cur_dev->next;
	}

	hid_free_enumeration(devs);
	return dev;
}

static void
update_rotation(float dt, vec3 ang_vel)
{
	pthread_mutex_lock(&mutex);
	float ang_vel_length = glm_vec3_norm(ang_vel);

	if (ang_vel_length > 0.0001f) {
		vec3 rot_axis = { ang_vel[0] / ang_vel_length, ang_vel[1] / ang_vel_length, ang_vel[2] / ang_vel_length };
		float rot_angle = ang_vel_length * dt;

		versor delta_rotation;
		glm_quatv(delta_rotation, rot_angle, rot_axis);
		glm_quat_mul(rotation, delta_rotation, rotation);
	}

	glm_quat_normalize(rotation);
	pthread_mutex_unlock(&mutex);
}

static void*
track(void* arg)
{
	unsigned char buffer[64] = {};
	uint32_t last_sample_tick = 0;
	air_sample sample = {};
	vec3 ang_vel = {};

	while (running) {
		int res = hid_read(device, (void*)&buffer, sizeof(buffer));
		if (res < 0) {
			printf("Unable to get feature report\n");
			break;
		}

		if (buffer[0] != 0x01 || buffer[1] != 0x02)
			continue;

		parse_report(buffer, sizeof(buffer), &sample);
		process_ang_vel(sample.ang_vel, ang_vel);

		uint32_t tick_delta = 3906;
		if (last_sample_tick > 0)
			tick_delta = sample.tick - last_sample_tick;

		float dt = tick_delta * TICK_LEN;
		last_sample_tick = sample.tick;

		update_rotation(dt, ang_vel);
	}

	return 0;
}

int
tracking_start()
{
	device = open_device();
	if (!device) {
		printf("Unable to open device\n");
		return -1;
	}

	uint8_t magic_payload[] = { 0x00, 0xaa, 0xc5, 0xd1, 0x21, 0x42, 0x04, 0x00, 0x19, 0x01 };
	int res = hid_write(device, magic_payload, sizeof(magic_payload));
	if (res < 0) {
		printf("Unable to write to device\n");
		return -2;
	}

	glm_quat_copy(GLM_QUAT_IDENTITY, rotation);

	running = true;
	int err = pthread_create(&thread, NULL, track, NULL);
	if (err) {
		printf("Unable to start tracking\n");
		return -3;
	}

	return 0;
}

void
tracking_get(versor out)
{
	pthread_mutex_lock(&mutex);
	glm_quat_copy(rotation, out);
	pthread_mutex_unlock(&mutex);
}

void
tracking_set(versor ref)
{
	pthread_mutex_lock(&mutex);
	glm_quat_copy(ref, rotation);
	pthread_mutex_unlock(&mutex);
}

void
tracking_stop()
{
	running = false;
	pthread_join(thread, NULL);
}
