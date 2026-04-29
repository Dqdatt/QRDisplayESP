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
#include <ArduinoJson.h>
#include "src/extra/libs/gif/lv_gif.h"
    
// --- KHAI BÁO BIẾN TOÀN CỤC CHO NTP ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;   // GMT+7 cho Việt Nam
const int   daylightOffset_sec = 0;    

String pendingJson = "";
bool hasNewPayment = false;
bool is_payment_shown = false;

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
unsigned long main_screen_timer = 0; 

void process_payment(String jsonBody);

// Hàm tải ảnh HTTPS từ VietQR lưu vào LittleFS
bool downloadVietQR(String url, String save_path) {
    WiFiClientSecure client;
    client.setInsecure(); 
    
    HTTPClient http;
    http.begin(client, url); 
    int httpCode = http.GET();
    bool success = false;
    
    if (httpCode == HTTP_CODE_OK) {
        File f = LittleFS.open(save_path, "w");
        if (f) {
            http.writeToStream(&f); 
            f.close();
            success = true;
        }
    }
    http.end();
    return success;
}

// =====================================================
// PROCESS PAYMENT (Đã cập nhật để đọc thẳng mã QR từ Webapp)
// =====================================================
void process_payment(String jsonBody) {

    Serial.println("Nhan yeu cau thanh toan tu Web...");
    Serial.println(jsonBody);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonBody);

    if (error) {
        Serial.print("Loi parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    String amountStr = doc["amount"].as<String>();
    const char* addInfo = doc["addInfo"];
    const char* accountNo = doc["accountNo"];
    const char* accountName = doc["accountName"];
    const char* acqId = doc["acqId"];

    String cleanAddInfo = String(addInfo);
    cleanAddInfo.replace("%20", " ");
    
    // Đọc trường qrText chứa chuỗi QR tĩnh từ Webapp gửi xuống
    const char* qrText = doc["qrText"]; 

    // Nếu webapp CÓ gửi thẳng chuỗi QR xuống -> Hiển thị luôn, KHÔNG cần ESP gọi API nữa
    if (qrText != nullptr && strlen(qrText) > 0) {
        is_payment_shown = true;
        ui_show_payment_qr_screen(amountStr.c_str(), cleanAddInfo.c_str(), qrText);
        Serial.println("HIEN THI QR TRUC TIEP TU WEBAPP THANH CONG");
        return; 
    }

    // --- BÊN DƯỚI LÀ LOGIC CŨ LÀM FALLBACK (Phòng trường hợp webapp không gửi qrText) ---
    Serial.println("Dang ket noi VietQR (Fallback)...");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    http.begin(client, "https://api.vietqr.io/v2/generate");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String payload =
        "{"
        "\"accountNo\":\"" + String(accountNo) + "\","
        "\"accountName\":\"" + String(accountName) + "\","
        "\"acqId\":\"" + String(acqId) + "\","
        "\"amount\":\"" + amountStr + "\","
        "\"addInfo\":\"" + cleanAddInfo + "\","
        "\"format\":\"text\","
        "\"template\":\"compact2\""
        "}";

    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_OK) {

        String response = http.getString();

        int start = response.indexOf("\"qrCode\":\"") + 10;
        int end = response.indexOf("\"", start);

        if (start > 10 && end > start) {

            String qrStr = response.substring(start, end);
            qrStr.replace("\\/", "/");

            is_payment_shown = true;
            ui_show_payment_qr_screen(amountStr.c_str(), addInfo, qrStr.c_str());

            Serial.println("HIEN THI QR QUA API THANH CONG");
        }

    } else {

        Serial.print("LOI API: ");
        Serial.println(http.errorToString(httpCode));
    }

    http.end();
}

// --- LVGL Driver Màn hình ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// --- LVGL LittleFS Driver ---
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
    lv_png_init(); 

    // 1. Load WiFi đã lưu trước khi khởi tạo UI
    load_wifi_credentials();

    // 2. Khởi tạo Driver hiển thị
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Khởi tạo File System cho LVGL (để hiện logo)
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'L';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&fs_drv);

    ui_init(); // Khởi tạo các màn hình

    // 4. Quyết định màn hình khởi đầu
    if (target_ssid != "" && target_ssid != "NULL") {
        // Có WiFi cũ: Thử kết nối
        WiFi.mode(WIFI_STA); 
        WiFi.begin(target_ssid.c_str(), target_pass.c_str());
        currentState = STATE_CONNECTING;
        ui_show_connecting_screen(target_ssid.c_str());
    } else {
        // Không có WiFi: Mở trạm phát để cấu hình
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID);
        currentState = STATE_AP_CONFIG;
        ui_show_qr_screen(AP_SSID, "http://192.168.4.1");
    }
    
    // 5. Chạy Web Server
    setup_web_server();

    // XÓA DÒNG NÀY: ui_show_qr_screen(AP_SSID, "http://192.168.4.1");
    // Vì nó đã được xử lý trong khối if/else ở trên rồi.
}
void loop() {
    lv_timer_handler();
    
    // [QUAN TRỌNG]: Đưa handle_web_server() ra ngoài để ESP luôn lắng nghe request 
    // từ Webapp ở chế độ chạy ngầm (kể cả khi ở Màn hình chính)
    handle_web_server();
    
    if (currentState == STATE_AP_CONFIG) {
        // Không làm gì thêm, web server đã xử lý
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
            Serial.print(WiFi.localIP().toString().c_str());
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
            
            main_screen_timer = millis();
            is_payment_shown = false;
        }
    }
    else if (currentState == STATE_CONNECTED_MAIN) {
        // [QUAN TRỌNG]: Bắt tín hiệu có request từ Webapp
        if (hasNewPayment) {
            hasNewPayment = false;       // Reset cờ
            process_payment(pendingJson); // Gọi hàm render QR
            main_screen_timer = millis(); // Reset bộ đếm thời gian (nếu sau này bạn muốn tự đóng UI)
        }
    }

    // Cập nhật đồng hồ và ngày tháng mỗi giây
    static unsigned long last_time_update = 0;
if (millis() - last_time_update > 1000) {
    last_time_update = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char t_buf[9];  // Cho giờ: HH:MM
        char d_buf[32]; // Cho ngày tiếng Việt

        // 1. Định dạng giờ
        strftime(t_buf, sizeof(t_buf), "%H:%M", &timeinfo);

        // 2. Tự tạo chuỗi Thứ bằng tiếng Việt
        String thuVN;
        switch(timeinfo.tm_wday) {
            case 0: thuVN = "Chu Nhat"; break;
            case 1: thuVN = "Thu 2"; break;
            case 2: thuVN = "Thu 3"; break;
            case 3: thuVN = "Thu 4"; break;
            case 4: thuVN = "Thu 5"; break;
            case 5: thuVN = "Thu 6"; break;
            case 6: thuVN = "Thu 7"; break;
        }

        char date_part[12];
        strftime(date_part, sizeof(date_part), "%d/%m/%Y", &timeinfo);
        sprintf(d_buf, "%s, %s", thuVN.c_str(), date_part);

        // Gọi hàm cập nhật lên màn hình[cite: 18]
        ui_update_datetime(t_buf, d_buf);
    }
}
    
    delay(5);
}