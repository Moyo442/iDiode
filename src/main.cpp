#include <Arduino.h>
#include <FastLED.h>

#include <WiFi.h>
#include "wifi_credentials.h"

// LEDs
#define NUM_LEDS 29 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

void maintainWifi(void* parameters) {
    uint32_t wifi_timeout_ms = 20000;
    uint32_t wifi_retry_delay = 15000;
    
    Serial.print("[WIFI TASK] WiFi connecting");

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("\n[WIFI TASK] WiFi connected at ");
            Serial.println(WiFi.localIP());
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint32_t startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED
                && millis() - startAttemptTime < wifi_timeout_ms) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }

        if (WiFi.status() != WL_CONNECTED) {
            Serial.print("\n[WIFI TASK] Connection failed. Trying again in ");
            Serial.print(wifi_retry_delay/1000);
            Serial.println("s.");

            vTaskDelay(pdMS_TO_TICKS(wifi_retry_delay));
        }
    }
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
}

void loop() {
    vTaskDelete(NULL);
}
