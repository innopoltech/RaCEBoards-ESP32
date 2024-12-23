#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "rpc_def.h"

#include <string.h>

#pragma once

bool rpc_function_init(void);
bool rpc_function_deinit(void);

int16_t rpc_function_find(char* name);
function_t rpc_function_ptr_from_indx(int16_t indx);

bool rpc_function_add(char* name, void* function_ptr);
bool rpc_function_delete(char* name);
