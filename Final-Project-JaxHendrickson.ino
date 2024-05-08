//Jax Hendrickson
//May 10th, 2024
//CPE 301 Final Project: Swamp Cooler

#include <LiquidCrystal.h>
#include <Stepper.h>
#include <Wire.h>
#include <RTClib.h>
#include <dht.h> // Include the DHT library

RTC_DS1307 rtc;
dht DHT;

const int stepsPerRevolution = 100;
Stepper myStepper = Stepper(stepsPerRevolution, 8, 10, 9, 11);
LiquidCrystal lcd(12, 13, 22, 24, 26, 28);

const int BUTTON_PIN_2 = 2;
const int BUTTON_PIN_3 = 3;
const int POWER_PIN = 7;
const int SIGNAL_PIN = A6;
const int THRESHOLD = 130;
const int DHT11_PIN = 50; // Define the pin for the DHT11 sensor
const int ON_PIN = 5;
const int OFF_PIN = 6;

//status led pins
const int ERROR_PIN = 47;
const int DISABLED_PIN = 45;
const int RUNNING_PIN = 43;
const int IDLE_PIN = 41;

const int MOTOR_ENABLE_PIN = 36;
const int MOTOR_INPUT1_PIN = 38;
const int MOTOR_INPUT2_PIN = 40;

bool firsterror = false;
bool firstidle = false;
bool isLowOnWater = false;
bool lastWaterState = false;
bool systemOn = false;
bool lastState = false;
volatile bool buttonPressed = false;

void setup() {
  pinMode(BUTTON_PIN_2, INPUT_PULLUP); //Allowed to use arduino libraries for motor sensor
  pinMode(BUTTON_PIN_3, INPUT_PULLUP); //Allowed to use arduino libraries for motor sensor
  //DDRH |= _BV(PH4); //water sensor pinsetup for if i can't use libraries for water sensor
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);

  pinMode(MOTOR_ENABLE_PIN, OUTPUT); //Allowed to use arduino libraries for motor sensor
  pinMode(MOTOR_INPUT1_PIN, OUTPUT); //Allowed to use arduino libraries for motor sensor
  pinMode(MOTOR_INPUT2_PIN, OUTPUT); //Allowed to use arduino libraries for motor sensor

  pinMode(ON_PIN, INPUT);
  pinMode(OFF_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ON_PIN), startButtonISR, RISING);

  pinMode(ERROR_PIN, OUTPUT);
  pinMode(IDLE_PIN, OUTPUT);
  pinMode(RUNNING_PIN, OUTPUT);
  pinMode(DISABLED_PIN, OUTPUT);

  Serial.begin(9600);
  //setupADC();
  Wire.begin();
  rtc.begin();
  lcd.begin(16, 2);

  // If the RTC isn't running, set it to the following date & time:
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Uncomment this line to set RTC to compiler time
}

void loop() {
  int onState = digitalRead(ON_PIN);
  int offState = digitalRead(OFF_PIN);
  if (onState == HIGH){
    systemOn = HIGH;
    lastState = LOW;
  }
  if (offState == HIGH){
    systemOn = LOW;
    lastState = HIGH;
  }
  if (systemOn){
    digitalWrite(DISABLED_PIN, LOW);
    if (lastState == LOW){
      Serial.println("Welcome Back! System On.");
      lastState = HIGH;
    }
    int buttonState2 = digitalRead(BUTTON_PIN_2);
    int buttonState3 = digitalRead(BUTTON_PIN_3);
    digitalWrite(POWER_PIN, HIGH);
    int sensorValue = analogRead(SIGNAL_PIN);
    digitalWrite(POWER_PIN, LOW);

    // Check water level
    //PORTH |= _BV(PH4); //activate power pin for water sensor (idk if im allowed to use libraries here, so I left this just in case)
    //int sensorValue = analogReadCustom(SIGNAL_PIN); //just in case i can't use libraries
    //PORTH &= ~_BV(PH4); //deactivate power pin for water sensor
    if (sensorValue < THRESHOLD) {
      isLowOnWater = true;
    } else {
      isLowOnWater = false;
    }

    if (isLowOnWater != lastWaterState) {
      if (isLowOnWater) {
        //light up error LED, show "Error: Water Level Low msg"
        Serial.println("Water Level is Low");
        showTime();
      } else {
        //show good light
        Serial.println("Water Level is Normal");
        showTime(); //to show when last filled
      }
      lastWaterState = isLowOnWater;
    }
    if (isLowOnWater){
      digitalWrite(IDLE_PIN, LOW);
      digitalWrite(ERROR_PIN, HIGH);
      if (!firsterror){
        Serial.println("Error: Water Level Too Low");
        showTime();
        firsterror = true;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error: Low");
      lcd.setCursor(0, 1);
      lcd.print("Water Level");
    } else {
      digitalWrite(ERROR_PIN, LOW);
      digitalWrite(IDLE_PIN, HIGH);
      if (!firstidle){
        Serial.println("Idle: Standing by, temperature and humidity too low for activation");
        firstidle = true;
      }
      int chk = DHT.read11(DHT11_PIN);
      int buttonState2 = digitalRead(BUTTON_PIN_2);
      int buttonState3 = digitalRead(BUTTON_PIN_3);
      if (buttonState2 == LOW && buttonState3 == HIGH) {
        myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
        myStepper.step(-stepsPerRevolution);
        Serial.println("Vent has been rotated CounterClockwise.");
        showTime();
      } else if (buttonState2 == HIGH && buttonState3 == LOW) {
        myStepper.setSpeed(50); // Set speed for clockwise rotation
        myStepper.step(stepsPerRevolution);
        Serial.println("Vent has been rotated Clockwise.");
        showTime();
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(DHT.temperature);
      lcd.print(" C");

      lcd.setCursor(0, 1);
      lcd.print("Humidity: ");
      lcd.print(DHT.humidity);
      lcd.print(" %");
      int firstRun = 0;

      //one minute loop setup
      unsigned long previousMillis = 0;
      const long interval = 60000; //1 minute

      while((DHT.temperature > 25 || DHT.humidity > 75) && !isLowOnWater){ //I'm very confused on whether or not we were supposed to update the temperature and 
        firstidle = false;
        firsterror = false;
        if (!firstRun){
          Serial.println("Swamp cooler has begun cooling!");
          firstRun = true; //makes it so the alert only happens the first time the swamp cooler runs.
        }
        if (sensorValue < THRESHOLD) {
            isLowOnWater = true;
        } else {
            isLowOnWater = false;
        }

        digitalWrite(RUNNING_PIN, HIGH);
        
        digitalWrite(IDLE_PIN, LOW);
        digitalWrite(MOTOR_ENABLE_PIN, HIGH);
        digitalWrite(MOTOR_INPUT1_PIN, HIGH);
        digitalWrite(MOTOR_INPUT2_PIN, LOW);

        int chk = DHT.read11(DHT11_PIN);

        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temp: ");
          lcd.print(DHT.temperature);
          lcd.print(" C");

          lcd.setCursor(0, 1);
          lcd.print("Humidity: ");
          lcd.print(DHT.humidity);
          lcd.print(" %");

          Serial.print("Temperature: ");
          Serial.print(DHT.temperature);
          Serial.print(" C, Humidity: ");
          Serial.print(DHT.humidity);
          Serial.println(" %");

          previousMillis = currentMillis; //resets timer
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temp: ");
          lcd.print(DHT.temperature);
          lcd.print(" C");

          lcd.setCursor(0, 1);
          lcd.print("Humidity: ");
          lcd.print(DHT.humidity);
          lcd.print(" %");
        }
        int buttonState2 = digitalRead(BUTTON_PIN_2);
        int buttonState3 = digitalRead(BUTTON_PIN_3);

        if (buttonState2 == LOW && buttonState3 == HIGH) {
            myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
            myStepper.step(-stepsPerRevolution);
            Serial.println("Vent has been rotated CounterClockwise.");
            showTime();
        } else if (buttonState2 == HIGH && buttonState3 == LOW) {
            myStepper.setSpeed(50); // Set speed for clockwise rotation
            myStepper.step(stepsPerRevolution);
            Serial.println("Vent has been rotated Clockwise.");
            showTime();
        }
        digitalWrite(POWER_PIN, HIGH);
        sensorValue = analogRead(SIGNAL_PIN);
        digitalWrite(POWER_PIN, LOW);
        if (sensorValue < THRESHOLD){
          isLowOnWater = HIGH;
        } else {
          isLowOnWater = LOW;
        }
        // Check if conditions are no longer met to stop the fan
        if ((DHT.temperature <= 25 && DHT.humidity <= 75) || isLowOnWater) {
            digitalWrite(RUNNING_PIN, LOW);
            digitalWrite(MOTOR_ENABLE_PIN, LOW);
            digitalWrite(MOTOR_INPUT1_PIN, LOW);
            digitalWrite(MOTOR_INPUT2_PIN, LOW);
        }
      }
      firstRun = false;
      digitalWrite(RUNNING_PIN, LOW);
    }
    delay(1000); // Update every second
    delay(100);
  }
  else {
    lcd.clear();
    digitalWrite(IDLE_PIN, LOW);
    digitalWrite(RUNNING_PIN, LOW);
    digitalWrite(ERROR_PIN, LOW);
    digitalWrite(DISABLED_PIN, HIGH);
    int buttonState2 = digitalRead(BUTTON_PIN_2);
    int buttonState3 = digitalRead(BUTTON_PIN_3);
    if (buttonState2 == LOW && buttonState3 == HIGH) {
      myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
      myStepper.step(-stepsPerRevolution);
      Serial.println("Vent has been rotated CounterClockwise.");
      showTime();
    } else if (buttonState2 == HIGH && buttonState3 == LOW) {
      myStepper.setSpeed(50); // Set speed for clockwise rotation
      myStepper.step(stepsPerRevolution);
      Serial.println("Vent has been rotated Clockwise.");
      showTime();
    }
    delay(1000);
  }
}

void showTime() {
  DateTime now = rtc.now();
  Serial.print("Current Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  printDigits(now.month());
  Serial.print('/');
  printDigits(now.day());
  Serial.print(' ');
  printDigits(now.hour());
  Serial.print(':');
  printDigits(now.minute());
  Serial.print(':');
  printDigits(now.second());
  Serial.println();
}

void printDigits(int digits) {
  // Add leading 0 to single digits
  if (digits < 10){
    Serial.print('0');
  }
  Serial.print(digits);
}

void startButtonISR(){
  buttonPressed = true;
}

//Below is for the Water Sensor, since I couldn't tell if I could or couldn't use the arduino library for it (i.e: pinMode and analogRead)
/*int analogReadCustom(byte pin) { //had to make my own analog read
  // Set the ADC channel based on the specified pin
  ADMUX = (ADMUX & 0xF0) | (pin & 0x0F);
  // Start the conversion
  ADCSRA |= _BV(ADSC);
  // Wait for the conversion to complete
  while (ADCSRA & _BV(ADSC));
  // Return the analog value
  return ADC;
}

void setupADC() {
  // Set the reference voltage to AVCC and select the ADC prescaler to 128
  ADMUX = _BV(REFS0);
  ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}*/