#include "ui.h"
#include "lv_qrcode.h"
#include <string.h>
#include <WiFi.h>
#include "src/extra/libs/gif/lv_gif.h"


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

// Hàm bổ trợ để thêm dấu chấm phân cách hàng nghìn
String formatNumberWithDots(String n) {
    int len = n.length();
    if (len <= 3) return n;
    String res = "";
    int count = 0;
    for (int i = len - 1; i >= 0; i--) {
        res = n[i] + res;
        count++;
        if (count % 3 == 0 && i != 0) {
            res = "." + res;
        }
    }
    return res;
}

// Hàm callback cho bộ đếm 60s
static void payment_timer_cb(lv_timer_t * timer) {
    countdown_val--;
    
    if (countdown_val > 0) {
        if (lbl_countdown != NULL) {
            lv_label_set_text_fmt(lbl_countdown, "MA HET HAN SAU: %ds", countdown_val);
            // Đổi sang màu đỏ rực
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0xFF0000), 0); 
        }
    } else {
        lv_timer_del(timer);
        payment_timer = NULL;
        ui_show_main_screen(); 
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

    // 1. Icon WiFi (Giữ nguyên bên trái)
    header_wifi_icon = lv_label_create(header);
    lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0x888888), 0);
    lv_label_set_text(header_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(header_wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    // 2. Label Ngày Tháng (Đưa ra CHÍNH GIỮA)
    header_wifi_name = lv_label_create(header); // Giữ tên biến cũ để đỡ sửa nhiều file
    lv_obj_set_style_text_color(header_wifi_name, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(header_wifi_name, &lv_font_montserrat_14, 0); 
    lv_label_set_text(header_wifi_name, "---, 00/00"); 
    lv_obj_align(header_wifi_name, LV_ALIGN_CENTER, -15, 0); // <--- Căn giữa Header

    // 3. Đồng hồ (Giữ nguyên bên phải)
    header_clock = lv_label_create(header);
    lv_obj_set_style_text_color(header_clock, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(header_clock, &lv_font_montserrat_14, 0);
    lv_label_set_text(header_clock, "00:00");
    lv_obj_align(header_clock, LV_ALIGN_RIGHT_MID, 0, 0);
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

    // Thay thế Spinner bằng GIF từ thẻ SD
    lv_obj_t * img_gif = lv_gif_create(scr_connecting);
    lv_gif_set_src(img_gif, "L:/wifi.gif"); 
    lv_obj_align(img_gif, LV_ALIGN_CENTER, 0, -20);

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

    ui_update_wifi_status(true); 
    lv_disp_load_scr(scr_system_ready);
}

void ui_show_main_screen() {
    // 1. Dọn dẹp timer đếm ngược QR cũ nếu còn chạy
    if (payment_timer != NULL) {
        lv_timer_del(payment_timer);
        payment_timer = NULL;
    }

    // 2. Kiểm tra và khởi tạo màn hình nếu là lần đầu
    if (scr_main == NULL) {
        scr_main = lv_obj_create(NULL);
    }
    
    // 3. Xóa các object cũ để vẽ lại mới hoàn toàn
    lv_obj_clean(scr_main);
    lv_obj_set_style_bg_color(scr_main, COLOR_BG, 0); // Đảm bảo màu nền chuẩn

    // 4. Vẽ Header
    ui_build_header(scr_main);


    // 6. Vẽ Tên Bệnh viện
    lv_obj_t * lbl_title = lv_label_create(scr_main);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0); // Dùng font size bạn muốn
    lv_label_set_text(lbl_title, "BENH VIEN\nBUU DIEN TPHCM");
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 50);

    // 7. Cập nhật thông tin Header
    if(header_wifi_name != NULL) {
        lv_label_set_text(header_wifi_name, WiFi.SSID().c_str());
    }
    
    // Cập nhật icon WiFi theo trạng thái thực tế thay vì ép true
    ui_update_wifi_status(WiFi.status() == WL_CONNECTED); 

    // 8. Load màn hình
    lv_scr_load(scr_main);
}

// --- HÀM VẼ UI THANH TOÁN (ĐÃ ĐƯỢC LÀM MỚI THEO YÊU CẦU) ---
void ui_show_payment_qr_screen(const char * amount_str, const char * name_str, const char * qr_text) {
    if(scr_payment == NULL) scr_payment = lv_obj_create(NULL);
    lv_obj_clean(scr_payment);
    lv_obj_set_style_bg_color(scr_payment, COLOR_BG, 0);

    // 1. Khởi tạo Header (Đồng hồ, Icon WiFi)
    ui_build_header(scr_payment);

    // Cập nhật thông tin WiFi lên Header ngay lập tức
    if(header_wifi_name != NULL) {
        lv_label_set_text(header_wifi_name, WiFi.SSID().c_str());
    }
    ui_update_wifi_status(WiFi.status() == WL_CONNECTED);

    // 2. Vùng nội dung chính (Container trắng)
    lv_obj_t * main_cont = lv_obj_create(scr_payment);
    lv_obj_set_size(main_cont, 240, 270);
    lv_obj_align(main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);

    // 3. Tiêu đề: QUET MA THANH TOAN
    lv_obj_t * lbl_title = lv_label_create(main_cont);
    lv_label_set_text(lbl_title, "QUET MA THANH TOAN");
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0); 
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);

    // 4. [CẬP NHẬT] Đếm ngược: Nằm ngay dưới Tiêu đề, màu Đỏ 0xFF0000
    countdown_val = 60;
    lbl_countdown = lv_label_create(main_cont);
    lv_label_set_text_fmt(lbl_countdown, "MA HET HAN SAU: %ds", countdown_val);
    lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0xFF0000), 0); // Màu đỏ rực
    lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_14, 0); // Font nhỏ hơn chút cho cân đối
    lv_obj_align(lbl_countdown, LV_ALIGN_TOP_MID, 0, 30); // Cách tiêu đề 20px

    // 5. QR Code: Dịch xuống giữa để nhường chỗ cho phần đếm ngược ở trên
    lv_obj_t * qr = lv_qrcode_create(main_cont, 150, lv_color_hex(0x000000), lv_color_hex(0xffffff));
    lv_qrcode_update(qr, qr_text, strlen(qr_text));
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, 0); // Đẩy xuống một chút

    // 6. Số tiền: Phân cách dấu chấm, màu Đỏ
    String formattedAmount = formatNumberWithDots(String(amount_str));
    lv_obj_t * lbl_amount = lv_label_create(main_cont);
    lv_label_set_text_fmt(lbl_amount, "SO TIEN: %s VND", formattedAmount.c_str());
    lv_obj_set_style_text_color(lbl_amount, lv_color_hex(0xFF0000), 0); 
    lv_obj_set_style_text_font(lbl_amount, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_amount, LV_ALIGN_BOTTOM_MID, 0, -20);

    // 7. Quản lý Timer đếm ngược
    if (payment_timer != NULL) {
        lv_timer_del(payment_timer);
        payment_timer = NULL;
    }
    payment_timer = lv_timer_create(payment_timer_cb, 1000, NULL);

    // 8. Hiển thị màn hình
    lv_scr_load(scr_payment);
}

void ui_update_datetime(const char * time_str, const char * date_str) {
    if(header_clock != NULL) {
        lv_label_set_text(header_clock, time_str);
    }
    if(header_wifi_name != NULL) {
        lv_label_set_text(header_wifi_name, date_str); 
    }
}

void ui_update_wifi_status(bool is_connected) {
    if (header_wifi_icon != NULL) {
        if (is_connected) {
            lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0x00E676), 0); 
            if(header_wifi_name) lv_label_set_text(header_wifi_name, WiFi.SSID().c_str());
        } else {
            lv_obj_set_style_text_color(header_wifi_icon, lv_color_hex(0xFF5252), 0); 
            if(header_wifi_name) lv_label_set_text(header_wifi_name, "No Signal");
        }
    }
}