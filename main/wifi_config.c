#include "wifi_config.h"
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_system.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "freertos/event_groups.h"
#include <stdbool.h>

#define MAX_REINTENTOS 5

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;
static const char *TAG = "WIFI_CONFIG";


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t raiz_get_handler(httpd_req_t *req);
static esp_err_t guardar_post_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);


// MODO ACCESS POINT

void arrancar_wifi_ap_web(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char nombre_red[32];
    sprintf(nombre_red, "esp%02x%02x", mac[4], mac[5]);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(nombre_red),
            .channel = 1,
            .password = "123456789",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = { .required = false },
        },
    };
    strcpy((char *)wifi_config.ap.ssid, nombre_red);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Modo AP Iniciado. SSID:%s password:123456789", nombre_red);
    
    start_webserver();
}



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // El WiFi STA ha arrancado
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Falló la conexión
        if (s_retry_num < MAX_REINTENTOS) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reintentando conectar al AP...");
        } else {
        
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Fallo al conectar al AP");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado. IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
   
}

    // Pagina web para introducir SSID y contraseña
static esp_err_t raiz_get_handler (httpd_req_t *req) {
    const char* html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'></head>"
                       "<body style='font-family: Arial; text-align: center; margin-top: 50px;'>"
                       "<h2>Access Point Grupo SBC26M13 </h2>"
                       "<form action='/guardar' method='POST'>"
                       "<input type='text' name='ssid' placeholder='SSID' style='margin: 10px; padding: 10px; width: 80%; max-width: 300px;'><br>"
                       "<input type='password' name='password' placeholder='Contraseña' style='margin: 10px; padding: 10px; width: 80%; max-width: 300px;'><br>"
                       "<input type='submit' value='Enviar' style='background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-size: 16px;'>"
                       "</form></body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t guardar_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) { return ESP_FAIL; }
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) { return ESP_FAIL; }
    buf[ret] = '\0';

    char ssid[32] = {0}, password[64] = {0};
    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "password", password, sizeof(password));

    FILE* f = fopen("/spiffs/config.txt", "w");
    if (f != NULL) {
        fprintf(f, "%s\n%s\n", ssid, password);
        fclose(f);
    }

    const char* resp = "<h1>Datos guardados</h1><p>El ESP32 se esta reiniciando para conectarse...</p>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_raiz = { .uri = "/", .method = HTTP_GET, .handler = raiz_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_raiz);
        httpd_uri_t uri_guardar = { .uri = "/guardar", .method = HTTP_POST, .handler = guardar_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_guardar);
        return server;
    }
    return NULL;
}

bool arrancar_wifi_sta(const char* ssid, const char* password)
{
    s_wifi_event_group = xEventGroupCreate();
    s_retry_num = 0;

    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    //  Registrar los eventos a nuestro handler
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    //  Configurar credenciales
    wifi_config_t wifi_config = {
        .sta = {
           
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start(); 

    ESP_LOGI(TAG, "wifi_init_sta terminado. Esperando conexión...");

	    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    
    
    bool exito = false;
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conexión exitosa al SSID:%s", ssid);
        exito = true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Fallo absoluto al conectar al SSID:%s tras %d intentos", ssid, MAX_REINTENTOS);
        
        esp_wifi_stop(); 
        exito = false;
    } else {
        ESP_LOGE(TAG, "Evento inesperado en EventGroup");
    }

    vEventGroupDelete(s_wifi_event_group);
    
    return exito;
}