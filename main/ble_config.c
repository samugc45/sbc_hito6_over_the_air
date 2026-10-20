#include "ble_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_mac.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

uint8_t ble_addr_type;
static char nombre_red_ble[32];

// 1. FUNCIÓN DEL BUZÓN
static int ble_escritura_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char buf[100] = {0};
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    os_mbuf_copydata(ctxt->om, 0, len, buf); 
    
    ESP_LOGI("BLE", "Mensaje recibido del movil: %s", buf);

    char *coma = strchr(buf, ',');
    if (coma != NULL) {
        *coma = '\0'; 
        char *nuevo_ssid = buf;
        char *nueva_pass = coma + 1;

        FILE* f = fopen("/spiffs/config.txt", "w");
        if (f != NULL) {
            fprintf(f, "%s\n%s\n", nuevo_ssid, nueva_pass);
            fclose(f);
            ESP_LOGI("BLE", "Nuevos datos guardados. Reiniciando...");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_restart();
        }
    } else {
        ESP_LOGE("BLE", "Formato incorrecto. Usa: MiRed,MiClave");
    }
    return 0;
}

// --- 2. LA TABLA GATT 
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x1234), // ID de nuestro servicio
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x5678), 
                .access_cb = ble_escritura_callback,
                .flags = BLE_GATT_CHR_F_WRITE,     
            },
            { 0 }
        }
    },
    { 0 }
};

// --- 3. EL ALTAVOZ 
static void ble_app_on_sync(void) {
    ble_hs_id_infer_auto(0, &ble_addr_type);
    
    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(nombre_red_ble, "esp%02x%02x", mac[4], mac[5]);
    ESP_LOGI("BLE", "Mi nombre Bluetooth sera: %s", nombre_red_ble);
    
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)nombre_red_ble; 
    fields.name_len = strlen(nombre_red_ble); 
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
}

// --- 4. TAREA EN SEGUNDO PLANO 
void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// --- 5. FUNCIÓN PÚBLICA PARA LLAMAR DESDE MAIN
void arrancar_bluetooth(void) {
    ESP_LOGI("BLE", "Arrancando modulo Bluetooth...");
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(ble_host_task);
}