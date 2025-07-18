#ifndef MANEJO_DISCO_H
#define MANEJO_DISCO_H

#include <utils/config.h>
#include<commons/txt.h>
#include<commons/temporal.h>
#include "common.h"

void dump_memory(tablas_por_pid* contenido, t_metricas *metricas_proceso); 
void suspender_proceso(tablas_por_pid* contenido, t_metricas *metricas_proceso);


#endif