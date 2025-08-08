/* 
 * Project: Scale with HX711, LCD, and TM1637 7-Segment Display
 * Developers: Kalpesh Solanki (https://kalpeshsolanki.me) and Nilesh Khandar
 * Description: This project involves creating a digital scale using an LOADCELL, STM32F103C8T6, HX711 module, LCD display, 
 *              and TM1637 7-segment display. The scale is calibrated using a known weight and can
 *              be reset using buttons. The weight is displayed on both the LCD and TM1637 displays.
 * Date: July 14, 2024
 *
 * Working:
 * 1. The HX711 module reads data from a load cell to measure weight.
 * 2. The TM1637 7-segment display and LCD display show the measured weight.
 * 3. Buttons are used for calibration and reset functionalities.
 * 4. The calibration button allows the scale to be calibrated using a known weight.
 * 5. The reset button resets the tare value to zero.
 * 6. Interrupts are used for button presses to ensure immediate response.
 * 7. The display is updated every second to show the current weight.
 *
 * Code Working:
 * - The Scale class encapsulates the functionality for the scale, including reading from the HX711, 
 *   displaying data on the TM1637 and LCD, and handling calibration and reset.
 * - The setup() function initializes the displays, HX711, and buttons.
 * - The loop() function continuously checks for button presses and updates the display.
 * - Interrupt service routines handle button presses to set flags.
 * - The tareScale() function averages multiple readings to get the tare value.
 * - The getWeight() function calculates the weight by applying the calibration factor to the raw HX711 readings.
 * - The calibrateScale() function prompts the user to place a known weight and adjusts the tare value accordingly.
 * - The resetScale() function clears the tare value and reinitializes the scale.
 */

#include <LiquidCrystal.h>  // Include the LiquidCrystal library for LCD
#include <HX711.h>          // Include the HX711 library for load cell
#include "TM1637_6D.h"      // Include the TM1637 library for 7-segment display

// Pin definitions for TM1637, can be changed to other ports
#define CLK PC15  // Clock pin for TM1637
#define DIO PC14  // Data pin for TM1637

// Define the pins for HX711 module
#define DT PA6    // Data pin for HX711
#define SCK PA7   // Clock pin for HX711

// Define pins for calibration and reset buttons
#define CALIBRATE_BUTTON PB0  // Calibration button pin
#define RESET_BUTTON PB1      // Reset button pin

// Define constants for tare calibration
const int TARE_SAMPLES = 10;        // Number of samples for tare
const float KNOWN_WEIGHT = 200.0;   // Known weight for calibration in grams
const long RAW_0G = 16773520;       // Raw value for 0 grams
const long RAW_100G = 16758735;     // Raw value for 100 grams
const float WEIGHT_DIFFERENCE = 100.0;   // Weight difference in grams
const long COUNT_DIFFERENCE = RAW_0G - RAW_100G;  // Count difference for calibration
const float CALIBRATION_FACTOR = WEIGHT_DIFFERENCE / COUNT_DIFFERENCE;  // Calibration factor
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;  // Display update interval in milliseconds

// Volatile flags for button presses
volatile bool calibratePressed = false;  // Flag for calibration button press
volatile bool resetPressed = false;      // Flag for reset button press

// Scale class definition
class Scale {
private:
  LiquidCrystal lcd;         // LCD object
  TM1637_6D tm1637;          // TM1637 object
  HX711 scale;               // HX711 object
  long tare;                 // Tare value
  unsigned long lastUpdateTime; // Last time display was updated

public:
  // Constructor
  Scale() : lcd(PA0, PA1, PA2, PA3, PA4, PA5), tm1637(CLK, DIO) {
    tare = 0;                 // Initialize tare value
    lastUpdateTime = 0;       // Initialize last update time
  }

  // Setup function
  void setup() {
    tm1637.init();           // Initialize TM1637 display
    tm1637.set(BRIGHT_TYPICAL); // Set TM1637 brightness
    lcd.begin(16, 2);        // Initialize LCD with 16 columns and 2 rows

    // Initialize HX711
    scale.begin(DT, SCK);

    // Initialize buttons with interrupts
    pinMode(CALIBRATE_BUTTON, INPUT_PULLUP);  // Set calibration button as input with pull-up
    pinMode(RESET_BUTTON, INPUT_PULLUP);      // Set reset button as input with pull-up
    attachInterrupt(digitalPinToInterrupt(CALIBRATE_BUTTON), onCalibrateButtonPress, FALLING); // Attach interrupt to calibration button
    attachInterrupt(digitalPinToInterrupt(RESET_BUTTON), onResetButtonPress, FALLING);         // Attach interrupt to reset button

    // Display initial message
    lcd.setCursor(0, 0);
    lcd.print("HX711 Calibrate");
    lcd.setCursor(0, 1);
    lcd.print("No weight");
    delay(2000);              // Wait for 2 seconds
    lcd.clear();              // Clear LCD
    tareScale();              // Perform tare
    lcd.setCursor(0, 0);
    lcd.print("Calibrating...");
    delay(2000);              // Wait for 2 seconds
    lcd.clear();              // Clear LCD
  }

  // Loop function
  void loop() {
    unsigned long currentTime = millis(); // Get current time

    // Check if the calibration button was pressed
    if (calibratePressed) {
      calibrateScale();       // Calibrate scale
      calibratePressed = false; // Reset the flag
    }

    // Check if the reset button was pressed
    if (resetPressed) {
      resetScale();           // Reset scale
      resetPressed = false;   // Reset the flag
      return;                 // Skip the rest of the loop to ensure immediate reset
    }

    // Update display every second
    if (currentTime - lastUpdateTime >= DISPLAY_UPDATE_INTERVAL) {
      updateDisplay();        // Update display
      lastUpdateTime = currentTime; // Update last update time
    }
  }

private:
  // Perform tare
  void tareScale() {
    long sum = 0;             // Initialize sum
    for (int i = 0; i < TARE_SAMPLES; i++) {
      sum += scale.read();    // Read HX711 value and add to sum
    }
    tare = sum / TARE_SAMPLES; // Calculate tare value
  }

  // Get weight
  float getWeight() {
    long raw = scale.read();  // Read HX711 value
    return (float)(raw - tare) * CALIBRATION_FACTOR * (-1); // Calculate weight
  }

  // Calibrate scale
  void calibrateScale() {
    lcd.clear();              // Clear LCD
    lcd.setCursor(0, 0);
    lcd.print("Put 200g Weight");

    unsigned long startTime = millis(); // Get current time
    while (millis() - startTime < 5000) { // Wait for 5 seconds
      if (resetPressed) {
        resetScale();         // Reset scale if reset button is pressed
        resetPressed = false; // Reset the flag
        return;               // Skip calibration if reset is pressed
      }
    }

    long sum = 0;             // Initialize sum
    for (int i = 0; i < TARE_SAMPLES; i++) {
      sum += scale.read();    // Read HX711 value and add to sum
    }
    long raw_known_weight = sum / TARE_SAMPLES; // Calculate raw known weight

    // Adjust tare based on known weight
    tare = raw_known_weight - (KNOWN_WEIGHT / CALIBRATION_FACTOR);
    tm1637.clearDisplay();    // Clear TM1637 display
    lcd.clear();              // Clear LCD
    lcd.setCursor(0, 0);
    lcd.print("Calibration Done");
    delay(2000);              // Wait for 2 seconds
    lcd.clear();              // Clear LCD
  }

  // Reset scale
  void resetScale() {
    lcd.clear();              // Clear LCD
    tm1637.clearDisplay();    // Clear TM1637 display
    lcd.setCursor(0, 0);
    lcd.print("Resetting...");
    tare = 0;                 // Reset tare value
    delay(2000);              // Wait for 2 seconds
    lcd.clear();              // Clear LCD
    lcd.setCursor(0, 0);
    lcd.print("Reset Done");
    delay(2000);              // Wait for 2 seconds
    lcd.clear();              // Clear LCD
    tareScale();              // Retare the scale after reset
  }

  // Update display
  void updateDisplay() {
    float weight = getWeight(); // Get weight
    tm1637.displayFloat(weight); // Display weight on TM1637
    lcd.setCursor(0, 0);
    lcd.print("Weight: ");
    lcd.print(weight, 2);    // Print weight with 2 decimal places on LCD
    lcd.print(" KG");
    lcd.setCursor(0, 1);
    lcd.print("Count: ");
    lcd.print(scale.read()); // Print raw HX711 value on LCD
  }

  // Interrupt service routine for calibration button press
  static void onCalibrateButtonPress() {
    calibratePressed = true; // Set calibration flag
  }

  // Interrupt service routine for reset button press
  static void onResetButtonPress() {
    resetPressed = true;     // Set reset flag
  }
};

// Create a Scale object
Scale scale;

// Arduino setup function
void setup() {
  scale.setup();             // Setup scale
}

// Arduino loop function
void loop() {
  scale.loop();              // Loop scale
}
