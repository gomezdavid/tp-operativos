#include "../include/mmu.h"


int32_t tamanio_pagina;
int32_t cant_entradas_tabla;
int32_t cant_niveles;
int32_t fd_memoria;
int32_t ptr_clock;

int cant_entradas_tlb;

t_config* config;
t_log* logger;

t_list* cache_de_paginas;
char* algoritmo_cache;
int32_t cant_entradas_cache;
int32_t retardo_cache;

void recibir_t_config_to_cpu() {
    t_paquete* paquete = recibir_paquete(fd_memoria);
    t_config_to_cpu *config_to_cpu = deserializar_t_config_to_cpu(paquete->buffer);
    tamanio_pagina = config_to_cpu->tam_paginas;
    cant_niveles = config_to_cpu->cantidad_niveles;
    cant_entradas_tabla = config_to_cpu->cant_entradas;
    free(config_to_cpu);
    destruir_paquete(paquete);
}

t_config_to_cpu *deserializar_t_config_to_cpu(t_buffer* buffer) {
    t_config_to_cpu *retorno = malloc(sizeof(t_config_to_cpu));
    retorno->cantidad_niveles = buffer_read_int32(buffer);
    retorno->tam_paginas = buffer_read_int32(buffer);
    retorno->cant_entradas = buffer_read_int32(buffer);
    return retorno;
}

int32_t numero_pagina(int32_t direccion_logica){
    return (int32_t)floor(direccion_logica / tamanio_pagina);
}

int32_t entrada_nivel_X(int32_t numero_pagina, int32_t nivel){
    int32_t potencia = pow(cant_entradas_tabla, cant_niveles-nivel);
    int32_t resultado = (int32_t)floor(numero_pagina / potencia) % cant_entradas_tabla;
    return resultado;
}

int32_t desplazamiento(int32_t direccion_logica){
    return direccion_logica % tamanio_pagina;
}

int32_t calcular_direccion_fisica(int32_t direccion_logica, int32_t pid) {
    int32_t nro_marco = consultar_marco(direccion_logica, pid);
    int32_t offset = desplazamiento(direccion_logica);
    return nro_marco * tamanio_pagina + offset;
}

int32_t consultar_marco(int32_t direccion_logica, int32_t pid) {
    int32_t nro_pagina = numero_pagina(direccion_logica); 
    int32_t nro_marco = -1;
    if (cant_entradas_tlb != 0) {
        nro_marco = buscar_en_tlb(nro_pagina);
        if(nro_marco != -1) {
            log_info(logger, "PID: %d - TLB HIT - Pagina: %d", pid, nro_pagina);
            return nro_marco;
        }
        log_info(logger, "PID: %d - TLB MISS - Pagina: %d", pid, nro_pagina);
    }
    int32_t *indices = calloc(cant_niveles, sizeof(int32_t));
    for (int i = 0; i < cant_niveles; i++) {
        indices[i] = entrada_nivel_X(nro_pagina, i + 1);
    }
    nro_marco = consultar_marco_memoria(indices, pid);
    entrada_tlb *entrada = malloc(sizeof(entrada_tlb));
    entrada->marco = nro_marco;
    entrada->pagina = nro_pagina;
    if(cant_entradas_tlb != 0) {
        agregar_a_tlb(entrada);
    }
    free(indices);
    log_info(logger, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid, nro_pagina, nro_marco);
    return nro_marco;
}

int32_t consultar_marco_memoria(int32_t* indices, int32_t pid) {
    t_buffer* buffer = serializar_indices_tablas(indices, pid);
    t_paquete* paquete = crear_paquete(CONSULTA_MARCO, buffer);
    enviar_paquete(paquete, fd_memoria);

    t_paquete* respuesta = recibir_paquete(fd_memoria);
    int32_t marco = buffer_read_int32(paquete->buffer);
    destruir_paquete(respuesta);
    return marco;
}

t_buffer* serializar_indices_tablas(int32_t* indices, int32_t pid) {
    t_buffer* buffer = buffer_create(sizeof(int32_t) * (cant_niveles + 1));
    buffer_add_int32(buffer, pid);
    for(int i = 0; i < cant_niveles; i++) {
        buffer_add_int32(buffer, indices[i]);
    }
    return buffer;
}

//CACHE DE PAGINAS

void inicializar_cache() {
    algoritmo_cache = config_get_string_value(config, "REEMPLAZO_CACHE");
    cant_entradas_tlb = config_get_int_value(config, "ENTRADAS_CACHE");
    retardo_cache = config_get_int_value(config, "RETARDO_CACHE");
    cache_de_paginas = list_create();
}

entrada_cache* buscar_en_cache(int32_t nro_pagina) {
    entrada_cache* entrada = entrada_cache_get_by_pagina(cache_de_paginas, nro_pagina);
    if (entrada) {
        entrada->uso = true;
    }
    return entrada;
}

void* leer_de_cache(int32_t direccion_logica, int32_t size, int32_t pid) {
    usleep(retardo_cache * 1000);
    void* retorno = malloc(size);
    int32_t nro_pagina = numero_pagina(direccion_logica);
    entrada_cache* entrada = buscar_en_cache(nro_pagina);
    int32_t offset = desplazamiento(direccion_logica);
    if(entrada) {
        log_info(logger, "PID: %d - Cache Hit - Pagina: %d", pid, nro_pagina);
        memcpy(retorno, entrada->contenido + offset, size);
    }
    else {
        log_info(logger, "PID: %d - Cache Miss - Pagina: %d", pid, nro_pagina);
        agregar_a_cache(direccion_logica, pid);
        log_info(logger, "PID: %d - Cache Add - Pagina: %d", pid, nro_pagina);
        entrada = buscar_en_cache(nro_pagina);
        memcpy(retorno, entrada->contenido + offset, size);
    }
    return retorno;
}

void escribir_en_cache(int32_t direccion_logica, int32_t size, int32_t pid, void* contenido) {
    usleep(retardo_cache * 1000);
    int32_t nro_pagina = numero_pagina(direccion_logica);
    entrada_cache* entrada = buscar_en_cache(nro_pagina);
    int32_t offset = desplazamiento(direccion_logica);
    if(entrada) {
        log_info(logger, "PID: %d - Cache Hit - Pagina: %d", pid, nro_pagina);
        memcpy(entrada->contenido + offset, contenido, size);
    }
    else {
        log_info(logger, "PID: %d - Cache Miss - Pagina: %d", pid, nro_pagina);
        agregar_a_cache(direccion_logica, pid);
        log_info(logger, "PID: %d - Cache Add - Pagina: %d", pid, nro_pagina);
        entrada = buscar_en_cache(nro_pagina);
        memcpy(entrada->contenido + offset, contenido, size);
    }
    entrada->modificado = true;
    entrada->uso = true;
}

void agregar_a_cache(int32_t direccion_logica, int32_t pid) {
    int32_t direccion_fisica = consultar_marco(direccion_logica, pid) * tamanio_pagina;

    t_buffer* buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, pid);
    buffer_add_int32(buffer, direccion_fisica);
    t_paquete* paquete = crear_paquete(LEER_PAGINA_COMPLETA, buffer);
    enviar_paquete(paquete, fd_memoria);

    t_paquete* respuesta = recibir_paquete(fd_memoria);
    void* contenido_agregar = malloc(tamanio_pagina);
    buffer_read(respuesta->buffer, contenido_agregar, tamanio_pagina);

    entrada_cache* entrada = malloc(sizeof(entrada_cache));
    entrada->uso = true;
    entrada->contenido = contenido_agregar;
    entrada->modificado = false;
    entrada->pagina = numero_pagina(direccion_logica);

    if(list_size(cache_de_paginas) == cant_entradas_cache) {
        int32_t indice_agregar = correr_algoritmo_cache(pid);
        list_add_in_index(cache_de_paginas, indice_agregar, entrada);
    }
    else {
        list_add(cache_de_paginas, entrada);
    }
    ptr_clock++;
    if(ptr_clock == cant_entradas_cache) {
        ptr_clock = 0;
    }
}

int32_t correr_algoritmo_cache(int32_t pid) {
    int32_t indice = -1;
    t_list* lista_despues_del_ptr = list_slice(cache_de_paginas, ptr_clock, cant_entradas_cache - ptr_clock);
    t_list* lista_antes_del_ptr = list_take(cache_de_paginas, ptr_clock);
    t_list_iterator* iterator_despues = list_iterator_create(lista_despues_del_ptr);
    t_list_iterator* iterator_antes = list_iterator_create(lista_antes_del_ptr);
    entrada_cache* entrada_a_reemplazar = NULL;
    bool encontrado = false;
    if (string_equals_ignore_case(algoritmo_cache, "CLOCK")) {
        for(int i = 0; i < 2; i++) {
            if(encontrado) {
                break;
            }
            while(list_iterator_has_next(iterator_despues)) {
                entrada_a_reemplazar = list_iterator_next(iterator_despues);
                if(!entrada_a_reemplazar->uso) {
                    encontrado = true;
                    indice = list_iterator_index(iterator_despues) + (cant_entradas_cache - ptr_clock);
                    break;
                }
                else {
                    entrada_a_reemplazar->uso = false;
                }
            }
            while(list_iterator_has_next(iterator_antes)) {
                entrada_a_reemplazar = list_iterator_next(iterator_antes);
                if(!entrada_a_reemplazar->uso) {
                    encontrado = true;
                    indice = list_iterator_index(iterator_antes);
                    break;
                }
                else {
                    entrada_a_reemplazar->uso = false;
                }
            }
        }
    }
    else {
        for(int i = 0; i < 4; i++) {
            if(encontrado) {
                break;
            }
            while(list_iterator_has_next(iterator_despues)) {
                entrada_a_reemplazar = list_iterator_next(iterator_despues);
                if(!entrada_a_reemplazar->uso && !entrada_a_reemplazar->modificado) {
                    encontrado = true;
                    indice = list_iterator_index(iterator_despues) + (cant_entradas_cache - ptr_clock);
                    break;
                }
                else if (i % 2 != 0){
                    if (!entrada_a_reemplazar->uso) {
                        encontrado = true;
                        indice = list_iterator_index(iterator_despues) + (cant_entradas_cache - ptr_clock);
                        break;
                    }
                    else {
                        entrada_a_reemplazar->uso = false;
                    }
                }
            }
            while(list_iterator_has_next(iterator_antes)) {
                entrada_a_reemplazar = list_iterator_next(iterator_antes);
                if(!entrada_a_reemplazar->uso && !entrada_a_reemplazar->modificado) {
                    encontrado = true;
                    indice = list_iterator_index(iterator_antes);
                    break;
                }
                else if (i % 2 == 0){
                    if (!entrada_a_reemplazar->uso) {
                        encontrado = true;
                        indice = list_iterator_index(iterator_antes);
                        break;
                    }
                    else {
                        entrada_a_reemplazar->uso = false;
                    }
                }
            }
        }
    }
    list_iterator_destroy(iterator_despues);
    list_iterator_destroy(iterator_antes);
    list_destroy(lista_despues_del_ptr);
    list_destroy(lista_antes_del_ptr);

    list_remove_element(cache_de_paginas, entrada_a_reemplazar);

    if(entrada_a_reemplazar->modificado) {
        escribir_pagina_cache_a_memoria(entrada_a_reemplazar, pid);
    }

    eliminar_entrada_cache((void*)entrada_a_reemplazar);
    return indice;
}

void eliminar_entrada_cache(void *ptr) {
    entrada_cache* entrada = (entrada_cache*)ptr;
    free(entrada->contenido);
    free(entrada);
}

void escribir_pagina_cache_a_memoria(entrada_cache* entrada, int32_t pid) {
    int32_t direccion_fisica = calcular_direccion_fisica(entrada->pagina * tamanio_pagina, pid);
    t_buffer* buffer = buffer_create(sizeof(tamanio_pagina));
    buffer_add_int32(buffer, pid);
    buffer_add_int32(buffer, direccion_fisica);
    buffer_add(buffer, entrada->contenido, tamanio_pagina);
    t_paquete* paquete = crear_paquete(ACTUALIZAR_PAGINA_COMPLETA, buffer);
    enviar_paquete(paquete, fd_memoria);
    log_info(logger, "PID: %d - Memory Update - Página: %d - Frame: %d", pid, entrada->pagina, direccion_fisica/tamanio_pagina);
}

entrada_cache* entrada_cache_get_by_pagina(t_list* entrada_cache_list, int32_t pagina) {
    bool pagina_equals(void *p_entrada_cache) {
        entrada_cache *entrada_cache_cast = (entrada_cache*)p_entrada_cache;
        return entrada_cache_cast->pagina == pagina;
    }
    return list_find(entrada_cache_list, pagina_equals);
}

void eliminar_entradas_cache(int32_t pid) {
    t_list_iterator* cache_de_paginas_iterator = list_iterator_create(cache_de_paginas);
    while(list_iterator_has_next(cache_de_paginas_iterator)) {
        entrada_cache* entrada = list_iterator_next(cache_de_paginas_iterator);
        list_iterator_remove(cache_de_paginas_iterator);
        if(entrada->modificado) {
            escribir_pagina_cache_a_memoria(entrada, pid);
        }
        eliminar_entrada_cache((void*)entrada);
    }
    list_iterator_destroy(cache_de_paginas_iterator);
}