#include <../include/tablas_paginas.h>
#include<commons/collections/list.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

t_config_memoria *cfg_memoria;
t_memoria* memoria_principal;
sem_t mutex_memoria;

tabla_paginas* crear_tabla(int32_t nivel){
    tabla_paginas* tabla = malloc(sizeof(tabla_paginas));
    tabla->entradas = malloc(cfg_memoria->ENTRADAS_POR_TABLA * sizeof(entrada_tabla));
    tabla->nivel = nivel;

    for (int i = 0; i < cfg_memoria->ENTRADAS_POR_TABLA; i++) {
        tabla->entradas[i].marco = -1;
        tabla->entradas[i].tabla_siguiente = NULL;
    }

    return tabla;
}

tablas_por_pid* crear_tabla_raiz(int32_t pid, int32_t tamanio_proceso) {
    tablas_por_pid* tabla_raiz_pid = malloc(sizeof(tablas_por_pid));
    tabla_raiz_pid->pid = pid;
    tabla_raiz_pid->tabla_raiz = crear_tabla(1);
    tabla_raiz_pid->cant_marcos = ceil((double)tamanio_proceso / (double)cfg_memoria->TAM_PAGINA);
    tabla_raiz_pid->marcos = malloc(tabla_raiz_pid->cant_marcos * sizeof(int));
    return tabla_raiz_pid;
}

void asignar_marcos(tabla_paginas* tabla_actual, int32_t* tamanio_proceso, int32_t nivel, int32_t* marcos, int32_t* indice_marcos, t_metricas *metricas_proceso) {
    for (int entrada = 0; entrada < cfg_memoria->ENTRADAS_POR_TABLA; entrada++) {
        if (*tamanio_proceso > 0) {
            if (nivel < cfg_memoria->CANTIDAD_NIVELES) {
                if (tabla_actual->entradas[entrada].tabla_siguiente == NULL) {
                    tabla_actual->entradas[entrada].tabla_siguiente = crear_tabla(nivel + 1);
                    tabla_actual->entradas[entrada].marco = -1;
                }

                asignar_marcos(tabla_actual->entradas[entrada].tabla_siguiente, tamanio_proceso, nivel + 1, marcos, indice_marcos, metricas_proceso);
            }
            else {
                *tamanio_proceso = *tamanio_proceso - cfg_memoria->TAM_PAGINA;
                tabla_actual->entradas[entrada].tabla_siguiente = NULL;
                tabla_actual->entradas[entrada].marco = obtener_marco_libre();
                marcos[*indice_marcos] = tabla_actual->entradas[entrada].marco;
                (*indice_marcos)++;
            }
        }
        else {
            break;
        }
    }
    metricas_proceso->subidas_a_mp++;
}

int32_t devolver_marco(tabla_paginas* tabla_actual, int32_t* indices, int32_t nivel, t_metricas *metricas_proceso) {
    int32_t marco = -1;
    usleep(cfg_memoria->RETARDO_MEMORIA * 1000);
    int indice_actual = indices[nivel-1];
    if (nivel < cfg_memoria->CANTIDAD_NIVELES) {
        marco = devolver_marco(tabla_actual->entradas[indice_actual].tabla_siguiente, indices, nivel + 1, metricas_proceso);
    }
    else {
        marco =  tabla_actual->entradas[indice_actual].marco;
    }
    metricas_proceso->accesos_tablas_paginas++;
    return marco;
}

void* leer_pagina_completa(int32_t direccion_fisica, int32_t pid, t_metricas *metricas_proceso) {
    log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, cfg_memoria->TAM_PAGINA);
    void* retorno = malloc(cfg_memoria->TAM_PAGINA);
    sem_wait(&mutex_memoria);
    memcpy(retorno, memoria_principal->datos + direccion_fisica, cfg_memoria->TAM_PAGINA);
    sem_post(&mutex_memoria);
    metricas_proceso->cantidad_lecturas++;
    return retorno;
}

bool actualizar_pagina_completa(int32_t direccion_fisica, void* pagina, int32_t pid, t_metricas *metricas_proceso) {
    log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, cfg_memoria->TAM_PAGINA);
    sem_wait(&mutex_memoria);
    memcpy(memoria_principal->datos + direccion_fisica, pagina, cfg_memoria->TAM_PAGINA);
    sem_post(&mutex_memoria);
    metricas_proceso->cantidad_escrituras++;
    return true;
}

void liberar_espacio_memoria(tablas_por_pid* proceso, int32_t tamanio_proceso, t_metricas* metricas_proceso) {
    if (tiene_entradas_swap(proceso)) {
        liberar_proceso_swap(proceso, tamanio_proceso, metricas_proceso);
    }
    else {
        for(int i = 0; i < proceso->cant_marcos; i++) {
            void* pagina_vacia = calloc(cfg_memoria->TAM_PAGINA, sizeof(void*));
            actualizar_pagina_completa(proceso->marcos[i] * cfg_memoria->TAM_PAGINA, pagina_vacia, proceso->pid, metricas_proceso);
            liberar_marco(proceso->marcos[i]);
            free(pagina_vacia);
            pagina_vacia = NULL;
        }
    }
    free(proceso->marcos);
    int32_t aux = tamanio_proceso;
    liberar_tablas_paginas(proceso->tabla_raiz, 1, &aux);
    free(proceso);
}

void liberar_tablas_paginas(tabla_paginas* tabla_actual, int32_t nivel, int32_t* tamanio_proceso) {
    for (int entrada = 0; entrada < cfg_memoria->ENTRADAS_POR_TABLA; entrada++) {
        if (tamanio_proceso < 0) {
            if (nivel < cfg_memoria->CANTIDAD_NIVELES) {
                liberar_tablas_paginas(tabla_actual->entradas[entrada].tabla_siguiente, nivel + 1, tamanio_proceso);
                free(&(tabla_actual->entradas[entrada]));
            }
            else {
                free(&(tabla_actual->entradas[entrada]));
                *tamanio_proceso = *tamanio_proceso - cfg_memoria->TAM_PAGINA;
            }
        }
        else {
            break;
        }
    }
}
