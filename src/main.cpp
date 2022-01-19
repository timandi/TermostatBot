/**============================================================================
 *                   JARVIS - Personal Home Assistant
 *           
 *      - OTA Updates
 *      - Telegram bot
 *===========================================================================**/

// Libraries used in this project
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <UniversalTelegramBot.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

// Define wifi credentials and a friendly host name
const char *ssid = "SkyAndSand";
const char *password = "sandalandala";
const char *host = "jarvis";

// Telegram Bot credentials
#define BOTtoken "1307133693:AAF7Yxytc8eTF9SCDJM9Y6p8DGo_R6_KTtI"
#define CHAT_ID "220204396"

// Define input and output pins
#define relayPin1 25  //output pin   - relay
#define relayPin2 26  //output pin   - relay
#define motionPin 27  //input  pin   - 5v (PIR)

// Variable for storing the states
int motionValue = 0;
int lastValue = 0;
bool lightState = false;
bool alarmState = false;

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Checks for motion every 200ms
int sensorCheckDelay = 50;
unsigned long lastSensorCheck;

// Store the delay time between alerts and lights timer
int messageDelay = 0;
int lightsOnDelay = 0;

// Define objects
WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Store the webpage html and css code
String style;
String loginIndex;
String serverIndex;

// Callback function called when motion is detected
void handleMotionDetected() {
    // If alarm is enabled, send a warning message
    if (messageDelay == 0 && alarmState == true) {
        bot.sendMessage(CHAT_ID, "Motion Detected!!");
        Serial.println("Alarm message sent");
    }

    // Either way, turn the lights on and set the cooldowns
    digitalWrite(relayPin1, LOW);
    Serial.println("Motion detected - Lights ON");
    lightState = true;
    lightsOnDelay = 60;
    messageDelay = 5;
}

// Callback function called when a new message arrived
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

        // Inline buttons with callbacks when pressed will raise a callback_query message
        if (bot.messages[i].type == "callback_query") {
            if (text == "alarmoff") {
                alarmState = false;
                Serial.println("Alarm turned OFF");
                bot.sendMessage(chat_id, "Alarm turned OFF", "");
            }
            if (text == "alarmon") {
                alarmState = true;
                Serial.println("Alarm turned ON");
                bot.sendMessage(chat_id, "Alarm turned ON", "");
            }
            if (text == "lights0") {
                digitalWrite(relayPin1, HIGH);
                lightState = false;
                Serial.println("Lights turned OFF");
                bot.sendMessage(chat_id, "Lights turned OFF", "");
            }
            if (text == "lights1") {
                digitalWrite(relayPin1, LOW);
                lightState = true;
                Serial.println("Lights turned ON");
                bot.sendMessage(chat_id, "Lights turned ON", "");
            }
        }

        // Handle commands
        else {
            if (strstr("/start help hello", text.c_str())) {
                String welcome =
                    "Welcome, " + from_name +
                    ".\n"
                    "Here is a list of commands that you can use:\n\n"
                    "/alarm  : returns the alarm controller\n"
                    "/lights : manually control the lights\n"
                    "/joke   : self explainatory, isn't it?\n";

                String keyboardJson =
                    "["
                    "[\"/alarm\", \"/lights\"],"
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
            if (text == "/alarm") {
                String alarmStateMessage = "Alarm is currently ";
                if (alarmState == true) {
                    alarmStateMessage += "ON";
                } else {
                    alarmStateMessage += "OFF";
                }
                String keyboardJson =
                    "["
                    "[{ \"text\" : \"Turn ON\", \"callback_data\" : \"alarmON\" }],"
                    "[{ \"text\" : \"Turn OFF\", \"callback_data\" : \"alarmOFF\" }]"
                    "]";
                bot.sendMessageWithInlineKeyboard(chat_id, alarmStateMessage, "", keyboardJson);
            }
            if (text == "/lights") {
                String keyboardJson =
                    "["
                    "[{ \"text\" : \"ON\", \"callback_data\" : \"lights1\" }],"
                    "[{ \"text\" : \"OFF\", \"callback_data\" : \"lights0\" }]"
                    "]";
                bot.sendMessageWithInlineKeyboard(chat_id, "Hallway lights", "", keyboardJson);
            }

            if (text == "/joke") {
                bot.sendChatAction(chat_id, "typing");
                delay(1000);
                bot.sendMessage(chat_id, ":|", "");
            }
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
        "<h1>JARVIS Login</h1>"
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

    // Connect to WiFi network
    WiFi.begin(ssid, password);
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
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

    // Use MDSN for a friendly hostname - http://jarvis.local
    if (!MDNS.begin(host)) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("You can access the dashboard at http://jarvis.local");

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
    Serial.println("All good ;)");
}

/**====================================
 *    SETUP function
 *===================================**/
void setup(void) {
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(motionPin, INPUT_PULLUP);

    digitalWrite(relayPin1, HIGH);
    digitalWrite(relayPin2, HIGH);

    Serial.begin(115200);

    setupWifi();
}

/**====================================
 *    LOOP function
 *===================================**/
void loop(void) {
    server.handleClient();

    if (millis() > lastTimeBotRan + botRequestDelay) {
        // Check for new messages
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

        while (numNewMessages) {
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();

        // Decrement the message cooldown
        if (messageDelay) {
            messageDelay--;
        }

        // Handle the lights cooldown
        if (lightsOnDelay) {
            lightsOnDelay--;
            if (lightsOnDelay == 0) {
                digitalWrite(relayPin1, HIGH);
                lightState = false;
                Serial.println("Lights OFF");
            }
        }
        Serial.print(".");
    }

    if (millis() > lastSensorCheck + sensorCheckDelay) {
        // Motion sensor reading
        motionValue = digitalRead(motionPin);
        if (motionValue == HIGH) {
            if (lastValue != HIGH) {
                handleMotionDetected();
                Serial.println("Motion Detected!");
                lastValue = HIGH;
            }
        } else if (lastValue == HIGH) {
            Serial.println("Motion Ended!");
            lastValue = LOW;
        }
        lastSensorCheck = millis();
    }
}
