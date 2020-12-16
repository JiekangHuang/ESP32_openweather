#include "config.h"

#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <SPI.h>

#include <HttpClient.h>
#include <SSLClient.h>
#include <TinyGsmClient.h>

#include <ArduinoJson.h>

#include "AllTrustAnchors.h"

// 設定點矩陣參數
int pinCS                      = 15;
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays   = 1;

// 設定字體顯示寬度
int spacer = 1;
int width  = 5 + spacer;     // The font width is 5 pixels

// 建立 4 個 8*8 點矩陣
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

#ifdef DEBUG_DUMP_AT_COMMAND
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger, AM7020_RESET);

#else
TinyGsm modem(SerialAT, AM7020_RESET);

#endif

// 建立 AM7020 TCP Client
TinyGsmClient tcpClient(modem);
// 建立 SSL Client，下層接 TCP Client
SSLClient sslClient(tcpClient, TAs, (size_t)TAs_NUM, A0);
// 建立 HTTP Client，下層接 SSL Client
HttpClient httpClient = HttpClient(sslClient, HTTP_SERVER, HTTP_PORT);

// 存放溫溼度、城市名稱
float       temperature, humidity;
const char *city;
bool        got_data_flag = false;

void nbConnect(void);
void display(String text, int delay_ms);

void setup()
{
    SerialMon.begin(MONITOR_BAUDRATE);
    SerialAT.begin(AM7020_BAUDRATE);

    // 設定點矩陣亮度
    matrix.setIntensity(7);

    // nbiot module 連線基地台
    nbConnect();
}

void loop()
{
    static unsigned long timer = 0;
    char                 buff[200];
    int                  state_code;
    String               body, show_text;

    // 檢查 nbiot 連線狀態
    if (!modem.isNetworkConnected()) {
        nbConnect();
    }
    // 抓取 openweather 溫濕度資料
    if (millis() >= timer) {
        timer = millis() + UPDATE_INTERVAL;

        SerialMon.println(F("HTTPS Get..."));
        sprintf(buff, "%s", HTTP_API);
        if (!httpClient.connected()) {
            httpClient.connect(HTTP_SERVER, HTTP_PORT);
        }
        httpClient.get(buff);
        state_code = httpClient.responseStatusCode();
        body       = httpClient.responseBody();

        if (state_code == 200) {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body.c_str());
            temperature = doc["main"]["temp"];
            humidity    = doc["main"]["humidity"];
            city        = doc["name"];
            SerialMon.println(city);
            SerialMon.println(temperature);
            SerialMon.println(humidity);
            got_data_flag = true;
        } else {
            got_data_flag = false;
        }

        SerialMon.print(F("GET state code = "));
        SerialMon.println(state_code);
        SerialMon.print(F("body = "));
        SerialMon.println(body);
        httpClient.stop();
    }

    if (got_data_flag) {
        // 顯示城市名稱
        display(city, 60);
        // 顯示溫度
        show_text = "Temp:" + String(temperature, 0) + "C";
        display(show_text, 100);
        // 顯示濕度
        show_text = "Hum:" + String(humidity, 0) + "%";
        display(show_text, 100);
    } else {
        display("Get Error !!", 60);
    }
}

void nbConnect(void)
{
    SerialMon.println(F("Initializing modem..."));
    while (!modem.init() || !modem.nbiotConnect(APN, BAND)) {
        SerialMon.print(F("."));
    }

    SerialMon.print(F("Waiting for network..."));
    while (!modem.waitForNetwork()) {
        SerialMon.print(F("."));
    }
    SerialMon.println(F(" success"));
}

void display(String text, int delay_ms)
{

    for (int i = 0; i < (int)(width * text.length() + matrix.width() - 1 - spacer); i++) {

        matrix.fillScreen(LOW);

        int letter = i / width;
        int x      = (matrix.width() - 1) - i % width;
        int y      = (matrix.height() - 8) / 2;     // center the text vertically

        while (x + width - spacer >= 0 && letter >= 0) {
            if (letter < (int)text.length()) {
                matrix.drawChar(x, y, text[letter], HIGH, LOW, 1);
            }

            letter--;
            x -= width;
        }

        matrix.write();     // Send bitmap to display

        delay(delay_ms);
    }
    matrix.fillScreen(LOW);
}
