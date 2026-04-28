//====================================================
// File: src/wifi_portal.h
//====================================================
#pragma once
#include <Arduino.h>

enum SystemState
{
    STATE_AP_CONFIG,
    STATE_CONNECTING,
    STATE_SYSTEM_READY,
    STATE_CONNECTED_MAIN
};

extern SystemState currentState;

extern String target_ssid;
extern String target_pass;


void setup_web_server();
void handle_web_server();