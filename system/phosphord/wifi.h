/* phosphord: WiFi from the menu (scan, connect, forget, order) on MiSTer's own WiFi setup.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_WIFI_H
#define PHOSD_WIFI_H

#include <stddef.h>
#include <time.h>

#define WIFI_SSID 33
#define WIFI_MAX 48
#define WIFI_SAVED_MAX 32

typedef struct {
	char iface[16], ssid[WIFI_SSID], ip[48];
	int present, connected, signal, freq, supplicant;
} wifi_status_t;

typedef struct {
	char ssid[WIFI_SSID];
	int signal;            /* dBm, strongest access point with this name */
	int bands;             /* 1 = 2.4 GHz, 2 = 5 GHz */
	int secure, psk, wpa3, enterprise;
} wifi_net_t;

typedef struct { char ssid[WIFI_SSID]; int priority, open; } wifi_saved_t;

void wifi_status(wifi_status_t *s);
int wifi_scan_start(void);                         /* background; 1 if already scanning */
int wifi_scan_results(wifi_net_t *out, int max, int *busy, time_t *at, char *err, size_t errn);
int wifi_saved(wifi_saved_t *out, int max);        /* first = tried first */
/* each of these backs up wpa_supplicant.conf first and makes wpa_supplicant read it again */
int wifi_connect(const char *ssid, const char *password, int hidden, char *err, size_t errn);
int wifi_forget(const char *ssid, char *err, size_t errn);
int wifi_move(const char *ssid, int dir, char *err, size_t errn);
int wifi_undo(char *err, size_t errn);
void wifi_start(void);   /* keeps WiFi up: starts wpa_supplicant once the adapter is there (saved network) */

#endif
