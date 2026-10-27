#include <stdio.h>                   
#include <string.h>                  
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"       
#include "freertos/task.h"           

#include "esp_adc/adc_oneshot.h"     
#include "hal/adc_types.h"           
#include "driver/adc_types_legacy.h" 

#include "esp_log.h"
#include "mqtt_client.h"

#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_pm.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"

#include "i2cdev.h"
#include "bme680.h"


#define CONFIG_BROKER_URL "mqtt://eu.thingsboard.cloud"
static const char *TAG = "RETO 6";
esp_mqtt_client_handle_t client = NULL;

#define SDA GPIO_NUM_21
#define SCL GPIO_NUM_22



static void mqtt_app_start(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void log_error_if_nonzero(const char *message, int error_code);
static volatile int periodo_ms = 1000;

void ejecutar_actualizacion_ota();



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
			
			
			
					
		    ESP_ERROR_CHECK(i2cdev_init());
		
		    bme680_t sensor_bme;
		    memset(&sensor_bme, 0, sizeof(bme680_t));
		
		    ESP_ERROR_CHECK(bme680_init_desc(&sensor_bme, BME680_I2C_ADDR_1, 0, 21, 22));
		
		    ESP_ERROR_CHECK(bme680_init_sensor(&sensor_bme));
		    bme680_set_oversampling_rates(&sensor_bme, BME680_OSR_1X, BME680_OSR_1X, BME680_OSR_1X);
		    bme680_set_filter_size(&sensor_bme, BME680_IIR_SIZE_3);
			esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        while(1){
            char caja1[128];
            char caja2[128];
            char mensaje[256];
            char mensaje2[64];

	        bme680_values_float_t valores;
	        uint32_t duracion;
	
	        bme680_get_measurement_duration(&sensor_bme, &duracion);
	
	        if (bme680_force_measurement(&sensor_bme) == ESP_OK) {
	            vTaskDelay(duracion); 
	
	            if (bme680_get_results_float(&sensor_bme, &valores) == ESP_OK) {
	                printf("Temperatura: %.2f ºC\n", valores.temperature);
	            }
	        }
            int valor_ldr = 0;
            adc_oneshot_read(adc1_handle, ADC1_CHANNEL_6, &valor_ldr);
            
            printf("Valor LDR: %d\n", valor_ldr);

            int valor = (valor_ldr * 10) / 4096;

            printf("LDR Bruto: %d | Digito: %d\n", valor_ldr, valor);
            
            sprintf(caja1,"LDR Bruto: %d ", valor_ldr);
            sprintf(caja2,"Digito: %d", valor);
		            
		    printf("Temp: %.2f ºC | Hum: %.2f %% | Pres: %.2f hPa | Gas: %.2f Ohm\n",valores.temperature, valores.humidity, valores.pressure, valores.gas_resistance);
		
			sprintf(mensaje, "{\"ldr\": %d, \"temperatura\": %.2f, \"humedad\": %.2f, \"presion\": %.2f, \"gas\": %.2f}",valor_ldr, valores.temperature, valores.humidity, valores.pressure, valores.gas_resistance);
		    esp_mqtt_client_publish(client, "v1/devices/me/telemetry", mensaje, 0, 0, 0);


			sprintf(mensaje2,"{\"Valor\": %d}", valor);
            esp_mqtt_client_publish(client, "v1/devices/me/telemetry", mensaje2, 0, 0, 0);

            vTaskDelay(pdMS_TO_TICKS(periodo_ms));
        }
}


static void mqtt_app_start(void)
{
    
        esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.username = "wRC8LOmrLtf2tC86molT",
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
    esp_mqtt_client_subscribe(client, "v1/devices/me/rpc/request/+", 1);
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
        
   case MQTT_EVENT_DATA: {
    char topic[96] = {0};
    char data[96] = {0};
    snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
    snprintf(data, sizeof(data), "%.*s", event->data_len, event->data);
    ESP_LOGI(TAG, "RPC %s -> %s", topic, data);

    if (strstr(data, "setPeriodo") != NULL) {
        char *p = strstr(data, "\"params\"");
        if (p != NULL && (p = strchr(p, ':')) != NULL) {
            int nuevo = atoi(p + 1);
            if (nuevo < 200) nuevo = 200;
            if (nuevo > 60000) nuevo = 60000;
            periodo_ms = nuevo;
        }
    }
    if (strstr(data, "updateFirmware") != NULL) {
        ESP_LOGI(TAG, "¡Orden de actualización OTA recibida desde ThingsBoard!");
        ejecutar_actualizacion_ota();
    }

    if (strncmp(event->topic, "v1/devices/me/rpc/request/", 26) == 0) {
        ejecutar_actualizacion_ota();
    }

    const char *id = strrchr(topic, '/');
    if (id != NULL) {
        char resp_topic[128];
        char resp[32];
        snprintf(resp_topic, sizeof(resp_topic), "v1/devices/me/rpc/response/%s", id + 1);
        snprintf(resp, sizeof(resp), "{\"periodo\": %d}", periodo_ms);
        esp_mqtt_client_publish(client, resp_topic, resp, 0, 1, 0);
    }
    break;
}

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

void ejecutar_actualizacion_ota() {
    esp_http_client_config_t conf_client = {
        .url = "https://github.com/samugc45/sbc_hito6_over_the_air/raw/refs/heads/main/build/app-template.bin",
        .crt_bundle_attach = esp_crt_bundle_attach, // Usa los certificados integrados de ESP-IDF
        .keep_alive_enable = true
    };

    esp_https_ota_config_t conf_ota = {
        .http_config = &conf_client,
    };

    esp_err_t err = esp_https_ota(&conf_ota);

    if(err == ESP_OK){
        ESP_LOGI("OTA","ACTUALIZACION EXITOSA OTA");
        esp_restart();
    } else {
        ESP_LOGE("OTA","FALLO ACTUALIZACION OTA: %s", esp_err_to_name(err));
    }
}