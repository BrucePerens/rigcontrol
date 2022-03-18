#pragma once
#include <nvs_flash.h>
#include <nvs.h>

typedef enum param_result {
  ERROR = -2,
  NOT_IN_PARAMETER_TABLE = -1,
  NORMAL = 0,
  SECRET = 1,
  NOT_SET = 2
} param_result_t;

typedef void (*list_params_coroutine_t)(const char *, const char *, const char *, param_result_t);
typedef void (*web_get_coroutine_t)(const char * data, size_t size);

// Call this when a WiFi parameter has been changed by the user.
extern void wifi_restart(void);
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
extern void list_params(list_params_coroutine_t coroutine);

// Get a parameter from non-volatile storage.
extern param_result_t param_get(const char * name, char * buffer, size_t size);
// Set a parameter in non-volatile storage.
extern param_result_t param_set(const char * name, const char * value);
// Erase a parameter from non-volatile storage.
extern param_result_t param_erase(const char * name);

// Get a variable to substitute in the pattern string.
typedef int (*pattern_coroutine_t)(const char * name, char * result, size_t result_size);
// Perform variable substitution on a string.
extern int pattern_string(const char * string, pattern_coroutine_t coroutine, char * buffer, size_t buffer_size);
