#include <Arduino.h>
#include <FastLED.h>

#include <WiFi.h>
#include "wifi_credentials.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

// LEDs
#define NUM_LEDS 29 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Webserver
static const char *HOSTNAME = "idiode";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// WiFi handling utilities
EventGroupHandle_t wifiEvents = xEventGroupCreate();
constexpr EventBits_t BIT_CONNECTED = BIT0;  // 1 constantly while WiFi connection is up
constexpr EventBits_t BIT_CONNECTION_FAILED = BIT1;  // 1 when connection is lost / failed to establish. consumed and cleared by maintain_wifi task
constexpr EventBits_t BIT_GOT_IP = BIT2;  // 1 upon establishing connection, consumed and cleared by run_server task
QueueHandle_t disconnectReasonQ = xQueueCreate(1, sizeof(uint8_t));


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
            }
        }
    }
}

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            // Serial.printf("[WEBSOCKET SERVER] ");
            break;
        default:
            break;
    }
}

void runServer(void* parameters) {
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

    vTaskDelete(NULL);
}

void renderLEDs(void* parameters) {
    CRGB leds[NUM_LEDS];
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    for (;;) {
        for (uint8_t led = 0; led < NUM_LEDS; led++) {
            leds[led] = CRGB::OrangeRed;
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(500));
            leds[led] = CRGB::Black;
        }    
    }
}

void setup() {
    Serial.begin(9600);

    xTaskCreate(
        renderLEDs,
        "render_LEDs",
        10000,
        NULL,
        1,
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
        runServer,
        "run_server",
        20000,
        NULL,
        2,
        NULL
    );
}

void loop() {
    vTaskDelete(NULL);
}
