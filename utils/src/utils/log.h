#ifndef LOG_PROP_H
#define LOG_PROP_H
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <stddef.h>
#include <stdlib.h>

t_log* crear_log(t_config* config, char* nombre);

#endif