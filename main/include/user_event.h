#ifndef _USER_EVENT_H_
#define _USER_EVENT_H_ 
#include "esp_log.h"   
#include "esp_event.h" 

typedef enum {
    EVENT_NETWORK_INIT = 0,  
    EVENT_NETWORK_GOT_IP,  
    EVENT_NETWORK_UP,
    EVENT_NETWORK_DOWN,  
    EVENT_COMMON_END = 99,    /*!< max common events id */
} event_id_t;

ESP_EVENT_DECLARE_BASE(NETWORK_EVENTS_BASE);  

esp_err_t user_event_handler_register(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *context);
esp_err_t user_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait) ;


#endif