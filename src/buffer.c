#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"

buffer *buffer_create(const int initial_capacity)
{
	buffer *buf = calloc(1, sizeof(buffer));
	if (buf) {
		buf->data = calloc(initial_capacity, sizeof(char));
		if (!buf->data) {
			perror("unable to allocate memory for buffer");
			exit(1);
		}

		buf->capacity = initial_capacity;
		buf->len = 0;
	}

	return buf;
}

void buffer_destroy(buffer *buffer) {
	free(buffer->data);
	free(buffer);
}

void buffer_append_string(buffer *buf, char *str, int n)
{
	int len = strlen(str);
	buffer_ensure_additional_capacity(buf, len);
	strncpy(buf->data + buf->len, str, len);
	buf->len += len;
	buffer_append_char(buf, '\0');
}

void buffer_append_char(buffer *buf, char ch)
{
	buffer_ensure_additional_capacity(buf, 1);
	buf->data[buf->len] = ch;
	buf->len++;
	buf->data[buf->len] = '\0';
}

void buffer_ensure_capacity(buffer *buffer, int new_capacity)
{
	if (new_capacity + 1 > buffer->capacity) {
		char *resized = realloc(buffer->data, buffer->capacity * 2);
		if (resized)
		{
			buffer->data = resized;
			buffer->capacity *= 2;
		}
		else
		{
			perror("Unable to resize buffer");
			exit(1);
		}
	}
}

void buffer_ensure_additional_capacity(buffer *buffer, int additional_capacity)
{
	buffer_ensure_capacity(buffer, buffer->len + additional_capacity);
}

char buffer_peek(buffer *buffer)
{
	char val = buffer->len ? buffer->data[buffer->len-1] : '\0';
	/*printf("buffer_peek: %d chars, last is %c\n", buffer->len, val);*/
	return val;
}

char *buffer_to_string(buffer *buffer)
{
	char *str = malloc(buffer->len + 1);
	str[buffer->len] = 0;
	strncpy(str, buffer->data, buffer->len);
	return str;
}

char *buffer_substring(buffer *buf, int offset, int len)
{
	if (offset + len > buf->capacity) {
		perror("Asked for substring outside of buffer bounds");
		exit(1);
	}
	
	char *str = calloc(len + 1, sizeof(char));
	return memcpy(str, buf->data + offset, len);
}

void buffer_printf(buffer *b, char *fmt, ...)
{
	// pulling these numbers out of thin air
	if (buffer_capacity(b) < 20)
	{
		buffer_ensure_additional_capacity(b, 20);
	}

	int size = 0;
	char *str = b->data + b->len;
	int n = 0;
	va_list args;

	while (true)
	{
		size = buffer_capacity(b);
		va_start(args, fmt);
		n = vsnprintf(str, size, fmt, args);
		/*printf("\trequested %d chars (%d actual)\n", n, size);*/
		va_end(args);

		if (n > -1 && n < size)
		{
			b->len += n;
			return;
		}
		else if (n > -1)
		{
			buffer_ensure_capacity(b, b->len + n);
		}
	}

}

buffer_shrink(buffer *b, int n)
{
	if (b->len >= n)
	{
		b->len = b->len - n;
	}
	else
	{
		b->len = 0;
	}
}

