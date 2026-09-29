#include <Arduino.h>
#include <FastLED.h>

#include <WiFi.h>

// General
#define NUM_LEDS 29 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// LEDs
CRGB leds[NUM_LEDS];

void renderLEDs(void* parameters) {
    CRGB leds[NUM_LEDS];
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    Serial.println("LEDs initialized.");

    for (;;) {
        for(uint8_t led = 0; led < NUM_LEDS; led++) {
            Serial.print("Drawing LED ");
            Serial.println(led+1);
            leds[led] = CRGB::OrangeRed;
            FastLED.show();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            leds[led] = CRGB::Black;
        }    
    }
}

void setup() {
    Serial.begin(9600);

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    // xTaskCreatePinnedToCore(
    //     renderLEDs,
    //     "render_LEDs",
    //     10000,
    //     NULL,
    //     1,
    //     NULL,
    //     CONFIG_ARDUINO_RUNNING_CORE
    // );
}

void loop() {
    for(uint8_t led = 0; led < NUM_LEDS; led++) {
        Serial.print("Drawing LED ");
        Serial.println(led+1);
        leds[led] = CRGB::OrangeRed;
        FastLED.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        leds[led] = CRGB::Black;
    } 
}
