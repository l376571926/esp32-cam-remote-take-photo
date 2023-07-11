#include <Arduino.h>
#include "WiFi.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>

#include <base64.h>
//hmac_sha1.c
#include <aaaa.h>
#include <qiniu_user_info.h>
#include <wifi_access_info.h>
#include <mqtt_user_info.h>

#include "soc/soc.h"
#include "esp_camera.h"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiMulti wifiMulti;
WiFiUDP ntpUDP; // 创建一个WIFI UDP连接
WiFiClient wiFiClient;

char lastToken[256];
String uploadResponseStr;

void callback(char *topic, byte *payload, unsigned int length);

bool sendPhoto2(long unixTimestamp);

void generate_token(long unixTimestamp);

void initMqtt();

void syncUnixTimestamp();

long getUnixTimestamp();

PubSubClient pubSubClient("183.230.40.39", 6002, callback, wiFiClient);

void callback(char *topic, byte *payload, unsigned int length) {
    String payloadStr;
    for (int i = 0; i < length; ++i) {
        payloadStr += (char) payload[i];
    }
    Serial.printf("topic = [%s] content = [%s] length = [%d]\n", topic, payloadStr.c_str(), length);
    if (strcmp(topic, "/topic/command/takePhoto/request") == 0) {
        long expiredTime = getUnixTimestamp() + 3600;
        generate_token(expiredTime);
        bool ret = sendPhoto2(expiredTime);
        if (ret) {
            Serial.println("send photo to qiniu success.");
            if (!pubSubClient.connected()) {
                Serial.println("callback mqtt server is disconnected, reconnect ,send response");
                initMqtt();
            }
            bool rett = pubSubClient.publish("/topic/command/takePhoto/response", uploadResponseStr.c_str());
            if (rett) {
                Serial.println("publish upload response message to topic /topic/command/takePhoto/response success");
            } else {
                Serial.println("publish upload response message to topic /topic/command/takePhoto/response failed");
            }
        } else {
            Serial.println("upload photo to qiniu failed.");
        }
    }
}

bool sendPhoto2(long unixTimestamp) {
//    const char *host = "up-z2.qiniup.com";//华南-广东
    const char *host = "up-na0.qiniup.com";//北美-洛杉矶
    const char *path = "/";
    uint16_t port = 80;

    String getAll;
    String getBody;

    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb) {
        esp_camera_fb_return(fb);
        Serial.println("Camera capture failed");
        return false;
    }

    if (!wiFiClient.connect(host, port)) {
        Serial.println("Connection qiniu upload server failed!");
        return false;
    } else {
        Serial.println("Connection successful!" + String("enid"));
        String bound = "boundry";
        String imageFileName = "image_" + String(unixTimestamp) + ".jpg";
        String form_data =
                "--" + bound + "\r\nContent-Disposition: form-data; name=\"key\"" + "\r\n\r\n" + imageFileName + "\r\n";
        form_data +=
                "--" + bound + "\r\nContent-Disposition: form-data; name=\"token\"" + "\r\n\r\n" + String(lastToken) +
                "\r\n";
        String image_head = "--" + bound +
                            "\r\nContent-Disposition: form-data; name=\"file\";filename=\"" + imageFileName +
                            "\"\r\nContent-Type: image/jpeg\r\n\r\n";
        String image_tail = "\r\n--" + bound + "--\r\n";

        // content length
        uint32_t contentLength = form_data.length() + image_head.length() + fb->len + image_tail.length();

        // 发送post请求头
        Serial.println("first step");
        wiFiClient.println("POST " + String(path) + " HTTP/1.1");
        wiFiClient.println("Cache-Control: no-cache");
        wiFiClient.println("Content-Type: multipart/form-data; boundary=" + bound);
        wiFiClient.println("Host: " + String(host));
        wiFiClient.println("Content-Length: " + String(contentLength));
        wiFiClient.println();

        //发送post请求体-表单文本
        char charBufKey[form_data.length() + 1];
        form_data.toCharArray(charBufKey, form_data.length() + 1);
        wiFiClient.write(charBufKey);

        //发送post请求体-表单文件
        Serial.println("second step");
        wiFiClient.print(image_head);
        uint8_t *fbBuf = fb->buf;
        size_t fbLen = fb->len;
        Serial.println("fb len: " + String(fbLen));
        long start = millis();
        wiFiClient.write(fbBuf, fbLen);
        Serial.println("send image cost time: " + String(millis() - start));
        wiFiClient.print(image_tail);

        esp_camera_fb_return(fb);

        int timoutTimer = 10000;
        long startTimer = millis();
        boolean state = false;

        Serial.println("third step");
        while ((startTimer + timoutTimer) > millis()) {
            Serial.print(".");
            delay(100);
            while (wiFiClient.available()) {
                char c = wiFiClient.read();
                if (c == '\n') {
                    if (getAll.length() == 0) {
                        state = true;
                    }
                    getAll = "";
                } else if (c != '\r') {
                    getAll += String(c);
                }
                if (state == true) {
                    getBody += String(c);
                }
                startTimer = millis();
            }
            if (getBody.length() > 0) {
                break;
            }
        }
        Serial.println("photo upload result: " + getBody + " " + getBody.length());

        const char *aaaa = getBody.c_str();
        for (int i = 0; i < getBody.length(); ++i) {
            uploadResponseStr += aaaa[i];
        }
        return true;
    }
}

void generate_token(long unixTimestamp) {
    String buket = "esp32-cam-image-us";
    String upload_param = "{\"scope\":\"" + buket + "\",\"deadline\":" + String(unixTimestamp) + "}";
    Serial.printf("%s\n", ".");
    Serial.printf("upload_param: %s\n", upload_param.c_str());

    String param_base64_result = base64::encode(upload_param);
    Serial.printf("base64 = %s\n", param_base64_result.c_str());

    const char *k = secretKey.c_str();
    const char *d = param_base64_result.c_str();

    unsigned char *key = (unsigned char *) k;
    unsigned char *dd = (unsigned char *) d;

    int key_length = strlen(k);
    int data_length = strlen(d);

    unsigned char out[160] = {0};

    hmac_sha1(key, key_length, dd, data_length, out);

    String strrrr(out, strlen(reinterpret_cast<const char *>(out)));
    String sign_result_base64_result = base64::encode(strrrr);
    sign_result_base64_result.replace("+", "-");
    sign_result_base64_result.replace("/", "_");

    //sign的结果可能不正确
    //如：
    // {"scope":"esp32-cam-image","deadline":1687256035}
    // eyJzY29wZSI6ImVzcDMyLWNhbS1pbWFnZSIsImRlYWRsaW5lIjoxNjg3MjU2MDM1fQ==
    // -->
    // 3HZNPK-FlkY
    // -->
    // oE6xigv1Yh9ioaeiEicw_WTFX3Dg4DldmIGvN--c:3HZNPK-FlkY=:eyJzY29wZSI6ImVzcDMyLWNhbS1pbWFnZSIsImRlYWRsaW5lIjoxNjg3MjU2MDM1fQ==
    Serial.printf("sign = %s\n", sign_result_base64_result.c_str());

    String token = accessKey + ":" + sign_result_base64_result + ":" + param_base64_result;
    Serial.printf("upload token = %s\n", token.c_str());

    for (int i = 0; i < strlen(lastToken); i++) {
        lastToken[i] = 0;
    }
    strcpy(lastToken, token.c_str());
}

void init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size = FRAMESIZE_SVGA;// 800x600
//    config.frame_size = FRAMESIZE_UXGA;// 1600x1200
    config.jpeg_quality = 10;
    config.fb_count = 2;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
    }
}

void initMqtt() {
    String clientId = MQTT_CLIENTID;
    String user = MQTT_USER;
    String pass = MQTT_PASS;

    bool ret = pubSubClient.connect(clientId.c_str(), user.c_str(), pass.c_str());
    if (ret) {
        Serial.println("connect to mqtt server success");
        pubSubClient.subscribe("/topic/command/takePhoto/request");
    } else {
        Serial.println("connect to mqtt server failed");
    }
}

void syncUnixTimestamp() {
    Serial.println("Start Time synced");
    // Beijing: UTC +8  -- 获取东八区时间(默认以英国格林威治天文台所在地的本初子午线为基准线的)
    const long utcOffsetInSeconds = 28800;
    const char *ntpServer = "ntp1.aliyun.com";
    //获取时间
    configTime(utcOffsetInSeconds, 0, ntpServer);
    while (true) {
        time_t now = time(nullptr);
        long unixTimestamp = static_cast<long>(now);  //获取unix时间戳
        //Current time is: Tue Jul 11 11:52:07 2023
        if (unixTimestamp >= 1689047527) {
            break;
        }
//        Serial.printf("Unix timestamp is: %ld\n",unixTimestamp);

        delay(1000);
        Serial.print(".");
    }
    Serial.println("Time synced successfully");
}

long getUnixTimestamp() {
    long unixTimestamp = 0;
    time_t now = time(nullptr);

    //Current time is: Tue Jul 11 11:24:39 2023
    Serial.print("Current time is: ");
    Serial.println(ctime(&now));  //打印时间

    // Convert current time to Unix timestamp
    unixTimestamp = static_cast<long>(now);  //获取unix时间戳

    //Unix timestamp is: 1689045879
    Serial.print("Unix timestamp is: ");
    Serial.println(unixTimestamp);
    return unixTimestamp;
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_MODE_STA);
    wifiMulti.addAP(ssid_from_AP_1.c_str(), your_password_for_AP_1.c_str());
    wifiMulti.addAP(ssid_from_AP_2.c_str(), your_password_for_AP_2.c_str());
    wifiMulti.addAP(ssid_from_AP_3.c_str(), your_password_for_AP_3.c_str());

    Serial.println("Connecting WiFi ...");
    while (wifiMulti.run() != WL_CONNECTED) {
        Serial.println(".");
        delay(1000);
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WiFi connected to ssid: ");
    Serial.println(WiFi.SSID());

    //同步时间戳
    syncUnixTimestamp();

    init_camera();

    initMqtt();

    Serial.println("setup init finish");
}

void loop() {
    getUnixTimestamp();
    if (wifiMulti.run() != WL_CONNECTED) {
        ESP.restart();
        return;
    }
    if (!pubSubClient.connected()) {
        Serial.println("loop mqtt disconnect ,reconnect ...");
        initMqtt();
    }

    pubSubClient.loop();
    delay(1000);
}