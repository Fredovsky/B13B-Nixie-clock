#include "Arduino.h"
#include "RTClib.h" //v1.12.4
#include "SPI.h"  // HV5530 interfacing

// Pin definitions
#define minuteButtonPin 7
#define hourButtonPin 6
#define brightnessPin 9 // PWM output to nixies
#define latchPin 10

// Timing constants
constexpr unsigned long DEBOUNCE_DELAY_MS = 50;
constexpr unsigned long LONG_PRESS_TIME_MS = 1000;
constexpr unsigned long BUTTON_REPEAT_RATE_MS = 150;
constexpr unsigned long BLINK_INTERVAL_MS = 800;
constexpr unsigned long TIME_CHECK_INTERVAL_MS = 500;
constexpr unsigned long MINUTE_REST_MS = 59000;

// Display constants
constexpr uint8_t BLANK_DIGIT = 10;
constexpr uint8_t CLEANING_HOUR = 3;

// Brightness constants
constexpr uint8_t PWM_ABSOLUTE_MIN = 15;
constexpr uint8_t PWM_ABSOLUTE_MAX = 255;
constexpr uint8_t DEFAULT_MIN_BRIGHTNESS = 15;
constexpr uint8_t DEFAULT_MAX_BRIGHTNESS = 240;
constexpr uint8_t TIME_START_DECREASE = 19;
constexpr uint8_t TIME_START_INCREASE = 6;

// Time variables
RTC_DS3231      rtc;
DateTime        now;
uint8_t         minute = 0;
unsigned long   lastTimeCheck = 0;

// Blinking variables
unsigned long lastBlink = 0;
bool blinkState = true; // true = digits visible, false = digits hidden

// Brightness variables
uint8_t       minBrightness = DEFAULT_MIN_BRIGHTNESS;
uint8_t       maxBrightness = DEFAULT_MAX_BRIGHTNESS;
int16_t       brightness = DEFAULT_MAX_BRIGHTNESS;
uint8_t       timeStartDecrease = TIME_START_DECREASE;
uint8_t       timeStartIncrease = TIME_START_INCREASE;

// Button state variables
bool minuteButtonPressed = false;
unsigned long timeMinuteButtonPressed = 0;
unsigned long lastMinuteRepeat = 0;

bool hourButtonPressed = false;
unsigned long timeHourButtonPressed = 0;
unsigned long lastHourRepeat = 0;

// Display Mode (0 = Time)
uint8_t displayMode = 0;

// Hardware wiring of [nixies digit 0-9] to corresponding {HV5530 pin}
// Driver 1
const uint8_t digit_1[10] = {22, 25, 24, 19, 16, 12, 14, 15, 18, 21};
const uint8_t digit_2[10] = { 6, 23, 20, 17, 13, 11, 10,  9,  8,  7};
// Driver 2
const uint8_t digit_3[10] = {13, 26, 25, 24, 23, 22, 21, 19, 15, 12};
const uint8_t digit_4[10] = { 7, 10, 11, 16, 17, 18, 20, 14,  9,  8};

// Current digits displayed
uint8_t d1 = 0;
uint8_t d2 = 0;
uint8_t d3 = 0;
uint8_t d4 = 0;

// Stored digit values for blinking
uint8_t stored_d1 = 0;
uint8_t stored_d2 = 0;
uint8_t stored_d3 = 0;
uint8_t stored_d4 = 0;

// Which digits should blink
bool blink_d1 = false;
bool blink_d2 = false;
bool blink_d3 = false;
bool blink_d4 = false;

// Data to be shifted to HV5530 drivers
uint32_t data1 = 0x00000000;
uint32_t data2 = 0x00000000;

// SPI Protocol
SPISettings HV5530(1000000, MSBFIRST, SPI_MODE1);


void updateDisplay() {
    // Sends individual digits to the display
    
    data1 = 0x00000000;
    if(d1 < BLANK_DIGIT) { data1 |= 1UL << (digit_1[d1] - 1); }
    if(d2 < BLANK_DIGIT) { data1 |= 1UL << (digit_2[d2] - 1); }
    
    data2 = 0x00000000;
    if(d3 < BLANK_DIGIT) { data2 |= 1UL << (digit_3[d3] - 1); }
    if(d4 < BLANK_DIGIT) { data2 |= 1UL << (digit_4[d4] - 1); }
    
    // Prepare buffer for SPI transfer
    uint8_t buffer[8] = {
        (uint8_t)(data2 >> 24), (uint8_t)(data2 >> 16),
        (uint8_t)(data2 >> 8),  (uint8_t)data2,
        (uint8_t)(data1 >> 24), (uint8_t)(data1 >> 16),
        (uint8_t)(data1 >> 8),  (uint8_t)data1
    };
    
    // Update Nixie tubes
    SPI.beginTransaction(HV5530);
    digitalWrite(latchPin, HIGH);
    SPI.transfer(buffer, 8);
    digitalWrite(latchPin, LOW);
    digitalWrite(latchPin, HIGH);
    SPI.endTransaction();
}

void generateDisplay(DateTime dt) {
    // Generates the required individual digits based on display mode
    
    switch (displayMode) {
        case 0: default:
            // Time mode (HH:MM)
            stored_d1 = dt.hour() / 10;
            stored_d2 = dt.hour() % 10;
            stored_d3 = dt.minute() / 10;
            stored_d4 = dt.minute() % 10;
            
            // Apply to display (will be blanked if blinking)
            d1 = stored_d1;
            d2 = stored_d2;
            d3 = stored_d3;
            d4 = stored_d4;
            break;
    }
    
    updateDisplay();
}

void makeBlink() {
    // Handle blinking of digits
    
    // Check if any digit needs to blink
    if(!blink_d1 && !blink_d2 && !blink_d3 && !blink_d4) {
        return;
    }
    
    // Check if it's time to toggle blink state
    if(millis() - lastBlink > BLINK_INTERVAL_MS) {
        blinkState = !blinkState;
        lastBlink = millis();
        
        // Update display based on blink state
        if(blinkState) {
            // Show digits
            if(blink_d1) d1 = stored_d1; else d1 = stored_d1;
            if(blink_d2) d2 = stored_d2; else d2 = stored_d2;
            if(blink_d3) d3 = stored_d3; else d3 = stored_d3;
            if(blink_d4) d4 = stored_d4; else d4 = stored_d4;
        } else {
            // Hide blinking digits
            if(blink_d1) d1 = BLANK_DIGIT; else d1 = stored_d1;
            if(blink_d2) d2 = BLANK_DIGIT; else d2 = stored_d2;
            if(blink_d3) d3 = BLANK_DIGIT; else d3 = stored_d3;
            if(blink_d4) d4 = BLANK_DIGIT; else d4 = stored_d4;
        }
        
        updateDisplay();
    }
}

void stopBlinking() {
    // Stop all blinking and restore normal display
    blink_d1 = false;
    blink_d2 = false;
    blink_d3 = false;
    blink_d4 = false;
    blinkState = true;
    
    d1 = stored_d1;
    d2 = stored_d2;
    d3 = stored_d3;
    d4 = stored_d4;
    
    updateDisplay();
}

void autoBrightness() {
    // Increase/decrease brightness by 1 PWM every minute at the specified time
    if(now.hour() >= timeStartDecrease || now.hour() < timeStartIncrease) {
        brightness = max((int16_t)(brightness - 1), (int16_t)minBrightness);
    } else {
        brightness = min((int16_t)(brightness + 1), (int16_t)maxBrightness);
    }
    
    // Ensure brightness stays within valid range
    brightness = constrain(brightness, minBrightness, maxBrightness);
    analogWrite(brightnessPin, brightness);
}

void shortCathodePoisoningPrevention() {
    // Cycles through all digits to clean less used cathodes (duration ~15s)
    // Short version is mainly for visual effect, real cleaning in long routine
    for(uint8_t i = 0; i < 10; i++) {
        for(uint8_t j = 5; j < 15; j++) {
            d1 = (j - 5);         // 0 - 1 ... 9
            d2 = j % 10;          // 5 - 6 .... 4
            d3 = (14 - j);        // 9 - 8 .... 0
            d4 = (19 - j) % 10;   // 4 - 3 ... 5
            updateDisplay();
            delay(100);
        }
    }
}

void longCathodePoisoningPrevention() {
    // Cycles through all digits to clean less used cathodes
    // Done once a day at 3am, lasts 30min
    for(uint8_t i = 0; i < 180; i++) {
        for(uint8_t j = 0; j < 10; j++) {
            d1 = j;
            d2 = j;
            d3 = j;
            d4 = j;
            updateDisplay();
            delay(1000);
        }
    }
}

void checkDateTimeChange() {
    // Monitor for RTC minute change and update display if necessary
    // This method keeps the display within 500ms of the RTC module
    
    if(millis() - lastTimeCheck > TIME_CHECK_INTERVAL_MS) {
        now = rtc.now();
        lastTimeCheck = millis();
        
        if(now.minute() != minute) {
            // Minute has changed, update display
            minute = now.minute();
            
            // Rest for 59 seconds before next frequent check
            lastTimeCheck += MINUTE_REST_MS;
            
            // Run cathode poisoning prevention
            if(minute == 0) {
                if(now.hour() != CLEANING_HOUR) {
                    // Every hour except cleaning hour
                    shortCathodePoisoningPrevention();
                } else {
                    // At cleaning hour (3am)
                    longCathodePoisoningPrevention();
                }
            }
            
            generateDisplay(now);
            
            // Adjust brightness if necessary
            autoBrightness();
        }
    }
}

void shortMinuteButtonAction() {
    // Increment minute by 1
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), 
                       now.hour(), (now.minute() + 1) % 60, 0));
    
    // Stop any blinking (time has been set)
    stopBlinking();
    
    now = rtc.now();
    minute = now.minute();
    generateDisplay(now);
}

void shortHourButtonAction() {
    // Increment hour by 1
    rtc.adjust(now + TimeSpan(0, 1, 0, 0)); // Add 1 hour properly
    
    // Stop any blinking (time has been set)
    stopBlinking();
    
    now = rtc.now();
    generateDisplay(now);
}

void buttonsProcess() {
    // Minute Button handling with debouncing and auto-repeat
    if(digitalRead(minuteButtonPin) == HIGH) {
        if(!minuteButtonPressed) {
            // Button pressed for the first time
            minuteButtonPressed = true;
            timeMinuteButtonPressed = millis();
            lastMinuteRepeat = millis();
        }
        
        // Check for long press (auto-repeat)
        if(millis() - timeMinuteButtonPressed >= LONG_PRESS_TIME_MS) {
            if(millis() - lastMinuteRepeat >= BUTTON_REPEAT_RATE_MS) {
                shortMinuteButtonAction();
                lastMinuteRepeat = millis();
            }
        }
    } else {
        // Button released
        if(minuteButtonPressed && millis() - timeMinuteButtonPressed >= DEBOUNCE_DELAY_MS) {
            // Only trigger if not already in auto-repeat mode
            if(millis() - timeMinuteButtonPressed < LONG_PRESS_TIME_MS) {
                shortMinuteButtonAction();
            }
        }
        minuteButtonPressed = false;
    }
    
    // Hour Button handling with debouncing and auto-repeat
    if(digitalRead(hourButtonPin) == HIGH) {
        if(!hourButtonPressed) {
            // Button pressed for the first time
            hourButtonPressed = true;
            timeHourButtonPressed = millis();
            lastHourRepeat = millis();
        }
        
        // Check for long press (auto-repeat)
        if(millis() - timeHourButtonPressed >= LONG_PRESS_TIME_MS) {
            if(millis() - lastHourRepeat >= BUTTON_REPEAT_RATE_MS) {
                shortHourButtonAction();
                lastHourRepeat = millis();
            }
        }
    } else {
        // Button released
        if(hourButtonPressed && millis() - timeHourButtonPressed >= DEBOUNCE_DELAY_MS) {
            // Only trigger if not already in auto-repeat mode
            if(millis() - timeHourButtonPressed < LONG_PRESS_TIME_MS) {
                shortHourButtonAction();
            }
        }
        hourButtonPressed = false;
    }
}

void setup() {
    SPI.begin();
    
    // Configure pins
    pinMode(minuteButtonPin, INPUT);
    pinMode(hourButtonPin, INPUT);
    pinMode(brightnessPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    
    // Set initial brightness
    analogWrite(brightnessPin, brightness);
    
    // Startup bulb check (cycling all digits)
    for(int8_t i = 9; i >= 0; i--) {
        d1 = i;
        d2 = i;
        d3 = i;
        d4 = i;
        updateDisplay();
        
        if(i == 9) {
            // Give more time on digit 9 to let user see it after plugging in
            delay(1000);
        } else {
            delay(500);
        }
    }
    
    // Initialize RTC
    if(!rtc.begin()) {
        // RTC communication error - show "88:88" blinking
        stored_d1 = 8;
        stored_d2 = 8;
        stored_d3 = 8;
        stored_d4 = 8;
        blink_d1 = true;
        blink_d2 = true;
        blink_d3 = true;
        blink_d4 = true;
        
        // Stay in error state
        while(1) {
            makeBlink();
            delay(10);
        }
    }
    
    // Check if RTC lost power
    if(rtc.lostPower()) {
        // RTC battery dead or removed - show "00:00" blinking
        blink_d1 = true;
        blink_d2 = true;
        blink_d3 = true;
        blink_d4 = true;
        
        // Set a default time (will blink until user sets time)
        rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
    }
    
    // Get and display the time
    now = rtc.now();
    minute = now.minute();
    generateDisplay(now);
}

void loop() {
    checkDateTimeChange();
    buttonsProcess();
    makeBlink();
    
    delay(10);
}
