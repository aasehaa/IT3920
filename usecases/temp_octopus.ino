#include "octopus.h"

unsigned long previousMillis = 0;
const long interval = 1000; // Interval in milliseconds
unsigned long blinkInterval = 100; // Blinking interval in milliseconds
unsigned long lastBlinkMillis = 0;
bool isBlinkOn = false;

// Button state variables
const int buttonPin = 7;  // Pin connected to the button
bool deviceOn = false; // Device state
bool longPressHandled = false; // To ensure long press is handled once
unsigned long buttonPressTime = 0;
const unsigned long longPressDuration = 2000; // Duration to consider as long press (2000ms)

// Define the number of records per file
const int RECORDS_PER_FILE = 100;

const int vbatPin = A0;         // Pin connected to VBAT_MEAS
const int chargeStatePin = 7;   // Pin connected to Charge_state

// Temperature thresholds
const float coldThreshold = 20.0; // Below 20°C is considered cold
const float hotThreshold = 25.0;  // Above 25°C is considered hot

void setup() {
Serial.begin(9600);
while (!Serial);

// Display welcome message
Serial.println("Welcome to Octopus Device\nA project by MIT\nHappy Hacking!\n");

// Initialize sensors
Serial.println("Initializing sensors...");
if (!Octopus::initializeSensors()) {
    Serial.println("Failed to initialize HS300x sensors.");
    while (1);
}
if (!Octopus::initializeSPS30()) {
    Serial.println("Failed to initialize SPS30 sensor.");
    while (1);
}
Serial.println("Sensors initialized.");

Octopus::setInterval(interval); // sets the interval for data logging

// Begin continuous reading of all sensors
Serial.println("Starting data collection...");
if (!Octopus::start()) {
    Serial.println("Failed to start data collection.");
    while (1);
}
Serial.println("Data collection started.");

// Initialize SD card
initSD(RECORDS_PER_FILE);
Serial.println("SD card initialized.");

// Initialize battery monitoring and RGB LED
initBatteryMonitoring();

// Initialize button
pinMode(buttonPin, INPUT_PULLUP); // Set the button pin as an input with internal pull-up resistor
}

void loop() {
unsigned long currentMillis = millis();

// Button handling
int buttonState = digitalRead(buttonPin);
if (buttonState == LOW) {
    if (buttonPressTime == 0) {
        buttonPressTime = millis(); // Record the time when the button is pressed
    }

    // Check for long press
    if ((millis() - buttonPressTime) >= longPressDuration) {
        if (!longPressHandled) {
            deviceOn = false;
            Serial.println("Device turned off");
            setDotStarColor(0, 0, 0); // Turn off LED
            Octopus::stopSPS30(); // Stop SPS30 measurement
            delay(100); // Debounce delay
            longPressHandled = true;
        }
    }
} else {
    // Button released
    if (buttonPressTime != 0) {
        if (!longPressHandled) {
            // Short press
            deviceOn = true;
            Serial.println("Device turned on");
            // Reinitialize components when device turns on
            initSD(RECORDS_PER_FILE);
            initBatteryMonitoring();
            Octopus::initializeSPS30(); // Start SPS30 measurement
        }
        buttonPressTime = 0; // Reset button press time
        longPressHandled = false; // Reset long press handled flag
        // Debounce delay
        delay(50);
    }
}

if (!deviceOn) {
    // Device is turned off, skip the rest of the loop
    delay(100);
    return;
}

if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Save the last time data was saved

    // Read all the sensor values
    float temperature = Octopus::readTemperature();
    float humidity = Octopus::readHumidity();

    // Read SPS30 data
    float pm1_0 = 0, pm2_5 = 0, pm4_0 = 0, pm10_0 = 0;
    if (!Octopus::readSPS30Data(pm1_0, pm2_5, pm4_0, pm10_0)) {
        Serial.println("Failed to read SPS30 data");
    }

    // Get current time
    unsigned long currentTime = millis();
    unsigned long seconds = currentTime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    // Format time
    String timestamp = String(hours) + ":" + String(minutes % 60) + ":" + String(seconds % 60);

    // Print time and sensor values
    Serial.print("Time: ");
    Serial.println(timestamp);

    Serial.print("Temperature = ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity = ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("PM1.0 = ");
    Serial.print(pm1_0);
    Serial.println(" µg/m³");

    Serial.print("PM2.5 = ");
    Serial.print(pm2_5);
    Serial.println(" µg/m³");

    Serial.print("PM4.0 = ");
    Serial.print(pm4_0);
    Serial.println(" µg/m³");

    Serial.print("PM10.0 = ");
    Serial.print(pm10_0);
    Serial.println(" µg/m³");

    // Battery monitoring and RGB LED control
    int vbatRaw = analogRead(vbatPin);
    float vbatVoltage = vbatRaw * (3.294 / 1023.0) * 1.279; // Adjust the scaling factor if needed
    bool chargeState = digitalRead(chargeStatePin);
    bool batteryConnected = vbatVoltage > 2.5;
    float batteryPercentage = batteryConnected ? calculateBatteryPercentage(vbatVoltage) : 0.0;

    // Set RGB LED based on temperature
    if (temperature < coldThreshold) {
        setDotStarColor(0, 0, 255); // Blue for cold
    } else {
        setDotStarColor(128, 0, 128); // Purple for moderate or hot
    }

    // Blink red LED for low battery or no battery
    if (vbatVoltage < 2.5 || !batteryConnected) {
        if (currentMillis - lastBlinkMillis >= blinkInterval) {
            lastBlinkMillis = currentMillis;
            isBlinkOn = !isBlinkOn;
            if (isBlinkOn) {
                setDotStarColor(255, 0, 0); // Red
            } else {
                setDotStarColor(0, 0, 0); // Off
            }
        }
    }

    // Log data to SD card
    String data = timestamp + "," + temperature + "," + humidity + "," + pm1_0 + "," + pm2_5 + "," + pm4_0 + "," + pm10_0 + "," + vbatVoltage + "," + (chargeState ? "1" : "0");
    logToSD(data);

    // Print the battery and charge state information
    Serial.print("VBAT Voltage: ");
    Serial.print(vbatVoltage, 2);
    Serial.print(" V, Charge State: ");
    Serial.print(chargeState ? "Charging" : "Not Charging");
    Serial.print(", Battery Percentage: ");
    Serial.print(batteryPercentage, 1);
    Serial.println(" %");

    // Print an empty line
    Serial.println();
}

// Wait for a short time before the next iteration
delay(100); // You can adjust this delay according to your needs
}
