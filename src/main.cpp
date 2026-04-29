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
#include <WiFiClientSecure.h> 
#include "src/extra/libs/png/lv_png.h"

// --- BIẾN ĐÃ CÓ TRONG wifi_portal.h ---
// Khai báo thực tế (Definition) để linker tìm thấy
String target_ssid = "TICOS-COFFEE-TRET";
String target_pass = "ticoscoffee247";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;   
const int   daylightOffset_sec = 0;    
const char* AP_SSID = "ESP32-Config";

TFT_eSPI tft = TFT_eSPI();

// Sử dụng extern để dùng chung server từ wifi_portal.cpp
extern WebServer server; 

static const uint16_t screenWidth  = 240; 
static const uint16_t screenHeight = 320; 
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 20];

SystemState currentState = STATE_AP_CONFIG;
unsigned long ready_screen_timer = 0;

// Hàm process_payment - GIỮ NGUYÊN TÊN VÀ LOGIC HIỂN THỊ CỦA BẠN
void process_payment(String amount, String addInfo) {
    Serial.println("Dang ket noi VietQR...");
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;

    http.begin(client, "https://api.vietqr.io/v2/generate");
    http.addHeader("Content-Type", "application/json");

    String payload = "{"
        "\"accountNo\":\"045704070016757\","
        "\"accountName\":\"BENH VIEN DA KHOA BUU DIEN\","
        "\"acqId\":\"970437\","
        "\"amount\":\"" + amount + "\","
        "\"addInfo\":\"" + addInfo + "\","
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
            // Gọi hàm UI gốc của bạn để hiển thị QR
            ui_show_payment_qr_screen(amount.c_str(), qrText.c_str());
            Serial.println("Cap nhat QR moi thanh cong!");
        }
    }
    http.end();
}

// Xử lý request từ WebApp
void handle_webapp_update() {
    // Header để trình duyệt không chặn kết nối (CORS)
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("amount") && server.hasArg("name")) {
        String amount = server.arg("amount");
        String name = server.arg("name");
        
        Serial.printf("WebApp Update: %s VND - %s\n", amount.c_str(), name.c_str());
        process_payment(amount, name);
        
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}

// --- GIỮ NGUYÊN 100% DRIVER LVGL ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    char full_path[64]; sprintf(full_path, "/%s", path);
    File *f = new File(); *f = LittleFS.open(full_path, mode == LV_FS_MODE_WR ? FILE_WRITE : FILE_READ);
    if(!(*f) || f->isDirectory()) { delete f; return NULL; } return (void *)f;
}
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) { File *f = (File *)file_p; f->close(); delete f; return LV_FS_RES_OK; }
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) { File *f = (File *)file_p; *br = f->read((uint8_t *)buf, btr); return LV_FS_RES_OK; }
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) { File *f = (File *)file_p; SeekMode mode; if(whence == LV_FS_SEEK_SET) mode = SeekSet; else if(whence == LV_FS_SEEK_CUR) mode = SeekCur; else if(whence == LV_FS_SEEK_END) mode = SeekEnd; f->seek(pos, mode); return LV_FS_RES_OK; }
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) { File *f = (File *)file_p; *pos_p = f->position(); return LV_FS_RES_OK; }

void setup() {
    Serial.begin(115200);
    if(!LittleFS.begin(true)) Serial.println("LittleFS Mount Failed!");
    
    tft.begin();
    tft.setRotation(0); 
    lv_init();
    lv_png_init(); 
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth; disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'L'; fs_drv.open_cb = fs_open; fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read; fs_drv.seek_cb = fs_seek; fs_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&fs_drv);

    ui_init();
    currentState = STATE_CONNECTING; 

    // Đăng ký cổng cập nhật
    server.on("/update", HTTP_GET, handle_webapp_update);
}

void loop() {
    lv_timer_handler();
    server.handleClient(); // Xử lý request WebApp liên tục
    
    if (currentState == STATE_CONNECTING) {
        WiFi.begin(target_ssid.c_str(), target_pass.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500); lv_timer_handler(); attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            currentState = STATE_SYSTEM_READY;
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            ui_show_system_ready_screen(WiFi.localIP().toString().c_str());
            
            Serial.println("");
            Serial.println("====================================");
            Serial.print("IP ESP32: "); Serial.println(WiFi.localIP());
            Serial.println("Dien IP nay vao index.html");
            Serial.println("====================================");
            
            ready_screen_timer = millis(); 
        } else {
            currentState = STATE_AP_CONFIG;
        }
    }
    else if (currentState == STATE_SYSTEM_READY) {
        if (millis() - ready_screen_timer > 3000) {
            currentState = STATE_CONNECTED_MAIN;
            ui_show_main_screen(); 
            server.begin(); 
            Serial.println("WebServer da khoi dong!");
        }
    }
    // LƯU Ý: Đã xóa phần if(STATE_CONNECTED_MAIN) tự động hiện QR sau 10s

    static unsigned long last_time_update = 0;
    if (millis() - last_time_update > 1000) {
        last_time_update = millis();
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char t_buf[6];
            strftime(t_buf, sizeof(t_buf), "%H:%M", &timeinfo);
            ui_update_time(t_buf);
        }
    }
    delay(5);
}