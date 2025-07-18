#ifndef MANEJO_MEMORIA_H
#define MANEJO_MEMORIA_H

#include <utils/config.h>
#include <stdint.h>
#include "common.h"

void* crear_espacio_de_memoria();
bool escribir_en_memoria(int32_t direccion_fisica, int32_t size, void* contenido, int32_t pid);
void* leer_de_memoria(int32_t direccion_fisica, int32_t size, int32_t pid);

#endif