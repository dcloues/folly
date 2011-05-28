#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
	char *data;
	int len;
	int capacity;
} buffer;

#define buffer_capacity(buffer) (buffer->capacity - buffer->len)

buffer *buffer_create(const int initial_capacity);
void buffer_destroy(buffer *buffer);
void buffer_ensure_capacity(buffer *buffer, int new_capacity);
void buffer_ensure_additional_capacity(buffer *buffer, int additional_capacity);
void buffer_append_string(buffer *buffer, char *str);
void buffer_append_char(buffer *buffer, char ch);
char buffer_peek(buffer *buffer);
void buffer_shrink(buffer *b, int n);

void buffer_printf(buffer *buffer, char *fmt, ...);
char *buffer_to_string(buffer *buffer);
char *buffer_substring(buffer *buf, int offset, int len);

#endif

