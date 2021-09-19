
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ModbusRTU.h>
#include <elapsedMillis.h>
#include <ArduinoOTA.h>

#define wifi_ssid ""
#define wifi_password ""

#define mqtt_server "192.168.1.240"
#define mqtt_user ""
#define mqtt_password ""

#if defined(ESP8266)
#include <SoftwareSerial.h>
// SoftwareSerial S(D1, D2, false, 256);

// receivePin, transmitPin, inverse_logic, bufSize, isrBufSize
// connect RX to D2 (GPIO4, Arduino pin 4), TX to D1 (GPIO5, Arduino pin 4)
SoftwareSerial S(4, 5);
#endif

ModbusRTU mb;


uint16_t res = 0;

WiFiClient espClient;
PubSubClient client(espClient);
elapsedMillis updateTimer;

void setup() {
  Serial.begin(115200);
#if defined(ESP8266)
  S.begin(9600, SWSERIAL_8N1);
  mb.begin(&S);
#elif defined(ESP32)
  Serial1.begin(9600, SERIAL_8N1);
  mb.begin(&Serial1);
#else
  Serial1.begin(9600, SERIAL_8N1);
  mb.begin(&Serial1);
  mb.setBaudrate(9600);
#endif
  mb.master();

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  ArduinoOTA.setHostname("ESP8266_Solar");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
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

}

bool coils[20];

void loop() {
  if (updateTimer > 5000) {
    updateTimer = 0;
    if (!mb.slave()) {

      float f = 0;
      
      // loadWatts
      mb.readHreg(255, 0x106, &res);
      waitTillModBusDone();
      client.publish("solarbattery/loadWatts", String(res).c_str(), true);
      Serial.print("Load Watts ");
      Serial.println(res);

      // solarWatts
      mb.readHreg(255, 0x109, &res);
      waitTillModBusDone();
      client.publish("solarbattery/pvWatts", String(res).c_str(), true);
      Serial.print("Load Watts ");
      Serial.println(res);

      // chargeamps
      mb.readHreg(255, 0x102, &res);
      waitTillModBusDone();
      f = (float) res / 100;
      client.publish("solarbattery/chargeamps", String(f).c_str(), true);
      Serial.print("Load Watts ");
      Serial.println(res);

      // SOC
      mb.readHreg(255, 0x100, &res);
      waitTillModBusDone();
      res = res & 0x00ff;
      client.publish("solarbattery/SOC", String(res).c_str(), true);
      Serial.print("SOC ");
      Serial.println(res);


      // batvolt
      mb.readHreg(255, 0x101, &res);
      waitTillModBusDone();
      f = (float) res / 10;
      client.publish("solarbattery/batVolts", String(f).c_str(), true);
      Serial.print("Batt Volts: ");
      Serial.println(res);


    }
    mb.task();
    yield();

  }


  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();

}

void setup_wifi() {
  delay(10);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (client.connect("Solar", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("solarbattery/io/switch1/set");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void waitTillModBusDone() {
  while (mb.slave()) { // Check if transaction is active
    mb.task();
    delay(10);
  }
}


void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, "solarbattery/io/switch1/set") == 0) {
    if (!strncmp((char *)payload, "ON", length)) {
      digitalWrite(BUILTIN_LED, LOW);
    }
    if (!strncmp((char *)payload, "OFF", length)) {
      digitalWrite(BUILTIN_LED, HIGH);
    }

  }


}
