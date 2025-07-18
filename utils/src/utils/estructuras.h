#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include<commons/collections/list.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

typedef enum {
    NOOP,
    WRITE,
    READ,
    GOTO,
    IO_SYSCALL,
    INIT_PROC,
    DUMP_MEMORY,
    EXIT
} t_instruccion;

typedef struct {
    int32_t pid;
    int32_t tiempo_bloqueo;
} kernel_to_io;

typedef struct {
    void* datos;
    int32_t datos_length;
} cpu_to_kernel;

typedef struct {
    t_instruccion syscall;
    char* parametros;
    int32_t parametros_length;
    int32_t pid;
    int32_t pc;
} t_syscall;

typedef struct {
    char* archivo;
    int32_t archivo_lenght;
    int32_t tamanio_proceso;
} init_proc_parameters;

typedef struct {
    int32_t tiempo_bloqueo;
    char* identificador;
    int32_t identificador_length;
} io_parameters;

//hay otra estructura para cpu que no se cual es la correcta, si la de arriba o esta de abajo:

typedef struct{
    int32_t pid;
    int32_t pc;
} kernel_to_cpu;

typedef struct {
    char* archivo;
    int32_t archivo_length;
    int32_t tamanio;
    int32_t pid;
} kernel_to_memoria;

typedef struct {
    int32_t direccion;
    int32_t datos_length;
    char* datos;
    int32_t pid;
} cpu_write;

typedef struct {
    int32_t direccion;
    int32_t tamanio;
    int32_t pid;
} cpu_read;


typedef struct {
    t_instruccion instruccion;
    int32_t parametros_length;
    char* parametros;
} struct_memoria_to_cpu;

typedef struct{
    int32_t tam_paginas;
    int32_t cantidad_niveles;
    int32_t cant_entradas;
} t_config_to_cpu;

#endif // ESTRUCTURAS_H