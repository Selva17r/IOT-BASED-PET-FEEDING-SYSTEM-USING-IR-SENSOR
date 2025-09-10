#define BLYNK_TEMPLATE_ID "TMPL3AuG3XzBW"
#define BLYNK_TEMPLATE_NAME "Pet Feeding System"
#define BLYNK_AUTH_TOKEN "yBVh2oAs7JOyKCUwXYwRlG_6e0e2VTef"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <WidgetRTC.h>

// WiFi Credentials
char ssid[] = "selva";
char pass[] = "selva157";

#define BOT_TOKEN "8074251840:AAFndHjsYgBuICau9zE7mpmVRGXdT3nDZbw"
#define CHAT_ID "1499018836"

// ThingSpeak Credentials
#define THINGSPEAK_API_KEY "UB32QO1FYNOCPWAA"  // Replace with your API Key
#define THINGSPEAK_URL "http://api.thingspeak.com/update"

// Hardware setup
#define IR_SENSOR_PIN 5
#define SERVO_PIN 18

Servo feederServo;
bool petDetected = false;
bool autoMode = false;
bool detectionDisabled = false;

unsigned long lastFeedingTime = 0;
const unsigned long feedingCooldown = 60000;

BlynkTimer timer;
WidgetRTC rtc;

BLYNK_CONNECTED() {
    rtc.begin();  // Sync time with Blynk Server
}

void sendToThingSpeak(int feedingStatus, int petDetected, int feedingMode) {
    HTTPClient http;
    String timestamp = String(hour()) + ":" + String(minute()) + ":" + String(second());
    
    String url = String(THINGSPEAK_URL) + "?api_key=" + THINGSPEAK_API_KEY +
                 "&field1=" + String(feedingStatus) +
                 "&field2=" + String(petDetected) +
                 "&field3=" + String(feedingMode) +
                 "&field4=" + timestamp;

    Serial.println("Sending feeding data to ThingSpeak.");
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        Serial.println("Data Sent Successfully");
    } else {
        Serial.print("Error Sending Data: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void sendTelegramMessage(String message) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) +
                 "/sendMessage?chat_id=" + String(CHAT_ID) +
                 "&text=" + message;

    Serial.println("Sending Telegram Message");
    http.begin(url);
    http.GET();
    http.end();
}

// Manual Feeding Button (V1) - Only works after pet detection
BLYNK_WRITE(V1) {
    int buttonState = param.asInt();
    if (!autoMode && petDetected) {
        if (buttonState == 1) {
            Serial.println("Manual Feeding Started...");
            feederServo.write(60);
            sendToThingSpeak(1, 1, 2);
           
        } else {
            Serial.println("Manual Feeding Complete!");
            feederServo.write(0);
            sendTelegramMessage("Manual Feeding Completed!");
            petDetected = false;  // Reset pet detection after manual feeding
        }
    } else {
        Serial.println("Manual Feeding Not Allowed - No Pet Detected");
    }
}

// Auto Mode Toggle (V2)
BLYNK_WRITE(V2) {
    autoMode = param.asInt();
    Serial.print("Auto Mode: ");
    Serial.println(autoMode ? "Enabled" : "Disabled");
    sendToThingSpeak(0, 0, autoMode);
}

//  Pet Detection Using IR Sensor
void checkPetMotion() {
    if (!detectionDisabled && digitalRead(IR_SENSOR_PIN) == LOW) {
        Serial.println(" Pet Detected!");
        sendTelegramMessage("Pet Detected!");
        sendToThingSpeak(0, 1, autoMode ? 1 : 0);
        petDetected = true;

        if (autoMode) {
            Serial.println("Auto Feeding Started");
            feederServo.write(60);
            delay(1000);
            feederServo.write(0);
            Serial.println("Auto Feeding Complete!");
            sendTelegramMessage("Auto Feeding Completed!");
            sendToThingSpeak(1, 1, 1);
            petDetected = false; // Reset pet detection after auto feeding
        }
        detectionDisabled = true;
        lastFeedingTime = millis();
    }

    if (detectionDisabled && millis() - lastFeedingTime >= feedingCooldown) {
        detectionDisabled = false;
        Serial.println(" IR Sensor Re-enabled. Ready to detect again.");
         sendTelegramMessage("Ready to detect again");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(IR_SENSOR_PIN, INPUT);
    feederServo.attach(SERVO_PIN);
    feederServo.write(0);

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi");
    }
    

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    timer.setInterval(1000L, checkPetMotion);
}

void loop() {
    Blynk.run();
    timer.run();
}
