#include "user_event.h"


#define TAG_EVENT "USER_EVENT_LOOP"

ESP_EVENT_DEFINE_BASE(NETWORK_EVENTS_BASE); 
// ESP_EVENT_DEFINE_BASE(COUNTER_EVENTS_BASE); 

static esp_event_loop_handle_t h_sensor_loop = NULL;
volatile static int register_count = 0; 


esp_err_t user_event_loop_create(void) {
    if (h_sensor_loop) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_event_loop_args_t loop_args = {
        .queue_size = 32,
        .task_name = "sensor_evt",
        .task_stack_size =( 1024 * 6),
#ifdef CONFIG_SENSORS_EVENT_TASK_PRIORITY
        .task_priority = CONFIG_SENSORS_EVENT_TASK_PRIORITY,
#else
        .task_priority = uxTaskPriorityGet(NULL),
#endif
        .task_core_id = 0
    };
    esp_err_t err = esp_event_loop_create(&loop_args, &h_sensor_loop);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_EVENT, "event loop created error");
        return err;
    }
    ESP_LOGI(TAG_EVENT, "event loop created succeed");
    return ESP_OK;
}

esp_err_t user_event_handler_register(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *context) {
    if (h_sensor_loop == NULL) {
        /* creat event loop if not inited*/
        if (ESP_OK != user_event_loop_create()) {
            return ESP_FAIL;
        }
        register_count = 0;
    } 

    esp_err_t ret = esp_event_handler_instance_register_with(h_sensor_loop, event_base, event_id, event_handler, event_handler_arg, context); 

    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EVENT, "register a new handler to event loop failed");
        return ret;
    }
    register_count++;
    ESP_LOGI(TAG_EVENT, "register a new handler to event loop succeed");


    return ret;    
}

esp_err_t user_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait) {
    if (h_sensor_loop == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_event_post_to(h_sensor_loop, event_base, event_id, event_data, event_data_size, ticks_to_wait);
}