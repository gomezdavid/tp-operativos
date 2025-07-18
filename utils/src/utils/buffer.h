#ifndef BUFFER_H_
#define BUFFER_H_
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<commons/log.h>
#include<commons/string.h>

typedef struct {
    int32_t size; // Tamaño del payload
    int32_t offset; // Desplazamiento dentro del payload
    void* stream; // Payload
} t_buffer;

// Crea un buffer vacío de tamaño size y offset 0
t_buffer *buffer_create(int32_t size);

// Libera la memoria asociada al buffer
void buffer_destroy(t_buffer *buffer);

// Agrega un stream al buffer en la posición actual y avanza el offset
void buffer_add(t_buffer *buffer, void *data, int32_t size);

// Guarda size bytes del principio del buffer en la dirección data y avanza el offset
void buffer_read(t_buffer *buffer, void *data, int32_t size);

// Agrega un int32_t al buffer
void buffer_add_int32(t_buffer *buffer, int32_t data);

// Lee un int32_t del buffer y avanza el offset
int32_t buffer_read_int32(t_buffer *buffer);

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_buffer *buffer, uint8_t data);

// Lee un uint8_t del buffer y avanza el offset
uint8_t buffer_read_uint8(t_buffer *buffer);

// Agrega string al buffer con un int32_t adelante indicando su longitud
void buffer_add_string(t_buffer *buffer, int32_t length, char *string);

// Lee un string y su longitud del buffer y avanza el offset
char *buffer_read_string(t_buffer *buffer, int32_t *length);

// Agrega un bool al buffer
void buffer_add_bool (t_buffer *buffer, bool data);

// Lee un bool del buffer y avanza el offset
bool buffer_read_bool (t_buffer *buffer);
#endif