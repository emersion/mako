#define _POSIX_C_SOURCE 200809L
#include <cairo/cairo.h>
#include <fcntl.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "pool-buffer.h"

static bool set_cloexec(int fd) {
	long flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		return false;
	}

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		return false;
	}

	return true;
}

static int create_pool_file(size_t size, char **name) {
	static const char template[] = "mako-XXXXXX";
	const char *path = getenv("XDG_RUNTIME_DIR");
	if (path == NULL) {
		fprintf(stderr, "XDG_RUNTIME_DIR is not set\n");
		return -1;
	}

	size_t name_size = strlen(template) + 1 + strlen(path) + 1;
	*name = malloc(name_size);
	if (*name == NULL) {
		fprintf(stderr, "allocation failed\n");
		return -1;
	}
	snprintf(*name, name_size, "%s/%s", path, template);

	int fd = mkstemp(*name);
	if (fd < 0) {
		return -1;
	}

	if (!set_cloexec(fd)) {
		close(fd);
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	struct pool_buffer *buffer = data;
	buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_handle_release,
};

static struct pool_buffer *create_buffer(struct wl_shm *shm,
		struct pool_buffer *buf, int32_t width, int32_t height) {
	const enum wl_shm_format wl_fmt = WL_SHM_FORMAT_ARGB8888;
	const cairo_format_t cairo_fmt = CAIRO_FORMAT_ARGB32;

	uint32_t stride = cairo_format_stride_for_width(cairo_fmt, width);
	size_t size = stride * height;

	void *data = NULL;
	if (size > 0) {
		char *name;
		int fd = create_pool_file(size, &name);
		if (fd == -1) {
			return NULL;
		}

		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (data == MAP_FAILED) {
			free(name);
			return NULL;
		}

		struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
		buf->buffer =
			wl_shm_pool_create_buffer(pool, 0, width, height, stride, wl_fmt);
		wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
		wl_shm_pool_destroy(pool);

		close(fd);
		fd = -1;
		unlink(name);
		free(name);
	}

	buf->data = data;
	buf->size = size;
	buf->width = width;
	buf->height = height;
	buf->surface = cairo_image_surface_create_for_data(data, cairo_fmt, width,
		height, stride);
	buf->cairo = cairo_create(buf->surface);
	buf->pango = pango_cairo_create_context(buf->cairo);
	return buf;
}

void finish_buffer(struct pool_buffer *buffer) {
	if (buffer->buffer) {
		wl_buffer_destroy(buffer->buffer);
	}
	if (buffer->cairo) {
		cairo_destroy(buffer->cairo);
	}
	if (buffer->surface) {
		cairo_surface_destroy(buffer->surface);
	}
	if (buffer->pango) {
		g_object_unref(buffer->pango);
	}
	if (buffer->data) {
		munmap(buffer->data, buffer->size);
	}
	memset(buffer, 0, sizeof(struct pool_buffer));
}

struct pool_buffer *get_next_buffer(struct wl_shm *shm,
		struct pool_buffer pool[static 2], uint32_t width, uint32_t height) {
	struct pool_buffer *buffer = NULL;
	for (size_t i = 0; i < 2; ++i) {
		if (pool[i].busy) {
			continue;
		}
		buffer = &pool[i];
	}
	if (!buffer) {
		return NULL;
	}

	if (buffer->width != width || buffer->height != height) {
		finish_buffer(buffer);
	}

	if (!buffer->buffer) {
		if (!create_buffer(shm, buffer, width, height)) {
			return NULL;
		}
	}

	return buffer;
}
