#include "ui.h"
#include "lv_qrcode.h"
#include <string.h>
#include "logo.h" 
#include <WiFi.h>

// Tự động bọc mảng uint16_t của bạn thành định dạng ảnh chuẩn của LVGL
const lv_img_dsc_t hospital_logo_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = LOGO_WIDTH,  // 180 (lấy từ logo.h)
        .h = LOGO_HEIGHT, // 61 (lấy từ logo.h)
    },
    .data_size = sizeof(hospital_logo),
    .data = (const uint8_t *)hospital_logo
};

lv_obj_t * scr_qr;
lv_obj_t * scr_connecting;
lv_obj_t * scr_system_ready;
lv_obj_t * scr_main;

lv_obj_t * scr_payment;
static lv_obj_t * lbl_countdown = NULL;
static lv_timer_t * payment_timer = NULL;
static int countdown_val = 60;

lv_obj_t * header_wifi_icon;
lv_obj_t * header_wifi_name;
lv_obj_t * header_clock;

// --- Bảng màu Light Mode ---
#define COLOR_BG        lv_color_hex(0xF4F7FA) // Xám trắng nhạt
#define COLOR_TEXT      lv_color_hex(0x1A1A1A) // Đen than
#define COLOR_ACCENT    lv_color_hex(0x2196F3) // Xanh Blue
#define COLOR_CARD      lv_color_hex(0xFFFFFF) // Trắng

// Hàm callback cho bộ đếm 60s
static void payment_timer_cb(lv_timer_t * timer) {
    if (countdown_val > 0) {
        countdown_val--;
        if (lbl_countdown != NULL) {
            lv_label_set_text_fmt(lbl_countdown, "(%ds)", countdown_val);
        }
    } else {
        if (lbl_countdown != NULL) {
            lv_label_set_text(lbl_countdown, "(Het han)");
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0x888888), 0); // Đổi sang màu xám
        }
        lv_timer_pause(timer); // Dừng bộ đếm
    }
}

// Hàm tạo Header dùng chung
void ui_build_header(lv_obj_t * parent) {
    lv_obj_t * header = lv_obj_create(parent);
    lv_obj_set_size(header, 240, 40);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_CARD, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    header_wifi_icon = lv_label_create(header);
    lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0x888888), 0); // Mặc định xám
    lv_label_set_text(header_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(header_wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    // Khởi tạo label tên WiFi cạnh icon
    header_wifi_name = lv_label_create(header);
    lv_obj_set_style_text_color(header_wifi_name, COLOR_TEXT, 0);
    // Sử dụng font Montserrat Bold nếu có, hoặc set style đậm
    lv_obj_set_style_text_font(header_wifi_name, &lv_font_montserrat_14, 0); 
    lv_label_set_text(header_wifi_name, ""); 
    lv_obj_align_to(header_wifi_name, header_wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    header_clock = lv_label_create(header);
    lv_obj_set_style_text_color(header_clock, COLOR_TEXT, 0);
    lv_label_set_text(header_clock, "--:--");
    lv_obj_align(header_clock, LV_ALIGN_RIGHT_MID, -10, 0);
}

void ui_init() {
    scr_qr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_qr, COLOR_BG, 0);

    scr_connecting = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_connecting, COLOR_BG, 0);

    scr_system_ready = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_system_ready, COLOR_BG, 0);

    scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_main, COLOR_BG, 0);

    // Khởi tạo màn hình thanh toán
    scr_payment = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_payment, COLOR_BG, 0);
}

void ui_show_qr_screen(const char * ssid, const char * ip_url) {
    lv_obj_clean(scr_qr);
    ui_build_header(scr_qr);

    lv_obj_t * title = lv_label_create(scr_qr);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_label_set_text(title, "Web Configuration");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_color_t qr_bg = lv_color_hex(0xffffff);
    lv_color_t qr_fg = lv_color_hex(0x000000);
    lv_obj_t * qr = lv_qrcode_create(scr_qr, 140, qr_fg, qr_bg);
    lv_qrcode_update(qr, ip_url, strlen(ip_url));
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * desc = lv_label_create(scr_qr);
    lv_obj_set_style_text_color(desc, lv_color_hex(0x666666), 0);
    lv_label_set_text_fmt(desc, "1. Connect WiFi: %s\n2. Scan QR to config", ssid);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(desc, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_disp_load_scr(scr_qr);
}

void ui_show_connecting_screen(const char * ssid) {
    lv_obj_clean(scr_connecting);
    ui_build_header(scr_connecting);

    lv_obj_t * spinner = lv_spinner_create(scr_connecting, 1000, 60);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_set_style_arc_color(spinner, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_INDICATOR);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t * lbl = lv_label_create(scr_connecting);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_label_set_text_fmt(lbl, "Connecting to\n%s...", ssid);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 45);

    lv_disp_load_scr(scr_connecting);
}

void ui_show_system_ready_screen(const char * ip_address) {
    lv_obj_clean(scr_system_ready);
    ui_build_header(scr_system_ready);

    lv_obj_t * icon = lv_label_create(scr_system_ready);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_label_set_text(icon, LV_SYMBOL_OK);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t * lbl = lv_label_create(scr_system_ready);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_label_set_text(lbl, "System Ready");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * ip_lbl = lv_label_create(scr_system_ready);
    lv_obj_set_style_text_color(ip_lbl, lv_color_hex(0x666666), 0);
    lv_label_set_text_fmt(ip_lbl, "IP: %s", ip_address);
    lv_obj_align(ip_lbl, LV_ALIGN_CENTER, 0, 40);

    // Cập nhật lại màu xanh cho icon WiFi trên header mới tạo
    ui_update_wifi_status(true); 
    lv_disp_load_scr(scr_system_ready);
}

void ui_show_main_screen() {
    lv_obj_clean(scr_main);
    ui_build_header(scr_main);

    // Hiển thị trực tiếp từ logo.h (đã bọc struct tự động ở trên)
    lv_obj_t * img_logo = lv_img_create(scr_main);
    lv_img_set_src(img_logo, &hospital_logo_dsc); 
    lv_obj_align(img_logo, LV_ALIGN_CENTER, 0, -30);

    // Hiển thị text không dấu để tránh lỗi font
    lv_obj_t * lbl_title = lv_label_create(scr_main);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT, 0);
    lv_label_set_text(lbl_title, "BENH VIEN\nBUU DIEN TPHCM");
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 50);

    if(header_wifi_name != NULL) {
        lv_label_set_text(header_wifi_name, WiFi.SSID().c_str());
    }

    // Cập nhật lại màu xanh cho icon WiFi trên header mới tạo
    ui_update_wifi_status(true); 
    lv_disp_load_scr(scr_main);
}

// --- HÀM VẼ UI THANH TOÁN (MỚI) ---
void ui_show_payment_qr_screen(const char * amount_str, const char * qr_text) {
    if(scr_payment == NULL) scr_payment = lv_obj_create(NULL);
    lv_obj_clean(scr_payment);
    lv_obj_set_style_bg_color(scr_payment, COLOR_BG, 0);

    ui_build_header(scr_payment);

    lv_obj_t * main_cont = lv_obj_create(scr_payment);
    lv_obj_set_size(main_cont, 240, 260);
    lv_obj_align(main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);

    // QR Code
    lv_obj_t * qr = lv_qrcode_create(main_cont, 180, lv_color_hex(0x000000), lv_color_hex(0xffffff));
    lv_qrcode_update(qr, qr_text, strlen(qr_text));
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 10);

    // Label thông tin
    lv_obj_t * lbl = lv_label_create(main_cont);
    lv_label_set_text_fmt(lbl, "SO TIEN: %s VND\nBV DA KHOA BUU DIEN", amount_str);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_scr_load(scr_payment);
}

void ui_update_time(const char * time_str) {
    if(header_clock != NULL) {
        lv_label_set_text(header_clock, time_str);
    }
}

void ui_update_wifi_status(bool is_connected) {
    if (header_wifi_icon != NULL) {
        if (is_connected) {
            lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0x00E676), 0); 
            // Khi kết nối thành công, hiện tên WiFi
            if(header_wifi_name) lv_label_set_text(header_wifi_name, WiFi.SSID().c_str());
        } else {
            lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0xFF5252), 0); 
            if(header_wifi_name) lv_label_set_text(header_wifi_name, "No Signal");
        }
    }
}


