#include "../include/liberacion.h"

void liberar_lista_instrucciones(t_list* lista) {
    if(lista == NULL) return;

    for(int i = 0; i < list_size(lista); i++) {
        struct_memoria_to_cpu* instruccion = list_get(lista, i);
        if(instruccion != NULL) {
            if(instruccion->parametros != NULL) {
                free(instruccion->parametros);
            }
            free(instruccion);
        }
    }
    list_destroy(lista);
}

void liberar_diccionario(t_dictionary* diccionario) {
    if(diccionario == NULL) return;

    t_list* keys = dictionary_keys(diccionario);
    
    for(int i = 0; i < list_size(keys); i++) {
        char* key = list_get(keys, i);
        if(key != NULL) {
            t_list* lista = dictionary_remove(diccionario, key);
            if(lista != NULL) {
                liberar_lista_instrucciones(lista);
            }
        }
    }
    
    list_destroy_and_destroy_elements(keys, free);
    dictionary_destroy(diccionario);
} 