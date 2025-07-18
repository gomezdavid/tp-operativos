#ifndef LIBERACION_H
#define LIBERACION_H

#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <utils/estructuras.h>
#include "common.h"

// Libera una lista de instrucciones y todas sus estructuras internas
void liberar_lista_instrucciones(t_list* lista);

// Libera el diccionario de procesos, sus listas y todas las estructuras internas
void liberar_diccionario(t_dictionary* diccionario);

#endif 