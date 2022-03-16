#pragma once
#include <nvs_flash.h>
#include <nvs.h>

typedef enum nvs_param_result {
  ERROR = -2,
  NOT_IN_PARAMETER_TABLE = -1,
  NORMAL = 0,
  NOT_SET = 1,
  SECRET = 2,
  ERASED = 3
} nvs_param_result_t;

typedef void (*list_nvs_params_coroutine_t)(const char *, const char *, nvs_param_result_t);
typedef void (*web_get_coroutine_t)(const char * data, size_t size);

extern nvs_handle_t nvs; // Open handle to non-volatile storage.
// Microseconds since the time was last synchronized.
extern int64_t time_last_synchronized;
// Convert microseconds to human-readable days, hours, minutes, seconds.
extern void timer_to_human(int64_t, char *, size_t);
// Copy the public IP address to a buffer.
extern int public_ip(char * data, size_t size);
// Get a web page to a buffer.
extern int web_get(const char *url, char *data, size_t size);
// Get a web page, and use a coroutine to deliver the output.
extern int web_get_with_coroutine(const char *url, web_get_coroutine_t coroutine);
// List the parameters stored in non-volatile-storage, using a coroutine
// to deliver the output.
extern void list_nvs_params(nvs_params_coroutine_t);

// Get a parameter from non-volatile storage.
extern nvs_param_result_t get_nvs_param(const char * name, char * buffer, size_t size);
// Set a parameter in non-volatile storage.
extern nvs_param_result_t set_nvs_param(const char * name, const char * value);
// Erase a parameter from non-volatile storage.
extern nvs_param_result_t erase_nvs_param(const char * name);
