#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

ADC_MODE(ADC_VCC);

#define DHTTYPE DHT22   // DHT11 DHT21 DHT22
const int DHTPin = 3;
DHT dht(DHTPin, DHTTYPE);

/*** SLEEPTIME is the interval between two data collections, 1 second = 1,000,000***/
/*** 50ms per reading ***/
/*** 5000ms per sending cycle ***/
#define SLEEPTIME 3600000000

/*** CollectSize is the number of data points to collect before sending to cloud***/
#define CollectSize 4
/*** HEXSTR is the HEX char representation of the value bytes ***/
char HEXSTR_hum[CollectSize*2 + 1];
char HEXSTR_tmp[CollectSize*2 + 1];

typedef struct {
  int CollectPointer;
  byte hum[CollectSize];
  byte tmp[CollectSize];
} rtcStoredInfo;

rtcStoredInfo rtcValues;


const byte rtcStartAddress = 64;





/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) ******************/
const char* ssid = "xxxxxxxx"; //type your WIFI information inside the quotes
const char* password = "xxxxxxx";
const char* mqtt_clientid = "basement_tmp";
const char* mqtt_server = "192.168.0.101";
const char* mqtt_username = "sensor_ge";
const char* mqtt_password = "xxxxxx";
const int mqtt_port = 1883;
const char* topic1 = "sensor/basement_tmp";
const char* topic2 = "sensor/basement_hum";
const char* topic3 = "sensor/Vbasement";
const char* topic4 = "sensor/status";

WiFiClient espClient;
PubSubClient client(espClient);

extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}


void setup() {
  //mark starting cycle time, cycle time will be subtracted from next sleep time
  unsigned long time_spent = micros();

  delay(5);
  //Serial.begin(115200);
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  pinMode(DHTPin, INPUT_PULLUP);
  Serial.println();

  rst_info* ri = system_get_rst_info();
  if (ri == NULL) return;
  switch (ri->reason) {
  case 5:
    system_rtc_mem_read(rtcStartAddress, &rtcValues, sizeof(rtcValues));
    break;
  case 6:
  case 0:
    //WiFi.disconnect();
    send_status();
    reset_all();
    probe();
    send_data();
    reset_all();
    system_rtc_mem_write(rtcStartAddress, &rtcValues, sizeof(rtcValues));
    break;
  }

  Serial.println("next pointer ");
  Serial.println(rtcValues.CollectPointer);


  probe();

  if (rtcValues.CollectPointer >= CollectSize)
  {
    Serial.print("send data ");
    send_data();
    delay(100);
    reset_all();
  }
  system_rtc_mem_write(rtcStartAddress, &rtcValues, sizeof(rtcValues));


  time_spent = (unsigned long)(micros() - time_spent);
  Serial.print("Time spent: ");
  Serial.println(time_spent);
  //do not adjust if time_spent is too long
  if ((time_spent<0) || (time_spent >= SLEEPTIME))
  {
    time_spent = 0;
  }


  if (rtcValues.CollectPointer == CollectSize - 1)
  {
    //Serial.println("enable wifi in next call");
    ESP.deepSleep((unsigned long)SLEEPTIME - time_spent, RF_CAL);
  }
  else
  {
    //Serial.println("disable wifi in next call");
    ESP.deepSleep((unsigned long)SLEEPTIME - time_spent, RF_DISABLED);
  }
}


void loop() {
}

void probe()
{
  //Read data
  dht.begin();
  delay(2000);
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float f = dht.readTemperature(true);
  float h = dht.readHumidity();


  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(f)) 
  {
    delay(100);
    float f = dht.readTemperature(true);
    float h = dht.readHumidity();
  } 

   

  if (isnan(h) || isnan(f))
  {
    //return;
  }

  byte fi = byte(f + 0.5);
  byte hi = byte(h + 0.5);

  Serial.println("Record ");  Serial.print(fi); Serial.print("  "); Serial.print(hi);

  rtcValues.hum[rtcValues.CollectPointer] = hi;
  rtcValues.tmp[rtcValues.CollectPointer] = fi;
  rtcValues.CollectPointer++;

}


void send_data()
{
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  delay(20);

  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  int t = 0;
  while (WiFi.status() != WL_CONNECTED) {
    t++;
    Serial.print(".");
    if (t>15)
    {
      Serial.println("Wifi failed");
      return;
    }
    delay(1000);
  }
  Serial.println("Wifi in");
  client.setServer(mqtt_server, mqtt_port);
  t = 0;
  while (!client.connected()) {
    client.connect(mqtt_username, mqtt_username, mqtt_password);
    t++;
    if (t>10)
    {
      return;
    }
    delay(250);
  }
  int ptr1 = 0;
  int ptr2 = 0;
  for (int i = 0; i < CollectSize; i++)
  {
    ptr1 += snprintf(HEXSTR_tmp + ptr1, sizeof(HEXSTR_tmp) - ptr1, "%02x", rtcValues.tmp[i]);
    ptr2 += snprintf(HEXSTR_hum + ptr2, sizeof(HEXSTR_hum) - ptr2, "%02x", rtcValues.hum[i]);
  }

  client.publish(topic1, HEXSTR_tmp, 0);
  client.publish(topic2, HEXSTR_hum, 0);

  Serial.println("send data tmp and hum");
  Serial.println(HEXSTR_tmp);
  Serial.println(HEXSTR_hum);

  int vcc = ESP.getVcc();
  char buffer[4];
  itoa(vcc, buffer, 10);
  client.publish(topic3, buffer, 1);
  delay(100); 
  //Serial.println(vcc);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

void send_status()
{
	// We start by connecting to a WiFi network
	WiFi.mode(WIFI_STA);
	delay(20);
	WiFi.begin(ssid, password);
	int t = 0;
	while (WiFi.status() != WL_CONNECTED) {
		t++;
		Serial.print(".");
		if (t > 15)
		{
			Serial.println("Wifi failed");
			return;
		}
		delay(1000);
	}


  
	client.setServer(mqtt_server, mqtt_port);
	t = 0;
	while (!client.connected()) {
		client.connect(mqtt_username, mqtt_username, mqtt_password);
		t++;
		if (t > 10)
		{
			return;
		}
		delay(250);
	}


	client.publish(topic4, mqtt_clientid, 0);

  delay(200); 
	//Serial.println(vcc);
	WiFi.disconnect();
	WiFi.mode(WIFI_OFF);
  delay(100); 
}


void reset_all()
{
  rtcValues.CollectPointer = 0;
  for (int i = 0; i < CollectSize; i++)
  {
    rtcValues.hum[i] = 0;
    rtcValues.tmp[i] = 0;
  }
}
