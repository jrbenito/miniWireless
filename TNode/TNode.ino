// NOTE: code sample was inspired from ideas gained from: https://github.com/aanon4/RFM69
//
// TNode - periodically send time(millis), radio temperature, VCC voltage to gateway...
// 2014 - anarduino.com
//
#include <RFM69.h>
#include <SPI.h>

#define GATEWAY_ID    1
#define NODE_ID       10    // node ID
#define NETWORKID     100    //the same on all nodes that talk to each other
#define MSG_INTERVAL  2000

// Uncomment only one of the following three to match radio frequency
#define FREQUENCY     RF69_433MHZ    
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ

//#define IS_RFM69HW   //NOTE: uncomment this ONLY for RFM69HW or RFM69HCW
#define ENCRYPT_KEY    "my-EncryptionKey"  // use same 16byte encryption key for all devices on net
#define ACK_TIME       50                  // max msec for ACK wait
#define LED            9                   // Anardino miniWireless has LEDs on D9
#define SERIAL_BAUD    115200
#define TNODE_VERSION  "1.0"

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
  Serial.print("TNode ");
  Serial.print(TNODE_VERSION);
  Serial.print(" startup at ");
  Serial.print((FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915));
  Serial.println("Mhz...");
  delay(20);
  radio.initialize(FREQUENCY, NODE_ID, NETWORKID);
  radio.encrypt(0);
  radio.promiscuous(promiscuousMode);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment #define ONLY if radio is of type: RFM69HW or RFM69HCW 
#endif
  msgBuf[0] = (byte)NODE_ID;  // load NODEID
}

void loop() {
  byte tempC =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
  int vcc = readVcc();
  int n=1; 
  lt.l = millis();  // load time
  msgBuf[1] = lt.b[3];
  msgBuf[2] = lt.b[2];
  msgBuf[3] = lt.b[1];
  msgBuf[4] = lt.b[0];
  msgBuf[5] = tempC;  // load temp
  it.i = vcc;           // load vcc
  msgBuf[6] = it.b[1];
  msgBuf[7] = it.b[0];
    
  radio.send((byte)GATEWAY_ID, (const void *)&msgBuf[0], (byte)sizeof(msgBuf), requestACK);
  delay(MSG_INTERVAL);
}

int readVcc() {   // return vcc voltage in millivolts
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);             // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);  // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate in mV
  int t = (int)result;
  return t;
}

