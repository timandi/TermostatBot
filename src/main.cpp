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
#include <WiFiClientSecure.h>

// Include optimization for debugging
// #define DEBUG
#include "debug.h"

// Include project configuration
#include "config.h"

// Declare the mqtt address
IPAddress mqttServer(192, 168, 0, 80);

// Declare the sensor object based on its type and pin
DHT dht(TEMPERATURE_PIN, DHT_TYPE);

// Create a web server for OTA
WebServer otaServer(80);
WiFiClient httpClient;

// Create MQTT client
PubSubClient mqttClient(httpClient);

// Declare a new JSON object containing three key-values pairs
const size_t capacity = JSON_OBJECT_SIZE(3);
DynamicJsonDocument doc(capacity);
char payload[256];

// Store the OTA html
String style;
String loginIndex;
String serverIndex;

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
}

/**====================================
 *    WIFI & OTA setup function
 *===================================**/
void setupWifi() {
    style =
        "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
        "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
        "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
        "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
        "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
        ".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

    // Login page
    loginIndex =
        "<form name=loginForm>"
        "<h1>Login</h1>"
        "<input name=userid placeholder='User ID'> "
        "<input name=pwd placeholder=Password type=Password> "
        "<input type=submit onclick=check(this.form) class=btn value=Login></form>"
        "<script>"
        "function check(form) {"
        "if(form.userid.value=='admin' && form.pwd.value=='admin')"
        "{window.open('/serverIndex')}"
        "else"
        "{alert('Error Password or Username')}"
        "}"
        "</script>" +
        style;

    // Update page
    serverIndex =
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
        "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
        "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
        "<label id='file-input' for='file'>   Choose file...</label>"
        "<input type='submit' class=btn value='Update'>"
        "<br><br>"
        "<div id='prg'></div>"
        "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
        "<script>"
        "function sub(obj){"
        "var fileName = obj.value.split('\\\\');"
        "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
        "};"
        "$('form').submit(function(e){"
        "e.preventDefault();"
        "var form = $('#upload_form')[0];"
        "var data = new FormData(form);"
        "$.ajax({"
        "url: '/update',"
        "type: 'POST',"
        "data: data,"
        "contentType: false,"
        "processData:false,"
        "xhr: function() {"
        "var xhr = new window.XMLHttpRequest();"
        "xhr.upload.addEventListener('progress', function(evt) {"
        "if (evt.lengthComputable) {"
        "var per = evt.loaded / evt.total;"
        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
        "$('#bar').css('width',Math.round(per*100) + '%');"
        "}"
        "}, false);"
        "return xhr;"
        "},"
        "success:function(d, s) {"
        "console.log('success!') "
        "},"
        "error: function (a, b, c) {"
        "}"
        "});"
        "});"
        "</script>" +
        style;

    // Attempt to connect to Wifi network:
    DPRINTF("Connecting to: ");
    DPRINTLN(WIFI_SSID);

    // Connect to WiFi network
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    DPRINTLN("");

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
    mqttClient.setServer(mqttServer, 1883);

    // Declare the callback function
    mqttClient.setCallback(callback);

    // Use MDSN for a friendly hostname - http://jarvis.local
    if (!MDNS.begin(HOST_NAME)) {
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
            if (upload.status == UPLOAD_FILE_START) 
            {
                Serial.printf("Update: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) 
                { 
                    Update.printError(Serial);
                }
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) 
            {
                // Flashing firmware to ESP
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) 
                {
                    Update.printError(Serial);
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) 
            {
                if (Update.end(true)) 
                { 
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                } 
                else 
                {
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
        DPRINTLNF("Failed to read from DHT sensor!");
        return;
    }

    DPRINTF("Temperature: ");
    DPRINT(temperature);
    DPRINTF("°C, humidity: ");
    DPRINTLN(humidity);
    DPRINTLN("%.");

    // Act only on changes
    if (temperature != prevTemp) {
        doc["celsius"] = temperature;
        doc["humidity"] = humidity;
        doc["index"] = heatIndex;
        serializeJson(doc, payload);
        mqttClient.publish("jarvis/temperature", payload);
        DPRINTLNF("Published on jarvis/temperature");
    }
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
    // Print basic info about the firmware, handy on first sight
    Serial.println(F("=============================================\n"));
    Serial.println(F("Kitchen Controller - OTA Server - MQTT client"));
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

    // Start serial communication
    Serial.begin(115200);

    // Call the full wifi setup
    setupWifi();

    // Start the DHT server
    dht.begin();
    DPRINTLNF("DHT sensor started");
}

/**====================================
 *    LOOP function
 *===================================**/
void loop(void) {
    if (millis() >= time1 + KEEP_ALIVE) {
        time1 += KEEP_ALIVE;
        if (!mqttClient.connected()) {
            reconnect();
        }
        mqttClient.loop();
    }
    if (millis() >= time2 + SENSOR_CHECK_DELAY) {
        time2 += SENSOR_CHECK_DELAY;
        handleSensorReadings();
    }
    // Handle possible http cpnnections
    otaServer.handleClient();
}