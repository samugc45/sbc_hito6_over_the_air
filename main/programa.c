#include <stdio.h>                   
#include <string.h>                  
#include "freertos/FreeRTOS.h"       
#include "freertos/task.h"           

#include "esp_adc/adc_oneshot.h"     
#include "hal/adc_types.h"           
#include "driver/adc_types_legacy.h" 

#include "esp_log.h"
#include "mqtt_client.h"


#define CONFIG_BROKER_URL "mqtt://eu.thingsboard.cloud"
static const char *TAG = "RETO 4";
esp_mqtt_client_handle_t client = NULL;



static void mqtt_app_start(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void log_error_if_nonzero(const char *message, int error_code);




void programa_main(void)
{


        mqtt_app_start();
            adc_oneshot_unit_handle_t adc1_handle;
            adc_oneshot_unit_init_cfg_t init_config1 = {
                .unit_id = ADC_UNIT_1,
            };
            
            adc_oneshot_new_unit(&init_config1, &adc1_handle);
            
            adc_oneshot_chan_cfg_t config = {
                .bitwidth = ADC_BITWIDTH_12,
                .atten = ADC_ATTEN_DB_12,
            };

            adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config);


        while(1){
            char caja1[128];
            char caja2[128];
            char mensaje[64];
            char mensaje2[64];


            int valor_ldr = 0;
            adc_oneshot_read(adc1_handle, ADC1_CHANNEL_6, &valor_ldr);
            
            printf("Valor LDR: %d\n", valor_ldr);

            int valor = (valor_ldr * 10) / 4096;

            printf("LDR Bruto: %d | Digito: %d\n", valor_ldr, valor);
            
            sprintf(caja1,"LDR Bruto: %d ", valor_ldr);
            sprintf(caja2,"Digito: %d", valor);
            
            sprintf(mensaje,"{\"ldr\": %d}", valor_ldr);
            esp_mqtt_client_publish(client, "v1/devices/me/telemetry", mensaje, 0, 0, 0);


			sprintf(mensaje2,"{\"Valor\": %d}", valor);
            esp_mqtt_client_publish(client, "v1/devices/me/telemetry", mensaje2, 0, 0, 0);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
}


static void mqtt_app_start(void)
{
    
        esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.username = "sbOtU5S3MGrksYmRK0yQ",
        };
         client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}