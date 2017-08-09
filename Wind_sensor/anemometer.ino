/*******************************************************************
  Chaque seconde, un nombre est émis par une carte Arduino munie
  d'un module nRF24L01.
  Ce message peut être capté par un autre Arduino, ou par un Raspberry Pi.
  http://electroniqueamateur.blogspot.ca/2017/02/communication-par-nrf24l01-entre-deux.html
********************************************************************/

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "DS3232RTC.h"
#include "PCF8583.h"
#include "Time.h"
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



uint8_t windpacket[7];

uint8_t state_machine_second = 0;
uint8_t measure_done = 0;  //0 : measure not started, 1: 1st edge detected, 2: second edge detected.
uint8_t dir_samples;
int     dir_sum;

long windspeed_32ksum = 0;
int  anemo_turns = 0;

unsigned long windspeed_sum = 0;
unsigned long windspeed_index = 0;


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

  winddir.i = 90; // for debug

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
  
  //Serial.print("wake! tic:"); Serial.println(tic);
  // check if we woke up before the 2s sleep   (32768 = 1sec)
  if (( t1 == 0 ) && (t2 == 0) && (tic <= 32768)) {                 // acquisition of 1st edge of anemometer period
    t1 = tic;  // record 1st edge
    Serial.println("t1!");delay(1); 
  }
  else {
    if (( t1 > 0 )  && (t2 == 0) && ((tic - t1) <= 32768)) {         // acquitision of 2nd edge of anemometer period: we have now a measured period! let's transmit value.
      t2 = tic;  // record 2nd edge, we have a wind measure !
      // process the wind measurement !
      windgust.f = 4 * 32768 / (float)(t2 - t1);
      windpacket[0] = winddir.b[0];
      windpacket[1] = winddir.b[1];
      windpacket[2] = windgust.b[0];
      windpacket[3] = windgust.b[1];
      windpacket[4] = windgust.b[2];
      windpacket[5] = windgust.b[3];
      windpacket[6] = 1;
      windspeed_sum += t2 - t1;  // add value to sum for average calculation
      windspeed_index++;
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
      windspeed_index++;
      t1 = 0;
      t2 = 0;
     /* while ( pcf8583.getCount() < 85000 ) {  // wait until we reach 3s
        LowPower.idle(SLEEP_250MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
      }*/
      while ( pcf8583.getCount() < 98304 ) {  // wait until we reach 3s
        LowPower.idle(SLEEP_15MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
      }
      pcf8583.setCount(0); delay(2);
       //Serial.println("###########no wind!");

    }
    RTC.read(tm);
    Serial.print(tm.Minute, DEC); Serial.print(":"); Serial.println(tm.Second, DEC);
    Serial.print("sum/index");Serial.print(windspeed_sum);Serial.print("/");Serial.println(windspeed_index);
    if (tm.Minute == 1) {
      Serial.print("10 minutes! wind =");Serial.print(   4 * 32768 / (float)(windspeed_sum/windspeed_index) );
      windspeed_sum = 0;
      windspeed_index = 0;
      // send the 10 minutes average packet
     


      RTC.write(tm0);
    }
  }
   //Serial.print("sleep!");Serial.println(pcf8583.getCount());delay(5);
  /*
    if  (( t1 == 0 ) && (t2 == 0)) {
    //  pcf8583.setCount(0); delay(2);
    }*/
  attachInterrupt(digitalPinToInterrupt(anemoPin) , risingedge, RISING);delay(1);
}




/*


  //detachInterrupt(digitalPinToInterrupt(anemoPin));
  noInterrupts();
  delay(5);
  Serial.print("wake up!"); //Serial.println(millis());


  if ( (t1 > 0) && (t2 > 0) && (t2 > t1) ) {
  windspeed.f = 4000 / (float)(t2 - t1);
  Serial.print("t1:"); Serial.print(t1); Serial.print(" t2:"); Serial.print(t2); Serial.print(" t2-t1:"); Serial.print(t2 - t1); Serial.print(" speed:"); Serial.println(windspeed.f); delay(10);
  t1 = 0;
  t2 = 0;


  // .... emission
  }
  else {
  if ( (t1 == 0) && (t2 == 0) ) {
    // no wind detected
    Serial.println("no wind - deep sleep8s"); delay(10);
    //     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    //detachInterrupt(digitalPinToInterrupt(anemoPin));


    // .....  emission wind = 0 ? (check alive sensor) every 10-30 sec?
    delay(8000);
  }

  }
  Serial.println("Powerdown 2s");
  delay(10);
  //LowPower.idle(SLEEP_4S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_ON, USART0_ON, TWI_ON);
  delay(10);
  Serial.println("restart");
  attachInterrupt(digitalPinToInterrupt(anemoPin) , risingedge, RISING);

  /*
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
  // Serial.println("enter sleep");
  // delay(100);
  LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);

  // delay(20);
  Serial.print("windspeed_32ksum=");Serial.print(windspeed_32ksum);Serial.print("  anemo_turns=");Serial.println(anemo_turns);


  // LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  setSyncProvider(RTC.get);
  //Serial.print("s="); Serial.print(second()); Serial.print(" IT:"); Serial.println(IT);

  if (IT == true) {
    if (first_edge_occured == false) {
      pcf8583.setCount(0);
      first_edge_occured = true;
    }
    else {
      windspeed_32ksum = pcf8583.getCount();
      anemo_turns ++ ;
    }

    IT = false;
    attachInterrupt(digitalPinToInterrupt(anemoPin) , anemometerIT, RISING);
  }
  else   {  //  IT == false)
    detachInterrupt(digitalPinToInterrupt(anemoPin));
    windspeed_32ksum = 0;
    anemo_turns = 0;
    Serial.println("8 sec wakeup!");delay(10);

  }



  if ( (second() % period == 0) && (task3sec == false) ) { // every 3seconds
    detachInterrupt(digitalPinToInterrupt(anemoPin));
    Serial.print(period);Serial.println("sec task");
    if (windspeed_32ksum > 0) {
      windspeed.f =  (4 * anemo_turns * 32768) / windspeed_32ksum ; // if we have seen 32768 counts (1sec) in RTC, in 10 ITs, anemo turns at 10Hz :  4*10*32768 / 32768 = 40km/h
    }
    else {
      windspeed.f = 0;
    }
    Serial.print("speed:");Serial.println(windspeed.f);delay(10);
    windspeed_32ksum = 0;
    anemo_turns = 0;
    first_edge_occured = false;
    task3sec = true;

  }
  if ( (second() % period) > 0) {
    task3sec = false;
  }

  // Serial.print("windspeed:"); Serial.println(windspeed.f);

  //attachInterrupt(digitalPinToInterrupt(anemoPin) , anemometerIT, RISING);


  /*

    if (windspeed_32ksum > 0) {
               windspeed.f =  (4 * anemo_turns * 32768) / windspeed_32ksum ; // if we have seen 32768 counts (1sec) in RTC, in 10 ITs, anemo turns at 10Hz :  4*10*32768 / 32768 = 40km/h
             }
            Serial.print("B");
            send_packet();
            state_machine_second++;
            windspeed_32ksum = 0;
            anemo_turns = 0;
            Serial.print("C");
            first_edge_occured = false;
            attachInterrupt(digitalPinToInterrupt(anemoPin) , anemometerIT, RISING);




        switch (state_machine_second) {
          case 0: {       // 1sec of 3sec cycle: emission of measured data. clear measurements etc...
            detachInterrupt(digitalPinToInterrupt(anemoPin));  // to disable any further measurement of anemometer while processing and sending data.
            Serial.print("A");

          } break;
          case 1: { // 2sec of 3sec cycle.
            state_machine_second++;
          } break;
          case 2: { // 3sec of 3sec cycle,
            state_machine_second = 0;
          } break;
        }  // end switch case state_machine_second
    IT = 0;
    attachInterrupt(digitalPinToInterrupt(wakeUpPin), wakeUp, RISING);
    } break;
    } // end switch case IT

    Serial.print("speed:"); Serial.println(windspeed.f);
    Serial.print("state_machine_second"); Serial.println(state_machine_second);
    delay(250);

    // LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
    //  LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_ON, TWI_ON);
    //detachInterrupt(digitalPinToInterrupt(wakeUpPin));
    //delay(1000);*/




void send_packet(uint8_t packet[7]) {

  radio.powerUp();
  delay(10);
  /* windpacket[0] = winddir.b[0];
    windpacket[1] = winddir.b[1];
    windpacket[2] = windspeed.b[0];
    windpacket[3] = windspeed.b[1];
    windpacket[4] = windspeed.b[2];
    windpacket[5] = windspeed.b[3];*/
  //Serial.print("J'envoie maintenant ");
  //Serial.print(winddir.i); Serial.print("-"); Serial.print(windgust.f); Serial.print("!");
  //Serial.println(radio.write(packet, 7));
  radio.write(packet, 7);
  delay(10);
  radio.powerDown();

}


