#include "conexion.h"
extern t_log *logger;


int32_t crear_socket_cliente(char *ip, char* port) {
    int32_t err;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err = getaddrinfo(ip, port, &hints, &server_info);
    if(err != 0) {
        log_error(logger, "Error al conseguir información de la dirección, codigo de error: %s", gai_strerror(err));
        abort();
    }
    int fd_conexion = socket(server_info->ai_family,
                            server_info->ai_socktype,
                            server_info->ai_protocol);

    if(fd_conexion == -1) {
        log_error(logger, "Error al crear el socket: %s", strerror(errno));
        abort();
    }

	err = connect(fd_conexion, server_info->ai_addr, server_info->ai_addrlen);

    if (err == -1) {
        log_error(logger, "Error al crear la conexion: %s", strerror(errno));
        abort();
    } 
    log_debug(logger, "Conexion creada con exito.");
    freeaddrinfo(server_info);
	return fd_conexion;
}

int32_t iniciar_servidor(char* puerto) {
    int socket_servidor;
    int err;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    err = getaddrinfo(NULL, puerto, &hints, &servinfo);
    if(err != 0) {
        log_error(logger, "Error al conseguir información de la dirección, codigo de error: %s", gai_strerror(err));
        abort();
    }

    // Creamos el socket de escucha del servidor
    socket_servidor = socket(servinfo->ai_family,
                            servinfo->ai_socktype,
                            servinfo->ai_protocol);
    if(socket_servidor == -1) {
        log_error(logger, "Error al crear el socket: %s", strerror(errno));
        abort();
    }
    // Asociamos el socket a un puerto
    err = setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));
    if (err == -1) {
        log_error(logger, "Error al liberar el puerto: %s", strerror(errno));
        abort();
    } 
    // Escuchamos las conexiones entrantes
    err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    if (err == -1) {
        log_error(logger, "Error al bindear el puerto al socket: %s", strerror(errno));
        abort();
    } 

    err = listen(socket_servidor, SOMAXCONN);
    freeaddrinfo(servinfo);
    log_trace(logger, "Listo para escuchar a mi cliente");

    return socket_servidor;
}

int32_t esperar_cliente(int32_t socket_servidor)
{
        // Aceptamos un nuevo cliente
        int32_t socket_cliente;
        socket_cliente = accept(socket_servidor, NULL, NULL);
        if(socket_cliente == -1) {
            log_warning(logger, "Error al aceptar a un cliente: %s", strerror(errno));
        }
        log_debug(logger, "Se conecto un cliente.");

        return socket_cliente;
}

uint8_t recibir_operacion(int32_t socket_cliente)
{
    uint8_t cod_op;
    int32_t err;
    err = recv(socket_cliente, &cod_op, sizeof(uint8_t), 0);
    if (err == -1) {
        log_error(logger, "Error al recibir codigo de operación: %s", strerror(errno));
        //abort();
    }
    return cod_op;
}

void liberar_conexion(int32_t socket_cliente)
{
	close(socket_cliente);
}