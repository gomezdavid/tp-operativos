#include <../include/marcos_libres.h>
#include <commons/bitarray.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

t_bitarray* bitmap_marcos;
int cantidad_total_marcos;


// Crea un bitmap con todos los marcos en 0 (libres)
void inicializar_bitmap(int cantidad_frames) {
    cantidad_total_marcos = cantidad_frames;
    int bytes_necesarios = (cantidad_frames + 7) / 8;
    char* buffer = calloc(bytes_necesarios, sizeof(char));
    bitmap_marcos = bitarray_create_with_mode(buffer, bytes_necesarios, LSB_FIRST);
}

// Recorre el bitmap buscando al menos un marco libre
bool hay_marcos_libres() {
    for (int i = 0; i < cantidad_total_marcos; i++) {
        if (!bitarray_test_bit(bitmap_marcos, i)) {
            return true;
        }
    }
    return false;
}


// Obtiene el marco libre para ocuparlo y después devolverlo
int obtener_marco_libre() {
    for (int i = 0; i < cantidad_total_marcos; i++) {
        if (!bitarray_test_bit(bitmap_marcos, i)) {
            bitarray_set_bit(bitmap_marcos, i);
            return i;
        }
    }
    return -1; // No hay marcos libres
}

// Marca como libre un marco usado
void liberar_marco(int marco) {
    if (marco >= 0 && marco < cantidad_total_marcos) {
        bitarray_clean_bit(bitmap_marcos, marco);
    }
}

// Libera de la memoria el bitmap
void destruir_bitmap() {
    free(bitmap_marcos->bitarray);
    bitarray_destroy(bitmap_marcos);
}
