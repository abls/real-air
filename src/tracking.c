#include <stdio.h>
#include <stdint.h>
#include <hidapi/hidapi.h>
#include <pthread.h>
#include "cglm/cglm.h"
#include "Fusion/Fusion.h"

#define AIR_VID 0x3318
#define AIR_PID 0x0424

// this is a guess
// ~3906000 tps, packets come every ~3906 ticks, 1000 Hz packets
#define TICK_LEN (1.0f / 3906000.0f)

// based on 24bit signed int w/ FSR = +/-2000 dps, datasheet option
#define GYRO_SCALAR (1.0f / 8388608.0f * 2000.0f)

// based on 24bit signed int w/ FSR = +/-16 g, datasheet option
#define ACCEL_SCALAR (1.0f / 8388608.0f * 16.0f)

static hid_device* device = 0;
static pthread_t thread = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static versor rotation = GLM_QUAT_IDENTITY_INIT;
static bool running = false;
typedef struct {
	uint64_t uptime;
	int32_t gyro[3];
	int32_t accel[3];
	int16_t mag[3];
} air_sample;

static int
parse_report(const unsigned char* buffer, int size, air_sample* out_sample)
{
	if (size != 64) {
		printf("Invalid packet size");
		return -1;
	}

	out_sample->uptime = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24) |
											 (buffer[8] << 32) | (buffer[9] << 40) | (buffer[10] << 48) | (buffer[11] << 56);

	out_sample->gyro[0] = buffer[18] | (buffer[19] << 8) | (buffer[20] << 16) | ((buffer[20] & 0x80) ? (0xff << 24) : 0);
	out_sample->gyro[1] = buffer[21] | (buffer[22] << 8) | (buffer[23] << 16) | ((buffer[23] & 0x80) ? (0xff << 24) : 0);
	out_sample->gyro[2] = buffer[24] | (buffer[25] << 8) | (buffer[26] << 16) | ((buffer[26] & 0x80) ? (0xff << 24) : 0);

	out_sample->accel[0] = buffer[33] | (buffer[34] << 8) | (buffer[35] << 16) | ((buffer[35] & 0x80) ? (0xff << 24) : 0);
	out_sample->accel[1] = buffer[36] | (buffer[37] << 8) | (buffer[38] << 16) | ((buffer[38] & 0x80) ? (0xff << 24) : 0);
	out_sample->accel[2] = buffer[39] | (buffer[40] << 8) | (buffer[41] << 16) | ((buffer[41] & 0x80) ? (0xff << 24) : 0);

	out_sample->mag[0] = buffer[48] | (buffer[49] << 8);
	out_sample->mag[1] = buffer[50] | (buffer[51] << 8);
	out_sample->mag[2] = buffer[52] | (buffer[53] << 8);

	return 0;
}

static void
process_gyro(const int32_t gyro[3], float out_vec[])
{
	out_vec[0] = (float)(gyro[0]) * GYRO_SCALAR;
	out_vec[1] = (float)(gyro[1]) * GYRO_SCALAR;
	out_vec[2] = (float)(gyro[2]) * GYRO_SCALAR;
}

static void
process_accel(const int32_t accel[3], float out_vec[])
{
	out_vec[0] = (float)(accel[0]) * ACCEL_SCALAR;
	out_vec[1] = (float)(accel[1]) * ACCEL_SCALAR;
	out_vec[2] = (float)(accel[2]) * ACCEL_SCALAR;
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
	uint64_t last_sample_time = 0;
	air_sample sample = {};
	vec3 ang_vel = {};

	// Define calibration (replace with actual calibration data if available)
	const FusionMatrix gyroscopeMisalignment = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector gyroscopeSensitivity = { 1.0f, 1.0f, 1.0f };
	const FusionVector gyroscopeOffset = { 0.0f, 0.0f, 0.0f };
	const FusionMatrix accelerometerMisalignment = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const FusionVector accelerometerSensitivity = { 1.0f, 1.0f, 1.0f };
	const FusionVector accelerometerOffset = { 0.0f, 0.0f, 0.0f };

	FusionOffset offset;
	FusionOffsetInitialise(&offset, 1000);

	FusionAhrs fusionAhrs;
  FusionAhrsInitialise(&fusionAhrs);
	
	const FusionAhrsSettings settings = {
			.convention = FusionConventionEnu,
			.gain = 0.5f,
			.accelerationRejection = 10.0f,
			.magneticRejection = 20.0f,
			.rejectionTimeout = 5 * 1000,
	};
	FusionAhrsSetSettings(&fusionAhrs, &settings);

	while (running) {
		int res = hid_read(device, (void*)&buffer, sizeof(buffer));
		if (res < 0) {
			printf("Unable to get feature report\n");
			break;
		}

		if (buffer[0] != 0x01 || buffer[1] != 0x02)
			continue;

		parse_report(buffer, sizeof(buffer), &sample);

		uint64_t time_delta = 3906;
		if (last_sample_time > 0)
			time_delta = sample.uptime - last_sample_time;
		last_sample_time = sample.uptime;

		float gyro[3] = {};
		float accel[3] = {};

		process_gyro(sample.gyro, gyro);
		process_accel(sample.accel, accel);

		FusionVector gyroscope = (FusionVector){gyro[0], gyro[1], gyro[2]};
		FusionVector accelerometer = (FusionVector){accel[0], accel[1], accel[2]};

		gyroscope = FusionCalibrationInertial(gyroscope, gyroscopeMisalignment, gyroscopeSensitivity, gyroscopeOffset);
		accelerometer = FusionCalibrationInertial(accelerometer, accelerometerMisalignment, accelerometerSensitivity, accelerometerOffset);

		gyroscope = FusionOffsetUpdate(&offset, gyroscope);

		FusionAhrsUpdateNoMagnetometer(
			&fusionAhrs,
			gyroscope,
			accelerometer,
			//time_delta * TICK_LEN
			1.0f / 1000.0f
		);

		FusionQuaternion quaternion = FusionAhrsGetQuaternion(&fusionAhrs);
		pthread_mutex_lock(&mutex);
		glm_quat_init(rotation, -quaternion.array[1], quaternion.array[3], quaternion.array[2], quaternion.array[0]);
		pthread_mutex_unlock(&mutex);
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
