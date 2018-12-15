#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const uint8_t PUMP_PIN = D0;
const uint8_t SOLENOID_1_PIN = D1;
const uint8_t SOLENOID_2_PIN = D2;

const uint8_t plants[] = {SOLENOID_1_PIN, SOLENOID_2_PIN};

// Connect to the WiFi
const char *ssid = "****";
const char *password = "****";

// topics
const char *waterPlantsTopic = "pwd-water-plants";
const char *configurePumpDelay = "pwd-configure-pump-delay";
const char *configureSolenoid1Duraion = "pwd-configure-solenoid-1-duration";
const char *configureSolenoid2Duraion = "pwd-configure-solenoid-2-duration";
const char *outputTopic = "pwd-output";
const char *outputStatusTopic = "pwd-status-output";

// timers
bool pumpState = false;
int pumpNumber = 1;
int pumpCounter = 0;
bool solenoid1State = false;
int solenoid1Number = 20;
int solenoid1Counter = 0;
bool solenoid2State = false;
int solenoid2Number = 20;
int solenoid2Counter = 0;

// actions
String actionWaterPlantsStart("start");

// state
byte stateBusy = 1;
byte stateFree = 0;
byte state = stateFree;

const IPAddress mqttServerIp(192, 168, 1, 101);

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topicData, byte *payloadData, unsigned int length)
{
  String payload;

  for (unsigned int i = 0; i < length; i++)
  {
    payload.concat((char)payloadData[i]);
  }
  String topic(topicData);

  if (state == stateBusy)
  {
    Serial.println("Busy");
    client.publish(outputTopic, "{\"error\":\"busy\"}");
    return;
  }

  if (topic.equals(waterPlantsTopic))
  {
    if (payload.equals(actionWaterPlantsStart))
    {
      state = stateBusy;
      Serial.println("Watering Plants Started");
      client.publish(outputTopic, "{\"success\":\"watering-plants-started\"}");
      return;
    }
    Serial.print("Unrecognized action: ");
    Serial.println(payload);
    client.publish(outputTopic, "{\"error\":\"unrecognized-message\"}");
    return;
  }

  if (topic.equals(configurePumpDelay))
  {
    const int delay = payload.toInt();
    if (payload.toInt() > 0)
    {

      pumpNumber = delay;
      Serial.printf("Pump delay changed to: %d seconds\n", delay);
      client.publish(outputTopic, "{\"success\":\"pump-delay-changed\"}");
      return;
    }
    Serial.print("Wrong delay: ");
    client.publish(outputTopic, "{\"error\":\"wrong-delay\"}");
    Serial.println(payload);
    return;
  }

  if (topic.equals(configureSolenoid1Duraion))
  {
    const int delay = payload.toInt();
    if (payload.toInt() > 0)
    {

      solenoid1Number = delay;
      Serial.printf("Solenoid 1 duration changed to: %d seconds\n", delay);
      client.publish(outputTopic, "{\"success\":\"solenoid-1-duration-changed\"}");
      return;
    }
    Serial.print("Wrong duration: ");
    client.publish(outputTopic, "{\"error\":\"wrong-duration\"}");
    Serial.println(payload);
    return;
  }

  if (topic.equals(configureSolenoid2Duraion))
  {
    const int delay = payload.toInt();
    if (payload.toInt() > 0)
    {

      solenoid2Number = delay;
      Serial.printf("Solenoid 2 duration changed to: %d seconds\n", delay);
      client.publish(outputTopic, "{\"success\":\"solenoid-2-duration-changed\"}");
      return;
    }
    Serial.print("Wrong duration: ");
    client.publish(outputTopic, "{\"error\":\"wrong-duration\"}");
    Serial.println(payload);
    return;
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");

      client.subscribe(waterPlantsTopic);
      client.subscribe(configurePumpDelay);
      client.subscribe(configureSolenoid1Duraion);
      client.subscribe(configureSolenoid2Duraion);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(57600);

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(SOLENOID_1_PIN, OUTPUT);
  pinMode(SOLENOID_2_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(SOLENOID_1_PIN, HIGH);
  digitalWrite(SOLENOID_2_PIN, HIGH);

  setup_wifi();
  client.setServer(mqttServerIp, 1883);
  client.setCallback(callback);
}

unsigned long previousMillis = millis();
unsigned long currentMillis;

int stage = 0;

void loop()
{
  currentMillis = millis();

  if (state == stateBusy && stage == 0)
  {
    stage = 1;
    pumpState = true;
    digitalWrite(PUMP_PIN, LOW);
    Serial.println("Pump on");
    client.publish(outputStatusTopic, "{\"stage\":\"1\",\"status\":\"pump-on\"}");
  }

  if (pumpCounter == pumpNumber && stage == 1)
  {
    stage = 2;
    solenoid1State = true;
    digitalWrite(SOLENOID_1_PIN, LOW);
    Serial.println("Solenoid 1 on");
    client.publish(outputStatusTopic, "{\"stage\":\"2\",\"status\":\"solenoid-1-on\"}");
  }

  if (solenoid1Counter == solenoid1Number && stage == 2)
  {
    stage = 3;

    solenoid1Counter = 0;
    solenoid1State = false;
    digitalWrite(SOLENOID_1_PIN, HIGH);
    Serial.println("Solenoid 1 off");
    client.publish(outputStatusTopic, "{\"stage\":\"3\",\"status\":\"solenoid-1-off\"}");

    solenoid2State = true;
    digitalWrite(SOLENOID_2_PIN, LOW);
    Serial.println("Solenoid 2 on");
    client.publish(outputStatusTopic, "{\"stage\":\"3\",\"status\":\"solenoid-2-on\"}");
  }

  if (solenoid2Counter == solenoid2Number && stage == 3)
  {
    stage = 0;
    state = stateFree;

    solenoid2Counter = 0;
    solenoid2State = false;
    digitalWrite(SOLENOID_2_PIN, HIGH);
    Serial.println("Solenoid 2 off");
    client.publish(outputStatusTopic, "{\"stage\":\"4\",\"status\":\"solenoid-2-off\"}");

    pumpCounter = 0;
    pumpState = false;
    digitalWrite(PUMP_PIN, HIGH);
    Serial.println("Pump off");
    client.publish(outputStatusTopic, "{\"stage\":\"4\",\"status\":\"pump-off\"}");
  }

  if (currentMillis - previousMillis > 1000)
  {
    if (pumpState)
    {
      pumpCounter++;
    }
    if (solenoid1State)
    {
      solenoid1Counter++;
    }
    if (solenoid2State)
    {
      solenoid2Counter++;
    }
    if (state == stateBusy)
    {
      Serial.println(".");
    }
    previousMillis = millis();
  }

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}