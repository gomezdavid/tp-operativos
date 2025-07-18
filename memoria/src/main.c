#include "../include/main.h"

t_log *logger;
t_dictionary *diccionario_procesos;
char *memoria_usuario;
pthread_mutex_t mutex_diccionario;
sem_t cpu_handshake;
sem_t kernel_handshake;
t_config *config;
t_list *lista_tablas_por_pid;
int32_t tam_memoria_actual;
int32_t fd_escucha_memoria;

int main(int argc, char *argv[])
{
    config = crear_config("memoria");
    logger = crear_log(config, "memoria");
    log_debug(logger, "Config y Logger creados correctamente.");
    //Leo el archivo de configuracion y la guardo en una variable global.
    leer_configuracion(config);
    memoria_principal = malloc(sizeof(t_memoria));
    memoria_principal->datos = calloc(cfg_memoria->TAM_MEMORIA,1);
    inicializar_bitmap(cfg_memoria->TAM_MEMORIA / cfg_memoria->TAM_PAGINA);

    // Inicializo diccionario de procesos y su mutex
    diccionario_procesos = dictionary_create();
    pthread_mutex_init(&mutex_diccionario, NULL);
    sem_init(&cpu_handshake, 0, 0);
    sem_init(&kernel_handshake, 0, 0);
    sem_init(&mutex_memoria, 0, 1);
    log_debug(logger, "Diccionario de procesos creado correctamente.");
    lista_tablas_por_pid = list_create();
    tam_memoria_actual = cfg_memoria->TAM_MEMORIA;

    fd_escucha_memoria = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA"));

    // Threads para CPU;
    for (int i = 0; i < cfg_memoria->CANT_CPU; i++) {
        pthread_t thread_escucha_cpu;
        pthread_create(&thread_escucha_cpu, NULL, (void*)handshake_cpu, NULL);
        pthread_detach(thread_escucha_cpu);
    }

    // Thread para atender a Kernel
    pthread_t thread_escucha_kernel;
    pthread_create(&thread_escucha_kernel, NULL, (void*)handshake_kernel, NULL);
    pthread_detach(thread_escucha_kernel);

    getchar();

    sem_destroy(&cpu_handshake);
    sem_destroy(&kernel_handshake);
    sem_destroy(&mutex_memoria);
    liberar_conexion(fd_escucha_memoria);
    config_destroy(config);
    liberar_diccionario(diccionario_procesos);
    pthread_mutex_destroy(&mutex_diccionario);
    free(memoria_usuario);

    log_info(logger, "Finalizo el proceso.");
    log_destroy(logger);

    return 0;
}

void handshake_kernel()
{
    int32_t cliente = esperar_cliente(fd_escucha_memoria);

    log_info(logger, "## Kernel Conectado - FD del socket: %d", cliente);
    char *identificador = recibir_handshake(cliente);

    if (string_equals_ignore_case(identificador, "KERNEL"))
    {
        log_debug(logger, "Handshake Kernel a Memoria OK.");
    }
    else
    {
        log_error(logger, "Handshake Kernel a Memoria error.");
        free(identificador);
        liberar_conexion(cliente);
        return;
    }

    free(identificador);
    enviar_handshake(cliente, "MEMORIA");
    liberar_conexion(cliente);
    sem_post(&cpu_handshake);
    for(int i = 0; i < cfg_memoria->CANT_CPU; i++) {
        sem_wait(&kernel_handshake);
    }
    // Loop para atender Kernel
    while (1)
    {
        int32_t pid = -1;
        char* pid_s = "";
        t_proceso* proceso = NULL;
        tablas_por_pid* tablas_proceso = NULL;
        cliente = esperar_cliente(fd_escucha_memoria);
        log_info(logger, "## Kernel Conectado - FD del socket: %d", cliente);
        t_paquete *paquete = recibir_paquete(cliente);
        if (paquete == NULL)
        {
            log_error(logger, "Error al recibir paquete de Kernel");
            break;
        }

        switch (paquete->codigo_operacion)
        {
            case SAVE_INSTRUCTIONS: {
                pid = recibir_instrucciones(paquete);
                //si guardo las instrucciones signifca que "admiti" un proceso entonces creo las tablas.
                //t_list* keys = dictionary_keys(diccionario_procesos);
                pid_s = string_itoa(pid);
                //pid = atoi(pid_s);
                t_proceso* proceso_aux = dictionary_get(diccionario_procesos, pid_s);
                int32_t tam_proceso_aux = proceso_aux->tamanio;
                
                tablas_por_pid* tabla_proceso = crear_tabla_raiz(pid, tam_proceso_aux);
                int32_t indice_marcos = 0; 
                asignar_marcos(tabla_proceso->tabla_raiz, &tam_proceso_aux, 1, tabla_proceso->marcos, &indice_marcos, proceso_aux->lista_metricas);

                list_add(lista_tablas_por_pid, tabla_proceso);

                tam_memoria_actual = tam_memoria_actual - proceso_aux->tamanio;

                t_buffer* buffer_res_save_ins = buffer_create(sizeof(bool));
                buffer_add_bool(buffer_res_save_ins, true);
                t_paquete* res_save_ins = crear_paquete(SAVE_INSTRUCTIONS, buffer_res_save_ins);
                enviar_paquete(res_save_ins, cliente); 

                log_info(logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, proceso_aux->tamanio);

                break;
            }
            case CONSULTA_MEMORIA_PROCESO: {
                recibir_consulta_memoria(cliente, paquete);
                break;
            }
            case TERMINAR_PROCESO: {
                pid = buffer_read_int32(paquete->buffer);
                pid_s = string_itoa(pid);
                proceso = dictionary_remove(diccionario_procesos, pid_s);
                tablas_proceso = tablas_por_pid_remove_by_pid(lista_tablas_por_pid, pid);
                mostrar_metricas(proceso->lista_metricas);

                liberar_espacio_memoria(tablas_proceso, proceso->tamanio, proceso->lista_metricas);
                liberar_lista_instrucciones(proceso->lista_instrucciones);
                tam_memoria_actual = tam_memoria_actual + proceso->tamanio;
                free(proceso->lista_metricas);
                free(proceso);

                t_buffer* buffer_exit = buffer_create(sizeof(bool));
                buffer_add_bool(buffer_exit, true);
                t_paquete* respuesta_exit = crear_paquete(TERMINAR_PROCESO, buffer_exit);
                enviar_paquete(respuesta_exit, cliente);
                break;
            }
            case DUMP_MEMORY_SYSCALL: {
                pid = buffer_read_int32(paquete->buffer);
                pid_s = string_itoa(pid);
                proceso = dictionary_get(diccionario_procesos, pid_s);
                tablas_proceso = tablas_por_pid_get_by_pid(lista_tablas_por_pid, pid);
                dump_memory(tablas_proceso, proceso->lista_metricas);
                t_buffer* buffer_dump = buffer_create(sizeof(bool));
                buffer_add_bool(buffer_dump, true);
                t_paquete* respuesta_dump = crear_paquete(DUMP_MEMORY_SYSCALL, buffer_dump);
                enviar_paquete(respuesta_dump, cliente);
                break;
            }
            case SUSPENDER_PROCESO: {
                pid = buffer_read_int32(paquete->buffer);
                pid_s = string_itoa(pid);
                proceso = dictionary_get(diccionario_procesos, pid_s);
                tablas_proceso = tablas_por_pid_get_by_pid(lista_tablas_por_pid, pid);
                suspender_proceso(tablas_proceso, proceso->lista_metricas);
                break;
            }
            case DESSUSPENDER_PROCESO: {
                pid = buffer_read_int32(paquete->buffer);
                pid_s = string_itoa(pid);
                proceso = dictionary_get(diccionario_procesos, pid_s);
                tablas_proceso = tablas_por_pid_get_by_pid(lista_tablas_por_pid, pid);
                dessuspender_procesos(tablas_proceso, proceso->tamanio, proceso->lista_metricas);
                break;
            }
            default:
                log_error(logger, "Operación desconocida de Kernel");
                break;
        }

        destruir_paquete(paquete);
        liberar_conexion(cliente);
    }

    log_info(logger, "Finalizando conexión con Kernel");
    
    return;
}

tablas_por_pid* tablas_por_pid_remove_by_pid(t_list* tablas_por_pid_list, int32_t pid) {
    bool pid_equals(void *p_tablas_por_pid) {
        tablas_por_pid *tablas_por_pid_cast = (tablas_por_pid*)p_tablas_por_pid;
        return tablas_por_pid_cast->pid == pid;
    }
    return list_remove_by_condition(tablas_por_pid_list, pid_equals);
}

tablas_por_pid* tablas_por_pid_get_by_pid(t_list* tablas_por_pid_list, int32_t pid) {
    bool pid_equals(void *p_tablas_por_pid) {
        tablas_por_pid *tablas_por_pid_cast = (tablas_por_pid*)p_tablas_por_pid;
        return tablas_por_pid_cast->pid == pid;
    }
    return list_find(tablas_por_pid_list, pid_equals);
}

void handshake_cpu()
{
    sem_wait(&cpu_handshake);
    int32_t cliente = esperar_cliente(fd_escucha_memoria);

    char *identificador = recibir_handshake(cliente);

    if (string_equals_ignore_case(identificador, "cpu"))
    {
        log_debug(logger, "Handshake CPU a Memoria OK.");
    }
    else
    {
        log_error(logger, "Handshake CPU a Memoria error.");
        free(identificador);
        liberar_conexion(cliente);
        return;
    }

    free(identificador);
    enviar_handshake(cliente, "MEMORIA");
    t_config_to_cpu* cfg_to_cpu = malloc(sizeof(t_config_to_cpu));
    cfg_to_cpu->cantidad_niveles = cfg_memoria->CANTIDAD_NIVELES;
    cfg_to_cpu->tam_paginas = cfg_memoria->TAM_PAGINA;
    cfg_to_cpu->cant_entradas = cfg_memoria->ENTRADAS_POR_TABLA;
    enviar_config_to_cpu(cfg_to_cpu, cliente);
    sem_post(&kernel_handshake);
    sem_post(&cpu_handshake);
    bool proceso_terminado = false;
    // Loop para atender peticiones de CPU
    while (1)
    {
        char* pid_s = NULL;
        int32_t pid = -1;
        int32_t direccion = -1;
        t_proceso* proceso = NULL;
        t_paquete *paquete = recibir_paquete(cliente);
        log_debug(logger, "Se recibio paquete de cpu");

        if (paquete == NULL)
        {
            log_error(logger, "Error al recibir paquete de CPU");
            break;
        }

        switch (paquete->codigo_operacion)
        {
        case FETCH:
            // CPU pide una instrucción
            usleep(cfg_memoria->RETARDO_MEMORIA * 1000);
            proceso_terminado = enviar_instruccion(cliente, paquete);
            if (proceso_terminado)
            {
                log_debug(logger, "Proceso terminado - instrucción EXIT enviada");
            }
            break;
        case READ_MEMORIA:{
            cpu_read *parametros = deserializar_cpu_read(paquete->buffer);
            char* retorno = (char*)leer_de_memoria(parametros->direccion, parametros->tamanio, parametros->pid);
            t_buffer *buffer_resp = buffer_create(sizeof(int32_t) + parametros->tamanio + 1);
            buffer_add_int32(buffer_resp, parametros->tamanio + 1);
            buffer_add_string(buffer_resp, parametros->tamanio + 1, retorno);
            t_paquete *paquete_read = crear_paquete(READ_MEMORIA, buffer_resp);
            enviar_paquete(paquete_read, cliente);
            break;
        }
        case WRITE_MEMORIA:{
            cpu_write *parametros_write = deserializar_cpu_write(paquete->buffer);
            escribir_en_memoria(parametros_write->direccion, parametros_write->datos_length, parametros_write->datos, parametros_write->pid);

            t_buffer* buffer_write = buffer_create(sizeof(bool));
            buffer_add_bool(buffer_write, true);
            t_paquete* respuesta_write = crear_paquete(WRITE_MEMORIA, buffer_write);
            enviar_paquete(respuesta_write, cliente);
            free(parametros_write->datos);
            free(parametros_write);
            break;
        }
        case CONSULTA_MARCO:{
            pid = buffer_read_int32(paquete->buffer);
            int32_t* indices = calloc(cfg_memoria->CANTIDAD_NIVELES, sizeof(int32_t));
            for(int i = 0; i < cfg_memoria->CANTIDAD_NIVELES; i++) {
                indices[i] = buffer_read_int32(paquete->buffer);
            }
            pid_s = string_itoa(pid);
            proceso = dictionary_get(diccionario_procesos, pid_s);
            tablas_por_pid* tablas_proceso = tablas_por_pid_get_by_pid(lista_tablas_por_pid, pid);
            int32_t marco = devolver_marco(tablas_proceso->tabla_raiz, indices, 1, proceso->lista_metricas);
            t_buffer* buffer_marco = buffer_create(sizeof(int32_t));
            buffer_add_int32(buffer_marco, marco);
            t_paquete* respuesta_marco = crear_paquete(CONSULTA_MARCO, buffer_marco);
            enviar_paquete(respuesta_marco, cliente);
            free(indices);
            break;
        }
        case LEER_PAGINA_COMPLETA:{
            pid = buffer_read_int32(paquete->buffer);
            direccion = buffer_read_int32(paquete->buffer);
            pid_s = string_itoa(pid);
            proceso = dictionary_get(diccionario_procesos, pid_s);
            void* contenido_pagina = leer_pagina_completa(direccion, pid, proceso->lista_metricas);
            t_buffer* buffer_leer = buffer_create(cfg_memoria->TAM_PAGINA);
            buffer_add(buffer_leer, contenido_pagina, cfg_memoria->TAM_PAGINA);
            t_paquete* respuesta = crear_paquete(LEER_PAGINA_COMPLETA, buffer_leer);
            enviar_paquete(respuesta, cliente);
            break;
        }
        case ACTUALIZAR_PAGINA_COMPLETA:{
            pid = buffer_read_int32(paquete->buffer);
            direccion = buffer_read_int32(paquete->buffer);
            void *contenido = malloc(cfg_memoria->TAM_PAGINA);
            buffer_read(paquete->buffer, contenido, cfg_memoria->TAM_PAGINA);
            pid_s = string_itoa(pid);
            proceso = dictionary_get(diccionario_procesos, pid_s);
            actualizar_pagina_completa(direccion, contenido, pid, proceso->lista_metricas);
            break;
        }
        default:
            log_error(logger, "Operación desconocida de CPU");
            break;
        }

        destruir_paquete(paquete);
    }

    log_info(logger, "Finalizando conexión con CPU");
    liberar_conexion(cliente);
    return;
}

//Recibe  un proceso de kernel, con su tamaño, uso funcion verificar_espacio y si hay, devuelvo TRUE.
bool recibir_consulta_memoria(int32_t fd_kernel, t_paquete *paquete){
    // creo buffer para leer el paquete con pid y tamaño que se me envio
    int32_t tamanio = buffer_read_int32(paquete->buffer);
    log_debug(logger, "Recibo consulta de espacio para tamaño %d", tamanio);
    bool hay_espacio = verificar_espacio_memoria( tamanio);
    // preparo el paquete de respuesta-creo buffer y le asigno tamaño del bool  que devuelve verificar_espacio_memoria
    t_buffer *buffer = buffer_create(sizeof(bool));
    buffer_add_bool(buffer, hay_espacio);
    t_paquete *respuesta = crear_paquete(CONSULTA_MEMORIA_PROCESO, buffer);  
    enviar_paquete(respuesta, fd_kernel);

    return hay_espacio;
}

//Recibo las instrucciones si hubiese espacio, y las cargo --> cargar_instrucciones()
int32_t recibir_instrucciones(t_paquete* paquete){

    if (paquete->codigo_operacion != SAVE_INSTRUCTIONS)
    {
        log_error(logger, "Codigo de operacion incorrecto para guardar las instrucciones");
        return -1;
    }

    kernel_to_memoria *kernelToMemoria = deserializar_kernel_to_memoria(paquete->buffer);

    log_info(logger, "Recibo archivo con instrucciones para PID %d", kernelToMemoria->pid);
    char *path_archivo = string_duplicate(cfg_memoria->PATH_INSTRUCCIONES);
    string_append(&path_archivo, kernelToMemoria->archivo);

    int32_t pid = kernelToMemoria->pid;

    cargar_instrucciones(path_archivo, kernelToMemoria);

    free(path_archivo);
    free(kernelToMemoria->archivo);
    free(kernelToMemoria);

    return pid;
}

bool verificar_espacio_memoria(int32_t tamanio)
{
    log_info(logger, "- Tamaño Memoria Total: %d - Tamaño Solicitado: %d - Espacio Disponible: %d",
             cfg_memoria->TAM_MEMORIA,
             tamanio,
             tam_memoria_actual);

    return tam_memoria_actual > tamanio;
}


kernel_to_memoria *deserializar_kernel_to_memoria(t_buffer *buffer)
{
    kernel_to_memoria *data = malloc(sizeof(kernel_to_memoria));

    data->archivo_length = buffer_read_int32(buffer);
    data->archivo = buffer_read_string(buffer, &data->archivo_length);
    data->tamanio = buffer_read_int32(buffer);
    data->pid = buffer_read_int32(buffer);
    return data;
}

//Parsea linea por linea el archivo.
struct_memoria_to_cpu *parsear_linea(char *linea){
    char **token = string_split(linea, " ");
    struct_memoria_to_cpu *struct_memoria_to_cpu = malloc(sizeof(struct_memoria_to_cpu));
    struct_memoria_to_cpu->parametros = NULL;

    if (string_equals_ignore_case(token[0], "READ"))
    {
        struct_memoria_to_cpu->instruccion = READ;
    }
    else if (string_equals_ignore_case(token[0], "WRITE"))
    {
        struct_memoria_to_cpu->instruccion = WRITE;
    }
    else if (string_equals_ignore_case(token[0], "GOTO"))
    {
        struct_memoria_to_cpu->instruccion = GOTO;
    }
    else if (string_equals_ignore_case(token[0], "NOOP"))
    {
        struct_memoria_to_cpu->instruccion = NOOP;
    }
    else if (string_equals_ignore_case(token[0], "EXIT"))
    {
        struct_memoria_to_cpu->instruccion = EXIT;
    }
    else if (string_equals_ignore_case(token[0], "IO"))
    {
        struct_memoria_to_cpu->instruccion = IO_SYSCALL;
    }
    else if (string_equals_ignore_case(token[0], "INIT_PROC"))
    {
        struct_memoria_to_cpu->instruccion = INIT_PROC;
    }
    else if (string_equals_ignore_case(token[0], "DUMP_MEMORY"))
    {
        struct_memoria_to_cpu->instruccion = DUMP_MEMORY;
    }
    else
    {
        log_error(logger, "Instrucción desconocida: %s\n", token[0]);
    }

    if (token[1] != NULL)
    {
        struct_memoria_to_cpu->parametros = string_new();
        for (int i = 1; token[i] != NULL; i++)
        {
            string_append_with_format(&struct_memoria_to_cpu->parametros, "%s%s", token[i], token[i + 1] ? " " : "");
        }
    }
    else
    {
        struct_memoria_to_cpu->parametros = string_duplicate("");
    }

    string_iterate_lines(token, (void*)free);
    free(token);

    return struct_memoria_to_cpu;
}

//Carga la instrucciones usando --> parsear_linea para "parsear" cada linea y poder guardarla en una lista.
//Al final, se guardan las instrucciones en un diccionario clave: pid, valor: lista de instrucciones.
void cargar_instrucciones(char *path_archivo, kernel_to_memoria* proceso_recibido){

    FILE *archivo = fopen(path_archivo, "r");
    if (!archivo)
    {
        perror("No se pudo abrir el archivo de instrucciones");
        return;
    }

    t_list *lista_instrucciones = list_create();
    char *linea = NULL;
    size_t len = 0;

    while (getline(&linea, &len, archivo) != -1)
    {
        string_trim(&linea);
        if (string_is_empty(linea))
            continue;

        struct_memoria_to_cpu *instruccion = parsear_linea(linea);
        list_add(lista_instrucciones, instruccion);

        free(linea);
        linea = NULL;
        len = 0;
    }

    free(linea);
    fclose(archivo);
    t_proceso* proceso = malloc(sizeof(t_proceso));

    proceso->tamanio = proceso_recibido->tamanio;
    proceso->lista_instrucciones = lista_instrucciones;
    proceso->lista_metricas = malloc(sizeof(t_metricas));
    proceso->lista_metricas->pid = proceso_recibido->pid;
    inicializar_metricas(proceso->lista_metricas);

    char *pid_str = string_itoa(proceso_recibido->pid);
    dictionary_put(diccionario_procesos, pid_str, proceso);
    free(pid_str);
}

void inicializar_metricas(t_metricas* metricas) {
    metricas->accesos_tablas_paginas = 0;
    metricas->bajadas_a_swap = 0;
    metricas->cantidad_escrituras = 0;
    metricas->cantidad_lecturas = 0;
    metricas->instrucciones_solicitadas = 0;
    metricas->subidas_a_mp = 0;
}

//Envia la instruccion a CPU y al final verifica si fue EXIT o no.
bool enviar_instruccion(int32_t fd_cpu, t_paquete* paquete){
    int32_t pid = buffer_read_int32(paquete->buffer);
    int32_t pc = buffer_read_int32(paquete->buffer);

    char *pid_str = string_itoa(pid);
    t_proceso* proceso = dictionary_get(diccionario_procesos, pid_str);
    //t_list *lista_instruccion_to_cpu = dictionary_get(diccionario_procesos, pid_str);
    free(pid_str);

    if (proceso->lista_instrucciones == NULL || pc >= list_size(proceso->lista_instrucciones))
    {
        log_error(logger, "PID no encontrado o PC no valido");
        destruir_paquete(paquete);
        return false;
    }

    struct_memoria_to_cpu *instruccion = list_get(proceso->lista_instrucciones, pc);
    int32_t tam_para = string_length(instruccion->parametros) + 1;
    t_buffer* buffer = buffer_create(sizeof(uint8_t) + sizeof(int32_t) + tam_para);
    buffer_add_uint8(buffer, instruccion->instruccion);
    buffer_add_int32(buffer, tam_para);
    buffer_add_string(buffer, tam_para, instruccion->parametros);
    t_paquete *paquete_retorno = crear_paquete(FETCH, buffer);
    
    enviar_paquete(paquete_retorno, fd_cpu);

    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s %s", pid, pc, t_instruccion_to_string(instruccion->instruccion), instruccion->parametros);

    return instruccion->instruccion == EXIT;
}

char *t_instruccion_to_string(t_instruccion instruccion) {
    switch(instruccion){
        case NOOP:
            return "NOOP";
            break;
        case WRITE:
            return "WRITE";
            break;
        case READ:
            return "READ";
            break;
        case IO_SYSCALL:
            return "IO_SYSCALL";
            break;
        case INIT_PROC:
            return "INIT_PROC";
            break;    
        case GOTO:
            return "GOTO";
            break;
        case DUMP_MEMORY:
            return "DUMP_MEMORY";
            break;
        case EXIT:
            return "EXIT";
            break;
        default:
            return "";
            break;
    }
}

void leer_configuracion(t_config* config){
    cfg_memoria = malloc(sizeof(t_config_memoria));
    cfg_memoria->PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
    cfg_memoria->TAM_MEMORIA = config_get_int_value(config, "TAM_MEMORIA");
    cfg_memoria->TAM_PAGINA = config_get_int_value(config, "TAM_PAGINA");
    cfg_memoria->ENTRADAS_POR_TABLA = config_get_int_value(config, "ENTRADAS_POR_TABLA");
    cfg_memoria->CANTIDAD_NIVELES = config_get_int_value(config, "CANTIDAD_NIVELES");    
    cfg_memoria->RETARDO_MEMORIA = config_get_int_value(config, "RETARDO_MEMORIA");
    cfg_memoria->PATH_SWAPFILE = config_get_string_value(config, "PATH_SWAPFILE");
    cfg_memoria->RETARDO_SWAP = config_get_string_value(config, "RETARDO_SWAP");
    cfg_memoria->LOG_LEVEL = config_get_string_value(config, "LOG_LEVEL");
    cfg_memoria->DUMP_PATH = config_get_string_value(config, "DUMP_PATH");
    cfg_memoria->PATH_INSTRUCCIONES = config_get_string_value(config, "PATH_INSTRUCCIONES"); 
    cfg_memoria->CANT_CPU = config_get_int_value(config, "CANT_CPU");
} 

/* Funciones para deserializar el CPU_READ y CPU WRITE que teniamos preventivamente*/
cpu_read *deserializar_cpu_read(t_buffer *data) {
    cpu_read *ret = malloc(sizeof(cpu_read));
    ret->direccion = buffer_read_int32(data);
    ret->tamanio = buffer_read_int32(data);
    return ret;
}

cpu_write *deserializar_cpu_write(t_buffer *data) {
    cpu_write *ret = malloc(sizeof(cpu_write));
    ret->direccion = buffer_read_int32(data);
    ret->datos_length = buffer_read_int32(data);
    ret->datos = buffer_read_string(data, &ret->datos_length);
    ret->pid = buffer_read_int32(data);
    return ret;
}

t_buffer* serializar_config_to_cpu(t_config_to_cpu* cfg_to_cpu){
    t_buffer* buffer = buffer_create(sizeof(int32_t) * 3);
    buffer_add_int32(buffer, cfg_to_cpu->cantidad_niveles);
    buffer_add_int32(buffer, cfg_to_cpu->tam_paginas);
    buffer_add_int32(buffer, cfg_to_cpu->cant_entradas);
    return buffer;
}

void enviar_config_to_cpu(t_config_to_cpu* cfg_to_cpu, int32_t socket){
    t_buffer* buffer = serializar_config_to_cpu(cfg_to_cpu);
    t_paquete* paquete = crear_paquete(HANDSHAKE, buffer);
    enviar_paquete(paquete, socket);
}
