#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "generic_main.h"

static const size_t increment_size = 20;

struct _Array {
  const void * *	data;
  size_t		used;
  size_t		size;
};
typedef struct _Array Array;

Array *
array_create()
{
  Array * array = (Array *)malloc(sizeof(Array));
  array->data = (const void * *)malloc(sizeof(*(array->data)) * increment_size);
  array->size = increment_size;
  array->used = 0;
  return array;
}

void
array_destroy(Array * array)
{
  free(array->data);
  free(array);
}

const void *
array_add(Array * array, const void * data)
{
  if ( array->used == array->size ) {
    array->size += increment_size;
    array->data = (const void * *)realloc(array->data, sizeof(*(array->data)) * array->size);
  }
  array->data[array->used++] = data;
  return data;
}

const void * *
array_data(Array * array)
{
  return array->data;
}

size_t
array_size(Array * array)
{
  return array->used;
}

const void *
array_get(Array * array, size_t index)
{
  if ( index > array->used - 1 ) {
    fprintf(stderr, "Array index %d out-of-bounds of size %d array.\n", index, array->size);
    abort();
  }
  return array->data[index];
}
