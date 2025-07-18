#include<main.h>

int main(int argc, char* argv[]) {
    iniciar_modulo();

    kernel_to_memoria *archivo_inicial = malloc(sizeof(kernel_to_memoria));
    archivo_inicial->archivo = argv[1];
    archivo_inicial->archivo_length = string_length(archivo_inicial->archivo) + 1;
    archivo_inicial->tamanio = atoi(argv[2]);
    list_add(archivos_instruccion, archivo_inicial);

    handshake_memoria();

    log_debug(logger, "Modulo iniciado correctamente.");

    pthread_t administrador_cpu_dispatch;
    pthread_create(&administrador_cpu_dispatch, NULL, (void*)administrar_cpus_dispatch, NULL);
    pthread_detach(administrador_cpu_dispatch);

    pthread_t administrador_cpu_interrupt;
    pthread_create(&administrador_cpu_interrupt, NULL, (void*)administrar_cpus_interrupt, NULL);
    pthread_detach(administrador_cpu_interrupt);

    pthread_t administrador_io;
    pthread_create(&administrador_io, NULL, (void*)administrar_dispositivos_io, NULL);
    pthread_detach(administrador_io);
    
    // pthread_t thread_io;
    // pthread_create(&thread_io, NULL, escucha_io, config); // Se crea un thread que corra escucha_io(config)
    // pthread_detach(thread_io);  // Se separa la ejecución del thread de la del programa principal

    // pthread_t thread_memoria;
    // pthread_create(&thread_memoria, NULL, (void*)handshake_memoria, config);
    // pthread_detach(thread_memoria); 

    // //sleep(1);

    // //Creo un hilo para que corra escucha_cpu(config)
    // pthread_t thread_cpu;
    // pthread_create(&thread_cpu, NULL, escucha_cpu, config);
    // pthread_detach(thread_cpu);

    getchar(); // Para que el progrma no termine antes que los threads
    inicio_modulo = false;
    liberar_conexion(fd_escucha_cpu);
    liberar_conexion(fd_escucha_cpu_interrupt);

    pthread_t planificador_corto_plazo;
    pthread_create(&planificador_corto_plazo, NULL, (void*)corto_plazo, NULL);
    pthread_detach(planificador_corto_plazo);

    pthread_t planificador_largo_plazo;
    pthread_create(&planificador_largo_plazo, NULL, (void*)largo_plazo, NULL);
    pthread_detach(planificador_largo_plazo);

    getchar();

    finalizar_modulo();

    return 0;
}

void iniciar_modulo() {
    config = crear_config("kernel");
    logger = crear_log(config, "kernel");
    log_debug(logger, "Config y Logger creados correctamente.");

    pid_counter = 0;
    est_inicial = 0;

    cola_new = list_create();
    cola_ready = list_create();
    cola_blocked = list_create();
    cola_exec = list_create();
    cola_susp_blocked = list_create();
    cola_susp_ready = list_create();

    archivos_instruccion = list_create();

    sem_init(&sem_largo_plazo, 0, 0);
    sem_init(&mutex_cpus, 0, 1);
    sem_init(&mutex_io, 0, 1);
    sem_init(&mutex_execute, 0, 1);
    sem_init(&sem_corto_plazo, 0, 0);
    sem_init(&mutex_ready, 0, 1);
    sem_init(&mutex_blocked, 0, 1);
    sem_init(&mutex_susp_blocked, 0, 1);
    sem_init(&mutex_susp_ready, 0, 1);

    inicio_modulo = true;

    fd_escucha_cpu = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH"));
    fd_escucha_cpu_interrupt = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT"));
    cpu_list = list_create();
    fd_escucha_io = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA_IO"));  
    io_list = list_create();
    io_queue_list = list_create();
}

void finalizar_modulo() {
    list_destroy(cola_new);
    list_destroy(cola_ready);
    list_destroy(cola_blocked);
    list_destroy(cola_exec);
    list_destroy(cola_susp_blocked);
    list_destroy(cola_susp_ready);
    list_destroy(archivos_instruccion);
    list_destroy(cpu_list);
    list_destroy(io_list);
    list_destroy(io_queue_list);

    sem_destroy(&sem_largo_plazo);
    sem_destroy(&mutex_cpus);
    sem_destroy(&mutex_io);
    sem_destroy(&mutex_execute);
    sem_destroy(&sem_corto_plazo);
    sem_destroy(&mutex_ready);
    sem_destroy(&mutex_blocked);

    liberar_conexion(fd_escucha_io);

    config_destroy(config);
    log_info(logger, "Finalizo el proceso.");
    log_destroy(logger);
}

void escucha_io(){  
    int32_t socket_io = esperar_cliente(fd_escucha_io);
    char* identificador = recibir_handshake(socket_io);
    if (!string_is_empty(identificador)) {
        log_info(logger, "Handshake IO a Kernel OK.");
        log_info(logger, "Se inicio la instancia: %s", identificador);
    }
    else {
        log_error(logger, "Handshake IO a Kernel fallido, la instancia de IO debe tener un nombre.");
    }
    free(identificador);
    enviar_handshake(socket_io, "KERNEL");
    
    //test
    t_buffer* buffer = buffer_create(sizeof(int32_t)*2);
    buffer_add_int32(buffer, 1);
    buffer_add_int32(buffer, 1000000*5);
    t_paquete* paquete = crear_paquete(IO, buffer);
    enviar_paquete(paquete, socket_io);
    recibir_paquete(socket_io);

    getchar();

    liberar_conexion(socket_io);
    liberar_conexion(fd_escucha_io);
    return;
}

//Creo escucha cpu
void escucha_cpu(){
    int32_t fd_escucha_cpu = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH"));    
    int32_t socket_cpu = esperar_cliente(fd_escucha_cpu);
    char *identificador = recibir_handshake(socket_cpu);
    if (string_equals_ignore_case(identificador, "cpu")) {
        log_debug(logger, "Handshake CPU a Kernel OK.");
    }
    else {
        log_error(logger, "Handshake CPU a Kernel error.");
    }
    free(identificador);
    enviar_handshake(socket_cpu, "KERNEL");
    liberar_conexion(socket_cpu);
    liberar_conexion(fd_escucha_cpu);
    return;
}


void handshake_memoria() {
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));

    enviar_handshake(fd_conexion_memoria, "KERNEL");

    char* identificador = recibir_handshake(fd_conexion_memoria);
    if (string_equals_ignore_case(identificador, "memoria")) {
        log_debug(logger, "Handshake Memoria a Kernel OK.");
    }
    else {
        log_error(logger, "Handshake Memoria a Kernel error.");
    }

    free(identificador);
    liberar_conexion(fd_conexion_memoria);
    return;
}

//LARGO PLAZO

void largo_plazo() {
    //Seleccion de algoritmo
    char* algoritmo_config = config_get_string_value(config, "ALGORITMO_COLA_NEW");
    if (string_equals_ignore_case(algoritmo_config, "FIFO")) {
        log_debug(logger, "Planificador de largo plazo con FIFO.");
        planificar_fifo_largo_plazo();
    }
    else if (string_equals_ignore_case(algoritmo_config, "PMCP")) {
        planificar_pmcp_largo_plazo();
    }
    else {
        log_error(logger, "Algoritmo desconocido para planificador de largo plazo.");
    }
    return;
}

void planificar_fifo_largo_plazo() {
    while(1) {
        t_pcb *pcb = NULL;
        bool consulta_memoria = false;
        if (!list_is_empty(cola_susp_ready)) {
            sem_wait(&mutex_susp_ready);
            pcb = list_remove(cola_susp_ready, 0);
            sem_post(&mutex_susp_ready);
            consulta_memoria = consultar_a_memoria_by_pcb(pcb);
            if (consulta_memoria) {
                dessuspender_proceso(pcb);
            }
            else {
                log_debug(logger, "## (%d) No hay espacio suficiente para des-suspender el proceso", pcb->pid);
                sem_wait(&sem_largo_plazo);
            }
            continue;
        }
        if (list_is_empty(archivos_instruccion)) {
            log_debug(logger, "No hay archivo de instruccion.");
            sem_wait(&sem_largo_plazo);
            continue;
        }
        pcb = crear_proceso();
        consulta_memoria = consultar_a_memoria();
        if (consulta_memoria) {
            enviar_instrucciones();
            list_remove(cola_new, 0);
            pasar_ready(pcb, list_get(pcb->metricas, NEW));
        }
        else {
            log_debug(logger, "## (%d) No hay espacio suficiente para inicializar el proceso", pcb->pid);
            sem_wait(&sem_largo_plazo);
        }
    }
}

void dessuspender_proceso(t_pcb* pcb) {
    log_debug(logger, "INICIO - enviar_instrucciones - pid:%d", pcb->pid);
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer *buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, pcb->pid);
    t_paquete *consulta = crear_paquete(DESSUSPENDER_PROCESO, buffer);
    enviar_paquete(consulta, fd_conexion_memoria);
    liberar_conexion(fd_conexion_memoria);
    pasar_ready(pcb, list_get(pcb->metricas, SUSP_READY));
}

t_estado_metricas *crear_metrica_estado(t_estado estado) {
    t_estado_metricas *metrica_agregar = malloc(sizeof(t_estado_metricas));
    metrica_agregar->estado = estado;
    metrica_agregar->ME = 0;
    metrica_agregar->MT = temporal_create();
    temporal_stop(metrica_agregar->MT);
    return metrica_agregar;
}

void agregar_metricas_estado(t_pcb *pcb) {
    for (t_estado i = NEW;i < 7;i++) {
        t_estado_metricas *new = crear_metrica_estado(i);
        list_add(pcb->metricas, new);
    }
}

void pasar_por_estado(t_pcb *pcb, t_estado estado, t_estado estado_anterior) {
    t_estado_metricas *estado_a_pasar = list_get(pcb->metricas, estado);
    estado_a_pasar->ME++;
    if (estado_a_pasar->MT == NULL) {
        estado_a_pasar->MT = temporal_create();
    }
    else {
        temporal_resume(estado_a_pasar->MT);
    }
    if (estado != NEW) {
        log_info(logger, "## (%d) Pasa del estado %s al estado %s", pcb->pid, t_estado_to_string(estado_anterior), t_estado_to_string(estado));
    }
}

t_pcb* crear_proceso() {
    kernel_to_memoria *archivo = list_get(archivos_instruccion, 0);
    t_pcb *pcb = malloc(sizeof(t_pcb));
    pcb->pid = pid_counter;
    pcb->pc = 0;
    pcb->metricas = list_create();
    agregar_metricas_estado(pcb);
    pasar_por_estado(pcb, NEW, NEW);
    pcb->estado_actual = NEW;
    pcb->rafaga_estimada = est_inicial;
    list_add(cola_new, pcb);
    archivo->pid = pid_counter;
    pid_counter += 1;
    log_info(logger, "## (%d) - Se crea el proceso - Estado: NEW", pcb->pid);
    return pcb;
}

bool consultar_a_memoria() {
    kernel_to_memoria *archivo = list_get(archivos_instruccion, 0);
    log_debug(logger, "INICIO - consultar_a_memoria - archivo:%s", archivo->archivo);
    int32_t tamanio_proceso = archivo->tamanio;
    int32_t pid = archivo->pid;
    bool ret = false;
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer* buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, tamanio_proceso);
    t_paquete* paquete = crear_paquete(CONSULTA_MEMORIA_PROCESO, buffer);
    enviar_paquete(paquete, fd_conexion_memoria);
    t_paquete *retorno = recibir_paquete(fd_conexion_memoria);
    if (retorno->codigo_operacion == CONSULTA_MEMORIA_PROCESO) {
        ret = buffer_read_bool(paquete->buffer);
    }
    else {
        log_error(logger, "## (%d) Codigo de operación incorrecto para consultar a memoria.", pid);
    }
    free(retorno);
    liberar_conexion(fd_conexion_memoria);
    log_debug(logger, "FIN - consultar_a_memoria - retorno:%d", ret);
    return ret;
}

bool consultar_a_memoria_by_pcb(t_pcb *pcb) {
    log_debug(logger, "INICIO - consultar_a_memoria_by_pcb - pid:%d", pcb->pid);
    int32_t tamanio_proceso = pcb->tamanio_proceso;
    int32_t pid = pcb->pid;
    bool ret = false;
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer* buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, tamanio_proceso);
    t_paquete* paquete = crear_paquete(CONSULTA_MEMORIA_PROCESO, buffer);
    enviar_paquete(paquete, fd_conexion_memoria);
    t_paquete *retorno = recibir_paquete(fd_conexion_memoria);
    if (retorno->codigo_operacion == CONSULTA_MEMORIA_PROCESO) {
        ret = buffer_read_bool(paquete->buffer);
    }
    else {
        log_error(logger, "## (%d) Codigo de operación incorrecto para consultar a memoria.", pid);
    }
    free(retorno);
    liberar_conexion(fd_conexion_memoria);
    log_debug(logger, "FIN - consultar_a_memoria - retorno:%d", ret);
    return ret;
}

void enviar_instrucciones() {
    kernel_to_memoria *archivo = list_remove(archivos_instruccion, 0);
    log_debug(logger, "INICIO - enviar_instrucciones - archivo:%s", archivo->archivo);
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer *buffer = serializar_kernel_to_memoria(archivo);
    t_paquete *consulta = crear_paquete(SAVE_INSTRUCTIONS, buffer);
    enviar_paquete(consulta, fd_conexion_memoria);
    t_paquete* respuesta = recibir_paquete(fd_conexion_memoria);
    destruir_paquete(respuesta);
    liberar_conexion(fd_conexion_memoria);
    free(archivo);
    return;
}

t_buffer *serializar_kernel_to_memoria(kernel_to_memoria* archivo) {
    t_buffer* buffer = buffer_create(sizeof(int32_t) * 3 + archivo->archivo_length);
    buffer_add_int32(buffer, archivo->archivo_length);
    buffer_add_string(buffer, archivo->archivo_length, archivo->archivo);
    buffer_add_int32(buffer, archivo->tamanio);
    buffer_add_int32(buffer, archivo->pid);
    return buffer;
}

void pasar_ready(t_pcb *pcb, t_estado_metricas* metricas) {
    temporal_stop(metricas->MT);
    pcb->estado_actual = READY;
    pasar_por_estado(pcb, READY, metricas->estado);
    sem_wait(&mutex_ready);
    list_add(cola_ready, pcb);
    sem_post(&mutex_ready);
    sem_post(&sem_corto_plazo);
}

bool terminar_proceso_memoria (int32_t pid) {
    bool retorno = false;
    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer *buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, pid);
    t_paquete *paquete = crear_paquete(TERMINAR_PROCESO, buffer);
    enviar_paquete(paquete, fd_conexion_memoria);
    t_paquete *respuesta = recibir_paquete(fd_conexion_memoria);
    if(respuesta->codigo_operacion != TERMINAR_PROCESO) {
        log_error(logger, "(%d) Codigo de operacion incorrecto para terminar_proceso.", pid);
    }
    else {
        bool confirmacion = buffer_read_bool(respuesta->buffer);
        if (!confirmacion) {
            log_error(logger, "(%d) Error en terminar_proceso. Revisar log memoria.", pid);
        }
        else{
            retorno = true;
        }
    }
    liberar_conexion(fd_conexion_memoria);
    destruir_paquete(respuesta);
    return retorno;
}

void terminar_proceso(int32_t pid) {
    bool consulta_memoria = terminar_proceso_memoria(pid);
    if (!consulta_memoria) {
        return;
    }
    sem_wait(&mutex_execute);
    t_pcb *proceso = pcb_remove_by_pid(cola_exec, pid);
    if (!proceso) {
        proceso = pcb_remove_by_pid(cola_blocked, pid);
        if (!proceso) {
            proceso = pcb_remove_by_pid(cola_susp_blocked, pid);
        }
    }
    t_cpu* cpu = cpu_find_by_id(proceso->cpu_id);
    cpu->estado = false;
    sem_post(&mutex_execute);
    sem_post(&sem_corto_plazo);
    t_estado_metricas *metricas_exec = list_get(proceso->metricas, proceso->estado_actual);
    temporal_stop(metricas_exec->MT);
    t_estado estado_anterior = proceso->estado_actual;
    proceso->estado_actual = EXIT_STATUS;
    pasar_por_estado(proceso, EXIT_STATUS, estado_anterior);
    log_info(logger, "## (%d) - Finaliza el proceso", pid);
    loggear_metricas_estado(proceso);
    free(proceso->cpu_id);
    free(proceso);
    sem_post(&sem_largo_plazo);

    return;
}

t_pcb* pcb_remove_by_pid(t_list* pcb_list, int32_t pid) {
    bool pid_equals(void *pcb) {
        t_pcb *pcb_cast = (t_pcb*)pcb;
        return pcb_cast->pid == pid;
    }
    return list_remove_by_condition(pcb_list, pid_equals);
}

t_pcb* pcb_get_by_pid(t_list* pcb_list, int32_t pid) {
    bool pid_equals(void *pcb) {
        t_pcb *pcb_cast = (t_pcb*)pcb;
        return pcb_cast->pid == pid;
    }
    return list_find(pcb_list, pid_equals);
}

void loggear_metricas_estado(t_pcb* proceso) {
    char* metricas_estado = string_new();
    t_list_iterator *metricas_iterator = list_iterator_create(proceso->metricas);
    while(list_iterator_has_next(metricas_iterator)){
        t_estado_metricas* metrica = list_iterator_next(metricas_iterator);
        string_append_with_format(&metricas_estado, "%s  (%d) (%lu), ", t_estado_to_string(metrica->estado), metrica->ME, temporal_gettime(metrica->MT));
        list_iterator_remove(metricas_iterator);
        temporal_destroy(metrica->MT);
        free(metrica);
    }
    list_destroy(proceso->metricas);
    log_info(logger, "## (%d) - Metricas de estado: %s", proceso->pid, metricas_estado);
}

char* t_estado_to_string(t_estado estado) {
    switch (estado)
    {
    case NEW:
        return "NEW";
        break;
    case READY:
        return "READY";
        break;
    case EXEC: 
        return "EXEC";
        break;
    case BLOCKED:
        return "BLOCKED";
        break;
    case SUSP_BLOCKED:
        return "SUSPENDED BLOCKED";
        break;
    case SUSP_READY:
        return "SUSPENDED READY";
        break;
    case EXIT_STATUS:
        return "EXIT";
        break;

    default:
        return "";
        break;
    }
}

void planificar_pmcp_largo_plazo() {
    while(1) {
        t_pcb *pcb = NULL;
        bool consulta_memoria = false;
        if (!list_is_empty(cola_susp_ready)) {
            sem_wait(&mutex_susp_ready);
            list_sort(cola_susp_ready, es_mas_chico_que);
            pcb = list_remove(cola_susp_ready, 0);
            sem_post(&mutex_susp_ready);
            consulta_memoria = consultar_a_memoria_by_pcb(pcb);
            if (consulta_memoria) {
                dessuspender_proceso(pcb);
            }
            else {
                log_debug(logger, "## (%d) No hay espacio suficiente para des-suspender el proceso", pcb->pid);
                sem_wait(&sem_largo_plazo);
            }
            continue;
        }
        if (list_is_empty(archivos_instruccion)) {
            log_debug(logger, "No hay archivo de instruccion.");
            sem_wait(&sem_largo_plazo);
        }
        pcb = crear_proceso();
        // Ordeno por mas chico, dejando en la posicion 0 al mas chico.
        list_sort(cola_new, es_mas_chico_que);
        consulta_memoria = consultar_a_memoria();
        if (consulta_memoria) {
            enviar_instrucciones();
            list_remove(cola_new, 0);
            pasar_ready(pcb, list_get(pcb->metricas, NEW));
        }
        else {
            log_debug(logger, "## (%d) No hay espacio suficiente para inicializar el proceso", pcb->pid);
            sem_wait(&sem_largo_plazo);
        }
    }
}

bool es_mas_chico_que(void *un_pcb, void *otro_pcb) {
    t_pcb *un_pcb_cast = (t_pcb*) un_pcb;
    t_pcb *otro_pcb_cast = (t_pcb*) otro_pcb;
    return un_pcb_cast->tamanio_proceso <= otro_pcb_cast->tamanio_proceso;
}

//CORTO PLAZO

void administrar_cpus_dispatch() {   
    while (inicio_modulo) {
        log_debug(logger, "Espero conexiones de cpu dispatch con fd=%d.", fd_escucha_cpu);
        pthread_t thread;
        int32_t *socket_cpu = malloc(sizeof(int32_t));
        *socket_cpu = esperar_cliente(fd_escucha_cpu);
        if (*socket_cpu != -1) {
            pthread_create(&thread, NULL, (void*)agregar_cpu_dispatch, socket_cpu);
            pthread_detach(thread);
        }
    } 
}

void agregar_cpu_dispatch(int32_t* socket) {
    log_debug(logger, "Recibo conexion de cpu dispatch con fd=%d.", *socket);
    char *identificador = recibir_handshake(*socket);
    enviar_handshake(*socket, "KERNEL");
    log_debug(logger, "Agrego cpu dispatch id=%s", identificador);
    t_cpu *cpu_agregar = malloc(sizeof(t_cpu));
    cpu_agregar->identificador = identificador;
    cpu_agregar->socket_dispatch = *socket;
    cpu_agregar->estado = false;
    sem_wait(&mutex_cpus);
    list_add(cpu_list, cpu_agregar);
    sem_post(&mutex_cpus);
    return;
}

void administrar_cpus_interrupt() {   
    while (inicio_modulo) {
        log_debug(logger, "Espero conexiones de cpu interrupt con fd=%d.", fd_escucha_cpu_interrupt);
        pthread_t thread;
        int32_t *socket_cpu = malloc(sizeof(int32_t));
        *socket_cpu = esperar_cliente(fd_escucha_cpu_interrupt);
        if(*socket_cpu != -1) {
            pthread_create(&thread, NULL, (void*)agregar_cpu_interrupt, socket_cpu);
            pthread_detach(thread);
        }
    } 
}

void agregar_cpu_interrupt(int32_t* socket) {
    log_debug(logger, "Recibo conexion de cpu interrupt con fd=%d.", *socket);
    char *identificador = recibir_handshake(*socket);
    enviar_handshake(*socket, "KERNEL");
    log_debug(logger, "Agrego cpu interrupt id=%s", identificador);
    t_cpu *cpu_a_guardar = cpu_find_by_id(identificador);
    sem_wait(&mutex_cpus);
    cpu_a_guardar->socket_interrupt = *socket;
    sem_post(&mutex_cpus);
    free(identificador);
    return;
}

t_cpu *cpu_find_by_id (char *id) {
    t_cpu *cpu_ret = malloc(sizeof(t_cpu));
    bool id_equals(void *cpu) {
        t_cpu *cpu_cast = (t_cpu*)cpu;
        return string_equals_ignore_case(cpu_cast->identificador, id);
    }   
    cpu_ret = list_find(cpu_list, id_equals);
    return cpu_ret;
}

void administrar_dispositivos_io () {
    while (1) {
        log_debug(logger, "Espero conexiones de io con fd=%d.", fd_escucha_io);
        pthread_t thread;
        int32_t *socket_io = malloc(sizeof(int32_t));
        *socket_io = esperar_cliente(fd_escucha_io);
        pthread_create(&thread, NULL, (void*)agregar_io, socket_io);
        pthread_detach(thread);
    } 
}

void agregar_io (int32_t *socket) {
    log_debug(logger, "Recibo conexion de io con fd=%d.", *socket);
    char *identificador = recibir_handshake(*socket);
    if (!string_is_empty(identificador)) {
        log_debug(logger, "Handshake IO a Kernel OK.");
    }
    else {
        log_error(logger, "Handshake IO a Kernel error.");
    }
    enviar_handshake(*socket, "KERNEL");
    log_debug(logger, "Agrego io id=%s", identificador);
    t_io *io_agregar = malloc(sizeof(t_io));
    io_agregar->identificador = identificador;
    io_agregar->estado = false;
    io_agregar->socket = *socket;
    io_agregar->proceso_ejecucion = -1;
    if (io_queue_find_by_id(identificador) == NULL) {
        t_io_queue *io_queue_agregar = malloc(sizeof(t_io_queue));
        io_queue_agregar->id = identificador;
        io_queue_agregar->cola_procesos = queue_create();
        sem_wait(&mutex_io);
        list_add(io_queue_list, io_queue_agregar);
        sem_post(&mutex_io);
    }
    sem_wait(&mutex_io);
    list_add(io_list, io_agregar);
    sem_post(&mutex_io);
}

void ejecutar_io_syscall (int32_t pid, char* id_io, int32_t tiempo) {
    t_list *io_buscada = io_filter_by_id(id_io);
    if (list_size(io_buscada) < 1) {
        terminar_proceso(pid);
        return;
    }
    sem_wait(&mutex_execute);
    t_pcb *proceso = pcb_remove_by_pid(cola_exec, pid);
    sem_post(&mutex_execute);

    pasar_blocked(proceso, id_io);

    kernel_to_io *io_enviar = malloc(sizeof(io_enviar));
    io_enviar->pid = pid;
    io_enviar->tiempo_bloqueo = tiempo;
    t_io_queue *io_queue_buscada = io_queue_find_by_id(id_io);
    sem_wait(&mutex_io);
    queue_push(io_queue_buscada->cola_procesos, io_enviar);
    sem_post(&mutex_io);
    enviar_kernel_to_io(id_io);
}

void pasar_blocked(t_pcb* proceso, char* id_io) {
    t_estado_metricas *metricas_exec = list_get(proceso->metricas, EXEC);
    temporal_stop(metricas_exec->MT);
    t_estado estado_anterior = proceso->estado_actual;
    proceso->estado_actual = BLOCKED;
    pasar_por_estado(proceso, BLOCKED, estado_anterior);
    log_info(logger, "## (%d) Bloqueado por IO: %s", proceso->pid, id_io);
    sem_wait(&mutex_blocked);
    list_add(cola_blocked, proceso);
    sem_post(&mutex_blocked);
    sem_post(&sem_corto_plazo);
}

void enviar_kernel_to_io (char* id) {
    t_list *io_buscada = io_filter_by_id(id);
    t_io *io_a_enviar = list_find(io_buscada, io_liberada);
    if (io_a_enviar == NULL) {
        return;
    }
    t_io_queue *io_queue_buscada = io_queue_find_by_id(id);
    sem_wait(&mutex_io);
    kernel_to_io *params = queue_pop(io_queue_buscada->cola_procesos);
    sem_post(&mutex_io);
    io_a_enviar->estado = true;
    io_a_enviar->proceso_ejecucion = params->pid;
    t_buffer *buffer = serializar_kernel_to_io(params);
    t_paquete *paquete = crear_paquete(IO, buffer);
    enviar_paquete(paquete, io_a_enviar->socket);
    pthread_t hilo_respuesta;
    pthread_create(&hilo_respuesta, NULL, (void*)manejar_respuesta_io, io_a_enviar);
    pthread_detach(hilo_respuesta); 
}

void manejar_respuesta_io(t_io *io_espera) {
    t_paquete *paquete = recibir_paquete(io_espera->socket);
    if (paquete->codigo_operacion != IO) {
        log_warning(logger, "(%d) Se desconecto IO", io_espera->proceso_ejecucion);
        terminar_proceso(io_espera->proceso_ejecucion);
        sem_wait(&mutex_io);
        t_io_queue *cola_io_finalizar = io_queue_find_by_id(io_espera->identificador);
        if (!queue_is_empty(cola_io_finalizar->cola_procesos)) {
            t_pcb* proceso = queue_pop(cola_io_finalizar->cola_procesos);
            terminar_proceso(proceso->pid);
        }
        list_remove_element(io_queue_list, cola_io_finalizar);
        queue_destroy(cola_io_finalizar->cola_procesos);
        free(cola_io_finalizar);
        sem_post(&mutex_io);
        sem_wait(&mutex_io);
        list_remove_element(io_list, io_espera);
        sem_post(&mutex_io);
        liberar_conexion(io_espera->socket);
        free(io_espera->identificador);
        free(io_espera);
        destruir_paquete(paquete);
        return;
    }
    
    desbloquear_proceso(io_espera->proceso_ejecucion);

    io_espera->estado = false;
    io_espera->proceso_ejecucion = -1;
    t_io_queue *cola_io = io_queue_find_by_id(io_espera->identificador);
    if (!queue_is_empty(cola_io->cola_procesos)) {
        enviar_kernel_to_io(io_espera->identificador);
    }
    return;
}

void desbloquear_proceso(int32_t pid) {
    sem_wait(&mutex_blocked);
    t_pcb *pcb = pcb_remove_by_pid(cola_blocked, pid);
    sem_post(&mutex_blocked);
    if (pcb == NULL) {
        sem_wait(&mutex_susp_blocked);
        pcb = pcb_remove_by_pid(cola_susp_blocked, pid);
        sem_post(&mutex_susp_blocked);
        pasar_susp_ready(pcb, list_get(pcb->metricas, SUSP_BLOCKED));
    }
    else {
        pasar_ready(pcb, list_get(pcb->metricas, BLOCKED));
    }
    log_info(logger, "## (%d) Finalizo IO y pasa a READY", pid);
}

void pasar_susp_ready(t_pcb *pcb, t_estado_metricas* metricas) {
    temporal_stop(metricas->MT);
    pcb->estado_actual = SUSP_READY;
    pasar_por_estado(pcb, SUSP_READY, metricas->estado);
    sem_wait(&mutex_susp_ready);
    list_add(cola_susp_ready, pcb);
    sem_post(&mutex_susp_ready);
    sem_post(&sem_largo_plazo);
}

t_list *io_filter_by_id (char *id) {
    t_list *io_ret = malloc(sizeof(t_io));
    bool id_equals(void *io) {
        t_io *io_cast = (t_io*)io;
        return string_equals_ignore_case(io_cast->identificador, id);
    }   
    io_ret = list_filter(io_list, id_equals);
    return io_ret;
}

t_io_queue *io_queue_find_by_id (char *id) {
    t_io_queue *io_ret = malloc(sizeof(t_io_queue));
    bool id_equals(void *io) {
        t_io_queue *io_cast = (t_io_queue*)io;
        return string_equals_ignore_case(io_cast->id, id);
    }   
    io_ret = list_find(io_queue_list, id_equals);
    return io_ret;
}

bool io_liberada(void* io) {
    t_io *io_cast = (t_io*) io;
    return !(io_cast->estado);
}

t_buffer *serializar_kernel_to_io (kernel_to_io* data) {
    t_buffer *buffer = buffer_create(sizeof(int32_t) * 2);
    buffer_add_int32(buffer, data->pid);
    buffer_add_int32(buffer, data->tiempo_bloqueo);
    return buffer;
}

void corto_plazo() {
    char* algoritmo_config = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    if (string_equals_ignore_case(algoritmo_config, "FIFO")) {
        planificar_fifo_corto_plazo();
    }
    else if (string_equals_ignore_case(algoritmo_config, "SJF")) {
        planificar_sjf_corto_plazo();
    }
    else if (string_equals_ignore_case(algoritmo_config, "SRT")) {
        planificar_srt_corto_plazo();
    }
    else {
        log_error(logger, "Algoritmo desconocido para planificador de corto plazo.");
    }
    return;
}

void planificar_fifo_corto_plazo() {
    while (1) {
        bool no_hay_proceso_ready = list_is_empty(cola_ready);
        t_cpu *cpu_a_enviar = list_find(cpu_list, find_cpu_libre);
        if (no_hay_proceso_ready || cpu_a_enviar == NULL) {
            log_debug(logger, "Planificador a corto plazo se queda esperando para mandar proceso a exec.");
            sem_wait(&sem_corto_plazo);
            continue;
        }
        t_pcb *proceso_a_ejecutar = list_remove(cola_ready, 0);

        pasar_exec(proceso_a_ejecutar);

        enviar_kernel_to_cpu(cpu_a_enviar->socket_dispatch, proceso_a_ejecutar);
        cpu_a_enviar->estado = true;
        proceso_a_ejecutar->cpu_id = string_duplicate(cpu_a_enviar->identificador);
        
        pthread_t respuesta_cpu; 
        pthread_create(&respuesta_cpu,NULL,(void*)atender_respuesta_cpu,(void*)cpu_a_enviar);
        pthread_detach(respuesta_cpu);   
    }
}

bool find_cpu_libre(void* cpu) {
    t_cpu *cpu_cast = (t_cpu*) cpu;
    return !(cpu_cast->estado);
}

void pasar_exec(t_pcb *pcb) {
    t_estado_metricas *metrica_ready = list_get(pcb->metricas, READY);
    temporal_stop(metrica_ready->MT);
    t_estado estado_anterior = pcb->estado_actual;
    pcb->estado_actual = EXEC;
    pasar_por_estado(pcb, EXEC, estado_anterior);
    sem_wait(&mutex_execute);
    list_add(cola_exec, pcb);
    sem_post(&mutex_execute);
}

void enviar_kernel_to_cpu(int32_t socket, t_pcb *pcb) {
    kernel_to_cpu *proceso = malloc(sizeof(kernel_to_cpu));
    proceso->pc = pcb->pc;
    proceso->pid = pcb->pid;
    t_buffer *buffer = serializar_kernel_to_cpu(proceso);
    t_paquete *paquete = crear_paquete(DISPATCH, buffer);
    enviar_paquete(paquete, socket);
    free(proceso);
}

t_buffer *serializar_kernel_to_cpu(kernel_to_cpu* param) {
    t_buffer *ret = buffer_create(sizeof(int32_t) * 2);
    buffer_add_int32(ret, param->pid);
    buffer_add_int32(ret, param->pc);
    return ret;
}

void atender_respuesta_cpu(t_cpu *cpu) {
    t_paquete *paquete = recibir_paquete(cpu->socket_dispatch);
    if (paquete->codigo_operacion != SYSCALL) {
        log_error(logger, "Codigo de operacion incorrecto. Esperado: %d, Recibido %d", SYSCALL, paquete->codigo_operacion);
    }
    t_syscall *syscall_recibida = deserializar_t_syscall(paquete->buffer);
    destruir_paquete(paquete);
    log_info(logger, "## (%d) - Solicito syscall: (%s)",syscall_recibida->pid, t_instruccion_to_string(syscall_recibida->syscall));
    char** parametros = NULL;
    t_pcb *pcb = pcb_get_by_pid(cola_exec, syscall_recibida->pid);
    pcb->pc = syscall_recibida->pc;
    switch (syscall_recibida->syscall){
        case INIT_PROC:
            parametros = string_split(syscall_recibida->parametros, " ");
            char* archivo = string_duplicate(parametros[0]);
            int32_t tamanio_proceso = atoi(parametros[1]);
            ejecutar_init_proc(syscall_recibida->pid, archivo, tamanio_proceso, cpu);
            string_iterate_lines(parametros, (void*)free);
            free(parametros);
            break;
        case IO_SYSCALL:
            cpu->estado = false;
            parametros = string_split(syscall_recibida->parametros, " ");
            char* dispositivo = string_duplicate(parametros[0]);
            int32_t tiempo = atoi(parametros[1]);
            ejecutar_io_syscall(syscall_recibida->pid, dispositivo, tiempo);
            string_iterate_lines(parametros, (void*)free);
            free(parametros);
            break;
        case DUMP_MEMORY:
            cpu->estado = false;
            ejecutar_dump_memory(syscall_recibida->pid);
            break; 
        case EXIT:
            cpu->estado = false;
            terminar_proceso(syscall_recibida->pid);
        default:
            break;
    }
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

t_syscall *deserializar_t_syscall(t_buffer* buffer) {
    t_syscall* ret = malloc(sizeof(t_syscall));
    ret->syscall = buffer_read_uint8(buffer);
    ret->parametros_length = buffer_read_int32(buffer);
    ret->parametros = buffer_read_string(buffer, &ret->parametros_length);
    ret->pid = buffer_read_int32(buffer);
    ret->pc = buffer_read_int32(buffer);
    return ret;
}

void ejecutar_init_proc(int32_t pid, char* nombre_archivo, int32_t tamanio_proceso, t_cpu* cpu) {
    kernel_to_memoria *archivo_inicial = malloc(sizeof(kernel_to_memoria));
    archivo_inicial->archivo = nombre_archivo;
    archivo_inicial->archivo_length = string_length(archivo_inicial->archivo) + 1;
    archivo_inicial->tamanio = tamanio_proceso;
    list_add(archivos_instruccion, archivo_inicial);
    
    t_pcb *pcb = pcb_get_by_pid(cola_exec, pid);
    enviar_kernel_to_cpu(cpu->socket_dispatch, pcb);

    pthread_t respuesta_cpu; 
    pthread_create(&respuesta_cpu,NULL,(void*)atender_respuesta_cpu,(void*)cpu);
    pthread_detach(respuesta_cpu);

    sem_post(&sem_largo_plazo);
}

void ejecutar_dump_memory(int32_t pid) {
    log_debug(logger, "INICIO - ejecutar_dump_memory - pid:%d", pid);
    sem_wait(&mutex_execute);
    t_pcb* proceso = pcb_remove_by_pid(cola_exec, pid);
    sem_post(&mutex_execute);

    pthread_t verificar_suspension; 
    pthread_create(&verificar_suspension,NULL,(void*)verificar_tiempo_suspension,(void*)proceso);
    pthread_detach(verificar_suspension);  

    pasar_blocked(proceso, "DUMP_MEMORY");

    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer *buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, pid);
    t_paquete *paquete = crear_paquete(DUMP_MEMORY_SYSCALL, buffer);
    enviar_paquete(paquete, fd_conexion_memoria);

    t_paquete* respuesta = recibir_paquete(fd_conexion_memoria);
    if (respuesta->codigo_operacion != DUMP_MEMORY_SYSCALL) {
        log_debug(logger, "ERROR - ejecutar_dump_memory - codop incorrecto - pid:%d", pid);
        terminar_proceso(pid);
        liberar_conexion(fd_conexion_memoria);
        return;
    }
    bool confirmacion = buffer_read_bool(respuesta->buffer);
    if (!confirmacion) {
        log_debug(logger, "ERROR - ejecutar_dump_memory - pid:%d", pid);
        terminar_proceso(pid);
        liberar_conexion(fd_conexion_memoria);
        return;
    }
    liberar_conexion(fd_conexion_memoria);
    log_debug(logger, "FIN - ejecutar_dump_memory - pid:%d", pid);
    desbloquear_proceso(pid);
}

void verificar_tiempo_suspension(t_pcb *proceso) {
    uint64_t tiempo_suspension = config_get_long_value(config, "TIEMPO_SUSPENSION");
    usleep(tiempo_suspension * 1000);
    if (proceso->estado_actual == BLOCKED) {
        suspender_proceso(proceso);
    }
}

void suspender_proceso(t_pcb *proceso) {
    sem_wait(&mutex_blocked);
    list_remove_element(cola_blocked, proceso);
    sem_post(&mutex_blocked);
    pasar_susp_blocked(proceso);

    int32_t fd_conexion_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));
    t_buffer *buffer = buffer_create(sizeof(int32_t));
    buffer_add_int32(buffer, proceso->pid);
    t_paquete *paquete = crear_paquete(SUSPENDER_PROCESO, buffer);
    enviar_paquete(paquete, fd_conexion_memoria);
    liberar_conexion(fd_conexion_memoria);

    sem_post(&sem_largo_plazo);
}

void pasar_susp_blocked(t_pcb *pcb) {
    t_estado_metricas *metrica_blocked = list_get(pcb->metricas, BLOCKED);
    temporal_stop(metrica_blocked->MT);
    t_estado estado_anterior = pcb->estado_actual;
    pcb->estado_actual = SUSP_BLOCKED;
    pasar_por_estado(pcb, SUSP_BLOCKED, estado_anterior);
    sem_wait(&mutex_susp_blocked);
    list_add(cola_exec, pcb);
    sem_post(&mutex_susp_blocked);
}

void planificar_sjf_corto_plazo() {
    alfa = atof(config_get_string_value(config, "ALFA"));
    est_inicial = atoi(config_get_string_value(config, "ESTIMACION_INICIAL"));
    while (1) {
        bool no_hay_proceso_ready = list_is_empty(cola_ready);
        t_cpu *cpu_a_enviar = list_find(cpu_list, find_cpu_libre);
        if (no_hay_proceso_ready || cpu_a_enviar == NULL) {
            log_debug(logger, "Planificador a corto plazo se queda esperando para mandar proceso a exec.");
            sem_wait(&sem_corto_plazo);
            continue;
        }
        list_sort(cola_ready, comparar_rafagas);
        t_pcb *proceso_a_ejecutar = list_remove(cola_ready, 0);

        pasar_exec(proceso_a_ejecutar);

        enviar_kernel_to_cpu(cpu_a_enviar->socket_dispatch, proceso_a_ejecutar);
        cpu_a_enviar->estado = true;
        proceso_a_ejecutar->cpu_id = string_duplicate(cpu_a_enviar->identificador);
        
        pthread_t respuesta_cpu; 
        pthread_create(&respuesta_cpu,NULL,(void*)atender_respuesta_cpu,(void*)cpu_a_enviar);
        pthread_detach(respuesta_cpu);   
    }
}

int32_t estimar_sjf (t_pcb* pcb) {
    t_estado_metricas* metrica_exec = list_get(pcb->metricas, EXEC);
    if (metrica_exec->MT == NULL) {
        return (1-alfa)*pcb->rafaga_estimada;
    }
    else {
        return alfa * temporal_gettime(metrica_exec->MT) + (1-alfa) * pcb->rafaga_estimada;
    }
}

bool comparar_rafagas (void* un_pcb, void* otro_pcb) {
    t_pcb *un_pcb_cast = (t_pcb*) un_pcb;
    t_pcb *otro_pcb_cast = (t_pcb*) otro_pcb;
    un_pcb_cast->rafaga_estimada = estimar_sjf(un_pcb_cast);
    otro_pcb_cast->rafaga_estimada = estimar_sjf(otro_pcb_cast);
    return un_pcb_cast->rafaga_estimada <= otro_pcb_cast->rafaga_estimada;
}

void planificar_srt_corto_plazo() {
    alfa = atof(config_get_string_value(config, "ALFA"));
    est_inicial = atoi(config_get_string_value(config, "ESTIMACION_INICIAL"));
    while (1) {
        bool no_hay_proceso_ready = list_is_empty(cola_ready);
        t_cpu *cpu_a_enviar = list_find(cpu_list, find_cpu_libre);
        if (no_hay_proceso_ready) {
            log_debug(logger, "Planificador a corto plazo se queda esperando para mandar proceso a exec.");
            sem_wait(&sem_corto_plazo);
            continue;
        }

        list_sort(cola_ready, comparar_rafagas);
        t_pcb *proceso_a_ejecutar = list_remove(cola_ready, 0);
        log_debug(logger, "intenta ejecutar %d", proceso_a_ejecutar->pid);
        if (cpu_a_enviar == NULL){
            t_pcb *proceso_con_mayor_rafaga = list_get_maximum(cola_exec, mayor_rafaga);

            proceso_a_ejecutar->rafaga_estimada = estimar_sjf(proceso_a_ejecutar);
            proceso_con_mayor_rafaga->rafaga_estimada = estimar_sjf(proceso_con_mayor_rafaga);
            
            if (proceso_con_mayor_rafaga->rafaga_estimada > proceso_a_ejecutar->rafaga_estimada) {
                log_debug(logger, "entro a interrupcion %d", proceso_a_ejecutar->pid);
                cpu_a_enviar = cpu_find_by_id(proceso_con_mayor_rafaga->cpu_id);
                interrumpir_proceso(proceso_con_mayor_rafaga, cpu_a_enviar);
            }
            else {
                log_debug(logger, "Planificador a corto plazo se queda esperando para mandar proceso a exec.");
                sem_wait(&sem_corto_plazo);
                continue;
            }
        }
        
        pasar_exec(proceso_a_ejecutar);

        enviar_kernel_to_cpu(cpu_a_enviar->socket_dispatch, proceso_a_ejecutar);
        cpu_a_enviar->estado = true;
        proceso_a_ejecutar->cpu_id = string_duplicate(cpu_a_enviar->identificador);
        
        pthread_t respuesta_cpu; 
        pthread_create(&respuesta_cpu,NULL,(void*)atender_respuesta_cpu,(void*)cpu_a_enviar);
        pthread_detach(respuesta_cpu);   
    }
}

void* mayor_rafaga (void* un_pcb, void* otro_pcb) {
    t_pcb *un_pcb_cast = (t_pcb*) un_pcb;
    t_pcb *otro_pcb_cast = (t_pcb*) otro_pcb;
    un_pcb_cast->rafaga_estimada = estimar_sjf(un_pcb_cast);
    otro_pcb_cast->rafaga_estimada = estimar_sjf(otro_pcb_cast);
    return un_pcb_cast->rafaga_estimada >= otro_pcb_cast->rafaga_estimada ? un_pcb_cast : otro_pcb_cast;
}

void interrumpir_proceso(t_pcb* proceso, t_cpu* cpu) {
    t_buffer *buffer = buffer_create(sizeof(int32_t));

    buffer_add_int32(buffer, proceso->pid);
    t_paquete *paquete = crear_paquete(INTERRUPT, buffer);

    enviar_paquete(paquete, cpu->socket_interrupt);
    log_debug(logger, "se envia interrupcion a cpu %d", proceso->pid);

    t_paquete *respuesta = recibir_paquete(cpu->socket_interrupt);
    kernel_to_cpu *proceso_cpu = deserializar_kernel_to_cpu(respuesta->buffer);
    proceso->pc = proceso_cpu->pc;
    pasar_ready(proceso, list_get(proceso->metricas, EXEC));
    log_info(logger, "## (%d) - Desalojado por algoritmo SJF/SRT", proceso->pid);

    destruir_paquete(respuesta);
    free(proceso_cpu);
}

kernel_to_cpu *deserializar_kernel_to_cpu(t_buffer* buffer) {
    kernel_to_cpu *paquete_proceso = malloc(sizeof(kernel_to_cpu));
    paquete_proceso->pid = buffer_read_int32(buffer);
    paquete_proceso->pc = buffer_read_int32(buffer);
    return paquete_proceso;
}