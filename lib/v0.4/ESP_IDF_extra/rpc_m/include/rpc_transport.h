#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "rpc_def.h"
#include "rpc_function.h"

#include <string.h>

#pragma once

/*
Формат пакета:
    1 - тип сообщения (запрос/стрим/ответ/ошибка)
    2 - порядковый номер отправленного сообщения (при запросе устанавливает отправитель, при ответе не изменяется)
    2..n - текстовое название функции/параметра без терминатора
    n+1 - разделительный 0
    n+2...k - полезная нагрузка в байтовом виде

Порядок приема:
    ожидание валидного пакета с data link layer
    1) принят запрос/стрим
        вызов связанной функции
        отправка ответа (только при запросе)
    2) принят ответ
        функция которая отправила запрос должна принять ответ из очереди
        если в очереди будет неожиданный ответ - пропустить его

Порядок передачи:
    отправить сообщение
    ожидать ответа из очереди
*/

bool  rpc_create(rpc_context_t* rpc_context, bool use_psram);
bool  rpc_delete(rpc_context_t* rpc_context);

bool rpc_start(rpc_context_t* rpc_context);
bool rpc_stop(rpc_context_t* rpc_context);

#if !RPC_USE_FREERTOS
    //~140 us for 1 empty function without phy work
    //~650 us with phy taskYIELD() and ~4ms in wireshark (wifi)
    //~15 ms with phy vTaskDelay(1) and ~20ms in wireshark (wifi)
    void rpc_transport_receive(rpc_context_t* rpc_context, rpc_transport_header_t* rpc_transport_header);
    bool rpc_transport_call_single(rpc_context_t* rpc_context, uint8_t type, uint8_t id, rpc_function_args_t* args);
#endif
