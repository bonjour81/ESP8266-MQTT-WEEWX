#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Important information : to get out of deepsleep, it is needed that GPIO16 (RTC output) is connected to RESET pin of the ESP8266
// GPIO16 is connected is D2 on my Wemos D1 R1, but it may be D0 on some others.

// WiFi connexion informations
const char* ssid = "your hotspot ssid";
const char* password = "your wifi password";
IPAddress ip(192, 168, 1, xx);      // set xx to the expected IP address
IPAddress gateway(192, 168, 1, xx); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network

// MQTT server informations
#define mqtt_server "<mosquitto_broker_ip>"
#define mqtt_user "<mqtt_username>"  //s'il a été configuré sur Mosquitto
#define mqtt_password "<mqtt_password>" //idem
#define temperature_topic "weewx/outTemp"  //Topic température
#define humidity_topic "weewx/outHumidity"        //Topic humidité


// Time to sleep (in seconds):
const int sleepTimeS = 60;
// set true to get more information on serial
bool debug = true;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);     // not necessary, only for debug
  setup_wifi();             // Connexion to wifi
  client.setServer(mqtt_server, 1883);    // mqtt server config
  while (!client.connected()) {     // make sure to reconnect to mqtt
    reconnect();
  }
  client.loop();
  // read humidity -> currently set to dummy value while waiting for the sensor
  float h = random(70,77);             
  //read temp -> currently set to dummy value while waiting for the sensor
  float t = random(17,20);  
  if ( debug ) {
     Serial.print("Temperature : ");
     Serial.print(t);
     Serial.print(" | Humidite : ");
     Serial.println(h);
     delay(10000);
   }  

   // publish the measured values on mqtt
   client.publish(temperature_topic, String(t).c_str());    //Publie la température sur le topic temperature_topic
   client.publish(humidity_topic, String(h).c_str());      //Et l'humidité
   if (debug) {
   Serial.println("closing connection, ESP8266 goes in deepsleep mode");
   }
   Serial.end();
   delay(1000);
   ESP.deepSleep(sleepTimeS * 1000000);
}


void loop() { 

}

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
  Serial.println("=> Addresse IP : ");
  Serial.print(WiFi.localIP());
}

//Reconnexion
void reconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!client.connected()) {
    Serial.print("Connexion au serveur MQTT...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("OK");
    } else {
      Serial.print("KO, erreur : ");
      Serial.print(client.state());
      Serial.println(" On attend 5 secondes avant de recommencer");
      delay(5000);
    }
  }
}

