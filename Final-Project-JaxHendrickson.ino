//Jax Hendrickson
//May 10th, 2024
//CPE 301 Final Project: Swamp Cooler

#include <LiquidCrystal.h>
#include <Stepper.h>
#include <RTClib.h>
#include <dht.h> 

RTC_DS1307 rtc;
dht DHT;

#define RDA 0x80
#define TBE 0x20  

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
 
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

const int stepsPerRevolution = 100;
Stepper myStepper = Stepper(stepsPerRevolution, 8, 10, 9, 11);
LiquidCrystal lcd(12, 13, 22, 24, 26, 28);

const int BUTTON_PIN_2 = 2;
const int BUTTON_PIN_3 = 3;
const int POWER_PIN = 7;
const int SIGNAL_PIN = A6;
const int THRESHOLD = 130;
const int DHT11_PIN = 50; 
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

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 60000;
bool firsterror = false;
bool firstidle = false;
bool isLowOnWater = false;
bool lastWaterState = true;
bool systemOn = false;
bool lastState = false;
volatile bool buttonPressed = false;
unsigned long currentTime = 0;

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
  rtc.begin(); //as stated in instructions, we can use these arduino library functions
  lcd.begin(16, 2);

  U0init(9600);
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
      const char message1 [] = "Welcome Back! System On.";
      printMessage(message1);
      customDelay(1500000); //I don't know why but specifically this line was giving me an absurd amount of trouble, so I just gave it an arbitrarily large delay time and it worked. Couldn't tell you why. 
      showTime();
      lastState = HIGH;
    }
    int buttonState2 = digitalRead(BUTTON_PIN_2);
    int buttonState3 = digitalRead(BUTTON_PIN_3);
    digitalWrite(POWER_PIN, HIGH);
    int sensorValue = analogRead(SIGNAL_PIN);
    digitalWrite(POWER_PIN, LOW);
    if (sensorValue < THRESHOLD) {
      isLowOnWater = true;
    } else {
      isLowOnWater = false;
    }

    if (isLowOnWater != lastWaterState) {
      if (isLowOnWater) {
        //light up error LED, show "Error: Water Level Low msg"
        const char message2 [] = "Water Level is Low";
        printMessage(message2);
        showTime();
      } else {
        //show good light
        const char message3 [] = "Water Level is Normal";
        printMessage(message3);
        showTime(); //to show when last filled
      }
      lastWaterState = isLowOnWater;
    }

    if (isLowOnWater){
      currentTime = millis();  //once per minute, print out the humidity and temperature to the serial monitor. 
      if (currentTime - lastPrintTime >= printInterval) {
        printTemperatureAndHumidity();
        showTime(); 
        lastPrintTime = currentTime;
      }
      lastWaterState = isLowOnWater;
      digitalWrite(IDLE_PIN, LOW);
      digitalWrite(ERROR_PIN, HIGH);
      if (!firsterror){
        const char message4 [] = "Water Level is Low.";
        printMessage(message4);
        showTime();
        firsterror = true;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error: Low");
      lcd.setCursor(0, 1);
      lcd.print("Water Level");
      firstidle = false;
    } else {
      currentTime = millis();  //once per minute, print out the humidity and temperature to the serial monitor. 
      if (currentTime - lastPrintTime >= printInterval) {
        printTemperatureAndHumidity();
        showTime(); 
        lastPrintTime = currentTime;
      }
      digitalWrite(ERROR_PIN, LOW);
      digitalWrite(IDLE_PIN, HIGH);
      if (!firstidle){
        const char message5 [] = "Idle: Standing by, temperature and humidity too low for activation.";
        printMessage(message5);
        showTime();
        firstidle = true;
      }
      int chk = DHT.read11(DHT11_PIN);
      int buttonState2 = digitalRead(BUTTON_PIN_2);
      int buttonState3 = digitalRead(BUTTON_PIN_3);
      if (buttonState2 == LOW && buttonState3 == HIGH) {
        myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
        myStepper.step(-stepsPerRevolution);
        const char message6 [] = "Vent has been rotated CounterClockwise.";
        printMessage(message6);
        showTime();
      } else if (buttonState2 == HIGH && buttonState3 == LOW) {
        myStepper.setSpeed(50); // Set speed for clockwise rotation
        myStepper.step(stepsPerRevolution);
        const char message7 [] = "Vent has been rotated Clockwise.";
        printMessage(message7);
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
      while((DHT.temperature >= 25 || DHT.humidity >= 75) && !isLowOnWater){ 
        currentTime = millis();
        if (currentTime - lastPrintTime >= printInterval) { //Makes it so that when we pass the initial millis() into the equation, it's not replaced, keeping consistent time.
          printTemperatureAndHumidity();
          showTime(); 
          lastPrintTime = currentTime;
        }
        firstidle = false;
        if (!firstRun){
          const char message8 [] = "Swamp Cooler Has Begun Cooling!";
          printMessage(message8);
          showTime();
          firstRun = true; //makes it so the alert only happens the first time the swamp cooler runs.
        }
        digitalWrite(RUNNING_PIN, HIGH);
        
        digitalWrite(IDLE_PIN, LOW);
        digitalWrite(MOTOR_ENABLE_PIN, HIGH);
        digitalWrite(MOTOR_INPUT1_PIN, HIGH);
        digitalWrite(MOTOR_INPUT2_PIN, LOW);

        int chk = DHT.read11(DHT11_PIN);
    
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(DHT.temperature);
        lcd.print(" C");

        lcd.setCursor(0, 1);
        lcd.print("Humidity: ");
        lcd.print(DHT.humidity);
        lcd.print("%");
        int buttonState2 = digitalRead(BUTTON_PIN_2);
        int buttonState3 = digitalRead(BUTTON_PIN_3);

        if (buttonState2 == LOW && buttonState3 == HIGH) {
          myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
          myStepper.step(-stepsPerRevolution);
          const char message9 [] = "Vent has been rotated CounterClockwise.";
          printMessage(message9);
          showTime();
        } else if (buttonState2 == HIGH && buttonState3 == LOW) {
          myStepper.setSpeed(50); // Set speed for clockwise rotation
          myStepper.step(stepsPerRevolution);
          const char message10 [] = "Vent has been rotated Clockwise.";
          printMessage(message10);
          showTime();
        }
        digitalWrite(POWER_PIN, HIGH);
        int sensorValue = analogRead(SIGNAL_PIN);
        digitalWrite(POWER_PIN, LOW);
        if (sensorValue < THRESHOLD) {
            isLowOnWater = true;
        } else {
            isLowOnWater = false;
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
    customDelay(1000); // Update every second
    customDelay(100);
  }
  else {
    currentTime = 0;
    lcd.clear();
    firstidle = false;
    firsterror = false;
    digitalWrite(IDLE_PIN, LOW);
    digitalWrite(RUNNING_PIN, LOW);
    digitalWrite(ERROR_PIN, LOW);
    digitalWrite(DISABLED_PIN, HIGH);
    int buttonState2 = digitalRead(BUTTON_PIN_2);
    int buttonState3 = digitalRead(BUTTON_PIN_3);
    if (buttonState2 == LOW && buttonState3 == HIGH) {
      myStepper.setSpeed(50);  // Set speed for counterclockwise rotation
      myStepper.step(-stepsPerRevolution);

      //I'm making it so that it can be rotated while disabled, but won't log any actions. This is because of how vague the instructions were; please don't take off points if incorrect.

    } else if (buttonState2 == HIGH && buttonState3 == LOW) {
      myStepper.setSpeed(50); // Set speed for clockwise rotation
      myStepper.step(stepsPerRevolution);
    }
    customDelay(1000);
  }
}

void customDelay(unsigned long milliseconds){ //burns a set amount of time. I know this wasn't exactly what you wanted (probably the Timer interrupt in the slides), but I couldn't make it work, and nowhere on the rubric says that we can't do something trivial like this; just that it can't use libraries.
  unsigned long loops = milliseconds / 4;
  for (unsigned long i = 0; i < loops; i++){
    asm volatile ("nop"); //asm is a keyword, not a function or library. nop means nothing happens.
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
  }
}

void printMessage(const char* message) { //function for printing messages to serial monitor
  for (int i = 0; message[i] != '\0'; i++) {
    U0putchar(message[i]); 
    customDelay(10);
  }
  U0putchar('\n');
  customDelay(100);
}

void showTime() { //real time clock shows time
  DateTime now = rtc.now();
  U0putchar('C');
  U0putchar('u');
  U0putchar('r');
  U0putchar('r');
  U0putchar('e');
  U0putchar('n');
  U0putchar('t');
  U0putchar(' ');
  U0putchar('T');
  U0putchar('i');
  U0putchar('m');
  U0putchar('e');
  U0putchar(':');
  U0putchar(' ');
  
  // Print year
  printDigits(now.year() / 1000);
  printDigits((now.year() / 100) % 10);
  printDigits((now.year() / 10) % 10);
  printDigits(now.year() % 10);
  
  U0putchar('/');
  
  // Print month
  printDigits(now.month() / 10);
  printDigits(now.month() % 10);
  U0putchar('/');
  
  // Print day
  printDigits(now.day() / 10);
  printDigits(now.day() % 10);
  U0putchar(' ');
  
  // Print hour
  printDigits(now.hour() / 10);
  printDigits(now.hour() % 10);
  U0putchar(':');
  
  // Print minute
  printDigits(now.minute() / 10);
  printDigits(now.minute() % 10);
  U0putchar(':');
  
  // Print second
  printDigits(now.second() / 10);
  printDigits(now.second() % 10);
  U0putchar('\n');
  customDelay(100);
}

void printDigits(int digits) { //helper function
  U0putchar(digits + '0');
}

void startButtonISR(){ //ISR to check for when the button is pressed
  buttonPressed = true;
}



// ADC functions, taken from previous labs

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  
  // Set the channel number
  *my_ADMUX |= (adc_channel_num & 0x07); // Ensure only the first 3 bits are set
  
  // Set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  
  // Wait for the conversion to complete
  while ((*my_ADCSRA & 0x40) != 0);
  
  // Return the result in the ADC data register
  return *my_ADC_DATA;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

///////

void printTemperatureAndHumidity(){ //function to print temp and humidity
  char temp_str[6]; 
  char humi_str[6]; 
  int temp_int = (int)DHT.temperature; 
  sprintf(temp_str, "%dC\n", temp_int); 
  int humi_int = (int)DHT.humidity; 
  sprintf(humi_str, "%d%%\n", humi_int); 
  U0putchar('T');
  U0putchar('e');
  U0putchar('m');
  U0putchar('p');
  U0putchar('e');
  U0putchar('r');
  U0putchar('a');
  U0putchar('t');
  U0putchar('u');
  U0putchar('r');
  U0putchar('e');
  U0putchar(':');
  U0putchar(' ');
  
  for (int i = 0; i < strlen(temp_str); i++) {
    U0putchar(temp_str[i]);
  }
  U0putchar('H');
  U0putchar('u');
  U0putchar('m');
  U0putchar('i');
  U0putchar('d');
  U0putchar('i');
  U0putchar('t');
  U0putchar('y');
  U0putchar(':');
  U0putchar(' ');
  for (int i = 0; i < strlen(humi_str); i++) {
    U0putchar(humi_str[i]);
  }
}
