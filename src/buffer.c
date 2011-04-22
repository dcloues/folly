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
	strncpy(str, buffer->data, buffer->len + 1);
	return str;
}

char *buffer_substring(buffer *buf, int offset, int len)
{
	if (offset + len > buf->capacity) {
		perror("Asked for substring outside of buffer bounds");
		exit(1);
	}
	
	char *str = malloc(len + 1);
	str[len-1] = '\0';
	return memcpy(str, buf->data + offset, len);
	
}
