

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
const long motionInterval = 1000 * 5;

char charPayload[50];

struct pirSensor {
  const char* topic;
  int timerActive;
  int pirState;
  unsigned long lastScene; //rename to lastSeen ??
  int pin;
};

pirSensor pir_diningRoom = {MQTT_CLIENT_NAME"/diningroompir/state", 0, 0, 0, 4};
pirSensor pir_mainHall = {MQTT_CLIENT_NAME"/mainhallpir/state", 0, 0, 0, 5};
pirSensor pir_tvRoom = {MQTT_CLIENT_NAME"/tvroompir/state", 0, 0, 0, 6};
pirSensor pir_upstairs = {MQTT_CLIENT_NAME"/upstairspir/state", 0, 0, 0, 7};
pirSensor pir_basementHall = {MQTT_CLIENT_NAME"/basementhallpir/state", 0, 0, 0, 8};
pirSensor pir_basementMain = {MQTT_CLIENT_NAME"/basementmainpir/state", 0, 0, 0, 9};

pirSensor sensors[6] {
  pir_diningRoom, pir_mainHall, pir_tvRoom, pir_upstairs, pir_basementHall, pir_basementMain
};

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

//Infrared sensors
OneButton diningRoomPir(pir_diningRoom.pin, false, false);
OneButton mainHallPir(pir_mainHall.pin, false, false);
OneButton tvRoomPir(pir_tvRoom.pin, false, false);
OneButton upstairsHallPir(sensors[3].pin, false, false);
OneButton basementHallPir(pir_basementHall.pin, false, false);
OneButton basementMainPir(pir_basementMain.pin, false, false);

//contact closure sensors
OneButton frontDoor(5, false, false);
OneButton backDoor(5, false, false);
OneButton sideDoor(5, false, false);

// motion occurs
// if timer running
//   reset timer
// if timer not running
// send ALERT
// start timer

//timer expires
// send CLEAR
// clear timer

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupOTA();
  setupMqtt();
  setupButtons();
  setupPinModes();
}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }
  pubSubClient.loop();

  tickButons();
  processSensors();
}

void processSensors() {
  processSensor(pir_diningRoom);
  processSensor(pir_mainHall);
  processSensor(pir_tvRoom);
  processSensor(pir_upstairs);
  processSensor(pir_basementHall);
  processSensor(pir_basementMain);
}


void tickButons() {
  diningRoomPir.tick();
  mainHallPir.tick();
  tvRoomPir.tick();
  upstairsHallPir.tick();
  basementHallPir.tick();
  basementMainPir.tick();
  frontDoor.tick();
  backDoor.tick();
  sideDoor.tick();
}

void processSensor(pirSensor sensor) {
  unsigned long currentMillis = millis();
  if (sensor.timerActive && currentMillis - sensor.lastScene >= motionInterval) {
    Serial.print(sensor.topic);
    Serial.println(" clear");
    pubSubClient.publish(sensor.topic, "clear");
    sensor.timerActive = 0;
  }
}

void setupPinModes() {
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

void setupButtons() {
  diningRoomPir.attachLongPressStart([]() {
    motionDetected(sensors[0]);
  });
  mainHallPir.attachLongPressStart([]() {
    motionDetected(sensors[1]);
  });
  tvRoomPir.attachLongPressStart([]() {
    motionDetected(sensors[2]);
  });
  upstairsHallPir.attachLongPressStart([]() {
    motionDetected(sensors[3]);
  });
  basementHallPir.attachLongPressStart([]() {
    motionDetected(sensors[4]);
  });
  basementMainPir.attachLongPressStart([]() {
    motionDetected(sensors[5]);
  });


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

void motionDetected(pirSensor sensor) {
  sensor.lastScene = millis();

  //start timer, send alert
  if (sensor.timerActive == 0) {
    sensor.timerActive = 1;
    Serial.print(sensor.topic);
    Serial.println("motion");
    pubSubClient.publish(sensor.topic, "motion");
  }
}


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
