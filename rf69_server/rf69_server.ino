// rf69_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing server
// with the RH_RF69 class. RH_RF69 class does not provide for addressing or
// reliability, so you should only use RH_RF69  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf69_client
// Demonstrates the use of AES encryption, setting the frequency and modem
// configuration.
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board

#include <SPI.h>
#include <Wire.h>
#include <Time.h>
#include <MCP7940RTC.h>
#include <RH_RF69.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define LEDPIN 9 

char buf[32];
char *p;
MCP7940RTC *prtc;    // setup rtc pointer
time_t delta;

// Singleton instance of the radio driver
RH_RF69 rf69;
//RH_RF69 rf69(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1


// Converts serial message to time for RTC
#define TIME_SYNC_LEN 11
time_t processSyncMessage() 
{
	static char c;
	// return the time if message is valid
	while(Serial.available() >= TIME_SYNC_LEN) {
		c = Serial.read();
		if( c == 'T' ) {
			time_t recvtime =0;
			for(int i=0; i < TIME_SYNC_LEN -1; i++) {
				c = Serial.read();
				if( c >= '0' && c <= '9' ){
					recvtime = (10 * recvtime) + (c - '0'); // to number
				}
			}
			return recvtime;
		}
	}
	return 0;
}

void digitalClockDisplay(){
  p = &buf[0];
  prtc->getDateStr(p);
  Serial.print("RTC:  ");
  Serial.println(p);

  time_t t = now(); // store the current time in time variable t 
  Serial.print("Time: ");
  Serial.print(year(t));          // the year for the given time t  
  Serial.print("-");
  Serial.print(month(t));         // the month for the given time t 
  Serial.print("-");
  Serial.print(day(t));           // the day for the given time t 
  Serial.print(" ");
  Serial.print(hour(t));          // returns the hour for the given time t
  Serial.print(":");
  Serial.print(minute(t));        // returns the minute for the given time t
  Serial.print(":");
  Serial.println(second(t));        // returns the minute for the given time t

}

void setup() 
{
  Serial.begin(115200);
  Serial.println("Iniciando os trabalhos");
  if (!rf69.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  // No encryption
  if (!rf69.setFrequency(433.0))
    Serial.println("setFrequency failed");

  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  rf69.setTxPower(14);

 // The encryption key has to be the same as the one in the client
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);
  
#if 0
  // For compat with RFM69 Struct_send
  rf69.setModemConfig(RH_RF69::GFSK_Rb250Fd250);
  rf69.setPreambleLength(3);
  uint8_t syncwords[] = { 0x2d, 0x64 };
  rf69.setSyncWords(syncwords, sizeof(syncwords));
  rf69.setEncryptionKey((uint8_t*)"thisIsEncryptKey");
#endif
  
  memset(buf,0,sizeof(buf));
  prtc = new MCP7940RTC();
/*  static time_t t = prtc->get();
  static tmElements_t tm1;
  breakTime(t,tm1);
  Serial.print("t=");
  Serial.println(t);
  Serial.print(tm1.Year);
  Serial.print(" ");
  Serial.print(tm1.Month);
  Serial.print(" ");
  Serial.print(tm1.Day);
  Serial.print(" ");
  Serial.print(tm1.Wday);
  Serial.print(" ");
  Serial.print(tm1.Hour);
  Serial.print(":");
  Serial.print(tm1.Minute);
  Serial.print(":");
  Serial.println(tm1.Second);
*/

  p = &buf[0];
  prtc->getDateStr(p);
  Serial.println(p);
  setTime(prtc->getTimeRTC());

  delta = now();
}

void loop()
{
  if (rf69.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len))
    {
//      RH_RF69::printBuffer("request: ", buf, len);
      Serial.print("got request: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf69.lastRssi(), DEC);
      
      // Send a reply
      uint8_t data[] = "And hello back to you";
      rf69.send(data, sizeof(data));
      rf69.waitPacketSent();
      Serial.println("Sent a reply");
    }
    else
    {
      Serial.println("recv failed");
    }
  }
  if(Serial.available())
  {
	  time_t t = processSyncMessage();
	  if (t > 0) {
		  prtc->setTimeRTC(t);
		  setTime(t);
	  }
  }
 
//  Serial.println(now());
  int diff = now() - delta;
//  Serial.println(diff); 
//  Serial.println(delta);
  if (diff > 5) {
     digitalClockDisplay();
     delta = now();
  }

}

