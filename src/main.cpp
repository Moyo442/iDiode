#include <common.h>

// General
#define NUM_LEDS 5 // one strip for now
#define DATA_PIN 26 // on wall-setup ESP
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// LEDs
CRGB leds[NUM_LEDS];
CRGB myCol = CRGB(40,180,0);

void setup() {
    Serial.begin(9600);
    
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    Serial.println("Setup complete.");
}

void loop() {
    fill_solid(leds, NUM_LEDS, myCol);
    FastLED.show();
    delay(20000);
}


// #include <common.h>

// // General
// #define NUM_LEDS 5 // one strip for now
// #define DATA_PIN 26 // on wall-setup ESP
// #define LED_TYPE WS2812B
// #define COLOR_ORDER GRB

// // LEDs
// CRGB leds[NUM_LEDS];

// void setup() {
//     Serial.begin(9600);
    
//     FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

//     Serial.println("Setup complete.");
// }

// void loop() {
//     fill_solid(leds, NUM_LEDS, CRGB::Red);   FastLED.show(); delay(1500);
//     fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show(); delay(1000);
//     fill_solid(leds, NUM_LEDS, CRGB::Blue);  FastLED.show(); delay(500);
// }
