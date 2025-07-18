#ifndef PAQUETE_H
#define PAQUETE_H
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include <errno.h>
#include<commons/log.h>
#include<commons/string.h>
#include "buffer.h"
#include "conexion.h"

typedef enum {
    HANDSHAKE,
    DISPATCH,
    SAVE_INSTRUCTIONS,
    SYSCALL,
    INTERRUPT,
    FETCH,
    IO,
    CONSULTA_MEMORIA_PROCESO,
    TERMINAR_PROCESO, 
    READ_MEMORIA,
    WRITE_MEMORIA,
    DUMP_MEMORY_SYSCALL,
    SUSPENDER_PROCESO,
    DESSUSPENDER_PROCESO,
    CONSULTA_MARCO,
    LEER_PAGINA_COMPLETA,
    ACTUALIZAR_PAGINA_COMPLETA
} t_operacion;

typedef struct {
    t_operacion codigo_operacion;
    t_buffer* buffer;
} t_paquete;

typedef struct {
    char* identificador;
    int32_t id_length;
} t_handshake;

//Crea un paquete a partir de un codigo de operacion y un t_buffer
t_paquete *crear_paquete(u_int8_t operacion, t_buffer *buffer);

//Envia un t_paquete a partir de un socket 
void enviar_paquete(t_paquete *paquete, int32_t socket);

//Recibe un t_paquete y lo devuelve a partir de un socket
t_paquete *recibir_paquete(int32_t socket);

//Libera la memoria asociada a un paquete
void destruir_paquete(t_paquete *paquete);

char* recibir_handshake(int32_t fd_conexion);

void enviar_handshake(int32_t fd_conexion, char* identificador);


#endif