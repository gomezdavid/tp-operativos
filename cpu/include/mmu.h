#ifndef MMU_H_
#define MMU_H_

#include <math.h>
#include <stdint.h>
#include <utils/config.h>
#include<utils/paquete.h>
#include<utils/estructuras.h>
#include<commons/temporal.h>

void recibir_t_config_to_cpu();
t_config_to_cpu *deserializar_t_config_to_cpu(t_buffer* buffer);
int32_t numero_pagina(int32_t direccion_logica);
int32_t entrada_nivel_X(int32_t direccion_logica, int32_t nivel);
int32_t desplazamiento(int32_t direccion_logica);
int32_t calcular_direccion_fisica(int32_t direccion_logica, int32_t pid);
int32_t consultar_marco(int32_t direccion_logica, int32_t pid);
int32_t consultar_marco_memoria(int32_t* indices, int32_t pid);
t_buffer* serializar_indices_tablas(int32_t* indices, int32_t pid);

typedef struct entrada_tlb {
    int32_t pagina;
    int32_t marco;
    t_temporal *tiempo_desde_ultimo_uso;
} entrada_tlb;

extern int cant_entradas_tlb;

int32_t buscar_en_tlb(int32_t nro_pagina);
void agregar_a_tlb(entrada_tlb* entrada);

//CACHE DE PAGINAS

typedef struct entrada_cache {
    int32_t pagina;
    void* contenido;
    bool uso;
    bool modificado;
} entrada_cache;

extern t_list* cache_de_paginas;
extern char* algoritmo_cache;
extern int32_t cant_entradas_cache;
extern int32_t retardo_cache;

void inicializar_cache();
entrada_cache* buscar_en_cache(int32_t nro_pagina);
void* leer_de_cache(int32_t direccion_logica, int32_t size, int32_t pid);
void escribir_en_cache(int32_t direccion_logica, int32_t size, int32_t pid, void* contenido);
void agregar_a_cache(int32_t direccion_logica, int32_t pid);
int32_t correr_algoritmo_cache(int32_t pid);
void eliminar_entrada_cache(void *ptr);
void escribir_pagina_cache_a_memoria(entrada_cache* entrada, int32_t pid);
entrada_cache* entrada_cache_get_by_pagina(t_list* entrada_cache_list, int32_t pagina);
void eliminar_entradas_cache(int32_t pid);

#endif