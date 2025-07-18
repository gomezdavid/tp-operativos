#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#include <utils/config.h>
#include <utils/log.h>
#include <utils/hello.h>
#include <utils/conexion.h>
#include <utils/paquete.h>
#include <pthread.h>
#include <utils/estructuras.h>
#include <commons/collections/dictionary.h>
#include <semaphore.h>

/*Variables Globales*/

extern t_log *logger;
extern t_dictionary* diccionario_procesos;
extern pthread_mutex_t mutex_diccionario;
extern sem_t mutex_memoria;

/*Funciones para el handshake inicial*/

void handshake_kernel();
void handshake_cpu();


typedef struct{
    int32_t tamanio;
    t_list* lista_instrucciones;
    struct t_metricas* lista_metricas;
} t_proceso;

typedef struct t_memoria{
    void* datos;
} t_memoria;

extern t_memoria* memoria_principal;

typedef struct{
    char* PUERTO_ESCUCHA;
    int TAM_MEMORIA;
    int TAM_PAGINA;
    int ENTRADAS_POR_TABLA;
    int CANTIDAD_NIVELES;
    int RETARDO_MEMORIA;
    char* PATH_SWAPFILE;
    char* RETARDO_SWAP;
    char* LOG_LEVEL;
    char* DUMP_PATH;
    char* PATH_INSTRUCCIONES;
    int CANT_CPU;
} t_config_memoria;

extern t_config_memoria *cfg_memoria;

typedef struct t_metricas{
    int32_t pid;
    int32_t accesos_tablas_paginas;
    int32_t instrucciones_solicitadas;
    int32_t bajadas_a_swap;
    int32_t subidas_a_mp;
    int32_t cantidad_lecturas;
    int32_t cantidad_escrituras;
} t_metricas;

extern t_list* metricas_por_procesos;

//Tabla de paginas.
typedef struct tabla_paginas{
    int32_t nivel;     //nivel(1...n)
    struct entrada_tabla* entradas;   //lista de struct entrada_pagina
} tabla_paginas;

typedef struct tablas_por_pid{
    int32_t pid;
    tabla_paginas* tabla_raiz;
    int32_t* marcos;
    int32_t cant_marcos;
} tablas_por_pid;

extern t_list *lista_tablas_por_pid;

void leer_configuracion(t_config *);
bool recibir_consulta_memoria(int32_t, t_paquete*);
int32_t recibir_instrucciones(t_paquete* paquete);
bool verificar_espacio_memoria(int32_t);
kernel_to_memoria* deserializar_kernel_to_memoria(t_buffer*);
void cargar_instrucciones(char *path_archivo, kernel_to_memoria* proceso_recibido);
struct_memoria_to_cpu* parsear_linea(char* linea);
bool enviar_instruccion(int32_t fd_cpu, t_paquete* paquete);
void liberar_lista_instrucciones(t_list* lista);
void liberar_diccionario();
cpu_read *deserializar_cpu_read(t_buffer *data);
cpu_write *deserializar_cpu_write(t_buffer *data);
tablas_por_pid* tablas_por_pid_remove_by_pid(t_list* tablas_por_pid_list, int32_t pid);
tablas_por_pid* tablas_por_pid_get_by_pid(t_list* tablas_por_pid_list, int32_t pid);
t_buffer* serializar_config_to_cpu(t_config_to_cpu* cfg_to_cpu);
void enviar_config_to_cpu(t_config_to_cpu* cfg_to_cpu, int32_t socket);
char *t_instruccion_to_string(t_instruccion instruccion);
void inicializar_metricas(t_metricas* metricas);


void* leer_pagina_completa(int32_t direccion_fisica, int32_t pid, t_metricas *metricas_proceso);
bool actualizar_pagina_completa(int32_t direccion_fisica, void* pagina, int32_t pid, t_metricas *metricas_proceso);
void asignar_marcos(tabla_paginas* tabla_actual, int32_t* tamanio_proceso, int32_t nivel, int32_t* marcos, int32_t* indice_marcos, t_metricas* metricas_proceso);

int obtener_marco_libre();
void liberar_marco(int marco);

void dessuspender_procesos (tablas_por_pid* contenido, int32_t tamanio_proceso, t_metricas *metricas_proceso);
bool tiene_entradas_swap(tablas_por_pid* proceso);
void liberar_proceso_swap(tablas_por_pid* contenido, int32_t tamanio_proceso, t_metricas *metricas_proceso); 

#endif