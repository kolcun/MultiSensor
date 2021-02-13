

#include <WiFi.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneButton.h>
#include "credentials.h"

#define HOSTNAME "multisensor"
#define MQTT_CLIENT_NAME "kolcun/indoor/multisensor"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;
const char* overwatchTopic = MQTT_CLIENT_NAME"/overwatch";

int timerActive = 0;
const long motionInterval = 1000 * 5;
int diningRoomPirState = 0;
unsigned long diningRoomMillis = 0;

char charPayload[50];

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

//Infrared sensors
OneButton diningRoomPir(4, false, false);
OneButton mainHallPir(4, false, false);
OneButton tvRoomPir(4, false, false);
OneButton upstairsHallPir(4, false, false);
OneButton basementHallPir(4, false, false);
OneButton basementMainPir(4, false, false);

//contact closure sensors
OneButton frontDoor(5, false, false);
OneButton backDoor(5, false, false);
OneButton sideDoor(5, false, false);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupOTA();
  setupMqtt();
  setupButtons();

  pinMode(4, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);
  pinMode(5, INPUT);

}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }
  pubSubClient.loop();

  diningRoomPir.tick();
  mainHallPir.tick();
  tvRoomPir.tick();
  upstairsHallPir.tick();
  basementHallPir.tick();
  basementMainPir.tick();
  frontDoor.tick();
  backDoor.tick();
  sideDoor.tick();

  unsigned long currentMillis = millis();

  if (timerActive && currentMillis - diningRoomMillis >= motionInterval) {
    Serial.println("clear");
    pubSubClient.publish(MQTT_CLIENT_NAME"/diningroompir/state", "clear");
    timerActive = 0;
  }

}

// motion occurs
// if timer running
//   reset timer
// if timer not running
// send ALERT
// start timer

//timer expires
// send CLEAR
// clear timer


void setupButtons() {
  diningRoomPir.attachLongPressStart(diningRoomMotion);
  mainHallPir.attachLongPressStart(mainHallMotion);
  tvRoomPir.attachLongPressStart(tvRoomMotion);
  upstairsHallPir.attachLongPressStart(upstairsHallMotion);
  basementHallPir.attachLongPressStart(basementHallMotion);
  basementMainPir.attachLongPressStart(basementMainMotion);


  frontDoor.attachLongPressStart([]() {
    publishOpen(MQTT_CLIENT_NAME"/frontdoor/state");
  });
  frontDoor.attachLongPressStop([]() {
    publishClose(MQTT_CLIENT_NAME"/frontdoor/state");
  });
  frontDoor.setPressTicks(100);

  backDoor.attachLongPressStart([]() {
    publishOpen(MQTT_CLIENT_NAME"/backdoor/state");
  });
  backDoor.attachLongPressStop([]() {
    publishClose(MQTT_CLIENT_NAME"/backdoor/state");
  });
  backDoor.setPressTicks(100);

  sideDoor.attachLongPressStart([]() {
    publishOpen(MQTT_CLIENT_NAME"/sidedoor/state");
  });
  sideDoor.attachLongPressStop([]() {
    publishClose(MQTT_CLIENT_NAME"/sidedoor/state");
  });
  sideDoor.setPressTicks(100);
}

void publishOpen(const char* topic) {
  pubSubClient.publish(topic, "open");
}
void publishClose(const char* topic) {
  pubSubClient.publish(topic, "closed");
}

void diningRoomMotion() {

  if (timerActive == 0) {
    //start timer, send alert
    timerActive = 1;
    Serial.println("motion");
    pubSubClient.publish(MQTT_CLIENT_NAME"/diningroompir/state", "motion");
    diningRoomMillis = millis();
  } else {
    //timer already running - reset it
    diningRoomMillis = millis();
  }
}

void mainHallMotion() {}
void tvRoomMotion() {}
void upstairsHallMotion() {}
void basementHallMotion() {}
void basementMainMotion() {}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  //  if (newTopic == MQTT_CLIENT_NAME"/mike/set") {
  //    if (newPayload == "open") {
  //      Serial.println("here");
  //    } else if (newPayload == "close") {
  //      //do something
  //      Serial.println("there");
  //    }
  //  }

}

void setupOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt() {
  pubSubClient.setServer(MQTT_SERVER, 1883);
  pubSubClient.setCallback(mqttCallback);
  if (!pubSubClient.connected()) {
    reconnect();
  }
}

void reconnect() {
  bool boot = true;
  int retries = 0;
  while (!pubSubClient.connected()) {
    if (retries < 10) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (pubSubClient.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWD)) {
        Serial.println("connected");
        // Once connected, publish an announcement...
        if (boot == true) {
          pubSubClient.publish(overwatchTopic, "Rebooted");
          boot = false;
        } else {
          pubSubClient.publish(overwatchTopic, "Reconnected");
        }
        //MQTT Subscriptions
        //        pubSubClient.subscribe(MQTT_CLIENT_NAME"/mike/set");
      } else {
        Serial.print("failed, rc=");
        Serial.print(pubSubClient.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    else {
      ESP.restart();
    }
  }
}
