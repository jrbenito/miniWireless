// TGateway - read TNodes... (see Tnode.ino example for more info)
//
// 2014 - anarduino.com
//
#include <RFM69.h>
#include <SPI.h>

#define GATEWAY_ID    1     // this is ME, TGateway
#define NETWORKID     100    //the same on all nodes that talk to each other

// Uncomment only one of the following three to match radio frequency
#define FREQUENCY     RF69_433MHZ    
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ

//#define IS_RFM69HW   //NOTE: uncomment this ONLY for RFM69HW or RFM69HCW
#define ENCRYPT_KEY    "my-EncryptionKey"  // use same 16byte encryption key for all devices on net
#define ACK_TIME       50                  // max msec for ACK wait
#define LED            9                   // Anardino miniWireless has LEDs on D9
#define SERIAL_BAUD    115200
#define TGW_VERSION  "1.0"

#define MSGBUFSIZE 16   // message buffersize, but for this demo we only use: 
                        // 1-byte NODEID + 4-bytes for time + 1-byte for temp in C + 2-bytes for vcc(mV)
byte msgBuf[MSGBUFSIZE];

RFM69 radio;                  // global radio instance
bool promiscuousMode = false; // set 'true' to sniff all packets on the same network
bool requestACK=false;

union itag {
  uint8_t b[2];
  uint16_t i;
}it;
union ltag {
  byte b[4];
  long l;
}lt; // used to force byte order in case we end up using result in various endian targets...

void setup() {
  memset(msgBuf,0,sizeof(msgBuf));
  Serial.begin(SERIAL_BAUD);
  Serial.print("TGateway ");
  Serial.print(TGW_VERSION);
  Serial.print(" startup at ");
  Serial.print((FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915));
  Serial.print("Mhz, Network ID: ");
  Serial.println(NETWORKID);
  delay(50);
  radio.initialize(FREQUENCY, GATEWAY_ID, NETWORKID);
  radio.encrypt(0);
  radio.promiscuous(promiscuousMode);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment #define ONLY if radio is of type: RFM69HW or RFM69HCW 
#endif
}

void loop() {
  if (radio.receiveDone()) {
    for(int i=0; i<radio.DATALEN; i++) {
       if(i==sizeof(msgBuf))
          break;  // better stop if we've reached our buffer limit (shouldn't get here, as DATALEN should be less than MSGBUFSIZE)
          msgBuf[i] = radio.DATA[i];
    }
    // Okay, extract and print out the data received
    Serial.print("Received from TNODE: ");
    Serial.print((byte)msgBuf[0],DEC);
    // extract TNode millis
    lt.b[3] = msgBuf[1];
    lt.b[2] = msgBuf[2];
    lt.b[1] = msgBuf[3];
    lt.b[0] = msgBuf[4];
    Serial.print(", t=");
    Serial.print(lt.l,DEC);
    Serial.print(", tempC=");
    Serial.print(msgBuf[5],DEC);
    Serial.print(", vcc=");
    it.b[1] = msgBuf[6];
    it.b[0] = msgBuf[7];
    // convert TNode VCC to volts
    float f = (float)it.i/1000.0;
    Serial.println(f,3);
  }
}
