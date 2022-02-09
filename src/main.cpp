/**============================================================================
 *                   JARVIS - Personal Home Assistant
 *
 *      - OTA Updates
 *      - Telegram bot
 *===========================================================================**/

// Libraries used in this project
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "wifi_setup.h"
// Include optimization for debugging
#define DEBUG
#include "debug.h"

// Include project configuration
#include "config.h"

// Declare the mqtt address
// IPAddress mqttServer(192, 168, 0, 80);

// Declare the sensor object based on its type and pin
DHT dht(TEMPERATURE_PIN, DHT_TYPE);

// Create a web server for OTA
WebServer otaServer(80);
WiFiClient httpClient;

// Create MQTT client
PubSubClient mqttClient(httpClient);

// Store the state of the outputs
bool heatingState = false;
bool ventilationState = false;
bool lightsState = false;
bool spareOutputState = false;
int buttonState;
bool prevButtonState = false;
float prevTemp = 0;

// Counters for scheduler
unsigned long time1 = 0;
unsigned long time2 = 0;

// Update the server status
void updateServerStatus() {
    // Declare a new JSON object containing three key-values pairs
    const size_t capacity = JSON_OBJECT_SIZE(3);
    DynamicJsonDocument doc(capacity);
    char payload[256];
    doc["heating"] = heatingState;
    doc["ventilation"] = ventilationState;
    doc["lights"] = lightsState;
    serializeJson(doc, payload);
    mqttClient.publish("jarvis/kitchen/status", payload);
}

// Callback function called when receiveing a message
//===================================================
void callback(char *topic, byte *message, unsigned int length) {
    DPRINTF("Message arrived on topic: ");
    DPRINT(topic);
    DPRINTF(". Message: ");
    String messageTemp;

    // Compose the message
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
    DPRINTLN(messageTemp);

    if (String(topic) == "jarvis/heating") {
        if (messageTemp == "on") {
            digitalWrite(HEATING_PIN, LOW);
            heatingState = true;
            DPRINTLNF("Heating turned on");
        }
        if (messageTemp == "off") {
            digitalWrite(HEATING_PIN, HIGH);
            heatingState = false;
            DPRINTLNF("Heating turned off");
        }
    }
    if (String(topic) == "jarvis/ventilation") {
        if (messageTemp == "on") {
            digitalWrite(VENTILATION_PIN, LOW);
            ventilationState = true;
            DPRINTLNF("Ventilation turned on");
        }
        if (messageTemp == "off") {
            digitalWrite(VENTILATION_PIN, HIGH);
            ventilationState = false;
            DPRINTLNF("Ventilation turned off");
        }
    }
    if (String(topic) == "jarvis/lights") {
        if (messageTemp == "on") {
            digitalWrite(LIGHTS_PIN, LOW);
            lightsState = true;
            DPRINTLNF("Lights turned on");
        }
        if (messageTemp == "off") {
            digitalWrite(LIGHTS_PIN, HIGH);
            lightsState = false;
            DPRINTLNF("Lights turned off");
        }
    }
    if (String(topic) == "jarvis/kitchen/update") {
        updateServerStatus();
    }
}

/**====================================
 *    WIFI & OTA setup function
 *===================================**/
void setupWifi() {
    // Attempt to connect to Wifi network:
    DPRINTF("Connecting to: ");
    DPRINTLN(WIFI_SSID);

    const char *hostName = HOST_NAME;
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(hostName);  // define hostname
    // Connect to WiFi network
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    DPRINTLN("");
    delay(30);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DPRINT(".");
    }
    DPRINTLN("");
    DPRINTF("Connected to ");
    DPRINTLN(WIFI_SSID);
    DPRINTF("IP address: ");
    DPRINTLN(WiFi.localIP());

    // Once online, connect to the mqtt server
    const char *mqttServer = MQTT_SERVER;
    mqttClient.setServer(mqttServer, 1883);

    // Declare the callback function
    mqttClient.setCallback(callback);

    // Use MDSN for a friendly hostname - http://termostat.local
    if (!MDNS.begin(hostName)) {
        DPRINTLNF("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    // Returns login page
    otaServer.on("/", HTTP_GET, []() {
        otaServer.sendHeader("Connection", "close");
        otaServer.send(200, "text/html", loginIndex);
    });

    // Returns server page
    otaServer.on("/serverIndex", HTTP_GET, []() {
        otaServer.sendHeader("Connection", "close");
        otaServer.send(200, "text/html", serverIndex);
    });

    // Handle upload of new firmware
    otaServer.on(
        "/update", HTTP_POST, []() {
        otaServer.sendHeader("Connection", "close");
        otaServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart(); }, []() {
            HTTPUpload& upload = otaServer.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("Update: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
                    Update.printError(Serial);
                }
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                // Flashing firmware to ESP
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) { 
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                } 
                else {
                    Update.printError(Serial);
                }
            } });
    otaServer.begin();

    DPRINTLNF("OTA update server started. ");
    DPRINTLNF("It can be accessed at http://kitchen.local\n");
}

// Called once every SENSOR_CHECK_DELAY
void handleSensorReadings() {
    // DHT sensor reading
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);

    // Validate readings
    if (isnan(temperature) || isnan(humidity)) {
        DPRINTLNF("Failed to read the temperature sensor!");
        return;
    }

    DPRINTF("Temperature: ");
    DPRINT(temperature);
    DPRINTF("°C, humidity: ");
    DPRINT(humidity);
    DPRINTLN("%.");

    // Declare a new JSON object containing three key-values pairs
    const size_t capacity = JSON_OBJECT_SIZE(3);
    DynamicJsonDocument doc(capacity);
    char payload[256];

    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["index"] = heatIndex;
    serializeJson(doc, payload);
    mqttClient.publish("jarvis/temperature", payload);
    DPRINTLNF("Published on jarvis/temperature");
}

// Reconnect function to keep the mqtt server running
void reconnect() {
    // Loop until we're reconnected
    while (!mqttClient.connected()) {
        DPRINTF("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect(MQTT_HOST, MQTT_USER, MQTT_PASS)) {
            DPRINTLNF("connected");
            // Subscribe
            mqttClient.subscribe("jarvis/heating");
            mqttClient.subscribe("jarvis/ventilation");
            mqttClient.subscribe("jarvis/lights");
            mqttClient.subscribe("jarvis/kitchen/update");
        } else {
            DPRINTF("failed, rc=");
            DPRINT(mqttClient.state());
            DPRINTLNF(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
/**====================================
 *    SETUP function
 *===================================**/
void setup() {
    // Start serial communication
    Serial.begin(115200);

    // Print basic info about the firmware, handy on first sight
    Serial.println(F("=============================================\n"));
    Serial.println(F("Kitchen Controller - OTA Update Server - MQTT client"));
    Serial.println(F("03.feb.2022"));
    Serial.println(F("https://github.com/timandi/TermostatBot\n\n"));

    // Initialize the pins as outputs and pull them high
    pinMode(HEATING_PIN, OUTPUT);
    pinMode(VENTILATION_PIN, OUTPUT);
    pinMode(LIGHTS_PIN, OUTPUT);
    pinMode(SPARE_PIN, OUTPUT);

    digitalWrite(HEATING_PIN, HIGH);
    digitalWrite(VENTILATION_PIN, HIGH);
    digitalWrite(LIGHTS_PIN, HIGH);
    digitalWrite(SPARE_PIN, HIGH);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(TEMPERATURE_PIN, INPUT);

    buttonState = digitalRead(BUTTON_PIN);

    // Call the full wifi setup
    setupWifi();

    // Start the DHT server
    dht.begin();
    DPRINTLNF("DHT sensor started");

    // Update server status
    updateServerStatus();
}

/**====================================
 *    LOOP function
 *===================================**/
void loop(void) {
    if (millis() >= time1 + KEEP_ALIVE) {
        time1 += KEEP_ALIVE;

        // Check the MQTT client connection
        if (!mqttClient.connected()) {
            reconnect();
        }
        mqttClient.loop();
    }
    if (millis() >= time2 + SENSOR_CHECK_DELAY) {
        time2 += SENSOR_CHECK_DELAY;
        handleSensorReadings();
    }
    // Handle possible http connections for OTA update
    otaServer.handleClient();
}