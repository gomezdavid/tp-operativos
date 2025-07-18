#ifndef TABLAS_PAGINAS_H
#define TABLAS_PAGINAS_H

#include<commons/collections/list.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<math.h>

#include "common.h"






//Estructura de marco en memoria
typedef struct t_marco{
    int32_t numero_de_marco;
    bool estado;    //1 libre, 0 ocupado.
    bool modificado; //1 si el marco fue modificado, 0 si no.
} t_marco;


//Entrada de una tabla de paginas.
typedef struct entrada_tabla{
    int32_t marco;                   //solo valido si es entrada de nivel final.
    tabla_paginas* tabla_siguiente;  //puntero a la siguiente tabla, solo valido si es entrada de nivel intermedio, sino es NULL.       
} entrada_tabla;


tabla_paginas* crear_tabla(int32_t nivel);
tablas_por_pid* crear_tabla_raiz(int32_t pid, int32_t tamanio_proceso);
int32_t devolver_marco(tabla_paginas* tabla_actual, int32_t* indices, int32_t nivel, t_metricas *metricas_proceso);
void liberar_espacio_memoria(tablas_por_pid* proceso, int32_t tamanio_proceso, t_metricas* metricas_proceso);
void liberar_tablas_paginas(tabla_paginas* tabla_actual, int32_t nivel, int32_t* tamanio_proceso);




#endif