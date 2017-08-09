/*******************************************************************
  Chaque seconde, un nombre est émis par une carte Arduino munie
  d'un module nRF24L01.
  Ce message peut être capté par un autre Arduino, ou par un Raspberry Pi.
  http://electroniqueamateur.blogspot.ca/2017/02/communication-par-nrf24l01-entre-deux.html
********************************************************************/

#include <SPI.h>
#include "TimeLib.h"
#include "nRF24L01.h"
#include "RF24.h"
#include "DS3232RTC.h"
#include "PCF8583.h"
#include "Wire.h"
#include "LowPower.h"

unsigned long duration;

const int anemoPin = 2;
bool IT = false;
bool task3sec = false;

unsigned long t1 = 0;
unsigned long t2 = 0;
unsigned long tic = 0;
unsigned long current;

PCF8583 pcf8583(0xA0); // counter for anemometer

tmElements_t tm;
tmElements_t tm0;




RF24 radio(7, 8);
const uint64_t addresse = 0x1111111111;

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

float wind_inst = 0;
float wind_avg  = 0;

uint8_t windpacket[7];

uint8_t state_machine_second = 0;
uint8_t measure_done = 0;  //0 : measure not started, 1: 1st edge detected, 2: second edge detected.
uint8_t dir_samples;
int     dir_sum;

long windspeed_32ksum = 0;
int  anemo_turns = 0;

float         windspeed_sum = 0;
unsigned long windspeed_index = 0;

// wind moving average calculation (windspeed = 10minutes average)
const int numReadings = 20; // 10 minutes, with 1 sample very 3s = 200
int readIndex = 0;
float readings[numReadings];
float total = 0;
float average = 0;


bool first_edge_occured = false;


/*

  void anemometerIT() {
  detachInterrupt(digitalPinToInterrupt(anemoPin));
  if (t1 == 0) {
    t1 = millis();
    attachInterrupt(digitalPinToInterrupt(anemoPin) , anemometerIT, RISING);
  }
  else if (t2 == 0) {
    t2 = millis();
  }
  }*/

void risingedge() {

  //detachInterrupt(digitalPinToInterrupt(anemoPin));
  // tic = pcf8583.getCount;
  //attachInterrupt(digitalPinToInterrupt(anemoPin) , risingedge, RISING);
  // Serial.println(t1);
  //attachInterrupt(digitalPinToInterrupt(anemoPin) , fallingedge, RISING);

}
void fallingedge() {
  detachInterrupt(digitalPinToInterrupt(anemoPin));
  t2 = pcf8583.getCount();
  // Serial.println(t2);
}


/*
  Serial.print("*");
  if(first_edge_occured == false) {
    Serial.print("A");
    pcf8583.setCount(0);
    first_edge_occured = true;
    Serial.print("B");
  }
  else {
  windspeed_32ksum = pcf8583.getCount();
  anemo_turns ++ ;
  }
  Serial.print("#");


  attachInterrupt(digitalPinToInterrupt(anemoPin) , anemometerIT,RISING);
*/



void setup(void)
{
  Serial.begin(115200);
  //DS3232 settings:
  setSyncProvider(RTC.get);
  RTC.squareWave(SQWAVE_1_HZ);   ///////   supprimer sqwave ???

  tm0.Hour = 0;             //set the tm structure to 0h00m00s on 1Jan1970
  tm0.Minute = 0;
  tm0.Minute = 0;
  tm0.Day = 1;
  tm0.Month = 1;
  tm0.Year = 0;


  // PCF8583 settings:
  pcf8583.setMode(MODE_EVENT_COUNTER);
  pcf8583.setCount(0);

  // setup nRF24l01
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
  radio.openWritingPipe(addresse);
  //Serial.println(""); Serial.println(""); Serial.println("Boot");

  /*
    Serial.println(millis());
    delay(1000);
    Serial.println(millis());
    // dummy values for test
    winddir.i = 1;
    windspeed.f = 1.5;
    Serial.println("debug");

    while (digitalRead(anemoPin) == LOW) {}
    Serial.println(millis());
    while (digitalRead(anemoPin) == HIGH) {}
    Serial.println(millis());
    while (digitalRead(anemoPin) == LOW) {}
    Serial.println(millis());
    while (digitalRead(anemoPin) == HIGH) {}
    Serial.println(millis());
  */

  /*
    duration = pulseIn(anemoPin, HIGH, 2000000);
    Serial.println(duration);
    duration = pulseIn(anemoPin, LOW, 2000000);
    Serial.println(duration);
    duration = pulseIn(anemoPin, HIGH, 2000000);
    Serial.println(duration);
    duration = pulseIn(anemoPin, LOW, 2000000);

  */
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  winddir.i = 90; // for debug
  windgust.f = 0;

  pinMode(anemoPin,  INPUT);
  attachInterrupt(digitalPinToInterrupt(anemoPin) , risingedge, RISING);
  interrupts();
  RTC.write(tm0);
  Serial.println("Go!");
  delay(20);
}

void loop(void) {


  LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
  detachInterrupt(digitalPinToInterrupt(anemoPin));

  tic = pcf8583.getCount();

  // check if we woke up before the 2s sleep   (32768 = 1sec)
  if (( t1 == 0 ) && (t2 == 0) && (tic <= 42000)) {                 // acquisition of 1st edge of anemometer period
    t1 = tic;  // record 1st edge
    //Serial.print("t1!"); Serial.println(t1); delay(1);
  }
  else {
    if (( t1 > 0 )  && (t2 == 0) && ((tic - t1) <= 42000)) {         // acquitision of 2nd edge of anemometer period: we have now a measured period! let's transmit value.
      t2 = tic;  // record 2nd edge, we have a wind measure !
      // process the wind measurement !
      wind_inst = 4 * 32768 / (float)(t2 - t1);
      if ( wind_inst > (wind_avg + 18)) {
        windgust.f = wind_inst;
        windpacket[0] = winddir.b[0];
        windpacket[1] = winddir.b[1];
        windpacket[2] = windgust.b[0];
        windpacket[3] = windgust.b[1];
        windpacket[4] = windgust.b[2];
        windpacket[5] = windgust.b[3];
        windpacket[6] = 1;
      }

      //      windspeed_sum += windgust.f;  // add value to sum for average calculation
      //      windspeed_index++;
      t1 = 0;
      t2 = 0;
      /*while ( pcf8583.getCount() < 85000 ) {  // wait until we reach 3s
        LowPower.idle(SLEEP_250MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
        }*/
      while ( pcf8583.getCount() < 97700 ) {  // wait until we reach 3s
        LowPower.idle(SLEEP_15MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
      }
      //Serial.print(" rtc:"); Serial.print(pcf8583.getCount()); Serial.println("***********");
      pcf8583.setCount(0);
      send_packet(windpacket);



    }
    else {    // no wind, reinit
      //windspeed_index++;
      t1 = 0;
      t2 = 0;
      /* while ( pcf8583.getCount() < 85000 ) {  // wait until we reach 3s
         LowPower.idle(SLEEP_250MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
        }*/
      while ( pcf8583.getCount() < 98304 ) {  // wait until we reach 3s
        LowPower.idle(SLEEP_15MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
      }
      pcf8583.setCount(0);
    }
    RTC.read(tm);
    total = total - readings[readIndex];
    readings[readIndex] = wind_inst;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
      readIndex = 0;
    }
    wind_avg = total / (float)(numReadings);

    Serial.print("wind_inst: "); Serial.print(wind_inst); Serial.print(" km/h ");
    Serial.print("avge :"); Serial.print(  wind_avg ); Serial.print(" km/h "); Serial.print("index: "); Serial.print(readIndex);
    if (windgust.f > 0) {
      Serial.print(" gust: ");
      Serial.print(windgust.f);
      Serial.println(" km/h");
    } else Serial.println("");
    delay(3);
    windgust.f = 0;

    if (tm.Minute == 1) {
      // send the 10 minutes average packet
      // ....

      // reinit RTC
      RTC.write(tm0);
    }
  }
  attachInterrupt(digitalPinToInterrupt(anemoPin) , risingedge, RISING); delay(1);
}








void send_packet(uint8_t packet[7]) {
  radio.powerUp();
  delay(10);
  radio.write(packet, 7);
  delay(10);
  radio.powerDown();
}

