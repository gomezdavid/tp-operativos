#include "buffer.h"

t_buffer *buffer_create(int32_t size) {
    t_buffer* buffer = malloc(sizeof(t_buffer));
    buffer->size = size;
    buffer->offset = 0;
    buffer->stream = malloc(buffer->size);
    return buffer;
}

void buffer_destroy(t_buffer *buffer) {
    free(buffer->stream);
    free(buffer);
}

void buffer_add(t_buffer *buffer, void *data, int32_t size) {
    memcpy(buffer->stream + buffer->offset, data, size);
    buffer->offset += size;
}

void buffer_read(t_buffer *buffer, void *data, int32_t size) {
    memcpy(data, buffer->stream + buffer->offset, size);
    buffer->offset += size;
}

void buffer_add_int32(t_buffer *buffer, int32_t data) {
    buffer_add(buffer, &data, sizeof(int32_t));
}

int32_t buffer_read_int32(t_buffer *buffer) {
    int32_t res = 0;
    buffer_read(buffer, &res, sizeof(int32_t));
    return res;
}

void buffer_add_uint8(t_buffer *buffer, uint8_t data) {
    buffer_add(buffer, &data, sizeof(uint8_t));
}

uint8_t buffer_read_uint8(t_buffer *buffer) {
    uint8_t res = 0;
    buffer_read(buffer, &res, sizeof(uint8_t));
    return res;
}

void buffer_add_string(t_buffer *buffer, int32_t length, char *string) {
    buffer_add(buffer, string, length);
}

char *buffer_read_string(t_buffer *buffer, int32_t *length) {
    char *res = malloc(*length);
    buffer_read(buffer, res, *length);
    return res;
}

void buffer_add_bool(t_buffer *buffer, bool data) {
    buffer_add(buffer, &data, sizeof(bool));
}

bool buffer_read_bool(t_buffer *buffer) {
    bool ret = false;
    buffer_read(buffer, &ret, sizeof(bool));
    return ret;
}