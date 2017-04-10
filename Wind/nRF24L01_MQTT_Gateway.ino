
// Preliminary test version, not functionnal !




#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ESP8266WiFi.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


// WiFi connexion informations //////////////////////////////////////////////////////////////
const char* ssid = "your_ssid";
const char* password = "your_pass";
const char* espname = "ESP_RF24_MQTT_gateway";
IPAddress ip(192, 168, 1, 191);
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network


// MQTT server informations /////////////////////////////////////////////////////////////////
#define AIO_SERVER      "192.168.1.180"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "your_username"
#define AIO_KEY         "your_mqtt_password"
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish windspeed     = Adafruit_MQTT_Publish(&mqtt, "weewx/windSpeed"  , 0);
Adafruit_MQTT_Publish windgust      = Adafruit_MQTT_Publish(&mqtt, "weewx/windGust"   , 0);
Adafruit_MQTT_Publish winddir       = Adafruit_MQTT_Publish(&mqtt, "weewx/windDir"    , 0);
Adafruit_MQTT_Publish windgustdir   = Adafruit_MQTT_Publish(&mqtt, "weewx/windGustDir", 0);
Adafruit_MQTT_Publish test_pub      = Adafruit_MQTT_Publish(&mqtt, "fred/test", 0);


// nRF24L01 parameters //////////////////////////////////////////////////////////////////////

RF24 radio(4, 5); //CE = GPIO4 / CSN = GPIO5
const uint64_t adresse = 0x1111111111;
const int taille = 32;
char message[taille + 1];
bool received = false;








void setup(void)
{

  Serial.begin(115200);

  // Init nRF24L01
  Serial.println("Recepteur RF24");
  radio.begin();
  radio.openReadingPipe(1, adresse);
  radio.startListening();

  // Connect to wifi
  setup_wifi();
  // Connect to MQTT broker
  MQTT_connect();

}

void loop(void)
{


  while ( radio.available() )
  {
    radio.read( message, taille );
    Serial.print("Message recu : ");
    Serial.println(message);
    received = true;
    yield();
  }
  if (received) {
    MQTT_connect();
    Serial.print("mqtt publishing:");
    Serial.println( test_pub.publish(message));
    received = false;
    //mqtt.disconnect();
  }
  delay(10);



}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Connexion au réseau WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion a ");
  Serial.println(ssid);
  // config static IP
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connexion WiFi etablie ");
  Serial.print("=> Addresse IP : ");
  Serial.println(WiFi.localIP());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MQTT_connect() : connexion/reconnexion to MQTT broket
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 1 seconds...");
    mqtt.disconnect();
    delay(1000);  // wait 1 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
