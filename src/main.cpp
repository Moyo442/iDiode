#include <common.h>

// General
#define NUM_LEDS 29 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// LEDs
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
}

void loop() {
    for(uint8_t led = 0; led < NUM_LEDS; led++) {
        leds[led] = CRGB::OrangeRed;
        FastLED.show();
        delay(500);
        leds[led] = CRGB::Black;
    }
}
