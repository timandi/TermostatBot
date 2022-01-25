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
#include <UniversalTelegramBot.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

// Include configuration file
#include "config.h"
// #define WIFI_SSID "*********"
// #define WIFI_PASS "*********"
// #define HOST_NAME "termostat"
// #define BOT_TOKEN "*********"
// #define CHAT_ID "*********"
// #define HEATING_PIN 25      // Output pin - relay
// #define VENTILATION_PIN 26  // Output pin - relay
// #define LIGHT_PIN 27        // Outout pin - relay
// #define SPARE_PIN 33        // Outout pin - relay
// #define BUTTON_PIN 32       // Input pin - button
// #define TEMPERATURE_PIN 33  // Input pin - sensor
// #define DHT_TYPE 22         // DHT22
// #define BOT_REQUEST_DELAY   1000
// #define SENSOR_CHECK_DELAY  200

// Define wifi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

// Store last bot action timestamp
unsigned long last_bot_request;
unsigned long last_sensor_check;
float temperature;
float humidity;
int button_state = LOW;
int last_button_state = LOW;
int heating_state = LOW;
int ventilation_state = LOW;
int light_state = LOW;
int spare_state = LOW;

// Define objects
WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
DHT dht(TEMPERATURE_PIN, DHT_TYPE);

// Store the webpage html and css code
String style;
String loginIndex;
String serverIndex;

//================================================================
// Main callback function reached when a new message arrives
//================================================================
void handleNewMessages(int numNewMessages) {
    Serial.print("\n\n New Telegram message received: ");

    // Handle any number of messages in the queue
    for (int i = 0; i < numNewMessages; i++) {
        String chat_id = bot.messages[i].chat_id;
        String text = bot.messages[i].text;
        String from_name = bot.messages[i].from_name;

        if (from_name == "") {
            from_name = "Guest";
        }

        Serial.println(text);
        text.toLowerCase();

        // Handle commands
        if (strstr("/start help hello", text.c_str())) {
            String welcome =
                "Welcome, " + from_name +
                ".\n"
                "Here is a list of commands that you can use:\n\n"
                "/status : see the status of the controllers\n"
                "/heating     : thermosrat controller\n"
                "/ventilation : ventilation controller\n"
                "/lights      : manually control the lights\n"
                "/joke        : self explainatory, isn't it?\n";

            String keyboardJson =
                "["
                "[\"/heating\", \"/ventilation\"],"
                "[\"/status\"],"
                "[\"/lights\"],"
                "[\"/joke\"]"
                "]";
            bot.sendMessageWithReplyKeyboard(chat_id, welcome, "Markdown", keyboardJson, true);
        }
        if (strstr("timandi site cv", text.c_str())) {
            String keyboardJson =
                "[[{ \"text\" : \"Go to the website\", \"url\" : \"https://timandi.xyz\" }],"
                "[{ \"text\" : \"Tell him I said Hi\", \"callback_data\" : \"Si el :))\" }]]";
            bot.sendMessageWithInlineKeyboard(chat_id, "Choose from one of the following options", "", keyboardJson);
        }
        if (strstr("/status sts", text.c_str())) {
            Serial.println("status 1");
            String menu =
                "Here's the status of all the controllers:\n\n"
                "Curently, there are ";
            menu += String(temperature);
            menu +=
                "°C. \n"
                " The heating is ";
            menu += heating_state ? "ON\n" : "OFF\n";
            menu += "The ventilation is ";
            menu += ventilation_state ? "ON\n" : "OFF\n";
            menu += "The light is ";
            menu += light_state ? "ON\n" : "OFF\n";
            Serial.println("status 2");
            String keyboardJson =
                "["
                "[\"/heating\", \"/ventilation\"],"
                "[\"/status\"],"
                "[\"/lights\"],"
                "[\"/joke\"]"
                "]";
            Serial.println("status 3");
            bot.sendMessageWithReplyKeyboard(chat_id, menu, "Markdown", keyboardJson, true);
            Serial.println("status 4");
        }
        if (strstr("/heating centrala caldura", text.c_str())) {
            String menu =
                "This is the Heating control panel\n\n"
                "Curently, there are ";
            menu += String(temperature);
            menu +=
                "°C. \n"
                " and the heating is ";
            menu +=
                heating_state
                    ? "ON"
                    : "OFF";
            String keyboardJson =
                "["
                "[{ \"text\" : \"Turn ON\", \"callback_data\" : \"/heating_on\" }],"
                "[{ \"text\" : \"Turn OFF\", \"callback_data\" : \"/heating_off\" }]"
                "]";
            bot.sendMessageWithInlineKeyboard(chat_id, menu, "", keyboardJson);
        }

        if (strstr("/ventilation hota fan", text.c_str())) {
            String menu =
                "This is the ventilation control panel\n\n"
                "Curently, the ventilation is ";
            menu +=
                heating_state
                    ? "ON"
                    : "OFF";
            String keyboardJson =
                "["
                "[{ \"text\" : \"Turn ON\", \"callback_data\" : \"/ventilation_on\" }],"
                "[{ \"text\" : \"Turn OFF\", \"callback_data\" : \"/ventilation_off\" }]"
                "]";
            bot.sendMessageWithInlineKeyboard(chat_id, menu, "", keyboardJson);
        }

        if (strstr("/light lumina bec", text.c_str())) {
            String menu =
                "This is the light control panel\n\n"
                "Curently, the light is ";
            menu +=
                heating_state
                    ? "ON"
                    : "OFF";
            String keyboardJson =
                "["
                "[{ \"text\" : \"Turn ON\", \"callback_data\" : \"/light_on\" }],"
                "[{ \"text\" : \"Turn OFF\", \"callback_data\" : \"/light_off\" }]"
                "]";
            bot.sendMessageWithInlineKeyboard(chat_id, menu, "", keyboardJson);
        }

        if (text == "/heating_on") {
            heating_state = HIGH;
            digitalWrite(HEATING_PIN, HIGH);
            Serial.println("---heating turned on---");
            bot.sendMessage(chat_id, "Heating turned ON ", "");
        }
        if (text == "/heating_off") {
            heating_state = LOW;
            digitalWrite(HEATING_PIN, LOW);
            Serial.println("---heating turned off---");
            bot.sendMessage(chat_id, "Heating turned OFF", "");
        }

        if (text == "/ventilation_on") {
            ventilation_state = HIGH;
            digitalWrite(VENTILATION_PIN, HIGH);
            Serial.println("---ventilation turned on---");
            bot.sendMessage(chat_id, "ventilation turned ON ", "");
        }
        if (text == "/ventilation_off") {
            ventilation_state = LOW;
            digitalWrite(VENTILATION_PIN, LOW);
            Serial.println("---ventilation turned off---");
            bot.sendMessage(chat_id, "ventilation turned OFF", "");
        }

        if (text == "/light_on") {
            light_state = HIGH;
            digitalWrite(LIGHT_PIN, HIGH);
            Serial.println("---light turned on---");
            bot.sendMessage(chat_id, "light turned ON ", "");
        }
        if (text == "/light_off") {
            light_state = LOW;
            digitalWrite(LIGHT_PIN, LOW);
            Serial.println("---light turned off---");
            bot.sendMessage(chat_id, "light turned OFF", "");
        }

        if (text == "/joke") {
            bot.sendChatAction(chat_id, "typing");
            delay(1000);
            bot.sendMessage(chat_id, ":|", "");
        }
    }
}
/**====================================
 *    WIFI & OTA setup function
 *===================================**/
void setupWifi() {
    // CSS style
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

    // Additional SSL certificate
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    // Attempt to connect to Wifi network:
    Serial.print("Connecting to: ");
    Serial.println(ssid);

    // Connect to WiFi network
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Use MDSN for a friendly hostname - http://termostat.local
    if (!MDNS.begin(HOST_NAME)) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("You can access the dashboard at http://termostat.local");

    // Returns login page
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", loginIndex);
    });

    // Returns server page
    server.on("/serverIndex", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
    });

    // Handle upload of new firmware
    server.on(
        "/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart(); }, []() {
            HTTPUpload& upload = server.upload();
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
    server.begin();
    Serial.println("WiFi connectiion successfull");
}

/**====================================
 *    SETUP function
 *===================================**/
void setup(void) {
    // Initialize the pins as outputs and pull them high
    pinMode(HEATING_PIN, OUTPUT);
    pinMode(VENTILATION_PIN, OUTPUT);
    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(SPARE_PIN, OUTPUT);

    digitalWrite(HEATING_PIN, heating_state);
    digitalWrite(VENTILATION_PIN, ventilation_state);
    digitalWrite(LIGHT_PIN, light_state);
    digitalWrite(SPARE_PIN, spare_state);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(TEMPERATURE_PIN, INPUT);

    button_state = digitalRead(BUTTON_PIN);

    // Start serial connection for debugging
    Serial.begin(115200);
    dht.begin();

    setupWifi();
}

/**====================================
 *    LOOP function
 *===================================**/
void loop(void) {
    server.handleClient();

    if (millis() > last_bot_request + BOT_REQUEST_DELAY) {
        // Check for new messages
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

        while (numNewMessages) {
            Serial.println("got response");
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        last_bot_request = millis();
    }

    if (millis() > last_sensor_check + SENSOR_CHECK_DELAY) {
        // Motion sensor reading
        last_button_state = button_state;
        temperature = dht.readTemperature();
        humidity = dht.readHumidity();
        button_state = digitalRead(BUTTON_PIN);

        Serial.print("Temperature: [");
        Serial.print(temperature);
        Serial.print("°C], [");
        Serial.print(humidity);
        Serial.print("%] hum");

        Serial.print(" Fan button - ");
        if (button_state) {
            Serial.print("[ON]");
        } else {
            Serial.print("[OFF]");
        }

        Serial.print(" Heating: ");
        if (heating_state) {
            Serial.print("[ON]");
        } else {
            Serial.print("[OFF]");
        }

        Serial.print(" Ventilation: ");
        if (ventilation_state) {
            Serial.print("[ON]");
        } else {
            Serial.print("[OFF]");
        }

        Serial.print(" Light: ");
        if (light_state) {
            Serial.print("[ON]");
        } else {
            Serial.print("[OFF]");
        }

        Serial.print(" Spare: ");
        if (spare_state) {
            Serial.print("[ON]");
        } else {
            Serial.print("[OFF]");
        }

        if (last_button_state != button_state) {
            Serial.println("---Button pressed---");
        }
        Serial.println("");
        last_sensor_check = millis();
    }
}
