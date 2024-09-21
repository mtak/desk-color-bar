#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG false

#define PIN_COLOR A3
#define PIN_BRIGHTNESS A1
#define PIN_SWITCH 5
#define NUM_LEDS 78 // 42 + 36
#define DATA_PIN 7

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Variables for debouncing
int switchState;             // Current state of the switch
int lastSwitchState = LOW;    // Previous state of the switch
unsigned long lastDebounceTime = 0; // Time of the last state change
unsigned long debounceDelay = 10;   // Debounce delay in milliseconds

// Color
struct Color {
  byte r;
  byte g;
  byte b;
};

Color prev_color = {0, 0, 0};
int prev_brightness = -1;

void setup() {
  if (DEBUG) Serial.begin(9600);
  if (DEBUG) Serial.println("Startup");
  pinMode(PIN_COLOR, INPUT);
  pinMode(PIN_BRIGHTNESS, INPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  pinMode(DATA_PIN, OUTPUT);
}

int getAveragePotValue(int numReadings, int potPin) {
  int total = 0;

  // Take multiple readings and sum them up
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(potPin);
    delay(10); // Small delay between readings (optional)
  }

  // Calculate and return the average
  int average = total / numReadings;
  return average;
}

int debounceSwitch(int switchPin) {
  // Read the current state of the switch
  int reading = digitalRead(switchPin);

  // Check if the switch state has changed
  if (reading != lastSwitchState) {
    // Reset the debounce timer
    lastDebounceTime = millis();
  }

  // If the state is stable for longer than the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Only change the switch state if it has truly changed
    if (reading != switchState) {
      switchState = reading;
    }
  }

  // Remember the last switch reading for the next loop
  lastSwitchState = reading;

  // Return the debounced switch state
  return switchState;
}

bool getColorMode(int switchPin) {
  // Check state of switch
  int switch_raw = debounceSwitch(switchPin);
  if (DEBUG) Serial.println("Pin switch: " + String(switch_raw));

  bool colorMode;
  if (switch_raw) colorMode = true;
  if (!switch_raw) colorMode = false;
  if (DEBUG) Serial.println("Color mode: " + String(colorMode));
  
  return colorMode;
}

Color colorWheel(int input) {
  Color color; // Struct to hold the resulting RGB values
  
  // Ensure the input is within 0-1023
  if (input < 0) input = 0;
  if (input > 1023) input = 1023;
  
  // Map input range 0-1023 to hue range 0-360
  float hue = ((float)input / 1023.0) * 360.0;
  
  // Ensure hue is within 0-360
  hue = fmod(hue, 360.0);
  
  // Saturation and Value are set to maximum for vibrant colors
  float saturation = 1.0;
  float value = 1.0;
  
  // Convert HSV to RGB
  float c = value * saturation; // Chroma
  float x = c * (1 - abs(fmod(hue / 60.0, 2) - 1));
  float m = value - c;
  
  float r_prime, g_prime, b_prime;
  
  if (hue >= 0 && hue < 60) {
    r_prime = c;
    g_prime = x;
    b_prime = 0;
  }
  else if (hue >= 60 && hue < 120) {
    r_prime = x;
    g_prime = c;
    b_prime = 0;
  }
  else if (hue >= 120 && hue < 180) {
    r_prime = 0;
    g_prime = c;
    b_prime = x;
  }
  else if (hue >= 180 && hue < 240) {
    r_prime = 0;
    g_prime = x;
    b_prime = c;
  }
  else if (hue >= 240 && hue < 300) {
    r_prime = x;
    g_prime = 0;
    b_prime = c;
  }
  else { // hue >= 300 && hue < 360
    r_prime = c;
    g_prime = 0;
    b_prime = x;
  }
  
  // Convert to 0-255 range and assign to struct
  color.r = (byte)((r_prime + m) * 255);
  color.g = (byte)((g_prime + m) * 255);
  color.b = (byte)((b_prime + m) * 255);
  
  return color;
}

Color whiteBalance(int input) {
  Color color; // Struct to hold the resulting RGB values
  
  // Ensure the input is within 0-1023
  if (input < 0) input = 0;
  if (input > 1023) input = 1023;
  
  // Map input range 0-1023 to color temperature range 1000K-8000K
  float temperature = 1000.0 + ((float)input / 1023.0) * (8000.0 - 1000.0);
  
  // Convert temperature to RGB using the algorithm by Tanner Helland
  // Reference: http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
  
  float temp = temperature / 100.0;
  
  // Calculate Red
  float red;
  if (temp <= 66.0) {
    red = 255;
  }
  else {
    red = temp - 60.0;
    red = 329.698727446 * pow(red, -0.1332047592);
    if (red < 0) red = 0;
    if (red > 255) red = 255;
  }
  
  // Calculate Green
  float green;
  if (temp <= 66.0) {
    green = 99.4708025861 * log(temp) - 161.1195681661;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
  }
  else {
    green = temp - 60.0;
    green = 288.1221695283 * pow(green, -0.0755148492);
    if (green < 0) green = 0;
    if (green > 255) green = 255;
  }
  
  // Calculate Blue
  float blue;
  if (temp >= 66.0) {
    blue = 255;
  }
  else {
    if (temp <= 19.0) {
      blue = 0;
    }
    else {
      blue = temp - 10.0;
      blue = 138.5177312231 * log(blue) - 305.0447927307;
      if (blue < 0) blue = 0;
      if (blue > 255) blue = 255;
    }
  }
  
  // Assign to struct
  color.r = (byte)red;
  color.g = (byte)green;
  color.b = (byte)blue;
  
  return color;
}

bool areColorsNotEqual(const Color& c1, const Color& c2) {
    return (c1.r != c2.r) || (c1.g != c2.g) || (c1.b != c2.b);
}

void loop() {
  if (DEBUG) delay(500);
  if (DEBUG) Serial.println();

  // Get values from pots and switch
  bool colorMode = getColorMode(PIN_SWITCH);

  int hue_raw = getAveragePotValue(3, PIN_COLOR);
  if (DEBUG) Serial.println("Hue pot: " + String(hue_raw));
  
  int brightness_raw = getAveragePotValue(3, PIN_BRIGHTNESS);
  if (DEBUG) Serial.println("Brightness pot: " + String(brightness_raw));

  // Map inputs to outputs (brightness, color, white balance)
  int brightness = map(brightness_raw, 0, 1023, 64, 255);
  Color color;
  if (colorMode) color = colorWheel(hue_raw);
  if (!colorMode) color = whiteBalance(hue_raw);
  if (DEBUG) Serial.println("R: " + String(color.r) + " G: " + String(color.g) + " B: " + String(color.b) + " Brightness: " + String(brightness));

  // Output  
  if (areColorsNotEqual(prev_color, color) || (prev_brightness != brightness)) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, color.r, color.g, color.b);
      strip.setBrightness(brightness);
    }
    strip.show();
    if (DEBUG) Serial.println("Updated color");
  }

  prev_color = color;
  prev_brightness = brightness;
}
