#include <Arduino.h>
#include <FastLED.h>

#include <WiFi.h>
#include "wifi_credentials.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

#include <ArduinoJson.h>

// Config
#define NUM_LEDS 29 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define TELEMETRY_MAX_LEVEL 3

// WiFi handling utilities
QueueHandle_t disconnectReasonQ = xQueueCreate(1, sizeof(uint8_t));
EventGroupHandle_t wifiEvents = xEventGroupCreate();
constexpr EventBits_t BIT_CONNECTED         = BIT0;  // 1 constantly while WiFi connection is up
constexpr EventBits_t BIT_CONNECTION_FAILED = BIT1;  // 1 when connection is lost / failed to establish. consumed and cleared by maintain_wifi task
constexpr EventBits_t BIT_GOT_IP            = BIT2;  // 1 upon establishing connection, consumed and cleared by run_server task

// Webserver
static const char *HOSTNAME = "idiode";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


void wifiEventHandler(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            xEventGroupSetBits(wifiEvents, BIT_CONNECTED | BIT_GOT_IP);
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            xQueueOverwrite(disconnectReasonQ, &reason);
            xEventGroupClearBits(wifiEvents, BIT_CONNECTED);
            xEventGroupSetBits(wifiEvents, BIT_CONNECTION_FAILED);
            break;
        }
        default:
            break;
    }
}

void maintainWifi(void* parameters) {
    EventBits_t wifiBits;
    
    Serial.println("[WIFI TASK] WiFi connecting");

    WiFi.persistent(false);  // do not store credentials in flash, as they are hard-coded right now. Later can be managed by overall persistent config storage
    WiFi.mode(WIFI_STA);  // ESP as STAtion (to connect to an access point (AP))
    WiFi.setAutoReconnect(true);
    WiFi.onEvent(wifiEventHandler);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    for (;;) {
        wifiBits = xEventGroupWaitBits(
            wifiEvents,
            BIT_GOT_IP | BIT_CONNECTION_FAILED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );

        if (wifiBits & BIT_GOT_IP) {
            Serial.printf("[WIFI TASK] WiFi connected at IP %s\n", WiFi.localIP().toString().c_str());
            if (!MDNS.begin(HOSTNAME)) {
                Serial.println("[WIFI TASK] mDNS start failed.");
            }
        }
        
        if (wifiBits & BIT_CONNECTION_FAILED) {
            uint8_t reason;
            if (xQueueReceive(disconnectReasonQ, &reason, 0) == pdTRUE) {
                Serial.printf("[WIFI TASK] WiFi connection failed. Reason %u.\n", reason);
                // reason codes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi-driver/station-scenarios.html#wi-fi-reason-code
            }
        }
    }
}

void sendMessageWs(AsyncWebSocketClient* c, const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    c->text(out);
}

void broadcastMessageWs(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

void sendErrorWs(AsyncWebSocketClient* c, const char* msg) {
    JsonDocument doc;
    doc["type"] = "error";
    doc["msg"] = msg;
    sendMessageWs(c, doc);
    Serial.printf("[WEBSOCKET SERVER] Client %u caused error %s\n", c->id(), msg);
}

void handleTelemetry(AsyncWebSocketClient* c, const JsonDocument& inDoc) {
    const uint8_t level = inDoc["level"] | TELEMETRY_MAX_LEVEL; 
    
    JsonDocument outDoc;
    outDoc["type"] = "telemetry";

    if (level >= 0) {
        outDoc["uptime"] = millis() / 1000;
    }
    if (level >= 1) {
        outDoc["heap"] = ESP.getFreeHeap();
    }
    if (level >= 2) {
        outDoc["rssi"] = WiFi.RSSI();
    }

    sendMessageWs(c, outDoc);
}

void handleWsData(AsyncWebSocketClient* c, const char* data, size_t len) {
    Serial.printf("[WEBSOCKET SERVER] Client %u sent: '%s'.\n", c->id(), data);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    
    if (err) {
        sendErrorWs(c, "JSON deserialization failed.");

        String out;
        serializeJson(doc, out);
        Serial.printf("[WEBSOCKET SERVER] JSON deserialization failed. It looks like '%s' and caused error '%u'.\n", out, err);
        
        return;
    }

    const char* type = doc["type"] | "none";
    
    if (strcmp(type, "none") == 0) {
        sendErrorWs(c, "No 'type' supplied.");
    } else if (strcmp(type, "telemetry") == 0) {
        handleTelemetry(c, doc);
    // } else if (strcmp(type, "segment")) {
    //     handleSegment(doc);
    // } else if (strcmp(type, "parameter")) {
    //     handleParameter(doc);
    } else {
        sendErrorWs(c, "Unknown 'type'.");
    }
}

void initializeWs(AsyncWebSocketClient* c) {
    JsonDocument doc;
    doc["type"] = "init";
    doc["host"] = HOSTNAME;
    // also send full current config state? Maybe should be done with a http GET/POST in webserver task
    sendMessageWs(c, doc);
}

void onWsEvent(AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            initializeWs(c);
            Serial.printf("[WEBSOCKET SERVER] Client %u entered the chat.\n", c->id());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WEBSOCKET SERVER] Client %u left the chat.\n", c->id());
            break;
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo *)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handleWsData(c, (const char *)data, len);
            }
            break;
        }
        default:
            break;
    }
}

void startServer(void* parameters) {
    // wait for CONNECTED event notification
    xEventGroupWaitBits(
        wifiEvents,
        BIT_CONNECTED,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
    Serial.println("[SERVER TASK] Server started.");

    // probably add loop to listen to newly connected devices and POST the full current config

    vTaskDelete(NULL);
}

void renderLEDs(void* parameters) {
    CRGB leds[NUM_LEDS];
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    uint8_t led = 5;
    for (;;) {
        // for (uint8_t led = 0; led < NUM_LEDS; led++) {
        //     leds[led] = CRGB::OrangeRed;
        //     FastLED.show();
        //     vTaskDelay(pdMS_TO_TICKS(100));
        //     leds[led] = CRGB::Black;
        // }
        vTaskDelay(pdMS_TO_TICKS(100));
        leds[led] = CRGB::OrangeRed;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100));
        leds[led] = CRGB::Black;    
        FastLED.show();
    }
}

void setup() {
    Serial.begin(9600);

    xTaskCreate(
        renderLEDs,
        "render_LEDs",
        10000,
        NULL,
        3,
        NULL
    );

    xTaskCreatePinnedToCore(
        maintainWifi,
        "maintain_wifi",
        10000,
        NULL,
        1,
        NULL,
        CONFIG_ARDUINO_RUNNING_CORE
    );

    xTaskCreate(
        startServer,
        "start_server",
        20000,
        NULL,
        2,
        NULL
    );
}

void loop() {
    vTaskDelete(NULL);
}
