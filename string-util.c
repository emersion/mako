#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

char *mako_asprintf(const char *fmt, ...) {
	char *text;
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (size < 0) {
		return NULL;
	}

	text = malloc(size + 1);
	if (text == NULL) {
		return NULL;
	}

	va_start(args, fmt);
	vsnprintf(text, size + 1, fmt, args);
	va_end(args);

	return text;
}
