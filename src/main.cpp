#include <Arduino.h>
#include "NTPClient.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include <WiFiClient.h>

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

const char *ssid = CUSTOM_WIFI_SSID.c_str();
const char *password = CUSTOM_WIFI_PASSWORD.c_str();

WiFiUDP ntpUDP; // 创建一个WIFI UDP连接

NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 60 * 60 * 8, 30 * 60 * 1000);

WiFiClient wiFiClient;

char lastToken[256];
String uploadResponseStr;

void callback(char *topic, byte *payload, unsigned int length);

bool sendPhoto2();

void generate_token();

bool generate_token_and_upload();

void initMqtt();

PubSubClient pubSubClient("183.230.40.39", 6002, callback, wiFiClient);

void callback(char *topic, byte *payload, unsigned int length) {
    String payloadStr;
    for (int i = 0; i < length; ++i) {
        payloadStr += (char) payload[i];
    }
    Serial.printf("topic = [%s] content = [%s] length = [%d]\n", topic, payloadStr.c_str(), length);
    if (strcmp(topic, "/topic/command/takePhoto/request") == 0) {
        bool ret = generate_token_and_upload();
        if (ret) {
            if (!pubSubClient.connected()) {
                initMqtt();
            }
            pubSubClient.publish("/topic/command/takePhoto/response", uploadResponseStr.c_str());
        }
    }
}

bool sendPhoto2() {
    const char *host = "up-z2.qiniup.com";
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
        Serial.println("Connection failed!");
        return false;
    } else {
        Serial.println("Connection successful!" + String("enid"));
        String bound = "boundry";
        String imageFileName = "image_" + String(timeClient.getEpochTime()) + ".jpg";
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
        wiFiClient.stop();
        Serial.println(getBody);

        const char *aaaa = getBody.c_str();
        for (int i = 0; i < getBody.length(); ++i) {
            uploadResponseStr += aaaa[i];
        }
        return true;
    }
}

void generate_token() {
    String buket = "esp32-cam-image";
    unsigned long time = timeClient.getEpochTime() + 3600L;
    String upload_param = "{\"scope\":\"esp32-cam-image\",\"deadline\":" + String(time) + "}";
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

    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
    }
}

bool generate_token_and_upload() {
    generate_token();
    return sendPhoto2();
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

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("ESP32-CAM IP Address: ");
    Serial.println(WiFi.localIP());

    init_camera();

    initMqtt();

    timeClient.begin();

    Serial.println("HTTP server started");
}

void loop() {
    if (!pubSubClient.connected()) {
        Serial.println("mqtt disconnect ,reconnect ...");
        initMqtt();
    }

    pubSubClient.loop();

    timeClient.update();

    delay(1000);
}