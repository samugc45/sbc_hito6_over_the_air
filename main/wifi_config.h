#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdbool.h>


void arrancar_wifi_ap_web(void);

bool arrancar_wifi_sta(const char* ssid, const char* password);

#endif