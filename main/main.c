#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc_types_legacy.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "ssd1306.h"


#include "programa.h"
#include "wifi_config.h"
#include "ble_config.h"

#include "esp_partition.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"

/* https://github.com/samugc45/sbc_hito6_over_the_air/raw/refs/heads/main/build/app-template.bin*/

static const char *TAG = "RETO4";
extern const char ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");

void ejecutar_actualizacion_ota(void);

void app_main(void)
{

     
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

 
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true 
    };
    esp_vfs_spiffs_register(&conf);

   

    
      FILE * f = fopen("/spiffs/config.txt", "r");

    if (f == NULL) {
        ESP_LOGW(TAG, "No hay config.txt. ");
            f = fopen("/spiffs/config.txt", "w");
    		fprintf(f,"SBC\nSBCwifi$\n,");
    		fclose(f);
    }
           f = fopen("/spiffs/config.txt", "r");

    	if (f!=NULL) {
        char ssid[32] = {0};
        char password[64] = {0};

        if (fgets(ssid, sizeof(ssid), f) != NULL) ssid[strcspn(ssid, "\r\n")] = 0;
        if (fgets(password, sizeof(password), f) != NULL) password[strcspn(password, "\r\n")] = 0;
        fclose(f); 

        ESP_LOGI(TAG, "Arrancando en modo Cliente para: %s", ssid);
        
        bool conectado = arrancar_wifi_sta(ssid, password); 

        if (conectado) {
            ESP_LOGI(TAG, "WiFi STA conectado con éxito. Arrancando Bluetooth Y Comprobando Actualizaciones");
            arrancar_bluetooth();
            ejecutar_actualizacion_ota();
            programa_main();

        } else {
            ESP_LOGW(TAG, "No se pudo conectar al WiFi guardado. Fallback a modo Access Point...");
            arrancar_wifi_ap_web();
        }
    }
}

void ejecutar_actualizacion_ota() {
    esp_https_client_config_t conf_client = {
        .url = "https://github.com/samugc45/sbc_hito6_over_the_air/raw/refs/heads/main/build/app-template.bin",
        .cert_pem = ca_cert_pem_start,
    };

    esp_https_ota_config_t conf_ota = {
        .http_config = &conf_client,
    };

    esp_err_t err = esp_https_ota(&conf_ota);

    if(err == ESP_OK){
        ESP_LOGI("OTA","ACTUALIZACION EXITOSA OTA");
        esp_restart();
    } else {
        ESP_LOGE("OTA","FALLO ACTUALIZACION OTA");
    }
}
