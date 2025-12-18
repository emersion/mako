#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#elif defined(HAVE_LIBELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_BASU)
#include <basu/sd-bus.h>
#endif

static void log_neg_errno(int ret, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, ": %s\n", strerror(-ret));
}

static int parse_uint32(uint32_t *out, const char *str) {
	char *end;
	errno = 0;
	uintmax_t u = strtoumax(str, &end, 10);
	if (errno != 0) {
		return -errno;
	} else if (end[0] != '\0') {
		return -EINVAL;
	} else if (u > UINT32_MAX) {
		return -ERANGE;
	}
	*out = (uint32_t)u;
	return 0;
}

static int new_method_call(sd_bus *bus, sd_bus_message **m, const char *member) {
	int ret = sd_bus_message_new_method_call(bus, m,
		"org.freedesktop.Notifications", "/fr/emersion/Mako",
		"fr.emersion.Mako", member);
	if (ret < 0) {
		log_neg_errno(ret, "sd_bus_message_new_method_call() failed for %s", member);
	}
	return ret;
}

static int call(sd_bus *bus, sd_bus_message *m, sd_bus_message **reply) {
	sd_bus_error error = {0};
	int ret = sd_bus_call(bus, m, 0, &error, reply);
	if (ret < 0) {
		fprintf(stderr, "%s (%s)\n", error.message, error.name);
		sd_bus_error_free(&error);
	}
	return ret;
}

static int call_method(sd_bus *bus, const char *member, sd_bus_message **reply,
		const char *types, ...) {
	sd_bus_message *m = NULL;
	int ret = new_method_call(bus, &m, member);
	if (ret < 0) {
		return ret;
	}

	va_list args;
	va_start(args, types);
	ret = sd_bus_message_appendv(m, types, args);
	va_end(args);
	if (ret < 0) {
		log_neg_errno(ret, "sd_bus_message_appendv() failed for %s", member);
		return ret;
	}

	ret = call(bus, m, reply);
	sd_bus_message_unref(m);
	return ret;
}

static int run_dismiss(sd_bus *bus, int argc, char *argv[]) {
	uint32_t id = 0;
	bool group = false;
	bool all = false;
	bool no_history = false;
	while (true) {
		const struct option options[] = {
			{ "all", no_argument, 0, 'a' },
			{ "group", no_argument, 0, 'g' },
			{ "no-history", no_argument, 0, 'h' },
			{0},
		};
		int opt = getopt_long(argc, argv, "aghn:", options, NULL);
		if (opt == -1) {
			break;
		}

		switch (opt) {
		case 'a':
			all = true;
			break;
		case 'g':
			group = true;
			break;
		case 'n':;
			int ret = parse_uint32(&id, optarg);
			if (ret < 0) {
				log_neg_errno(ret, "invalid notification ID");
				return 1;
			}
			break;
		case 'h':;
			no_history = true;
			break;
		default:
			return -EINVAL;
		}
	}

	if (all && group) {
		fprintf(stderr, "-a and -g cannot be used together\n");
		return -EINVAL;
	} else if ((all || group) && id != 0) {
		fprintf(stderr, "-n cannot be used with -a or -g\n");
		return -EINVAL;
	}

	char types[6] = "a{sv}";

	sd_bus_message *msg = NULL;
	int ret = new_method_call(bus, &msg, "DismissNotifications");
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_open_container(msg, 'a', "{sv}");
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_append(msg, "{sv}", "id", "u", id);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_append(msg, "{sv}", "group", "b", (int)group);
	if (ret < 0) {
		return ret;
	}

	int history = !no_history;
	ret = sd_bus_message_append(msg, "{sv}", "history", "b", (int)history);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_append(msg, "{sv}", "all", "b", (int)all);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_close_container(msg);
	if (ret < 0) {
		return ret;
	}

	ret = call(bus, msg, NULL);
	sd_bus_message_unref(msg);
	return ret;

	return call_method(bus, "DismissNotifications", NULL, types, &msg);
}

static int run_invoke(sd_bus *bus, int argc, char *argv[]) {
	uint32_t id = 0;
	while (true) {
		int opt = getopt(argc, argv, "n:");
		if (opt == -1) {
			break;
		}

		switch (opt) {
		case 'n':;
			int ret = parse_uint32(&id, optarg);
			if (ret < 0) {
				log_neg_errno(ret, "invalid notification ID");
				return 1;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	const char *action = "default";
	if (optind < argc) {
		action = argv[optind];
	}

	return call_method(bus, "InvokeAction", NULL, "us", id, action);
}

static int read_actions(sd_bus_message *msg, char ***out) {
	int ret = sd_bus_message_enter_container(msg, 'v', "a{ss}");
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_enter_container(msg, 'a', "{ss}");
	if (ret < 0) {
		return ret;
	}

	size_t actions_len = 0, actions_cap = 0;
	char **actions = NULL;
	while (true) {
		ret = sd_bus_message_enter_container(msg, 'e', "ss");
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		const char *key = NULL, *title = NULL;
		ret = sd_bus_message_read(msg, "ss", &key, &title);
		if (ret < 0) {
			return ret;
		}

		// Need space for key, title and NULL terminator
		if (actions_len + 3 > actions_cap) {
			actions_cap *= 2;
			if (actions_cap == 0) {
				actions_cap = 32;
			}
			actions = realloc(actions, actions_cap * sizeof(char *));
		}

		actions[actions_len] = strdup(key);
		actions[actions_len + 1] = strdup(title);
		actions_len += 2;

		ret = sd_bus_message_exit_container(msg);
		if (ret < 0) {
			return ret;
		}
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		return ret;
	}

	if (actions != NULL) {
		actions[actions_len] = NULL;
	}

	*out = actions;
	return 0;
}

static void free_strv(char **strv) {
	if (strv == NULL) {
		return;
	}
	for (size_t i = 0; strv[i] != NULL; i++) {
		free(strv[i]);
	}
	free(strv);
}

static bool is_empty_str(const char *str) {
	return str == NULL || str[0] == '\0';
}

static void escape_and_print_json_string(const char *s) {
	putchar('"');

	for (; *s; ++s) {
		switch (*s) {
			case '\"': printf("\\\""); break;
			case '\\': printf("\\\\"); break;
			case '\b': printf("\\b");  break;
			case '\f': printf("\\f");  break;
			case '\n': printf("\\n");  break;
			case '\r': printf("\\r");  break;
			case '\t': printf("\\t");  break;
			default:
				// control characters
				if ((unsigned char) *s < 0x20) {
					printf("\\u%04x", (unsigned char)*s);
				} else {
					putchar(*s);
				}
		}
	}

	putchar('"');
}

static int print_json_object(sd_bus_message *reply);

static int print_json_value(sd_bus_message *message) {
	int ret;
	char type;
	const char *signature;

	ret = sd_bus_message_peek_type(message, &type, &signature);
	if (ret < 0) {
		return ret;
	}

	switch ((char) type) {
		case SD_BUS_TYPE_STRING: {
			const char *value;
			ret = sd_bus_message_read_basic(message, 's', &value);
			if (ret < 0) {
				return ret;
			}
			escape_and_print_json_string(value);
			return ret;
		}
		case SD_BUS_TYPE_BOOLEAN: {
			bool value;
			ret = sd_bus_message_read_basic(message, 'b', &value);
			if (ret < 0) {
				return ret;
			}
			printf(value ? "true" : "false");
			return ret;
		}
		case SD_BUS_TYPE_BYTE: {
			uint8_t value;
			ret = sd_bus_message_read_basic(message, 'y', &value);
			if (ret < 0) {
				return ret;
			}
			printf("%u", value);
			return ret;
		}
		case SD_BUS_TYPE_UINT32: {
			uint32_t value;
			ret = sd_bus_message_read_basic(message, 'u', &value);
			if (ret < 0) {
				return ret;
			}
			printf("%u", value);
			return ret;
		}
		case SD_BUS_TYPE_INT32: {
			int32_t value;
			ret = sd_bus_message_read_basic(message, 'i', &value);
			if (ret < 0) {
				return ret;
			}
			printf("%d", value);
			return ret;
		}
		case SD_BUS_TYPE_VARIANT: {
			ret = sd_bus_message_enter_container(message, 'v', NULL);
			if (ret < 0) {
				return ret;
			}
			ret = print_json_value(message);
			if (ret < 0) {
				return ret;
			}
			return sd_bus_message_exit_container(message);  // 'v'
		}
		case SD_BUS_TYPE_ARRAY: {
			bool outer_first = true;

			printf("[");

			if (strcmp(signature, "{sv}") == 0) {
				while ((ret = sd_bus_message_enter_container(message, 'a', "{sv}")) > 0) {
					if (!outer_first) {
						printf(",");
					}
					outer_first = false;

					print_json_object(message);  // {sv}
					sd_bus_message_exit_container(message);
				}
			} else if (strcmp(signature, "{ss}") == 0) {
				while ((ret = sd_bus_message_enter_container(message, 'a', "{ss}")) > 0) {
					bool inner_first = true;

					if (!outer_first) {
						printf(",");
					}
					outer_first = false;

					printf("{");

					while ((ret = sd_bus_message_enter_container(message, 'e', NULL)) > 0) {
						const char *key, *value;

						ret = sd_bus_message_read(message, "ss", &key, &value);
						if (ret < 0) {
							return ret;
						}

						if (!inner_first) {
							printf(",");
						}
						inner_first = false;

						escape_and_print_json_string(key);
						printf(":");
						escape_and_print_json_string(value);

						sd_bus_message_exit_container(message);  // e
					}
					printf("}");

					sd_bus_message_exit_container(message);  // a{ss}
				}
			} else {
				while ((ret = print_json_value(message)) > 0) {
					if (!outer_first) {
						printf(",");
					}
					outer_first = false;
				}
			}

			printf("]");

			break;
		}
		default: {
			// skip unknown
			sd_bus_message_skip(message, NULL);
			printf("null");
		}
	}

	return 1;
}

static int print_json_object(sd_bus_message *reply) {
	int ret;
	bool is_first = true;

	printf("{");

	while ((ret = sd_bus_message_enter_container(reply, 'e', "sv")) > 0) {
		const char *key;

		ret = sd_bus_message_read_basic(reply, 's', &key);
		if (ret < 0) {
			return ret;
		}

		if (!is_first) {
			printf(",");
		}
		is_first = false;

		escape_and_print_json_string(key);
		printf(":");

		ret = print_json_value(reply);
		if (ret < 0) {
			return ret;
		}

		sd_bus_message_exit_container(reply);  // e{sv}
	}

	printf("}");

	return 1;
}

static int print_notification(sd_bus_message *reply) {
	uint32_t id = 0;
	const char *summary = NULL, *app_name = NULL, *category = NULL,
		*desktop_entry = NULL;
	uint8_t urgency = -1;
	char **actions = NULL;
	while (true) {
		int ret = sd_bus_message_enter_container(reply, 'e', "sv");
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		const char *key = NULL;
		ret = sd_bus_message_read(reply, "s", &key);
		if (ret < 0) {
			return ret;
		}

		if (strcmp(key, "id") == 0) {
			ret = sd_bus_message_read(reply, "v", "u", &id);
		} else if (strcmp(key, "actions") == 0) {
			ret = read_actions(reply, &actions);
		} else if (strcmp(key, "summary") == 0) {
			ret = sd_bus_message_read(reply, "v", "s", &summary);
		} else if (strcmp(key, "app-name") == 0) {
			ret = sd_bus_message_read(reply, "v", "s", &app_name);
		} else if (strcmp(key, "category") == 0) {
			ret = sd_bus_message_read(reply, "v", "s", &category);
		} else if (strcmp(key, "desktop-entry") == 0) {
			ret = sd_bus_message_read(reply, "v", "s", &desktop_entry);
		} else if (strcmp(key, "urgency") == 0) {
			ret = sd_bus_message_read(reply, "v", "y", &urgency);
		} else {
			ret = sd_bus_message_skip(reply, "v");
		}
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_exit_container(reply);
		if (ret < 0) {
			return ret;
		}
	}

	printf("Notification %" PRIu32 ":", id);
	if (!is_empty_str(summary)) {
		printf(" %s", summary);
	}
	printf("\n");

	if (!is_empty_str(app_name)) {
		printf("  App name: %s\n", app_name);
	}
	if (!is_empty_str(category)) {
		printf("  Category: %s\n", category);
	}
	if (!is_empty_str(desktop_entry)) {
		printf("  Desktop entry: %s\n", desktop_entry);
	}

	const char *urgency_desc = NULL;
	switch (urgency) {
	case 0:
		urgency_desc = "low";
		break;
	case 1:
		urgency_desc = "normal";
		break;
	case 2:
		urgency_desc = "critical";
		break;
	}
	if (urgency_desc != NULL) {
		printf("  Urgency: %s\n", urgency_desc);
	}

	if (actions != NULL) {
		printf("  Actions:\n");
		for (size_t i = 0; actions[i] != NULL; i += 2) {
			const char *key = actions[i], *title = actions[i + 1];
			printf("    %s: %s\n", key, title);
		}
	}

	free_strv(actions);
	return 0;
}

static int print_notification_list(sd_bus_message *reply, bool json_output) {
	int ret = sd_bus_message_enter_container(reply, 'a', "a{sv}");
	if (ret < 0) {
		return ret;
	}

	bool is_first = true;
	if (json_output) {
		printf("[");
	}

	while ((ret = sd_bus_message_enter_container(reply, 'a', "{sv}")) > 0) {
		if (json_output) {
			if (!is_first)
				printf(",");
			is_first = false;
			ret = print_json_object(reply);
		} else {
			ret = print_notification(reply);
		}

		sd_bus_message_exit_container(reply);  // a{sv}
	}

	if (json_output) {
		printf("]");
	}

	return sd_bus_message_exit_container(reply);
}

static int run_history(sd_bus *bus, int argc, char *argv[]) {
	sd_bus_message *reply = NULL;
	bool json_output = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-j") == 0) {
			json_output = true;
			break;
		}
	}

	int ret = call_method(bus, "ListHistory", &reply, "");
	if (ret < 0) {
		return ret;
	}

	ret = print_notification_list(reply, json_output);
	sd_bus_message_unref(reply);
	return ret;
}


static int run_list(sd_bus *bus, int argc, char *argv[]) {
	sd_bus_message *reply = NULL;	bool json_output = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-j") == 0) {
			json_output = true;
			break;
		}
	}

	int ret = call_method(bus, "ListNotifications", &reply, "");
	if (ret < 0) {
		return ret;
	}

	ret = print_notification_list(reply, json_output);
	sd_bus_message_unref(reply);
	return ret;
}

static int exec_menu(char *argv[], FILE **in, FILE **out, pid_t *pid_ptr) {
	int in_pipe[2], out_pipe[2];
	if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
		perror("pipe() failed");
		return -errno;
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork() failed");
		return -errno;
	} else if (pid == 0) {
		if (dup2(in_pipe[0], STDIN_FILENO) < 0 ||
				dup2(out_pipe[1], STDOUT_FILENO) < 0) {
			perror("dup2() failed");
			_exit(1);
		}

		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);

		execvp(argv[0], argv);
		perror("execvp() failed");
		_exit(1);
	}

	close(in_pipe[0]);
	close(out_pipe[1]);

	*in = fdopen(in_pipe[1], "w");
	*out = fdopen(out_pipe[0], "r");
	*pid_ptr = pid;
	return 0;
}

static int find_actions(sd_bus_message *reply, uint32_t select_id, uint32_t *id_out, char ***actions_out) {
	int ret = sd_bus_message_enter_container(reply, 'a', "a{sv}");
	if (ret < 0) {
		return ret;
	}

	bool found = false;
	uint32_t id = 0;
	char **actions = NULL;
	while (true) {
		ret = sd_bus_message_enter_container(reply, 'a', "{sv}");
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		while (true) {
			ret = sd_bus_message_enter_container(reply, 'e', "sv");
			if (ret < 0) {
				return ret;
			} else if (ret == 0) {
				break;
			}

			const char *key = NULL;
			ret = sd_bus_message_read(reply, "s", &key);
			if (ret < 0) {
				return ret;
			}

			if (strcmp(key, "id") == 0) {
				ret = sd_bus_message_read(reply, "v", "u", &id);
				if (ret < 0) {
					return ret;
				}
			} else if (strcmp(key, "actions") == 0) {
				ret = read_actions(reply, &actions);
				if (ret < 0) {
					return ret;
				}
			} else {
				ret = sd_bus_message_skip(reply, "v");
				if (ret < 0) {
					return ret;
				}
			}

			ret = sd_bus_message_exit_container(reply);
			if (ret < 0) {
				return ret;
			}
		}

		if (select_id == 0 || id == select_id) {
			found = true;
			break;
		}

		free_strv(actions);
		actions = NULL;
		id = 0;

		ret = sd_bus_message_exit_container(reply);
		if (ret < 0) {
			return ret;
		}
	}

	ret = sd_bus_message_exit_container(reply);
	if (ret < 0) {
		return ret;
	}

	if (!found) {
		fprintf(stderr, "Notification not found\n");
		return -ENOENT;
	}

	*id_out = id;
	*actions_out = actions;
	return 0;
}

static int run_menu(sd_bus *bus, int argc, char *argv[]) {
	uint32_t select_id = 0;
	while (true) {
		int opt = getopt(argc, argv, "n:");
		if (opt == -1) {
			break;
		}

		switch (opt) {
		case 'n':;
			int ret = parse_uint32(&select_id, optarg);
			if (ret < 0) {
				log_neg_errno(ret, "invalid notification ID");
				return 1;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Missing menu command\n");
		return -EINVAL;
	}
	char **menu_argv = &argv[optind];

	sd_bus_message *reply = NULL;
	int ret = call_method(bus, "ListNotifications", &reply, "");
	if (ret < 0) {
		return ret;
	}

	uint32_t id = 0;
	char **actions = NULL;
	ret = find_actions(reply, select_id, &id, &actions);
	sd_bus_message_unref(reply);
	if (ret < 0) {
		return ret;
	} else if (actions == NULL) {
		fprintf(stderr, "Notification has no actions\n");
		return -ENOENT;
	}

	pid_t menu_pid = 0;
	FILE *in = NULL, *out = NULL;
	ret = exec_menu(menu_argv, &in, &out, &menu_pid);
	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; actions[i] != NULL; i += 2) {
		const char *title = actions[i + 1];
		fprintf(in, "%s\n", title);
	}
	fclose(in);

	char *selected_title = NULL;
	size_t size = 0;
	errno = 0;
	ssize_t n_read = getline(&selected_title, &size, out);
	if (n_read < 0) {
		if (feof(out)) {
			fprintf(stderr, "No action selected\n");
			return -ECANCELED;
		} else {
			perror("getline() failed");
			return -errno;
		}
	}
	fclose(out);

	if (n_read > 0 && selected_title[n_read - 1] == '\n') {
		selected_title[n_read - 1] = '\0';
	}

	int stat = 0;
	if (waitpid(menu_pid, &stat, 0) < 0) {
		perror("waitpid() failed");
		return -errno;
	} else if (stat != 0) {
		if (WIFEXITED(stat)) {
			fprintf(stderr, "Menu failed with exit code %d\n", WEXITSTATUS(stat));
		} else if (WIFSIGNALED(stat)) {
			fprintf(stderr, "Menu failed with signal %d\n", WTERMSIG(stat));
		} else {
			abort(); // unreachable
		}
		return -ECANCELED;
	}

	char *selected_key = NULL;
	for (size_t i = 0; actions[i] != NULL; i += 2) {
		const char *key = actions[i], *title = actions[i + 1];
		if (strcmp(title, selected_title) == 0) {
			selected_key = strdup(key);
			break;
		}
	}
	if (selected_title != NULL && selected_key == NULL) {
		fprintf(stderr, "Action not found\n");
		return -ENOENT;
	}

	free(selected_title);
	free_strv(actions);

	ret = call_method(bus, "InvokeAction", NULL, "us", id, selected_key);
	free(selected_key);
	return ret;
}

static int find_mode(char **modes, int modes_len, const char *mode) {
	for (int i = 0; i < modes_len; i++) {
		if (strcmp(modes[i], mode) == 0) {
			return i;
		}
	}
	return -1;
}

static char **add_mode(char **modes, int *modes_len, const char *mode) {
	modes = realloc(modes, (*modes_len + 2) * sizeof(modes[0]));
	modes[*modes_len] = strdup(mode);
	modes[*modes_len + 1] = NULL;
	(*modes_len)++;
	return modes;
}

static void remove_mode(char **modes, int *modes_len, int i) {
	free(modes[i]);
	modes[i] = modes[*modes_len - 1];
	modes[*modes_len - 1] = NULL;
	(*modes_len)--;
}

static int run_mode(sd_bus *bus, int argc, char *argv[]) {
	sd_bus_message *reply = NULL;
	int ret = call_method(bus, "ListModes", &reply, "");
	if (ret < 0) {
		return ret;
	}

	char **modes = NULL;
	ret = sd_bus_message_read_strv(reply, &modes);
	if (ret < 0) {
		log_neg_errno(ret, "sd_bus_message_read_strv() failed");
		return ret;
	}

	int modes_len = 0;
	while (modes != NULL && modes[modes_len] != NULL) {
		modes_len++;
	}

	bool add_remove_toggle_flag = false, set_flag = false;
	while (true) {
		int opt = getopt(argc, argv, "a:r:t:s");
		if (opt == -1) {
			break;
		}

		int i;
		switch (opt) {
		case 'a':
			add_remove_toggle_flag = true;
			modes = add_mode(modes, &modes_len, optarg);
			break;
		case 'r':
			add_remove_toggle_flag = true;
			i = find_mode(modes, modes_len, optarg);
			if (i >= 0) {
				remove_mode(modes, &modes_len, i);
			}
			break;
		case 't':
			add_remove_toggle_flag = true;
			i = find_mode(modes, modes_len, optarg);
			if (i >= 0) {
				remove_mode(modes, &modes_len, i);
			} else {
				modes = add_mode(modes, &modes_len, optarg);
			}
			break;
		case 's':
			set_flag = true;
			break;
		default:
			return -EINVAL;
		}
	}
	if (add_remove_toggle_flag && set_flag) {
		fprintf(stderr, "-a/-r/-t and -s cannot be used together\n");
		return -EINVAL;
	}
	if (set_flag) {
		for (int i = 0; i < modes_len; i++) {
			free(modes[i]);
		}
		modes_len = argc - optind;
		modes = realloc(modes, (modes_len + 1) * sizeof(modes[0]));
		for (int i = 0; i < modes_len; i++) {
			modes[i] = strdup(argv[optind + i]);
		}
		modes[modes_len] = NULL;
	} else if (optind < argc) {
		fprintf(stderr, "positional arguments can only be used with -s\n");
		return -EINVAL;
	}

	if (add_remove_toggle_flag || set_flag) {
		sd_bus_message *m = NULL;
		ret = new_method_call(bus, &m, "SetModes");
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append_strv(m, modes);
		if (ret < 0) {
			log_neg_errno(ret, "sd_bus_message_append_strv() failed");
			return ret;
		}

		ret = call(bus, m, NULL);
		sd_bus_message_unref(m);
		if (ret < 0) {
			return ret;
		}
	}

	for (int i = 0; i < modes_len; i++) {
		printf("%s\n", modes[i]);
		free(modes[i]);
	}
	free(modes);
	sd_bus_message_unref(reply);
	return 0;
}

static const char usage[] =
	"Usage: makoctl <command> [options...]\n"
	"\n"
	"Commands:\n"
	"  dismiss [-n id]                Dismiss the notification with the\n"
	"                                 given id, or the last notification\n"
	"                                 if none is given\n"
	"          [-a|--all]             Dismiss all notifications\n"
	"          [-g|--group]           Dismiss all the notifications\n"
	"                                 in the last notification's group\n"
	"          [-h|--no-history]      Dismiss w/o adding to history\n"
	"  restore                        Restore the most recently expired\n"
	"                                 notification from the history buffer\n"
	"  invoke [-n id] [action]        Invoke an action on the notification\n"
	"                                 with the given id, or the last\n"
	"                                 notification if none is given\n"
	"  menu [-n id] <prog> [arg ...]  Use <prog> [args ...] to select one\n"
	"                                 action to be invoked on the notification\n"
	"                                 with the given id, or the last\n"
	"                                 notification if none is given\n"
	"  list                           List notifications\n"
	"  history                        List history\n"
	"  reload                         Reload the configuration file\n"
	"  mode                           List modes\n"
	"  mode [-a mode]... [-r mode]... Add/remove modes\n"
	"  mode [-t mode]...              Toggle modes (add if not present, remove if present)\n"
	"  mode -s mode...                Set modes\n"
	"  help                           Show this help\n";

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		fprintf(stderr, "%s", usage);
		return 1;
	}
	const char *cmd = argv[1];
	int cmd_argc = argc - 1;
	char **cmd_argv = &argv[1];

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
			strcmp(cmd, "-h") == 0) {
		fprintf(stderr, "%s", usage);
		return 0;
	}

	sd_bus *bus = NULL;
	int ret = sd_bus_open_user(&bus);
	if (ret < 0) {
		log_neg_errno(ret, "sd_bus_open_user() failed");
		return 1;
	}

	if (strcmp(cmd, "dismiss") == 0) {
		ret = run_dismiss(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "invoke") == 0) {
		ret = run_invoke(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "history") == 0) {
		ret = run_history(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "list") == 0) {
		ret = run_list(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "menu") == 0) {
		ret = run_menu(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "mode") == 0) {
		ret = run_mode(bus, cmd_argc, cmd_argv);
	} else if (strcmp(cmd, "reload") == 0) {
		ret = call_method(bus, "Reload", NULL, "");
	} else if (strcmp(cmd, "restore") == 0) {
		ret = call_method(bus, "RestoreNotification", NULL, "");
	} else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		return 1;
	}

	sd_bus_unref(bus);
	return ret >= 0 ? 0 : 1;
}
