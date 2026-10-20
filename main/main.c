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

static const char *TAG = "RETO4";

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
            ESP_LOGI(TAG, "WiFi STA conectado con éxito. Arrancando Bluetooth...");
            arrancar_bluetooth();
            programa_main(); 

        } else {
            ESP_LOGW(TAG, "No se pudo conectar al WiFi guardado. Fallback a modo Access Point...");
            arrancar_wifi_ap_web();
        }
    }
}