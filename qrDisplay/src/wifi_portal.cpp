//====================================================
// File: src/wifi_portal.cpp
// PlatformIO Modular Version
//====================================================
#include "wifi_portal.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

Preferences preferences;

//------------------------------------
// Các biến global từ main.cpp
//------------------------------------
extern String target_ssid;
extern String target_pass;

// Sửa lại kiểu dữ liệu thành SystemState cho khớp với wifi_portal.h
extern SystemState currentState; 

// Biến cho luồng Payment
extern String pendingJson;
extern bool hasNewPayment;

//------------------------------------
WebServer server(80);

//------------------------------------
// Lưu và đọc wifi
//------------------------------------

// 1. Hàm LOAD: Đọc dữ liệu đã lưu
void load_wifi_credentials() {
    preferences.begin("pharma-wifi", true); // Mode read-only
    target_ssid = preferences.getString("ssid", ""); 
    target_pass = preferences.getString("pass", "");
    preferences.end();
}

// 2. Hàm SAVE: Ghi dữ liệu vào Flash
void save_wifi_credentials(String ssid, String pass) {
    preferences.begin("pharma-wifi", false); // Mode read-write
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
}

// 3. Hàm HANDLE: Xử lý sự kiện từ Web
void handle_wifi_save() {
    String req_ssid = server.arg("ssid");
    String req_pass = server.arg("pass");

    if (req_ssid != "") {
        // Ghi vào bộ nhớ bền vững (NVS)
        save_wifi_credentials(req_ssid, req_pass); 

        // Gửi giao diện thông báo thành công (Copy từ code cũ của bạn)
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{margin:0;font-family:Arial;display:flex;justify-content:center;align-items:center;height:100vh;background:#f8fafc;}
.box{background:white;padding:30px;border-radius:22px;text-align:center;box-shadow:0 10px 30px rgba(0,0,0,.12);width:90%;max-width:360px;}
.icon{font-size:54px;}
h2{color:#16a34a;margin:14px 0;}
p{color:#64748b;font-size:15px;}
</style>
</head>
<body>
<div class="box">
<h2>COMPLETE</h2>
<p>ESP32-QR is connecting wifi....</p>
</div>
</body>
</html>
)rawliteral";

        server.send(200, "text/html", html);
        
        // Chờ một chút để trình duyệt kịp hiển thị rồi mới Restart
        delay(2000);
        ESP.restart(); // Khởi động lại để máy tự kết nối bằng thông tin mới
    } else {
        server.send(400, "text/plain", "SSID Empty");
    }
}
//------------------------------------
// Xử lý request thanh toán từ Webapp
//------------------------------------
void handle_payment_request() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST,OPTIONS");

    if (server.method() == HTTP_OPTIONS) {
        server.send(200);
        return;
    }

    String body = server.arg("plain");
    Serial.println("=== BODY RECEIVED ===");
    Serial.println(body);

    pendingJson = body;
    hasNewPayment = true;

    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

//------------------------------------
// Quét WiFi
//------------------------------------
String scanWifiOptions()
{
    String options = "";

    int total = WiFi.scanNetworks();

    if (total <= 0)
    {
        options += "<option value=''>Không tìm thấy WiFi</option>";
        return options;
    }

    for (int i = 0; i < total; i++)
    {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);

        if (ssid.length() == 0) continue;

        options += "<option value='" + ssid + "'>";
        options += ssid + " (" + String(rssi) + " dBm)";
        options += "</option>";
    }

    return options;
}

//------------------------------------
void setup_web_server()
{
    server.on("/", HTTP_GET, []()
    {
        String wifiList = scanWifiOptions();

        String html = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>WiFi Setup</title>

<style>
*{
margin:0;
padding:0;
box-sizing:border-box;
font-family:Arial,Helvetica,sans-serif;
}

body{
background:linear-gradient(135deg,#0f172a,#1e293b);
min-height:100vh;
display:flex;
justify-content:center;
align-items:center;
padding:18px;
}

.card{
width:100%;
max-width:420px;
background:white;
border-radius:24px;
padding:24px;
box-shadow:0 20px 45px rgba(0,0,0,.2);
}

.logo{
font-size:46px;
text-align:center;
margin-bottom:10px;
}

.title{
font-size:28px;
font-weight:700;
text-align:center;
color:#111827;
}

.sub{
font-size:14px;
text-align:center;
color:#6b7280;
margin-top:8px;
margin-bottom:22px;
}

label{
display:block;
font-size:14px;
font-weight:700;
margin-bottom:8px;
margin-top:14px;
color:#374151;
}

select,input{
width:100%;
padding:14px;
border:1px solid #d1d5db;
border-radius:14px;
font-size:16px;
outline:none;
}

select:focus,input:focus{
border-color:#2563eb;
box-shadow:0 0 0 3px rgba(37,99,235,.15);
}

button{
width:100%;
margin-top:20px;
padding:15px;
border:none;
border-radius:14px;
background:#2563eb;
color:white;
font-size:16px;
font-weight:700;
}

button:active{
transform:scale(.98);
}

.footer{
margin-top:18px;
text-align:center;
font-size:13px;
color:#9ca3af;
}
</style>
</head>

<body>

<div class="card">

<div class="logo">📶</div>

<div class="title">WiFi Setup</div>
<div class="sub">Chọn mạng WiFi và nhập mật khẩu</div>

<form action="/connect" method="POST">

<label>Danh sách WiFi</label>

<select name="ssid">
)rawliteral";

        html += wifiList;

        html += R"rawliteral(
</select>

<label>Mật khẩu</label>
<input type="password" name="pass" placeholder="Nhập mật khẩu">

<button type="submit">Kết nối</button>

</form>

<div class="footer">
ESP32 Configuration Portal
</div>

</div>

</body>
</html>
)rawliteral";

        server.send(200, "text/html", html);
    });

    // ĐĂNG KÝ ROUTE XỬ LÝ LƯU WIFI TẠI ĐÂY
    server.on("/connect", HTTP_POST, handle_wifi_save); 

    //------------------------------------
    // ĐĂNG KÝ ROUTE NHẬN LỆNH TỪ WEBAPP
    //------------------------------------
    server.on("/payment", HTTP_POST, handle_payment_request);
    server.on("/payment", HTTP_OPTIONS, handle_payment_request);

    //------------------------------------
    server.begin();
    Serial.println("WEB SERVER & CONFIG PORTAL READY");
}

//------------------------------------
void handle_web_server()
{
    server.handleClient();
}