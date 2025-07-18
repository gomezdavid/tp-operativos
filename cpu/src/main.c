#include <main.h>

int32_t fd_dispatch;
int32_t fd_interrupt;
sem_t sem_handshake;

int main(int argc, char* argv[]){
    char* id_cpu = argv[1];
    char log_filename[64];
    sprintf(log_filename, "cpu-%s.log", id_cpu);
    
    sem_init(&sem_handshake, 0, 0);
    interrupcion = false;

    config = crear_config("cpu");
    logger = crear_log(config, log_filename);
    log_debug(logger, "Logger de CPU id:%s creado.", id_cpu);

    cant_entradas_cache = config_get_int_value(config, "ENTRADAS_CACHE");
    cant_entradas_tlb = config_get_int_value(config, "ENTRADAS_TLB");
    algoritmo_cache = config_get_string_value(config, "REEMPLAZO_CACHE");
    retardo_cache = config_get_int_value(config, "RETARDO_CACHE");

    // Conexion a Memoria
    pthread_t thread_memoria;
    pthread_create(&thread_memoria, NULL, (void*)handshake_memoria, (void*)id_cpu);
    pthread_detach(thread_memoria); 

    // Conexion a Kernel , dispatch y interrupt
    pthread_t thread_kernel;
    pthread_create(&thread_kernel, NULL, (void*)handshake_kernel, (void*)id_cpu);
    pthread_detach(thread_kernel);

    getchar();

    log_destroy(logger);
    config_destroy(config);
    return 0;
}

void handshake_memoria(void* arg){
    char* id_cpu = (char*)arg;
    fd_memoria = crear_socket_cliente(config_get_string_value(config, "IP_MEMORIA"), config_get_string_value(config, "PUERTO_MEMORIA"));

    enviar_handshake(fd_memoria, "CPU");
    char* identificador = recibir_handshake(fd_memoria);

    if (string_equals_ignore_case(identificador, "memoria")) {
        log_debug(logger, "Handshake Memoria a CPU-%s OK.",id_cpu);
    }
    else {
        log_error(logger, "Handshake Memoria a CPU-%s error.",id_cpu);
    }
    recibir_t_config_to_cpu();

    free(identificador);
    sem_post(&sem_handshake);
}

void handshake_kernel(void* arg){
    sem_wait(&sem_handshake);
    char* id_cpu = (char*)arg;

    // Conexion dispatch
    fd_dispatch = crear_socket_cliente(config_get_string_value(config,"IP_KERNEL"),config_get_string_value(config,"PUERTO_KERNEL_DISPATCH"));
    enviar_handshake(fd_dispatch, id_cpu);
    char* identificador_dispatch = recibir_handshake(fd_dispatch);

    if (string_equals_ignore_case(identificador_dispatch, "kernel")) {
        log_debug(logger, "Handshake Kernel DISPATCH a CPU-%s OK.", id_cpu);
    } else {
        log_error(logger, "Handshake Kernel DISPATCH a CPU-%s error.", id_cpu);
    }
    free(identificador_dispatch);

    // Conexion interrupt
    fd_interrupt = crear_socket_cliente(config_get_string_value(config,"IP_KERNEL"),config_get_string_value(config,"PUERTO_KERNEL_INTERRUPT"));
    enviar_handshake(fd_interrupt, id_cpu);
    char* identificador_interrupt = recibir_handshake(fd_interrupt);

    if (string_equals_ignore_case(identificador_interrupt, "kernel")) {
        log_debug(logger, "Handshake Kernel INTERRUPT a CPU-%s OK.", id_cpu);
    } else {
        log_error(logger, "Handshake Kernel INTERRUPT a CPU-%s error.", id_cpu);
    }
    free(identificador_interrupt);

    recibir_proceso(NULL);
}

void recibir_proceso(void* _){
    while(1){
        t_paquete* paquete = recibir_paquete(fd_dispatch);

        kernel_to_cpu *paquete_proceso = malloc(sizeof(kernel_to_cpu));
        paquete_proceso->pid = buffer_read_int32(paquete->buffer);
        paquete_proceso->pc = buffer_read_int32(paquete->buffer);

        log_debug(logger, "Recibido PID: %d - PC: %d", paquete_proceso->pid, paquete_proceso->pc);

        destruir_paquete(paquete);

        pthread_t check_interrupt_thread;
        pthread_create(&check_interrupt_thread, NULL, (void*)check_interrupt, paquete_proceso->pid);
        pthread_detach(check_interrupt_thread);

        // FETCH a memoria
        solicitar_instruccion(paquete_proceso);

    }
}

void solicitar_instruccion(kernel_to_cpu* instruccion){
    
    struct_memoria_to_cpu *instruccion_recibida = malloc(sizeof(struct_memoria_to_cpu));

    t_paquete* siguiente_instruccion = malloc(sizeof(t_paquete));


    do{
    // fetch
    t_buffer* buffer = serializar_kernel_to_cpu(instruccion);

    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", instruccion->pid, instruccion->pc);

    t_paquete* paquete = crear_paquete(FETCH,buffer);
    enviar_paquete(paquete,fd_memoria);
    log_debug(logger, "Se envio paquete a memoria");

    // recibo la siguiente instruccion de memoria
    siguiente_instruccion = recibir_paquete(fd_memoria);
    log_debug(logger, "Se recibio paquete de memoria");
    
    instruccion_recibida->instruccion = buffer_read_uint8(siguiente_instruccion->buffer);
    instruccion_recibida->parametros_length  = buffer_read_int32(siguiente_instruccion->buffer);
    instruccion_recibida->parametros = buffer_read_string(siguiente_instruccion->buffer, &instruccion_recibida->parametros_length);
    int32_t pid = instruccion->pid;
    int32_t pc = instruccion->pc;

    switch (instruccion_recibida->instruccion) {
        case NOOP:
            log_debug(logger, "PID: %d - EXECUTE - NOOP", pid);
            log_debug(logger, "PID: %d - NOOP completado", pid);

            break;
        case WRITE: {
            cpu_write *escribir = malloc(sizeof(cpu_write));
            char** parametros = string_split(instruccion_recibida->parametros, " ");
            escribir->datos = parametros[1];
            escribir->datos_length = string_length(parametros[1]); 
            escribir->direccion = atoi(parametros[0]);
            escribir->pid = pid;

            if(cant_entradas_cache != 0) {
                escribir_en_cache(escribir->direccion, escribir->datos_length, pid, (void*)escribir->datos);
                log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: CACHE - Valor: %s", pid, escribir->datos);
                break;
            }

            int32_t direccion_fisica = calcular_direccion_fisica(escribir->direccion, pid);
            escribir->direccion = direccion_fisica;
            
            log_debug(logger, "PID: %d - EXECUTE - WRITE - Dirección: %d, Valor: %s", pid, escribir->direccion, escribir->datos);
            
            t_buffer* buffer = serializar_cpu_write(escribir);

            t_paquete* paquete = crear_paquete(WRITE_MEMORIA, buffer);
            enviar_paquete(paquete, fd_memoria);

            t_paquete* respuesta = recibir_paquete(fd_memoria);
            
            destruir_paquete(respuesta);
            log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", pid, direccion_fisica, escribir->datos);
            
            // free(mensaje);
            // destruir_paquete(respuesta);
            string_iterate_lines(parametros, (void*)free);
            free(parametros);

            break;
        }
        case READ: {
            cpu_read *leer = malloc(sizeof(cpu_read));
            char** parametros = string_split(instruccion_recibida->parametros, " ");
            leer->direccion = atoi(parametros[0]);
            leer->tamanio = atoi(parametros[1]);

            if(cant_entradas_cache != 0) {
                void* lectura = leer_de_cache(leer->direccion, leer->tamanio, pid);
                log_info(logger, "PID: %d - Acción: LEER - Dirección Física: CACHE - Valor: %s", pid, (char*)lectura);
                break;
            }

            int32_t direccion_fisica = calcular_direccion_fisica(leer->direccion, pid);
            leer->direccion = direccion_fisica;
            log_debug(logger, "PID: %d - EXECUTE - READ - Dirección: %d, Tamaño: %d", pid, leer->direccion, leer->tamanio);            
            
            t_buffer* buffer = serializar_cpu_read(leer);

            t_paquete* paquete = crear_paquete(READ_MEMORIA,buffer);
            enviar_paquete(paquete,fd_memoria);
    
            t_paquete* respuesta = recibir_paquete(fd_memoria);
            int32_t length_respuesta = buffer_read_int32(respuesta->buffer);
            char* valor_leido = buffer_read_string(respuesta->buffer, &length_respuesta);
            
            log_info(logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %s", pid, direccion_fisica, valor_leido);
            
            free(valor_leido);
            destruir_paquete(respuesta);
            string_iterate_lines(parametros, (void*)free);
            free(parametros);

            break;
        }
        case GOTO: {
            int32_t nueva_direccion = atoi(instruccion_recibida->parametros);
            instruccion->pc = nueva_direccion;
            break;
        }
        case IO_SYSCALL: {
           log_debug(logger, "PID: %d - EXECUTE - IO - Parámetros: %s", pid, instruccion_recibida->parametros);

            t_syscall *syscall = malloc(sizeof(t_syscall));
            syscall->syscall = IO_SYSCALL;
            syscall->parametros = instruccion_recibida->parametros;
            syscall->parametros_length = strlen(instruccion_recibida->parametros);
            syscall->pid = pid;
            syscall->pc = pc + 1;
            t_buffer* buffer = serializar_t_syscall(syscall);

            t_paquete* paquete = crear_paquete(SYSCALL, buffer);
            enviar_paquete(paquete, fd_dispatch);
            destruir_t_syscall(syscall);
            break;
        }
        case INIT_PROC: {
            log_debug(logger, "PID: %d - EXECUTE - INIT_PROC - Parámetros: %s", pid, instruccion_recibida->parametros);

            t_syscall *syscall = malloc(sizeof(t_syscall));
            syscall->syscall = INIT_PROC;
            syscall->parametros = instruccion_recibida->parametros;
            syscall->parametros_length = strlen(instruccion_recibida->parametros);
            syscall->pid = pid;
            syscall->pc = pc + 1;
            t_buffer* buffer = serializar_t_syscall(syscall);

            t_paquete* paquete = crear_paquete(SYSCALL, buffer);
            enviar_paquete(paquete, fd_dispatch);
            

            destruir_t_syscall(syscall);
            break;
        }
        case DUMP_MEMORY: {
            log_debug(logger, "PID: %d - EXECUTE - DUMP_MEMORY", pid);

            t_syscall *syscall = malloc(sizeof(t_syscall));
            syscall->syscall = DUMP_MEMORY;
            syscall->parametros = instruccion_recibida->parametros;
            syscall->parametros_length = strlen(instruccion_recibida->parametros);
            syscall->pid = pid;
            syscall->pc = pc + 1;
            t_buffer* buffer = serializar_t_syscall(syscall);
            
            t_paquete* paquete = crear_paquete(SYSCALL, buffer);
            enviar_paquete(paquete, fd_dispatch);
            destruir_t_syscall(syscall);
            break;
        }
        case EXIT: {
            log_debug(logger, "PID: %d - EXECUTE - EXIT", pid);

            t_syscall *syscall = malloc(sizeof(t_syscall));
            syscall->syscall = EXIT;
            syscall->parametros = instruccion_recibida->parametros;
            syscall->parametros_length = strlen(instruccion_recibida->parametros);
            syscall->pid = pid;
            syscall->pc = pc + 1;
            t_buffer* buffer = serializar_t_syscall(syscall);

            t_paquete* paquete = crear_paquete(SYSCALL, buffer);
            enviar_paquete(paquete, fd_dispatch);
            destruir_t_syscall(syscall);
            break;
        }
        default:
            log_error(logger, "PID: %d - Instrucción desconocida", pid);
            break;
    }

        if(instruccion_recibida->instruccion != GOTO) { 
            instruccion->pc++;
            pc++;
        }

        log_info(logger, "## PID: %d - Ejecutando: %s - %s", instruccion->pid, t_instruccion_to_string(instruccion_recibida->instruccion), instruccion_recibida->parametros);

        if (interrupcion) {
            interrumpir_proceso(pid, pc);
        }

    } while(instruccion_recibida->instruccion != EXIT && instruccion_recibida->instruccion != INIT_PROC && instruccion_recibida->instruccion != DUMP_MEMORY && instruccion_recibida->instruccion != IO_SYSCALL && !interrupcion);
    
    interrupcion = false;
    free(instruccion_recibida->parametros);
    destruir_paquete(siguiente_instruccion);
}

t_buffer *serializar_cpu_write(cpu_write *data) {
    t_buffer *buffer = buffer_create(sizeof(int32_t) * 3 + data->datos_length);
    buffer_add_int32(buffer, data->direccion);
    buffer_add_int32(buffer, data->datos_length);
    buffer_add_string(buffer, data->datos_length, data->datos);
    buffer_add_int32(buffer, data->pid);
    return buffer;
}

t_buffer *serializar_cpu_read(cpu_read *data) {
    t_buffer *buffer = buffer_create(sizeof(int32_t) * 2);
    buffer_add_int32(buffer, data->direccion);
    buffer_add_int32(buffer, data->tamanio);
    return buffer;
}

t_buffer *serializar_t_syscall(t_syscall *data) {
    t_buffer *buffer = buffer_create(sizeof(int32_t) * 3 + sizeof(uint8_t) + data->parametros_length);
    buffer_add_uint8(buffer, data->syscall);
    buffer_add_int32(buffer, data->parametros_length);
    buffer_add_string(buffer, data->parametros_length, data->parametros);
    buffer_add_int32(buffer, data->pid);
    buffer_add_int32(buffer, data->pc);
    return buffer;
}

void destruir_t_syscall(t_syscall *data) {
    free(data);
}

t_buffer *serializar_kernel_to_cpu(kernel_to_cpu* param) {
    t_buffer *ret = buffer_create(sizeof(int32_t) * 2);
    buffer_add_int32(ret, param->pid);
    buffer_add_int32(ret, param->pc);
    return ret;
}

void check_interrupt(int32_t pid) {
    t_paquete* paquete = recibir_paquete(fd_interrupt);
    if(paquete != NULL) {
        if (paquete->codigo_operacion == INTERRUPT) {
            int32_t pid_interrupcion = buffer_read_int32(paquete->buffer);
            if(pid_interrupcion == pid) {
                log_info(logger, "## Llega interrupción al puerto Interrupt");
                interrupcion = true;
            }
            else {
                log_debug(logger, "PID: %d - Interrupción descartada (para PID: %d)", pid, pid_interrupcion);
            }
        } 
        else {
            log_debug(logger, "Codop incorrecto para interrupcion.");
        }
    }
    destruir_paquete(paquete);
}

void interrumpir_proceso(int32_t pid, int32_t pc) {
    log_debug(logger, "PID: %d - Interrupción recibida del kernel", pid);
    t_buffer* buffer = buffer_create(sizeof(int32_t) * 2);
    buffer_add_int32(buffer, pid);
    buffer_add_int32(buffer, pc);
            
    t_paquete* respuesta = crear_paquete(INTERRUPT, buffer);
    enviar_paquete(respuesta, fd_interrupt);
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

//TLB

void inicializar_tlb() {
    algoritmo_tlb = config_get_string_value(config, "REEMPLAZO_TLB");
    cant_entradas_tlb = config_get_int_value(config, "ENTRADAS_TLB");
    tlb = list_create();
    

}

int32_t buscar_en_tlb(int32_t nro_pagina) {
    entrada_tlb* entrada = entrada_tlb_get_by_pagina(tlb, nro_pagina);
    if (entrada) {
        entrada->tiempo_desde_ultimo_uso = temporal_create();
        return entrada->marco;
    }
    else {
        return -1;
    }
}

void agregar_a_tlb(entrada_tlb* entrada) {
    if(list_size(tlb) == cant_entradas_tlb) {
        correr_algoritmo_tlb();
    }
    entrada->tiempo_desde_ultimo_uso = temporal_create();
    list_add(tlb, entrada);
}

void correr_algoritmo_tlb() {
    if(string_equals_ignore_case(algoritmo_tlb, "LRU")) {
        list_sort(tlb, es_mas_reciente);
    }
    entrada_tlb* entrada = list_remove(tlb, 0);
    eliminar_entrada_tlb((void*)entrada);
}

void eliminar_entrada_tlb(void *ptr) {
    entrada_tlb* entrada = (entrada_tlb*)ptr;
    temporal_destroy(entrada->tiempo_desde_ultimo_uso);
    free(entrada);
}

entrada_tlb* entrada_tlb_get_by_pagina(t_list* entrada_tlb_list, int32_t pagina) {
    bool pagina_equals(void *p_entrada_tlb) {
        entrada_tlb *entrada_tlb_cast = (entrada_tlb*)p_entrada_tlb;
        return entrada_tlb_cast->pagina == pagina;
    }
    return list_find(entrada_tlb_list, pagina_equals);
}

bool es_mas_reciente(void* a, void* b) {
    entrada_tlb* entrada_a = (entrada_tlb*) a;
    entrada_tlb* entrada_b = (entrada_tlb*) b;
    return temporal_gettime(entrada_a->tiempo_desde_ultimo_uso) >= temporal_gettime(entrada_b->tiempo_desde_ultimo_uso);
}

void eliminar_entradas_tlb() {
    list_clean_and_destroy_elements(tlb, eliminar_entrada_tlb);
}

