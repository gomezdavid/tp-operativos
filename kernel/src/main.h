#include <commons/log.h>
#include <utils/hello.h>
#include <utils/conexion.h>
#include <utils/paquete.h>
#include <commons/config.h>
#include <pthread.h>
#include <utils/config.h>
#include <utils/log.h>
#include <commons/temporal.h>
#include <commons/collections/queue.h>
#include <utils/estructuras.h>
#include <semaphore.h>

typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_BLOCKED,
    SUSP_READY,
    EXIT_STATUS
} t_estado;

typedef struct {
    t_estado estado;
    int32_t ME;
    t_temporal *MT;
} t_estado_metricas;

typedef struct {
    int32_t pid;
    int32_t pc;
    t_list* metricas;
    t_estado estado_actual;
    int32_t tamanio_proceso;
    int32_t rafaga_estimada;
    char* cpu_id;
} t_pcb;

typedef struct {
    char* identificador;
    bool estado;
    int32_t socket_dispatch;
    int32_t socket_interrupt;
} t_cpu;

typedef struct {
    char* identificador;
    bool estado;
    int32_t socket;
    int32_t proceso_ejecucion;
} t_io;

typedef struct {
    char* id;
    t_queue *cola_procesos;
} t_io_queue;

void iniciar_modulo();
void finalizar_modulo();
void escucha_io();
void handshake_memoria();
void escucha_cpu();

//largo plazo

void largo_plazo();
void planificar_fifo_largo_plazo();
t_estado_metricas *crear_metrica_estado(t_estado);
void agregar_metricas_estado(t_pcb *);
void pasar_por_estado(t_pcb *, t_estado, t_estado );
t_pcb *crear_proceso();
bool consultar_a_memoria();
void enviar_instrucciones();
t_buffer *serializar_kernel_to_memoria(kernel_to_memoria*);
void pasar_ready(t_pcb *, t_estado_metricas*);
bool terminar_proceso_memoria (int32_t);
void terminar_proceso(int32_t);
t_pcb* pcb_remove_by_pid(t_list*, int32_t);
t_pcb* pcb_get_by_pid(t_list* pcb_list, int32_t pid);
void loggear_metricas_estado(t_pcb*);
char* t_estado_to_string(t_estado);
void planificar_pmcp_largo_plazo();
bool es_mas_chico_que(void *un_pcb, void *otro_pcb);
void dessuspender_proceso(t_pcb* pcb);
bool consultar_a_memoria_by_pcb(t_pcb *pcb);
void pasar_susp_ready(t_pcb *pcb, t_estado_metricas* metricas);

//corto plazo

void administrar_cpus_dispatch();
void agregar_cpu_dispatch(int32_t*);
void administrar_cpus_interrupt();
void agregar_cpu_interrupt(int32_t*);
t_cpu *cpu_find_by_id (char *);
void agregar_io (int32_t *);
void administrar_dispositivos_io();
void ejecutar_io_syscall (int32_t , char* , int32_t );
void enviar_kernel_to_io (char*);
void manejar_respuesta_io(t_io *);
void desbloquear_proceso(int32_t pid);
t_list *io_filter_by_id (char *);
t_io_queue *io_queue_find_by_id (char *);
bool io_liberada(void* );
t_buffer *serializar_kernel_to_io (kernel_to_io* );
void corto_plazo();
void planificar_fifo_corto_plazo();
bool find_cpu_libre(void*);
void pasar_exec(t_pcb *);
void enviar_kernel_to_cpu(int32_t , t_pcb *); 
t_buffer *serializar_kernel_to_cpu(kernel_to_cpu* );
void atender_respuesta_cpu(t_cpu *);
char *t_instruccion_to_string(t_instruccion ); 
t_syscall *deserializar_t_syscall(t_buffer* );
void ejecutar_init_proc(int32_t , char* , int32_t , t_cpu* ); 
void planificar_sjf_corto_plazo();
int32_t estimar_sjf (t_pcb* pcb);
bool comparar_rafagas (void* un_pcb, void* otro_pcb);
void planificar_srt_corto_plazo();
void* mayor_rafaga (void* un_pcb, void* otro_pcb);
void interrumpir_proceso(t_pcb* proceso, t_cpu* cpu);
kernel_to_cpu *deserializar_kernel_to_cpu(t_buffer* buffer); 
void ejecutar_dump_memory(int32_t);
void pasar_blocked(t_pcb* proceso, char* id_io);
void verificar_tiempo_suspension(t_pcb *proceso);
void suspender_proceso(t_pcb *proceso);
void pasar_susp_blocked(t_pcb *pcb);


t_log *logger;
t_config* config;
int32_t pid_counter;
int32_t fd_escucha_cpu;
int32_t fd_escucha_cpu_interrupt;
int32_t fd_escucha_io;
float alfa; 
int32_t est_inicial;
t_list *cola_new;
t_list *cola_ready;
t_list *cola_exec;
t_list *cola_blocked;
t_list *cola_susp_blocked;
t_list *cola_susp_ready;
t_list *archivos_instruccion;
t_list *io_list;
t_list *io_queue_list;
t_list *cpu_list;
bool inicio_modulo;
sem_t sem_largo_plazo;
sem_t mutex_cpus;
sem_t mutex_io;
sem_t mutex_execute;
sem_t sem_corto_plazo;
sem_t mutex_ready;
sem_t mutex_blocked;
sem_t mutex_susp_blocked;
sem_t mutex_susp_ready;