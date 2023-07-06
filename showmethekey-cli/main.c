#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>

#include <libudev.h>
#include <libinput.h>
#include <libevdev/libevdev.h>

#include "config.h"

#define MAX_BUFFER_LENGTH 512

enum error_code {
	NO_ERROR,
	UDEV_FAILED,
	LIBINPUT_FAILED,
	SEAT_FAILED,
	PERMISSION_FAILED
};

struct input_handler_data {
	struct udev *udev;
	struct libinput *libinput;
};

static void *handle_input(void *user_data)
{
	struct input_handler_data *input_handler_data = user_data;

	char line[MAX_BUFFER_LENGTH];
	while (fgets(line, MAX_BUFFER_LENGTH, stdin) != NULL) {
		if (strcmp(line, "stop\n") == 0) {
			libinput_unref(input_handler_data->libinput);
			udev_unref(input_handler_data->udev);
			exit(EXIT_SUCCESS);
		}
	}

	return NULL;
}

static int open_restricted(const char *path, int flags, void *user_data)
{
	int fd = open(path, flags);
	if (fd < 0)
		fprintf(stderr, "Failed to open %s because of %s.\n", path,
			strerror(errno));
	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
	close(fd);
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

static int print_key_event(struct libinput_event *event)
{
	struct libinput_event_keyboard *keyboard =
		libinput_event_get_keyboard_event(event);

	enum libinput_event_type event_type = libinput_event_get_type(event);
	uint32_t time_stamp = libinput_event_keyboard_get_time(keyboard);
	uint32_t key_code = libinput_event_keyboard_get_key(keyboard);
	const char *key_name = libevdev_event_code_get_name(EV_KEY, key_code);
	key_name = key_name ? key_name : "null";
	enum libinput_key_state state_code =
		libinput_event_keyboard_get_key_state(keyboard);
	const char *state_name = state_code == LIBINPUT_KEY_STATE_PRESSED ?
					 "PRESSED" :
					       "RELEASED";

	return printf("{"
		      "\"event_name\": \"KEYBOARD_KEY\", "
		      "\"event_type\": %d, "
		      "\"time_stamp\": %d, "
		      "\"key_name\": \"%s\", "
		      "\"key_code\": %d, "
		      "\"state_name\": \"%s\", "
		      "\"state_code\": %d"
		      "}\n",
		      event_type, time_stamp, key_name, key_code, state_name,
		      state_code);
}

static int print_button_event(struct libinput_event *event)
{
	struct libinput_event_pointer *pointer =
		libinput_event_get_pointer_event(event);

	enum libinput_event_type event_type = libinput_event_get_type(event);
	uint32_t time_stamp = libinput_event_pointer_get_time(pointer);
	uint32_t button_code = libinput_event_pointer_get_button(pointer);
	const char *button_name =
		libevdev_event_code_get_name(EV_KEY, button_code);
	enum libinput_button_state state_code =
		libinput_event_pointer_get_button_state(pointer);
	const char *state_name = state_code == LIBINPUT_BUTTON_STATE_PRESSED ?
					 "PRESSED" :
					       "RELEASED";
	return printf("{"
		      "\"event_name\": \"POINTER_BUTTON\", "
		      "\"event_type\": %d, "
		      "\"time_stamp\": %d, "
		      "\"key_name\": \"%s\", "
		      "\"key_code\": %d, "
		      "\"state_name\": \"%s\", "
		      "\"state_code\": %d"
		      "}\n",
		      event_type, time_stamp, button_name, button_code,
		      state_name, state_code);
}

static int handle_events(struct libinput *libinput)
{
	int result = -1;
	struct libinput_event *event;

	if (libinput_dispatch(libinput) < 0)
		return result;

	// Please keep printing a line per json.
	while ((event = libinput_get_event(libinput)) != NULL) {
		switch (libinput_event_get_type(event)) {
		// This program only handle key event.
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			print_key_event(event);
			break;
		// Sorry, mouse button is also a key.
		case LIBINPUT_EVENT_POINTER_BUTTON:
			print_button_event(event);
			break;
		default:
			break;
		}
		// Do a `fflush(stdout)` here, so when we write to pipes,
		// the other one can always get a latest result.
		// If we don't have `fflush(stdout)` here, pipe will save
		// some lines in buffer and pass them together.
		fflush(stdout);
		libinput_event_destroy(event);
		result = 0;
	}

	return result;
}

static int run_mainloop(struct libinput *libinput)
{
	struct pollfd fd;
	fd.fd = libinput_get_fd(libinput);
	fd.events = POLLIN;
	fd.revents = 0;

	if (handle_events(libinput) != 0) {
		fprintf(stderr,
			"Expected device added events on startup but "
			"got none. Maybe you don't have the right permissions?"
			"\n");
		return -1;
	}
	while (poll(&fd, 1, -1) > -1)
		handle_events(libinput);
	return 0;
}

void print_help(char *program_name)
{
	printf("The backend of Show Me The Key.\n");
	printf("Version " PROJECT_VERSION ".\n");
	printf("Usage: %s [OPTION…]\n", program_name);
	printf("Options:\n");
	printf("\t-h, --help\tDisplay help then exit.\n");
	printf("\t-v, --version\tDisplay version then exit.\n");
	printf("Warning: This is the backend and is not designed to run "
	       "by users. You should run the frontend of Show Me The Key, "
	       "and the frontend will run this.\n");
}

int main(int argc, char *argv[])
{
	const struct option long_options[] = { { "version", no_argument, 0,
						 'v' },
					       { "help", no_argument, 0, 'h' },
					       { NULL, 0, NULL, 0 } };

	int option_index = 0;
	int opt = 0;
	while ((opt = getopt_long(argc, argv, "vh", long_options,
				  &option_index)) != -1) {
		switch (opt) {
		case 0:
			// We don't use this.
			break;
		case 'v':
			printf(PROJECT_VERSION "\n");
			return 0;
		case 'h':
			print_help(argv[0]);
			return 0;
		case '?':
			// getopt_long already printed an error message.
			break;
		default:
			fprintf(stderr, "%s: Invalid option `-%c`.\n", argv[0],
				opt);
			break;
		}
	}

	struct udev *udev = udev_new();
	if (udev == NULL) {
		fprintf(stderr, "Failed to initialize udev.\n");
		return UDEV_FAILED;
	}

	struct libinput *libinput =
		libinput_udev_create_context(&interface, NULL, udev);
	if (!libinput) {
		fprintf(stderr, "Failed to initialize libinput from udev.\n");
		return LIBINPUT_FAILED;
	}

	// TODO: Support custom seat.
	if (libinput_udev_assign_seat(libinput, "seat0") != 0) {
		fprintf(stderr, "Failed to set seat.\n");
		libinput_unref(libinput);
		udev_unref(udev);
		return SEAT_FAILED;
	}

	// Typically this will be run with pkexec as a subprocess,
	// and the parent cannot kill it because it is privileged,
	// so we use another thread to see if it gets "stop\n" from stdin,
	// it will exit by itself.
	pthread_t input_handler;
	struct input_handler_data input_handler_data = { udev, libinput };
	pthread_create(&input_handler, NULL, handle_input, &input_handler_data);

	if (run_mainloop(libinput) < 0)
		return PERMISSION_FAILED;

	libinput_unref(libinput);
	udev_unref(udev);

	return NO_ERROR;
}
