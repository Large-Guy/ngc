#ifndef NGC_DYN_ARRAY_H
#define NGC_DYN_ARRAY_H

#include <stdlib.h>

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} ngc_array_t;

__attribute__((always_inline)) 
inline ngc_array_t* ngc_array_create(size_t element_size, size_t capacity) {
    ngc_array_t* array = (ngc_array_t*)malloc(sizeof(ngc_array_t));
    array->data = (char*)malloc(element_size * capacity);
    array->capacity = capacity;
    return array;
}

__attribute__((always_inline)) 
inline void ngc_array_destroy(ngc_array_t* array) {
    free(array->data);
    free(array);
}

__attribute__((always_inline)) 
inline void ngc_array_add(ngc_array_t* array, const size_t element_size, char* value) {
    if (array->size >= array->capacity) {
        array->capacity *= 2;
        array->data = (char*)realloc(array->data, array->capacity * element_size);
    }
    for (int i = 0; i < element_size; i++) {
        array->data[array->size + i] = value[i];
    }
}

__attribute__((always_inline)) 
inline void* ngc_array_get(const ngc_array_t* array, const size_t element_size, size_t index) {
    if (index >= array->capacity) {
        return NULL;
    }
    return array->data + index * element_size;
}

__attribute__((always_inline))
inline void ngc_remove(ngc_array_t* array, const size_t element_size, size_t index) {
    if (index >= array->capacity) {
        return;
    }
    for (size_t i = index; i < array->capacity - 1; i++) {
        for (size_t j = 0; j < element_size; j++) {
            array->data[i * element_size] = array->data[i * element_size + j];
        }
    }
    array->size--;
}

__attribute__((always_inline))
inline size_t ngc_array_size(const ngc_array_t* array) {
    return array->size;
}

__attribute__((always_inline))
inline size_t ngc_array_capacity(const ngc_array_t* array) {
    return array->capacity;
}

#endif //NGC_DYN_ARRAY_H
