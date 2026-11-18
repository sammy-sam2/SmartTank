#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int RED_PIN = 3;
const int GREEN_PIN = 5;
const int BLUE_PIN = 6;

const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

const int BUZZER_PIN = 11;

const int BUTTON_PIN = 12;

const int RELAY_PIN = 7;

//Relay configuration
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

//Tank calibration
const float EMPTY_DISTANCE = 30.0;
const float FULL_DISTANCE = 2.0;

const float USABLE_DISTANCE = EMPTY_DISTANCE - FULL_DISTANCE;

//Ultra sonic sensor configuration
const float US_ROUNDTRIP_PER_CM = 58.31;
const int NUM_SAMPLES = 5;
const unsigned long ECHO_TIMEOUT = 30000;
const unsigned long MIN_TRIGGER_INTERVAL = 100000;

//Variables for detecting sensor error
const unsigned long SENSOR_TIMEOUT = 2000;
unsigned long lastValidReadingTime = 0;
unsigned long systemStartTime = 0;

//Variable for controlling how ofter sensor error message is printed
bool sensorError = false;
bool errorMessagePrinted = false;

float latestDistance = 0.0; //Holds the averaged distance measurement

float waterLevel = 0.0; //Holds the calculated water level percentage

float totalDistance = 0; // Accumulates readings before averaging
int sampleCount = 0; //Tracks the number of readings collected

bool distanceAvailable = false; //Indicates whether a valid measurement is available

//ultrasonic sensor variables for getting distance
unsigned long lastTriggerTime = 0;
unsigned long startTime = 0;
unsigned long echoWaitStart = 0;

int buttonState = HIGH; //Stores the debounced/accepted button state
int previousButtonState = HIGH; // Stores the accepted previous button state
int lastButtonReading = HIGH; //Stores the raw previous button state

unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

//varriable for keeping track of the current displaying screen
byte currentScreen = 0;
bool screenChanged = true;
const byte NUM_SCREENS = 4;

//LCD object
LiquidCrystal_I2C lcd(0x27, 16, 2);

//LCD variables
const unsigned long LCD_REFRESH_INTERVAL = 500;
unsigned long lastLCDUpdate = 0;

//Pump controll varibles
const float PUMP_ON_THRESHOLD = 30.0;
const float PUMP_OFF_THRESHOLD = 80.0;

bool pumpRunning = false;

//RGB Error handling variables
const unsigned long ERROR_LED_INTERVAL = 500;
unsigned long lastErrorLEDUpdate = 0;
byte errorLEDState = 0;

bool previousSensorError = false;

//Buzzere Variables
const unsigned long BUZZER_ON_INTERVAL = 200;
const unsigned long BUZZER_OFF_INTERVAL = 800;

unsigned long lastBuzzerUpdate = 0;
bool buzzerState = false;


enum EchoState
{
  READY_TO_TRIGGER,
  WAITING_FOR_START,
  WAITING_FOR_END
};

EchoState echoState = READY_TO_TRIGGER;

// Enumeration for categorizing water levels
enum WaterLevelState
{
  WATER_LOW,
  WATER_MEDIUM,
  WATER_HIGH
};
WaterLevelState currentWaterLevel = WATER_LOW;

//Function for reading raw distance values
bool readDistance(float &distance)
{
  unsigned long currentMicros = micros();

  if (echoState == READY_TO_TRIGGER)
  {
    if (currentMicros - lastTriggerTime >= MIN_TRIGGER_INTERVAL)
    {
      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);

      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);

      digitalWrite(TRIG_PIN, LOW);

      lastTriggerTime = micros();
      echoWaitStart = lastTriggerTime;
      echoState = WAITING_FOR_START;
    }
    return false;

  }

  if (echoState == WAITING_FOR_START)
{
  if (micros() - echoWaitStart >= ECHO_TIMEOUT)
  {
    echoState = READY_TO_TRIGGER;
    return false;
  }

  if (digitalRead(ECHO_PIN) == HIGH)
  {
    startTime = micros();
    echoState = WAITING_FOR_END;
  }

  return false;
}

if (echoState == WAITING_FOR_END)
{
  if (micros() - startTime >= ECHO_TIMEOUT)
  {
    echoState = READY_TO_TRIGGER;
    return false;
  }

  if (digitalRead(ECHO_PIN) == LOW)
  {
    unsigned long endTime = micros();
    unsigned long duration = endTime - startTime;

    distance = duration / US_ROUNDTRIP_PER_CM;
    echoState = READY_TO_TRIGGER;

    return true;
  }
}
  return false;
}

// Function for averaging acquired raw distances from readDistance()
bool readAverageDistance(float &averageDistance)
{
  float rawDistance = 0;
  if(readDistance(rawDistance))
  {
    //If statement to ensure only values within the range are averaged
    if(rawDistance >= FULL_DISTANCE && rawDistance <= EMPTY_DISTANCE)
    {
      totalDistance += rawDistance;
      sampleCount++;
    }

    if(sampleCount >= NUM_SAMPLES)
    {
      averageDistance = totalDistance / sampleCount;
      sampleCount = 0; //Reset so that it is updated in the next cycle
      totalDistance = 0;
      return true;
    }
  }
  return false;
}

// Function for calculating water level
float calculateWaterLevel(float measuredDistance)
{
  float waterLevelPercentage = ((EMPTY_DISTANCE - measuredDistance) / USABLE_DISTANCE) * 100.0;
  waterLevelPercentage = constrain(waterLevelPercentage, 0.0, 100.0);

  return waterLevelPercentage;
}

//Function for classifying water level
WaterLevelState classifyWaterLevel(float percentage)
{
  if(percentage <= 30)
  {
    return WATER_LOW;
  }
  else if(percentage <= 70)
  {
    return WATER_MEDIUM;
  }
  else
  {
    return WATER_HIGH;
  }
}

//Function for water level text
 const char*getWaterLevelText(WaterLevelState state)
{
  switch(state)
  {
    case WATER_LOW:
    return "LOW";

    case WATER_MEDIUM:
    return "MEDIUM";

    case WATER_HIGH:
    return "HIGH";

    default:
    return "UNKNOWN";
  }
}

void handleButton()
{
  int currentButtonState = digitalRead(BUTTON_PIN);
  if(currentButtonState != lastButtonReading)
  {
    lastDebounceTime = millis();
    lastButtonReading = currentButtonState;
  }

  if(millis() - lastDebounceTime >= DEBOUNCE_DELAY)
  {
    buttonState = currentButtonState;
  }

  if(buttonState == LOW && previousButtonState == HIGH)
  {
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    screenChanged = true;
  }
  previousButtonState = buttonState;
}

//Function for helping clear out the 2nd row before writing new data
void clearLCDRow(byte row)
{
  const byte SPACES_NEEDED = 16;
  lcd.setCursor(0, row);
  for(int i = 0; i < SPACES_NEEDED; i++)
  {
    lcd.print(" ");
  }
  
}

//Function for displaying water level
void displayWaterLevel()
{
  clearLCDRow(0);
  lcd.setCursor(0,0);
  lcd.print("WATER LEVEL");

  clearLCDRow(1);
  lcd.setCursor(0,1);

  //Checks whether there is a valid distance
  if(!distanceAvailable)
  {
    lcd.print("SENSOR ERROR");
    return;
  }

  lcd.print("Level: ");
  lcd.print(waterLevel, 1);
  lcd.print("%");
}

// Functio for displaying water level status
void displayWaterStatus(const char*status)
{
  clearLCDRow(0);
  lcd.setCursor(0, 0);
  lcd.print("TANK STATUS");

  clearLCDRow(1);
  lcd.setCursor(0, 1);

  if(!distanceAvailable)
  {
    lcd.print("SENSOR ERROR");
    return;
  }

  lcd.print(status);
}

//Function for displaying sensor distance
void displaySensorDistance()
{
  clearLCDRow(0);
  lcd.setCursor(0, 0);
  lcd.print("SENSOR DISTANCE");

  clearLCDRow(1);
  lcd.setCursor(0, 1);

  if(!distanceAvailable)
  {
    lcd.print("SENSOR ERROR");
    return;
  }

  lcd.print(latestDistance, 1);
  lcd.print(" cm");
}

//Function for displaying pump status
void displayPumpStatus()
{
  clearLCDRow(0);
  lcd.setCursor(0, 0);
  lcd.print("PUMP STATUS");

  clearLCDRow(1);
  lcd.setCursor(0, 1);

  if(sensorError)
  {
    lcd.print("OFF - SENSOR ERR");
  }
  else if(!distanceAvailable)
  {
    lcd.print("WAITING FOR DATA");
  }
  else
  {
    lcd.print(pumpRunning ? "PUMP: ON" : "PUMP: OFF");
  }
}

// Function which controlls which information to be displayed
void updateLCD()
{
  bool shouldUpdate = screenChanged || millis() - lastLCDUpdate >= LCD_REFRESH_INTERVAL;

  if(!shouldUpdate)
  {
    return;
  }

  lastLCDUpdate = millis();

  switch(currentScreen)
  {
    case 0:
    displayWaterLevel();
    break;

    case 1:
    displayWaterStatus(getWaterLevelText(currentWaterLevel));
    break;

    case 2:
    displaySensorDistance();
    break;

    case 3:
    displayPumpStatus();
    break;

    default:
    lcd.clear();
    lcd.print("INVALID SCREEN");
    break;
  }

  screenChanged = false;
}

//Function for controlling the pump
void updatePumpControl()
{
  if(!distanceAvailable)
  {
    pumpRunning = false;
  }

  //Our Hysteris logic for preventing the pump from flickering
  else if(!pumpRunning && waterLevel <= PUMP_ON_THRESHOLD)
  {
    pumpRunning = true;
  }
  else if(pumpRunning && waterLevel >= PUMP_OFF_THRESHOLD)
  {
    pumpRunning = false;
  }
  digitalWrite(RELAY_PIN, pumpRunning ? RELAY_ON : RELAY_OFF);
}

//Function for setting color intesity
void setRGBColor(byte red, byte green, byte blue)
{
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

//Function fro updating the LED depending on the water Level
void updateStatusLED()
{
  switch(currentWaterLevel)
  {
    case WATER_LOW:
    setRGBColor(255, 0, 0);
    break;

    case WATER_MEDIUM:
    setRGBColor(0, 0, 255);
    break;

    case WATER_HIGH:
    setRGBColor(0, 255, 0);
    break;

    default:
    setRGBColor(0, 0, 0);
    break;
  }
}

//Function for controlling LED in case of sensor error
void handleSensorErrorLED()
{
  unsigned long currentTime = millis();

  if(currentTime - lastErrorLEDUpdate >= ERROR_LED_INTERVAL)
  {
    lastErrorLEDUpdate = currentTime;
    errorLEDState = (errorLEDState + 1) % 3;
  }

  switch(errorLEDState)
  {
    case 0:
    setRGBColor(255, 0, 0); //This activates red.
    break;

    case 1:
    setRGBColor(0, 255, 0); // This activates green.
    break;

    case 2:
    setRGBColor(0, 0, 255); //This activates blue.
    break;
  }
}

void handleSensorErrorBuzzer()
{
  unsigned long currentBuzzerUpdate = millis();
  unsigned long interval = buzzerState ? BUZZER_ON_INTERVAL: BUZZER_OFF_INTERVAL;

  if(currentBuzzerUpdate - lastBuzzerUpdate >= interval)
  {
    lastBuzzerUpdate = currentBuzzerUpdate;
    buzzerState = !buzzerState;

    digitalWrite(BUZZER_PIN, buzzerState ? HIGH: LOW);
  }
}

void updateBuzzer()
{
  if(sensorError)
  {
    handleSensorErrorBuzzer();
  }
  else
  {
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
    lastBuzzerUpdate = millis();
  }
}

void setup()
{
  Serial.begin(9600);

  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  systemStartTime = millis();

  // LCD Intialization
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("WATER MONITORING");

  lcd.setCursor(0, 1);
  lcd.print("Initializing....");
}

void loop()
{
  float averageDistance = 0;
  if(readAverageDistance(averageDistance))
  {
    latestDistance = averageDistance;
    distanceAvailable = true;

    sensorError = false;
    errorMessagePrinted = false;

    lastValidReadingTime = millis();

    waterLevel = calculateWaterLevel(latestDistance);
    currentWaterLevel = classifyWaterLevel(waterLevel);

   const char*waterLevelText = getWaterLevelText(currentWaterLevel);


    Serial.print("Distance: ");
    Serial.print(latestDistance, 2);
    Serial.println(" cm");

    Serial.print("Water level: ");
    Serial.print(waterLevel, 2);
    Serial.println(" %");

    Serial.print("STATUS: ");
    Serial.println(waterLevelText);

  }

  if((distanceAvailable && millis() - lastValidReadingTime >= SENSOR_TIMEOUT) || (!distanceAvailable && millis() - systemStartTime >= SENSOR_TIMEOUT))
  {
    sensorError = true;
    distanceAvailable = false;
  }

  if(sensorError && !previousSensorError)
  {
    errorLEDState = 0;
    lastErrorLEDUpdate = millis();
  }
  previousSensorError = sensorError;

  if(sensorError && !errorMessagePrinted)
  {
    Serial.println("SENSOR ERROR");
    errorMessagePrinted = true;
  }

  updateBuzzer();

  updatePumpControl();

  if(sensorError)
  {
    handleSensorErrorLED();
  }
  else if(distanceAvailable)
  {
    updateStatusLED();
  }
  else
  {
    setRGBColor(0, 0, 0);
  }

static bool previousPumpState = false;

if(pumpRunning != previousPumpState)
{
  if(pumpRunning)
  {
    Serial.println("PUMP: ON");
  }
  else
  {
    Serial.println("PUMP: OFF");
  }

  previousPumpState = pumpRunning;
}

  handleButton();

  updateLCD();
}
