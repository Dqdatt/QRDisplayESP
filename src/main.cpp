#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <time.h>
#include <LittleFS.h>
#include "ui.h"
#include "wifi_portal.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // BẮT BUỘC để tải ảnh HTTPS từ VietQR
#include "src/extra/libs/png/lv_png.h"

// --- KHAI BÁO BIẾN TOÀN CỤC CHO NTP ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;   // GMT+7 cho Việt Nam
const int   daylightOffset_sec = 0;    

const char* AP_SSID = "ESP32-Config";

TFT_eSPI tft = TFT_eSPI();
static const uint16_t screenWidth  = 240; 
static const uint16_t screenHeight = 320; 
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 20];

SystemState currentState = STATE_AP_CONFIG;

String target_ssid = "";
String target_pass = "";
unsigned long ready_screen_timer = 0;
unsigned long main_screen_timer = 0; // Đếm 10s cho màn hình chính
bool is_payment_shown = false;       // Cờ kiểm tra đã chuyển màn hình chưa

// Hàm tải ảnh HTTPS từ VietQR lưu vào LittleFS
bool downloadVietQR(String url, String save_path) {
    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua kiểm tra chứng chỉ SSL để tải nhanh
    
    HTTPClient http;
    http.begin(client, url); // Mở kết nối HTTPS
    int httpCode = http.GET();
    bool success = false;
    
    if (httpCode == HTTP_CODE_OK) {
        File f = LittleFS.open(save_path, "w");
        if (f) {
            http.writeToStream(&f); // Lưu stream ảnh trực tiếp vào file
            f.close();
            success = true;
        }
    }
    http.end();
    return success;
}

// Hàm kích hoạt quy trình thanh toán
void process_payment() {
    Serial.println("Dang ket noi VietQR...");
    
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;

    http.begin(client, "https://api.vietqr.io/v2/generate");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000); // Sửa lỗi -11 bằng cách tăng timeout

    // Payload đúng theo thông tin link bạn gửi
    String payload = "{"
        "\"accountNo\":\"045704070016757\","
        "\"accountName\":\"BENH VIEN DA KHOA BUU DIEN\","
        "\"acqId\":\"970437\","
        "\"amount\":\"532860\","
        "\"addInfo\":\"MAI VAN CUU CK TIEN THUOC CS1\","
        "\"format\":\"text\","
        "\"template\":\"compact2\""
    "}";

    int httpCode = http.POST(payload);
    
    
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        int start = response.indexOf("\"qrCode\":\"") + 10;
        int end = response.indexOf("\"", start);
        
        if(start > 10 && end > start) {
            String qrText = response.substring(start, end);
            qrText.replace("\\/", "/"); 
            
            // Gọi hiển thị (giữ nguyên workflow 10s của bạn)
            ui_show_payment_qr_screen("532.860", qrText.c_str());
            Serial.println("Hien thi QR thanh cong!");
        }
    } else {
        // Nếu vẫn lỗi -11, in ra để debug
        Serial.printf("Loi API: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

// --- LVGL Driver Màn hình ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    
    // MẸO: Nếu màu màn hình bị ngược (như trong ảnh của bạn nền xám bị thành xanh dương)
    // Hãy thử đổi tham số cuối cùng từ `true` thành `false` ở dòng dưới đây.
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// --- LVGL LittleFS Driver (Hỗ trợ đọc ảnh PNG/BMP) ---
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    char full_path[64];
    sprintf(full_path, "/%s", path);
    File *f = new File();
    *f = LittleFS.open(full_path, mode == LV_FS_MODE_WR ? FILE_WRITE : FILE_READ);
    if(!(*f) || f->isDirectory()) { delete f; return NULL; }
    return (void *)f;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    File *f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File *f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File *f = (File *)file_p;
    SeekMode mode;
    if(whence == LV_FS_SEEK_SET) mode = SeekSet;
    else if(whence == LV_FS_SEEK_CUR) mode = SeekCur;
    else if(whence == LV_FS_SEEK_END) mode = SeekEnd;
    f->seek(pos, mode);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File *f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

void setup() {
    Serial.begin(115200);

    if(!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed!");
    }

    tft.begin();
    tft.setRotation(0); 

    lv_init();
    
    // KÍCH HOẠT BỘ GIẢI MÃ PNG CỦA LVGL (RẤT QUAN TRỌNG)
    // Lưu ý: Đảm bảo #define LV_USE_PNG 1 trong lv_conf.h
    lv_png_init(); 

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'L';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&fs_drv);

    ui_init();

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    setup_web_server();

    ui_show_qr_screen(AP_SSID, "http://192.168.4.1");
}

void loop() {
    lv_timer_handler();
    
    if (currentState == STATE_AP_CONFIG) {
        handle_web_server();
    } 
    else if (currentState == STATE_CONNECTING) {
        WiFi.softAPdisconnect(true);
        ui_show_connecting_screen(target_ssid.c_str());
        
        WiFi.begin(target_ssid.c_str(), target_pass.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(10);
            lv_timer_handler(); 
            delay(490);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            currentState = STATE_SYSTEM_READY;
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            ui_show_system_ready_screen(WiFi.localIP().toString().c_str());
            ui_update_wifi_status(true);
            ready_screen_timer = millis(); 
        } else {
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(AP_SSID);
            currentState = STATE_AP_CONFIG;
            ui_show_qr_screen(AP_SSID, "http://192.168.4.1");
        }
    }
    else if (currentState == STATE_SYSTEM_READY) {
        if (millis() - ready_screen_timer > 3000) {
            currentState = STATE_CONNECTED_MAIN;
            ui_show_main_screen(); 
            
            // Khởi động cờ đếm 10 giây cho màn hình chính
            main_screen_timer = millis();
            is_payment_shown = false;
        }
    }
    else if (currentState == STATE_CONNECTED_MAIN) {
        // Sau 10 giây ở màn hình chính -> Hiển thị QR thanh toán
        if (!is_payment_shown && (millis() - main_screen_timer > 10000)) {
            is_payment_shown = true;
            process_payment(); 
        }
    }

    // Cập nhật đồng hồ mỗi giây
    static unsigned long last_time_update = 0;
    if (millis() - last_time_update > 1000) {
        last_time_update = millis();
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char buf[6];
            strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
            ui_update_time(buf);
        }
    }
    
    delay(5);
}
