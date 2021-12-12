#include "Arduino.h"
#include "RTClib.h" //v1.12.4
#include "SPI.h"  // HV5530 interfacing

#define brightnessSensorPin A2
#define plusButtonPin 7
#define minusButtonPin 6
#define modeButtonPin 5
#define longPressTime 1000
#define brightnessPin 9 // PWM output to nixies
#define latchPin 10

#define blinkingTime 800


// Time variables
RTC_DS3231      rtc;
DateTime        now;
uint8_t         minute  = 0;    // Used to monitor RTC minute change
unsigned long   lastTimeCheck = 0;


// Blinking variables
unsigned long lastBlink = 0;


// Brightness variables
uint8_t       minBrightness = 15;     // Min PWM 95
uint8_t       maxBrightness = 240;    // Max PWM 255
int16_t       brightness = maxBrightness;     //Current brightness value
int16_t       brightnessCorrection = 0; // Manual correction of brightness
uint8_t       timeStartDecrease = 19;   //Time at which PWM signal starts decreasing
uint8_t       timeStartIncrease = 6;    //Time at which PWM signal starts increasing


// Switches
const int debounceDelay = 50;
bool modeButtonPressed = false;  // True if button physical press detected
bool modeButtonActioned = false;  // True if action associated with button executed for current press
unsigned long timeModeButtonPressed = 0;  // Used for debouncing and short/long press handling

bool plusButtonPressed = false;  // True if button physical press detected
unsigned long timePlusButtonPressed = 0;  // Used for debouncing and short/long press handling

bool minusButtonPressed = false;  // True if button physical press detected
unsigned long timeMinusButtonPressed = 0;  // Used for debouncing and short/long press handling


/* Display Mode 
    0 = Time / Set HOUR
    1 = Time / Set MINUTE
    2 = Date / Set DAY
    3 = Date / Set MONTH
    4 = Year / Set YEAR
*/

uint8_t displayMode = 0;
uint8_t setMode = 0;

// Hardware wiring of [nixies digit 0-9] to corresponding {HV5530 pin}
//Driver 1
const uint8_t digit_1[10] = {22, 25, 24, 19, 16, 12, 14, 15, 18, 21};
const uint8_t digit_2[10] = { 6, 23, 20, 17, 13, 11, 10,  9,  8,  7};
//Driver 2
const uint8_t digit_3[10] = {13, 26, 25, 24, 23, 22, 21, 19, 15, 12};
const uint8_t digit_4[10] = { 7, 10, 11, 16, 17, 18, 20, 14,  9,  8};

// Each digits. Anything >= 10 would be blank
uint8_t d1 = 0;
uint8_t d2 = 0;
uint8_t d3 = 0;
uint8_t d4 = 0;

// Stored digit values when blinking
uint8_t _d1 = 0;
uint8_t _d2 = 0;
uint8_t _d3 = 0;
uint8_t _d4 = 0;

// Which digits to blink
uint8_t blink_d1 = 0;
uint8_t blink_d2 = 0;
uint8_t blink_d3 = 0;
uint8_t blink_d4 = 0;


// Data to be shifted to HV5530 drivers
uint32_t data1 = 0x00000000;
uint32_t data2 = 0x00000000;

// SPI Protocol
SPISettings HV5530(1000000, MSBFIRST, SPI_MODE1); // was 100000


void updateDisplay() {
    // Sends individual digits to the display

    data1 = 0x00000000;
    if(d1<10) { data1 |=  1UL << (digit_1[d1]-1); }
    if(d2<10) { data1 |=  1UL << (digit_2[d2]-1); }

    data2 = 0x00000000;
    if(d3<10) { data2 |=  1UL << (digit_3[d3]-1); }
    if(d4<10) { data2 |=  1UL << (digit_4[d4]-1); }


    //Serial.println(data2, BIN);
    //Serial.println(data1, BIN);
    //Serial.println();
    // if(d1<10) {Serial.print(d1); } else { Serial.print(" "); }
    // if(d2<10) {Serial.print(d2); } else { Serial.print(" "); }
    // if(d3<10) {Serial.print(d3); } else { Serial.print(" "); }
    // if(d4<10) {Serial.println(d4); } else { Serial.println(" "); }


    // Update Nixie tubes with correct display 8 bits at a time
    SPI.beginTransaction(HV5530);
    digitalWrite(latchPin, HIGH);
    SPI.transfer(data2>>24);
    SPI.transfer(data2>>16);
    SPI.transfer(data2>>8);
    SPI.transfer(data2);
    SPI.transfer(data1>>24);
    SPI.transfer(data1>>16);
    SPI.transfer(data1>>8);
    SPI.transfer(data1);
    digitalWrite(latchPin, LOW); //logic-low pulse to latch
    digitalWrite(latchPin, HIGH);
    SPI.endTransaction();
}

void generateDisplay(DateTime dt) {
  // Generates the required individual digits

  switch (displayMode)
        {
        case 2: case 3:
        //Date mode (DDMM)
        if(d1<10) { d1 = (dt.day()/10)%10; }    else { _d1 = (dt.day()/10)%10; }
        if(d2<10) { d2 = dt.day()%10; }         else { _d2 = dt.day()%10; }
        if(d3<10) { d3 = (dt.month()/10)%10; }  else { _d3 = (dt.month()/10)%10; }
        if(d4<10) { d4 = dt.month()%10; }       else { _d4 = dt.month()%10; }

        break;

        case 4:
        //Year mode (YYYY)
        if(d1<10) { d1 = dt.year()/1000; }      else { _d1 = dt.year()/1000; }
        if(d2<10) { d2 = (dt.year()/100)%10; }  else { _d2 = (dt.year()/100)%10; }
        if(d3<10) { d3 = (dt.year()/10)%10; }   else { _d3 = (dt.year()/10)%10; }
        if(d4<10) { d4 = dt.year()%10; }        else { _d4 = dt.year()%10; }

        break;

        case 0: case 1: default:
        //Time mode (HHMM)
        if(d1<10) { d1 = dt.hour()/10; }    else { _d1 = dt.hour()/10; }
        if(d2<10) { d2 = dt.hour()%10; }    else { _d2 = dt.hour()%10; }
        if(d3<10) { d3 = dt.minute()/10; }  else { _d3 = dt.minute()/10; }
        if(d4<10) { d4 = dt.minute()%10; }  else { _d4 = dt.minute()%10; }

    
        break;
        }

    updateDisplay();
}

void autoBrightness() {
    // Increase/decreases brightness by 1 PWM every minute at the specified time and keep between min/max values
    if(now.hour() >= timeStartDecrease || now.hour() < timeStartIncrease){
        brightness = max(brightness-1, minBrightness);
        analogWrite(brightnessPin, brightness);
    } else {
        brightness = min(brightness+1, maxBrightness);
        analogWrite(brightnessPin, brightness);
    }
}

void cathodePoisoningPrevention() {
    // Cycles through all digits to clean less used cathodes
    for(uint8_t i=0; i<10; i++) {
        // Run 10 times the following
        for(uint8_t j=5; j<15; j++){
            d1=(j-5);       // 0 - 1 ... 9
            d2=j%10;        // 5 - 6 .... 4
            d3=(14-j);      // 9 - 8 .... 0
            d4=(19-j)%10;   // 4 - 3 ... 5
            updateDisplay();
            delay(100);
        }
    }

}

void checkDateTimeChange() {
  // Monitor for RTC minute change and updates display if necessary
  // This methods allows the display to stay within 500ms of the RTC module
  
  if((unsigned long) (millis()-lastTimeCheck) > 500) {
    // Check frequently (500ms) until next minute change
    now = rtc.now();
    lastTimeCheck = millis();
    //Serial.print(".");

    if(now.minute()!= minute){
      // Minute has changed, update display and rest for 59sec before next time check
      minute = now.minute();
      lastTimeCheck += 59000;
      if(minute%5 == 0 && !setMode) {
          //Every 5 minutes
          cathodePoisoningPrevention();
      }
      generateDisplay(now);

      // Change brightness if necessary
      autoBrightness();
    }
  }
}

void resetBlink() {
    // Resets the blink timing and digits values
    
    // Restore digit values
    d1 = (d1<10)?d1:_d1;
    d2 = (d2<10)?d2:_d2;
    d3 = (d3<10)?d3:_d3;
    d4 = (d4<10)?d4:_d4;

    lastBlink = millis();

    //updateDisplay();
}

void changeBlink() {
    //Make blink one or several individual digits
    
    resetBlink();

    if(setMode) {
        switch (displayMode)
            {
            case 0: case 2:
                blink_d1 = 1;
                blink_d2 = 1;
                blink_d3 = 0;
                blink_d4 = 0;
                break;
            case 1: case 3:
                blink_d1 = 0;
                blink_d2 = 0;
                blink_d3 = 1;
                blink_d4 = 1;
                break;
            case 4:
                blink_d1 = 1;
                blink_d2 = 1;
                blink_d3 = 1;
                blink_d4 = 1;
                break;
            default:    //bug, normally not shown
                blink_d1 = 0;
                blink_d2 = 0;
                blink_d3 = 0;
                blink_d4 = 0;
                break;
            }
    } else {
        /* setMode = 0 */
        blink_d1 = 0;
        blink_d2 = 0;
        blink_d3 = 0;
        blink_d4 = 0;
    }

}

void makeBlink() {
    
    // Check if any digits to blink
    if(blink_d1 || blink_d2 || blink_d3 || blink_d4){
        if((unsigned long) (millis()-lastBlink) > blinkingTime) {
            lastBlink = millis();
            // Toggle digits
            if(blink_d1){
                if(d1<10){
                    _d1 = d1;
                    d1 = 10;
                } else {
                    d1 = _d1;
                }
            }

            if(blink_d2){
                if(d2<10){
                    _d2 = d2;
                    d2 = 10;
                } else {
                    d2 = _d2;
                }
            }

            if(blink_d3){
                if(d3<10){
                    _d3 = d3;
                    d3 = 10;
                } else {
                    d3 = _d3;
                }
            }

            if(blink_d4){
                if(d4<10){
                    _d4 = d4;
                    d4 = 10;
                } else {
                    d4 = _d4;
                }
            }
        updateDisplay();
        }
    }
}

void shortPlusButtonAction() {
    // Short Plus Button press detected and debounced
    if(setMode) {
        switch (displayMode)
        {
        case 0:
            /* Set Hours */
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour()+1)%24, now.minute(), 0));
            break;
        case 1:
            /* Set Minute */
            minute = (now.minute()+1)%60;
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), minute, 0));
            break;
        case 2:
            /* Set Day */
            rtc.adjust(now + TimeSpan(1,0,0,0));
            break;
        case 3:
            /* Set Month */
            rtc.adjust(DateTime(now.year(), (now.month()%12)+1, now.day(), now.hour(), now.minute(), 0));
            break;
        case 4:
            /* Set Year */
            rtc.adjust(DateTime(now.year()+1, now.month(), now.day(), now.hour(), now.minute(), 0));
            break;
        default:
            break;
        }
        // Display changes
        resetBlink();
        now = rtc.now();
        generateDisplay(now);
    } else {
        // Adjust brightness
        if(brightness == minBrightness) {
            // Try to increasing min brightness value 
            minBrightness = min(minBrightness+10, maxBrightness-10);  // maxBrightness+10 is the max minBrightness Value
        }
        if(brightness == maxBrightness) {
            // Try increasing the max brightness value if possible
            maxBrightness = min(maxBrightness+10, 255); //255 is the absolute max value
        }
        brightness = constrain(brightness+10, minBrightness, maxBrightness);
        analogWrite(brightnessPin, brightness);
    }

}

void shortMinusButtonAction() {
    // Short Minus Button press detected and debounced
    if(setMode) {
        switch (displayMode)
        {
        case 0:
            /* Set Hours */
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour()-1)%24, now.minute(), 0));
            break;
        case 1:
            /* Set Minute */
            minute = (now.minute()>0)?(now.minute()-1):59;
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), minute, 0));
            break;
        case 2:
            /* Set Day */
            rtc.adjust(now - TimeSpan(1,0,0,0));
            break;
        case 3:
            /* Set Month */

            rtc.adjust(DateTime(now.year(), (now.month()>1)?(now.month()-1):12, now.day(), now.hour(), now.minute(), 0));
            break;
        case 4:
            /* Set Year */
            rtc.adjust(DateTime(now.year()-1, now.month(), now.day(), now.hour(), now.minute(), 0));
            break;
        default:
            break;
        }

        resetBlink();
        now = rtc.now();
        generateDisplay(now);
    } else {
        // Adjust brightness
        if(brightness == minBrightness) {
            // Try to lower min brightness value if possibe
            minBrightness = max(minBrightness-10, 15);  // 15 is absolute min value
        }
        if(brightness == maxBrightness) {
            // Lower the max brightness value
            maxBrightness = max(maxBrightness-10, minBrightness+10); //minBrightness+10 is the minimal maxBrightness value
        }
        brightness = constrain(brightness-10, minBrightness, maxBrightness);
        analogWrite(brightnessPin, brightness);
    }
}

void shortModeButtonAction() {
    // Short Mode Button press detected, change mode
    if(setMode) {
        displayMode = (displayMode + 1)%5;

    } else {
        if(displayMode%2) {
            displayMode = (displayMode+1)%5;
        } else {
            displayMode = (displayMode+2)%6;  // 0 -> 2 -> 4 -> 0...
        }
    }
    changeBlink();
    // Update the display
    generateDisplay(now);
}

void longModeButtonAction() {
    //Long Mode Button press detected -> enter or exit set mode
    if (setMode)
    {
        // Exit SET mode
        setMode = 0;

    } else {
        // Enter SET mode
        setMode = 1;
    }
    changeBlink();
    generateDisplay(now);
}

void buttonsProcess() {
    // Mode Button handling and software debouncing
    if (digitalRead(modeButtonPin) == HIGH)
    {
        if(modeButtonPressed == false)
        {
            // Button pressed detected for the first time
            modeButtonPressed = true;
            timeModeButtonPressed = millis();
        }
        if((unsigned long) (millis() - timeModeButtonPressed) >= longPressTime && modeButtonActioned == false)
        {
            longModeButtonAction();
            modeButtonActioned = true;  // prevent entering the short push routine below
        }
    } else {
        if(modeButtonPressed && !modeButtonActioned && (unsigned long) (millis() - timeModeButtonPressed) >= debounceDelay)
        {
            shortModeButtonAction();
        }   
    modeButtonPressed = false;
    modeButtonActioned = false;
    }


    // Plus Button handling and software debouncing
    if (digitalRead(plusButtonPin) == HIGH)
    {
        if(plusButtonPressed == false)
        {
            // Button pressed detected for the first time
            plusButtonPressed = true;
            timePlusButtonPressed = millis();
        }

        if((unsigned long) (millis() - timePlusButtonPressed) >= longPressTime) {
            // continuous long press action
            shortPlusButtonAction();
            delay(100);
        }

    } else {
        if(plusButtonPressed && (unsigned long) (millis() - timePlusButtonPressed) >= debounceDelay)
        {
            shortPlusButtonAction();
  
        }   
    plusButtonPressed = false;
    }


    // Minus Button handling and software debouncing
    if (digitalRead(minusButtonPin) == HIGH)
    {
        if(minusButtonPressed == false)
        {
            // Button pressed detected for the first time
            minusButtonPressed = true;
            timeMinusButtonPressed = millis();
        }

        if((unsigned long) (millis() - timeMinusButtonPressed) >= longPressTime) {
            // continuous long press action
            shortMinusButtonAction();
            delay(100);
        }
    } else {
        if(minusButtonPressed && (unsigned long) (millis() - timeMinusButtonPressed) >= debounceDelay)
        {
            shortMinusButtonAction();
  
        }   
    minusButtonPressed = false;
    }
    
}

void setup () {

    SPI.begin();

    // Defining pins function
    pinMode(brightnessSensorPin, INPUT);
    pinMode(plusButtonPin, INPUT);
    pinMode(minusButtonPin, INPUT);
    pinMode(modeButtonPin, INPUT);

    pinMode(brightnessPin, OUTPUT);
    analogWrite(brightnessPin, brightness);  // Max brightness at startup


    // Startup bulb check (cycling all digits 1 second)
    for (int8_t i = 9 ; i >= 0; i--) {
        d1 = i;
        d2 = i;
        d3 = i;
        d4 = i;
        updateDisplay();

        if(i==9) {
            // Give more time on the digit 9 to let user a chance to see it after plugging the device in
            delay(1000);
        } else {
            delay(500);
        }
    }

    rtc.begin();

    if (rtc.lostPower()) {
        //Serial.println("RTC lost power !");
        // When time needs to be set on a new device, or after a power loss
        blink_d1 = 1;
        blink_d2 = 1;
        blink_d3 = 1;
        blink_d4 = 1;
    }

    // Get and display the time
    now = rtc.now();
    minute=now.minute();
    generateDisplay(now);
}

void loop () {

    checkDateTimeChange();
    buttonsProcess();
    makeBlink();

    delay(10);

}