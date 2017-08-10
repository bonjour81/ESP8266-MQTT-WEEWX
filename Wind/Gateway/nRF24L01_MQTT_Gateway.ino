
/***********************************************************
   Sketch permettant à l'Arduino de recevoir des messages
   en provenance d'un autre Arduino, ou d'un Raspberry, par
   l'entremise d'un module nRF24L01.
   Les messages reçus sont affichés dans le moniteur série.
   http://electroniqueamateur.blogspot.ca/2017/02/communication-par-nrf24l01-entre-deux.html
*************************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

#include "passwords.h"


// WiFi connexion informations //////////////////////////////////////////////////////////////
const char* espname = "ESP_RF24_MQTT_gateway";
IPAddress ip(192, 168, 1, 190);  // pour l'ESP8266 "R1 UNO"
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
int trials = 0;


// MQTT server informations /////////////////////////////////////////////////////////////////
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish windspeedPub     = Adafruit_MQTT_Publish(&mqtt, "weewx/windSpeed"  , 0);
Adafruit_MQTT_Publish windgustPub      = Adafruit_MQTT_Publish(&mqtt, "weewx/windGust"   , 0);
Adafruit_MQTT_Publish winddirPub       = Adafruit_MQTT_Publish(&mqtt, "weewx/windDir"    , 0);
Adafruit_MQTT_Publish windgustdirPub   = Adafruit_MQTT_Publish(&mqtt, "weewx/windGustDir", 0);
Adafruit_MQTT_Publish test_pubPub      = Adafruit_MQTT_Publish(&mqtt, "fred/test", 0);


// nRF24L01 parameters //////////////////////////////////////////////////////////////////////

RF24 radio(4, 5); //CE = GPIO4 / CSN = GPIO5
const uint64_t adresse = 0x1111111111;
const int taille = 7;
char message[taille + 1];
bool received = false;


union {
   int i;
   uint8_t b[2];
} winddir;
union {
   float f;
   uint8_t b[4];
} windspeed;
union {
   float f;
   uint8_t b[4];
} windgust;
union {
   int i;
   uint8_t b[2];
} windgustdir;



uint8_t windpacket[7];


unsigned long top = 0;
unsigned long previous_top = 0;
unsigned long previous_top2 = 0;

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
      radio.read( windpacket, taille );
      top = millis();

      switch (windpacket[6]) {
         case 1:  // windgust
            windgustdir.b[0] = windpacket[0];
            windgustdir.b[1] = windpacket[1];
            windgust.b[0] =    windpacket[2];
            windgust.b[1] =    windpacket[3];
            windgust.b[2] =    windpacket[4];
            windgust.b[3] =    windpacket[5];
            Serial.print(top - previous_top); Serial.print(" Message recu :"); Serial.print(windpacket[6]); Serial.print(" dir: "); Serial.print(windgustdir.i);
            Serial.print(" gust: "); Serial.print(windgust.f); Serial.println(" km/h");
            setup_wifi();
            MQTT_connect();
            Serial.println(windgustdirPub.publish(windgustdir.i));delay(10);
            setup_wifi();
            MQTT_connect();
            Serial.println(windgustPub.publish(windgust.f));delay(10);
            
            
            break;
         case 2:   // windspeed
            winddir.b[0] =   windpacket[0];
            winddir.b[1] =   windpacket[1];
            windspeed.b[0] = windpacket[2];
            windspeed.b[1] = windpacket[3];
            windspeed.b[2] = windpacket[4];
            windspeed.b[3] = windpacket[5];
            Serial.print(top - previous_top); Serial.print(" Message recu :"); Serial.print(windpacket[6]); Serial.print(" dir: "); Serial.print(winddir.i);
            Serial.print(" wind: "); Serial.print(windspeed.f); Serial.print(" km/h "); Serial.println(top - previous_top2);
            previous_top2 = top;
            setup_wifi();
            MQTT_connect();
            Serial.println(winddirPub.publish(winddir.i));delay(10);
            setup_wifi();
            MQTT_connect();
            Serial.println(windspeedPub.publish(windspeed.f));delay(10);
            break;
         case 3:   // voltage /current



            break;
      } // end case
   } // end while


   /*  winddir.b[0] =   windpacket[0];
      winddir.b[1] = windpacket[1];
      windgust.b[0] = windpacket[2];
      windgust.b[1] = windpacket[3];
      windgust.b[2] = windpacket[4];
      windgust.b[3] = windpacket[5];
      Serial.print("Message recu :"); Serial.print(top - previous_top); Serial.print(" dir: "); Serial.print(winddir.i);  Serial.print(" gust: ");
      Serial.print(windgust.f); Serial.print(" km/h");
      if ((top - previous_top) > 3100) {
        Serial.println("************");
      }
      else {
        Serial.println("");
      }
      previous_top = top;


      received = true;
      //yield();
      }

      /*
      if (received) {
      MQTT_connect();
      Serial.print("mqtt publishing:");
      Serial.println( test_pub.publish(message));
      received = false;
      //mqtt.disconnect();
      }*/
   //  delay(10);



}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Connexion au réseau WiFi
void setup_wifi() {
   if (WiFi.status() != WL_CONNECTED) {   // function can be used to connect or reconnect check, so if connected, no need to do anything.
      Serial.println();
      Serial.print("Connexion a ");
      Serial.println(ssid);
      // config static IP
      WiFi.config(ip, gateway, subnet);
      WiFi.begin(ssid, password);

      while ( ( WiFi.status() != WL_CONNECTED) && ( trials < 20 )) {
         delay(500);
         Serial.print(".");
         trials++;
      }
      if (trials >= 25) ESP.restart();
      trials = 0;
      Serial.println("");
      Serial.println("Connexion WiFi etablie ");
      Serial.print("=> Addresse IP : ");
      Serial.println(WiFi.localIP());
   }
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
