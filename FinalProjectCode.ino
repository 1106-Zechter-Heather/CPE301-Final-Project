#include <DHT.h>
#include <LiquidCrystal.h>
#include <Stepper.h>

const int DHT_PIN = 2;
const int MOTOR_ENABLE = 5;
const int MOTOR_PIN1 = 3;
const int MOTOR_PIN2 = 4;
const int WATER_SENSOR = A0;
const int VENT_POT = A1;
const int YELLOW_LED = 6;    
const int RGB_RED = 9;       
const int RGB_GREEN = 10;    
const int RGB_BLUE = 11;     
const int STEPS_PER_REV = 2048;

LiquidCrystal lcd(22, 23, 24, 25, 26, 27);
Stepper ventStepper(STEPS_PER_REV, 7, 8, 12, 13);
DHT dht(DHT_PIN, DHT11);

const float TEMP_THRESHOLD = 74.0;
const int WATER_THRESHOLD = 100;    
const unsigned long SENSOR_READ_INTERVAL = 2000;
const unsigned long LCD_UPDATE_INTERVAL = 2000;

enum SystemState {
    DISABLED,
    IDLE,
    RUNNING,
    ERROR
};

volatile SystemState currentState = DISABLED;
volatile bool startButtonPressed = false;
volatile bool stopButtonPressed = false;
int lastVentPosition = 0;
unsigned long lastReadTime = 0;
unsigned long lastLCDUpdate = 0;

void initADC() {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t readADC(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void updateLEDs(SystemState state) {
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RGB_RED, LOW);
    digitalWrite(RGB_GREEN, LOW);
    digitalWrite(RGB_BLUE, LOW);
    
    switch(state) {
        case DISABLED:
            digitalWrite(YELLOW_LED, HIGH);
            break;
        case IDLE:
            digitalWrite(RGB_GREEN, HIGH);
            break;
        case RUNNING:
            digitalWrite(RGB_BLUE, HIGH);
            break;
        case ERROR:
            digitalWrite(RGB_RED, HIGH);
            break;
    }
}

void updateDisplay(float temp, float humidity, int waterLevel) {
    Serial.print("Temperature: ");
    Serial.print(temp, 1);
    Serial.print("°F | Humidity: ");
    Serial.print(humidity, 1);
    Serial.print("% | Water Level: ");
    Serial.print(waterLevel);
    Serial.print(" | State: ");
    
    switch(currentState) {
        case DISABLED: Serial.println("DISABLED"); break;
        case IDLE: Serial.println("IDLE"); break;
        case RUNNING: Serial.println("RUNNING"); break;
        case ERROR: Serial.println("ERROR"); break;
    }
    
    lcd.clear();
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("F H:");
    lcd.print(humidity, 1);
    lcd.print("%");
    lcd.setCursor(0, 1);
    
    switch(currentState) {
        case DISABLED: lcd.print("System Off"); break;
        case IDLE: lcd.print("Ready"); break;
        case RUNNING: lcd.print("Fan Running"); break;
        case ERROR: lcd.print("Error-Water Low"); break;
    }
}

void handleVentControl() {
    int potValue = readADC(1);
    int newPosition = map(potValue, 0, 1023, 0, STEPS_PER_REV/8);
    
    if (abs(newPosition - lastVentPosition) > 2) {
        int steps = newPosition - lastVentPosition;
        ventStepper.step(steps);
        lastVentPosition = newPosition;
        
        Serial.print("Vent Position: ");
        Serial.println(lastVentPosition);
    }
}

void setup() {
    Serial.begin(9600);
    
    initADC();
    
    pinMode(YELLOW_LED, OUTPUT);
    pinMode(RGB_RED, OUTPUT);
    pinMode(RGB_GREEN, OUTPUT);
    pinMode(RGB_BLUE, OUTPUT);
    pinMode(MOTOR_ENABLE, OUTPUT);
    pinMode(MOTOR_PIN1, OUTPUT);
    pinMode(MOTOR_PIN2, OUTPUT);
    
    analogWrite(27, 120);
    
    lcd.begin(16, 2);
    lcd.print("Starting...");
    
    dht.begin();
    ventStepper.setSpeed(10);
    
    digitalWrite(MOTOR_PIN1, HIGH);
    digitalWrite(MOTOR_PIN2, LOW);
    digitalWrite(MOTOR_ENABLE, LOW);
    
    Serial.println("System initialized!");
    delay(2000);
    
    currentState = IDLE;
    updateLEDs(currentState);
}

void loop() {
    float temp = dht.readTemperature(true);
    float humidity = dht.readHumidity();
    int waterLevel = readADC(0);
    
    if (!isnan(temp) && !isnan(humidity)) {
        updateDisplay(temp, humidity, waterLevel);
        
        if (waterLevel < WATER_THRESHOLD && currentState != ERROR) {
            currentState = ERROR;
            digitalWrite(MOTOR_ENABLE, LOW);
            updateLEDs(currentState);
        }
        
        if (currentState == IDLE && temp >= TEMP_THRESHOLD) {
            currentState = RUNNING;
            digitalWrite(MOTOR_ENABLE, HIGH);
            updateLEDs(currentState);
        } else if (currentState == RUNNING && temp < (TEMP_THRESHOLD - 2)) {
            currentState = IDLE;
            digitalWrite(MOTOR_ENABLE, LOW);
            updateLEDs(currentState);
        }
    } else {
        Serial.println("Failed to read DHT sensor!");
    }
    
    if (currentState != ERROR) {
        handleVentControl();
    }
    
    delay(100);
}