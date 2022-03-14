#pragma once
typedef void (*web_get_coroutine_t)(const char * data, size_t size);

extern int web_get(const char *url, char *data, size_t size);
extern int web_get_with_coroutine(const char *url, web_get_coroutine_t coroutine);
