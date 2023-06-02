#include <ESP32Servo.h> // 0.13.0
#include <MFRC522.h> // 1.4.10
#include <SPI.h> // 2.0.0
#include <NewPing.h> // 1.9.7

// RC522
#define SS_PIN 5
#define RST_PIN 0
// MOSI - 23
// MISO - 19
// SCK - 18

// Button
#define BUTTON_PIN 21
#define DEBOUNCE_TIME  300

// Leds
#define RED_LED_PIN 22
#define GREEN_LED_PIN 17

// Ultrasonic sensor
#define SENSOR_TRIG_PIN 25
#define SENSOR_ECHO_PIN 26
#define MAX_DISTANCE 200
#define OBSTACLE_THRESHOLD 13

// Servo motor
#define SERVO_PIN 27

int indexUid = 0;
unsigned long uidVector[10];
Servo servo;
int angle = 0;
unsigned long lastDebounce = 0;
bool doorOpen = false;
bool registerMode = false;
bool lastButtonState = HIGH;

NewPing sonar(SENSOR_TRIG_PIN, SENSOR_ECHO_PIN, MAX_DISTANCE);

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key; 

// Function that gets the ID of the card and returns it as an unsigned long
unsigned long getID(){
    unsigned long hex_num;
    hex_num =  rfid.uid.uidByte[0] << 24;
    hex_num += rfid.uid.uidByte[1] << 16;
    hex_num += rfid.uid.uidByte[2] <<  8;
    hex_num += rfid.uid.uidByte[3];
    rfid.PICC_HaltA(); // Stop reading
    return hex_num;
}

void setup() {
    Serial.begin(115200);
    servo.attach(SERVO_PIN);
    servo.write(angle);
    SPI.begin(); // Init SPI bus
    rfid.PCD_Init(); // Init MFRC522 
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(SENSOR_TRIG_PIN, OUTPUT);
    pinMode(SENSOR_ECHO_PIN, INPUT);
}

void openDoor() {
    digitalWrite(GREEN_LED_PIN, HIGH);
    for(; angle <= 160; angle++) {
        servo.write(angle);               
        delay(15);                   
    }
}

void closeDoor() {
    int distanceToObstacle = MAX_DISTANCE;
    unsigned int uS;
    
    for(angle = 160; angle >= 0; angle--) {
        digitalWrite(GREEN_LED_PIN, HIGH);

        // check if there is an obstacle in front of the door
        do {
            uS = sonar.ping();
            pinMode(SENSOR_ECHO_PIN, OUTPUT);
            digitalWrite(SENSOR_ECHO_PIN, LOW);
            pinMode(SENSOR_ECHO_PIN, INPUT);
            distanceToObstacle = uS / US_ROUNDTRIP_CM;
            Serial.print("Ping: ");
            Serial.print(distanceToObstacle);
            Serial.println("cm");
            if (distanceToObstacle < OBSTACLE_THRESHOLD) {
                Serial.println("Obstacle detected");
                openDoor();
            }
        } while (distanceToObstacle < OBSTACLE_THRESHOLD || distanceToObstacle == MAX_DISTANCE);

        Serial.println("Closing door");
        servo.write(angle);           
        delay(15);       
    }
}

void loop() {
    digitalWrite(GREEN_LED_PIN, LOW);

    // check if the register mode is on
    if (registerMode) {
        digitalWrite(RED_LED_PIN, HIGH);
    } else {
        digitalWrite(RED_LED_PIN, LOW);
    }

    // check if the button was pressed
    if (digitalRead(BUTTON_PIN) != lastButtonState) {
        if (millis() - lastDebounce > DEBOUNCE_TIME) {
            registerMode = !registerMode;
            Serial.print("Register mode: ");
            Serial.println(registerMode);
            lastDebounce = millis();
        }
    }

    if (!rfid.PICC_IsNewCardPresent()) {
        return;   
    }

    if (!rfid.PICC_ReadCardSerial()) {
        return;
    }
    
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.println(rfid.PICC_GetTypeName(piccType));

    // add the UID of the card to a vector
    if (registerMode) {
        Serial.println("Registering");
        unsigned long uid = getID();

        if(uid != 0){
            Serial.print("Card detected, UID: ");
            Serial.println(uid);
            uidVector[indexUid] = uid;
            registerMode = false;
            indexUid++;
        }
    } else {
        // check if the card is in the vector
        Serial.println("Checking");
        unsigned long uid = getID();
        if(uid != 0){
            Serial.print("Card detected, UID: ");
            Serial.println(uid);
            for (int i = 0; i < indexUid; i++) {
                Serial.print("UID in vector: ");
                Serial.println(uidVector[i]);
                if (uidVector[i] == uid) {
                    Serial.println("Access granted");
                    openDoor();
                    digitalWrite(GREEN_LED_PIN, LOW);
                    delay(5000);
                    closeDoor();
                    break;
                }
            }
        }
    }

    rfid.PICC_HaltA();
}
