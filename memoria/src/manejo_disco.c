#include<../include/manejo_disco.h>

void dump_memory(tablas_por_pid* contenido, t_metricas* metricas_proceso) {
    log_info(logger, "## PID: %d - Memory Dump solicitado", contenido->pid);
    char* contenido_dump = string_new();
    for(int i = 0; i < contenido->cant_marcos; i++) {
        void* contenido_marco = leer_pagina_completa(contenido->marcos[i] * cfg_memoria->TAM_PAGINA, contenido->pid, metricas_proceso);
        string_append(&contenido_dump, (char*)contenido_marco);
        free(contenido_marco);
        contenido_marco = NULL;
    }
    char * path_archivo = string_duplicate(cfg_memoria->DUMP_PATH);
    string_append_with_format(&path_archivo, "%d-%s.dmp", contenido->pid, temporal_get_string_time("%H:%M:%S:%MS"));
    FILE* archivo_dump = txt_open_for_append(path_archivo);
    txt_write_in_file(archivo_dump, contenido_dump);
    txt_close_file(archivo_dump);
    free(contenido_dump);
    free(path_archivo);
}

void suspender_proceso(tablas_por_pid* contenido, t_metricas *metricas_proceso) {
    char* contenido_a_swap = string_new();
    for(int i = 0; i < contenido->cant_marcos; i++) {
        void* contenido_marco = leer_pagina_completa(contenido->marcos[i] * cfg_memoria->TAM_PAGINA, contenido->pid, metricas_proceso);
        string_append(&contenido_a_swap, (char*)contenido_marco);
        void* pagina_vacia = calloc(cfg_memoria->TAM_PAGINA, sizeof(void*));
        actualizar_pagina_completa(contenido->marcos[i] * cfg_memoria->TAM_PAGINA, pagina_vacia, contenido->pid, metricas_proceso);
        liberar_marco(contenido->marcos[i]);
        free(pagina_vacia);
        pagina_vacia = NULL;
    }
    free(contenido->marcos);
    contenido->marcos = NULL;
    FILE* swapfile = txt_open_for_append(cfg_memoria->PATH_SWAPFILE);
    char* pid = string_itoa(contenido->pid);
    string_append(&contenido_a_swap, "\n");
    string_append(&pid, "\n");
    
    //Escribo en el swafile una linea con el pid, y otra con todo el contenido de las paginas junto.
    txt_write_in_file(swapfile, pid);
    txt_write_in_file(swapfile, contenido_a_swap);
    txt_close_file(swapfile);
    free(contenido_a_swap);
    free(pid);
    metricas_proceso->bajadas_a_swap++;
}

void dessuspender_procesos (tablas_por_pid* contenido, int32_t tamanio_proceso, t_metricas *metricas_proceso) {
    char* swapfile_tmp_path = cfg_memoria->PATH_SWAPFILE;
    string_append(&swapfile_tmp_path, ".tmp");
    FILE* swapfile_tmp = txt_open_for_append(swapfile_tmp_path);
    FILE* swapfile = fopen(cfg_memoria->PATH_SWAPFILE, "r");
    char* pid_s = string_itoa(contenido->pid);
    char* linea = malloc(cfg_memoria->TAM_MEMORIA);
    bool encontrado = false;
    int32_t aux = tamanio_proceso;
    int32_t indices_marcos = 0;
    asignar_marcos(contenido->tabla_raiz, &aux, 1, contenido->marcos, &indices_marcos, metricas_proceso);

    //Crea un nuevo archivo sin el pid buscado y sus paginas, para "borrarlo" del swapfile.
    while (fgets(linea, cfg_memoria->TAM_MEMORIA, swapfile)) {
        linea[string_length(linea) - 1] = '\0';

        if (string_equals_ignore_case(linea, pid_s)) {
            encontrado = true;
        }
        else if (encontrado) {
            for (int i = 0; i < contenido->cant_marcos; i++) {
                char* contenido_a_marco = malloc(cfg_memoria->TAM_PAGINA);
                contenido_a_marco = string_substring(linea, i * cfg_memoria->TAM_PAGINA, (i + 1) * cfg_memoria->TAM_PAGINA);
                actualizar_pagina_completa(contenido->marcos[i] * cfg_memoria->TAM_PAGINA, (void*)contenido_a_marco, contenido->pid, metricas_proceso);
                free(contenido_a_marco);
                contenido_a_marco = NULL;
            }
            encontrado = false;
        }
        else {
            linea[string_length(linea) - 1] = '\n';
            txt_write_in_file(swapfile_tmp, linea);
        }
    }

    txt_close_file(swapfile_tmp);
    fclose(swapfile);

    remove(cfg_memoria->PATH_SWAPFILE);
    rename(swapfile_tmp_path, cfg_memoria->PATH_SWAPFILE);

    free(linea);
    free(pid_s);
    free(swapfile_tmp_path);
    metricas_proceso->subidas_a_mp++;
}

bool tiene_entradas_swap(tablas_por_pid* proceso) {
    bool retorno = false;
    char* pid_s = string_itoa(proceso->pid);
    char* linea = malloc(cfg_memoria->TAM_MEMORIA);
    FILE* swapfile = fopen(cfg_memoria->PATH_SWAPFILE, "r");
    while (fgets(linea, cfg_memoria->TAM_MEMORIA, swapfile)) {
        linea[string_length(linea) - 1] = '\0';

        if (string_equals_ignore_case(linea, pid_s)) {
            retorno = true;
            break;
        }
    }
    return retorno;
}

void liberar_proceso_swap(tablas_por_pid* contenido, int32_t tamanio_proceso, t_metricas *metricas_proceso) {
    char* swapfile_tmp_path = string_duplicate(cfg_memoria->PATH_SWAPFILE);
    string_append(&swapfile_tmp_path, ".tmp");
    FILE* swapfile_tmp = txt_open_for_append(swapfile_tmp_path);
    FILE* swapfile = fopen(cfg_memoria->PATH_SWAPFILE, "r");
    char* pid_s = string_itoa(contenido->pid);
    char* linea = malloc(cfg_memoria->TAM_MEMORIA);
    bool encontrado = false;
    int32_t aux = tamanio_proceso;
    int32_t indices_marcos = 0;
    asignar_marcos(contenido->tabla_raiz, &aux, 1, contenido->marcos, &indices_marcos, metricas_proceso);

    //Crea un nuevo archivo sin el pid buscado y sus paginas, para "borrarlo" del swapfile.
    while (fgets(linea, cfg_memoria->TAM_MEMORIA, swapfile)) {
        linea[string_length(linea) - 1] = '\0';

        if (string_equals_ignore_case(linea, pid_s)) {
            encontrado = true;
        }
        else if (encontrado) {
            encontrado = false;
        }
        else {
            linea[string_length(linea) - 1] = '\n';
            txt_write_in_file(swapfile_tmp, linea);
        }
    }

    txt_close_file(swapfile_tmp);
    fclose(swapfile);

    remove(cfg_memoria->PATH_SWAPFILE);
    rename(swapfile_tmp_path, cfg_memoria->PATH_SWAPFILE);

    free(linea);
    free(pid_s);
    free(swapfile_tmp_path);
}

