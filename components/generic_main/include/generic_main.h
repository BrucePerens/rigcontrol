#pragma once
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_console.h>
#include <esp_netif.h>

#define CONSTRUCTOR static void __attribute__ ((constructor))

typedef enum param_result {
  PR_ERROR = -2,
  PR_NOT_IN_PARAMETER_TABLE = -1,
  PR_NORMAL = 0,
  PR_SECRET = 1,
  PR_NOT_SET = 2
} param_result_t;

typedef void (*list_params_coroutine_t)(const char *, const char *, const char *, param_result_t);
typedef void (*web_get_coroutine_t)(const char * data, size_t size);

extern nvs_handle_t nvs; // Open handle to non-volatile storage.
// Microseconds since the time was last synchronized.
extern int64_t time_last_synchronized;
extern esp_console_repl_t * repl;
extern esp_netif_t *sta_netif;
extern esp_netif_t *ap_netif;

// Call this when a WiFi parameter has been changed by the user.
extern void wifi_restart(void);
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

// Growing array.
struct _Array;
typedef struct _Array Array;
extern Array * array_create(void);
extern void array_destroy(Array * array);
extern const void * array_add(Array * array, const void * data);
extern const void * * array_data(Array * array);
extern size_t array_size(Array * array);
extern const void * array_get(Array * array, size_t index);
extern void command_register(const esp_console_cmd_t * command);
extern void command_add_registered_to_console(void);
