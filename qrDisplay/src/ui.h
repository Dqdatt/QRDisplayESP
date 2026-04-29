#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include <Arduino.h>

// Các màn hình
extern lv_obj_t * scr_qr;
extern lv_obj_t * scr_connecting;
extern lv_obj_t * scr_system_ready;
extern lv_obj_t * scr_main;
extern lv_obj_t * scr_payment;

// Các thành phần Header
extern lv_obj_t * header_wifi_icon;
extern lv_obj_t * header_clock;

// Các hàm khởi tạo và điều khiển UI
void ui_init();
void ui_build_header(lv_obj_t * parent);
void ui_show_qr_screen(const char * ssid, const char * ip_url);
void ui_show_connecting_screen(const char * ssid);
void ui_show_system_ready_screen(const char * ip_address);
void ui_show_main_screen();
void ui_show_payment_qr_screen(const char * amount_str, const char * name_str, const char * qr_text);
void ui_update_datetime(const char * time_str, const char * date_str);
void ui_update_wifi_status(bool is_connected);
extern void process_payment(String json_data);

#endif