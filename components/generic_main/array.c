#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "generic_main.h"

static const size_t increment_size = 20;

struct _GM_Array {
  const void * *	data;
  size_t		used;
  size_t		size;
};
typedef struct _GM_Array GM_Array;

GM_Array *
gm_array_create()
{
  GM_Array * array = (GM_Array *)malloc(sizeof(GM_Array));
  array->data = (const void * *)malloc(sizeof(*(array->data)) * increment_size);
  array->size = increment_size;
  array->used = 0;
  return array;
}

void
gm_array_destroy(GM_Array * array)
{
  free(array->data);
  free(array);
}

const void *
gm_array_add(GM_Array * array, const void * data)
{
  if ( array->used == array->size ) {
    array->size += increment_size;
    array->data = (const void * *)realloc(array->data, sizeof(*(array->data)) * array->size);
  }
  array->data[array->used++] = data;
  return data;
}

const void * *
gm_array_data(GM_Array * array)
{
  return array->data;
}

size_t
gm_array_size(GM_Array * array)
{
  return array->used;
}

const void *
gm_array_get(GM_Array * array, size_t index)
{
  if ( index > array->used - 1 ) {
    fprintf(stderr, "GM_Array index %d out-of-bounds of size %d array.\n", index, array->size);
    abort();
  }
  return array->data[index];
}
