// Anarduino.com simple testcode to show Memory and RTC is functional
//
#include <SPI.h>
#include <Wire.h>
#include <Time.h>
#include <MCP7940RTC.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define MCP7940_CTRL_ID 0x6F 

//#define CSMEM_PIN 6 // usb_stick_V3
#define CSMEM_PIN 5//9 //gps tracker//15  //network bridge
//#define VMEM_ENABLE 5
//#define VMEM_ENABLE 16 // vdmescc enable for flash memory, usb_stick_v3
//#define VMEM_ENABLE 5 // vmem vcc enable for flash memory, miniCtl

#define LEDPIN 9

//opcodes
#define WREN  6  // write enable
#define WRDI  4  // write disable
#define RDSR  5  // read status register
#define READ  3  // read memory
#define RSTEN 0x66  // reset enable
#define RST 0x99    // reset
#define RDID 0x90
#define JEDECID 0x9f
#define EWSR 0x50  // enable write status register
#define WRSR  1
#define WRITE 2

#define CMDBUFSIZE 40

long sleepIntervalSec=5;//quick, 5-second sleep
long loopCnt=0;
int wuPin = 3;//2;
int gStat=0;
uint8_t d[8];
uint8_t dAlarm[8];
int d1[8];
int iContext=0;

union ltag {
    byte b[4];
    unsigned long l;
}lt;

uint8_t memIdAvg=0;

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

void fmGetSID() {
 uint8_t a,b,c,d;
 digitalWrite(CSMEM_PIN,LOW);
 SPI.transfer(0x88);
 SPI.transfer(0x00);
 SPI.transfer(0x00);
 Serial.print("SID=");
 a = SPI.transfer(0x00);
 b = SPI.transfer(0x00);
 c = SPI.transfer(0x00);
 d = SPI.transfer(0x00);
 Serial.print(a,HEX);
 Serial.print(" ");
 Serial.print(b,HEX);
 Serial.print(" ");
 Serial.print(c,HEX);
 Serial.print(" ");
 Serial.print(d,HEX);
 Serial.print(" ");
 Serial.println();
 digitalWrite(CSMEM_PIN,HIGH);
}

void spPrintRDID() {
  long lResp=0;
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(JEDECID);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  for(int i=0; i<320; i++) {
     byte b = SPI.transfer(0x00);
     lResp += b;
     if(i>0 && i%32 ==0) Serial.println();
     if(b<0x10) Serial.print("0");
     Serial.print(b,HEX);
     Serial.print(" "); 
  }
  digitalWrite(CSMEM_PIN,HIGH);
  memIdAvg = lResp/320;  // simple average of the 1st 320 bytes.
}

uint8_t fmGetMfgId() {
  uint8_t mfgId=0;
  // Read mfgID
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(RDID);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  mfgId = (uint8_t)SPI.transfer(0x00);
  digitalWrite(CSMEM_PIN,HIGH);
  return mfgId;  
}
uint8_t fmGetDeviceId() {
  uint8_t deviceId=0;
  // Read mfgID
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(RDID);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x01);
  deviceId = (uint8_t)SPI.transfer(0x00);
  digitalWrite(CSMEM_PIN,HIGH);
  return deviceId;  
}
long fmGetJedecId() {
  uint8_t a=0,b=0,c=0;
  // Read JEDECID
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(JEDECID);
  a = SPI.transfer(0x00);
  b = SPI.transfer(0x00);
  c = SPI.transfer(0x00);
  digitalWrite(CSMEM_PIN,HIGH);  
  ltag lt;
  lt.l = 0;
  lt.b[2] = a;
  lt.b[1] = b;
  lt.b[0] = c;
  return lt.l;  
}
void fmWriteEnable() {
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(WREN);  
  digitalWrite(CSMEM_PIN,HIGH);
}
uint8_t fmGetStatus() {
  uint8_t status=0;
  // Read Status Register
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(RDSR);
  status = SPI.transfer(0x00);
  digitalWrite(CSMEM_PIN,HIGH);
  return status;  
}

void printHexByte(uint8_t b) {
  if(b<0x10)
     Serial.print("0"); 
  Serial.print(b,HEX);
}

uint8_t fmReadByte(long addr) {
  uint8_t b=0;
  lt.l = (uint32_t)addr;
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(READ);
  SPI.transfer(lt.b[2]);
  SPI.transfer(lt.b[1]);
  SPI.transfer(lt.b[0]);
  b = SPI.transfer(0x00);
  //Serial.print("rd-b=");
  //Serial.println(b,HEX);
  digitalWrite(CSMEM_PIN,HIGH);
  return b;  
}
void fmWriteByte(uint8_t b, long addr) {
  fmWriteEnable();
  lt.l = (uint32_t)addr;
  digitalWrite(CSMEM_PIN,LOW);
  SPI.transfer(0x12);//WRITE);
  SPI.transfer(0);
  SPI.transfer(lt.b[2]);
  SPI.transfer(lt.b[1]);
  SPI.transfer(lt.b[0]);
  SPI.transfer(b);
  //Serial.print("wr-b=");
  //Serial.println(b,HEX);
  digitalWrite(CSMEM_PIN,HIGH);  
}

//print len-bytes of RTCmemory beginning at relative address, addr
// TODO: research RD/WR timing and handling in RTC specification and do more rigorous testing
void printRTCmem(int addr, int len) {
  uint8_t a = addr + 020;  // 
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write(a);
  delay(1);
  Wire.endTransmission();
  delay(1);
  Wire.requestFrom(MCP7940_CTRL_ID, len);
  for(int i=0; i<len; i++) {
    while(Wire.available()) {
      byte b = Wire.read();
      if(b<0x10) Serial.print("0");
      Serial.print(b,HEX);
      Serial.print(" ");
    }
  }
  Wire.endTransmission();
  Serial.println();
}

// TODO: check RD/WR timing and handling in RTC specification and do more rigorous testing
void writeRTCmem(int addr, int len, int pattern) {
  uint8_t a = addr + 020;  // 
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write(a);
  byte b=0;
  if(pattern== -1) {
    for(int i=0; i<len; i++) {
      b = i;
      Wire.write(b);
    }
  }else {
    b = pattern;
    for(int i=0; i<len; i++) {
      Wire.write(b);
    }  
  }
  Wire.endTransmission();
}

void compareAlarm0() {
  uint8_t dmask[] = { 0x7f, 0x7f, 0x7f, 0x07, 0x3f, 0x1f };

  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  Wire.requestFrom(MCP7940_CTRL_ID, 7); 
  memset(d,0,8);
  int i=0;
  while(Wire.available()) {
    if(i==7) break;
    d[i] = Wire.read() & dmask[i];
    i++;
    delay(1);
  }
  Wire.endTransmission();
  
  Serial.print("0-6: ");
  for(int i=0; i<7; i++) {
    if(d[i]<0x10) Serial.print("0");
    Serial.print((uint8_t)d[i],HEX);
    Serial.print(" ");
  }
  Serial.println(" (rtc time)");  
  
  memset(d,0,8);
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0a);
  Wire.endTransmission();
  Wire.requestFrom(MCP7940_CTRL_ID, 6); 
  memset(d,0,8);
  i=0;
  while(Wire.available()) {
    if(i==6) break;
    d[i] = Wire.read() & dmask[i];
    i++;
    delay(1);
  }
  Wire.endTransmission();
  
  Serial.print("0-5: ");
  for(int i=0; i<6; i++) {
    if(d[i]<0x10) Serial.print("0");
    Serial.print((uint8_t)d[i],HEX);
    Serial.print(" ");
  }
  Serial.println(" (alarm0 time)");
  delay(10);
}

void setAlarm(time_t t) {
  tmElements_t tm;
  breakTime(t, tm);
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0a);
  Wire.write((uint8_t)dec2bcd(tm.Second) & 0x7f);       // Seconds
  Wire.write((uint8_t)dec2bcd(tm.Minute) & 0x7f);       // Minutes
  Wire.write((uint8_t)dec2bcd(tm.Hour) & 0x3f);         // Hour
  Wire.write((uint8_t)(dec2bcd(tm.Wday) | 0x70) & 0xf7);// wDay, trigger on minutes matching
  Wire.write((uint8_t)dec2bcd(tm.Day) & 0x3f);          // Day
  Wire.write((uint8_t)dec2bcd(tm.Month) & 0x1f);        // Hour
  Wire.endTransmission();  
  delay(2);
  // turn on alarm  
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x07);
  Wire.write((uint8_t)0x17);
  Wire.endTransmission();
}
  
void clearAlarm() {
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x07);
  Wire.write((uint8_t)0x80);
  Wire.endTransmission();
  delay(20);

// reset the interrupt flag
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0d);
  Wire.endTransmission();
  delay(20);
  Wire.requestFrom(MCP7940_CTRL_ID, 1);
  byte b1 = Wire.read();
  Wire.endTransmission();
  delay(20);
  b1 &= 0xf7;
  Wire.beginTransmission(MCP7940_CTRL_ID);
  Wire.write((uint8_t)0x0d);
  Wire.write((uint8_t)b1 & 0xf7);
  Wire.endTransmission();
  delay(20);
}
 
void setNewAlarm() {
  static MCP7940RTC rtc;
  time_t t = rtc.get();
  //Serial.print("dayofweek=");
  //Serial.println(dayOfWeek(t));
  // must get proper wday... TODO: Fix in library
  
  //Serial.print(" ");
  Serial.print("t=");
  Serial.print(t);
  Serial.print(" ");
  tmElements_t tm1;
  breakTime(t,tm1);
  uint8_t b=0;
  // check tm1.Wday for differences...
  // Must ensure Wday consistency in RTC chip!!!
    Wire.beginTransmission(MCP7940_CTRL_ID);
    Wire.write((uint8_t)0x03);
    Wire.endTransmission();    
    Wire.requestFrom(MCP7940_CTRL_ID, 1);     
    while(Wire.available()) {
      b = Wire.read();
    }
    Wire.endTransmission();
    delay(5);
    b &= 0xf8;
    b |= dec2bcd(dayOfWeek(t));
    Wire.beginTransmission(MCP7940_CTRL_ID);
    Wire.write((uint8_t)0x03);
    Wire.write(b);
    Wire.endTransmission();
  //end of RTC setting of Wday

  Serial.print("tm1=");
  Serial.print(tm1.Year);//+1970);
  Serial.print(" ");
  Serial.print(tm1.Month);
  Serial.print(" ");
  Serial.print(tm1.Wday);
  Serial.print(" ");
  Serial.print(tm1.Day);
  Serial.print(" ");
  Serial.print(tm1.Hour);
  Serial.print(" ");
  Serial.print(tm1.Minute);
  Serial.print(" ");
  Serial.print(tm1.Second);
  t += sleepIntervalSec;
  Serial.print("  tA=");
  Serial.print(t);  
  Serial.println();
  
  //rtc.setAlarm0(t);
  setAlarm(t);
  delay(10);
  compareAlarm0();
  loopCnt++;
}
 
void sleepHandler() {
  Serial.println("Processing...");  
  gStat=1;
}    

void setup() {
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
 
  delay(1000);
  Serial.begin(115200);
  Serial.println("Startup...");
    
  gStat=0;
  char ch='0';
  int i=0;
  long l=0; 
  char buf[50];
  pinMode(wuPin,INPUT);
  Wire.begin();
  boolean bdone=false;  
  pinMode(CSMEM_PIN, OUTPUT);
  digitalWrite(CSMEM_PIN, HIGH);
  SPI.setDataMode(SPI_MODE0);//3);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.begin();  
 
  int dly=100;
  for(int i=0; i<12; i++) {
    digitalWrite(LEDPIN, HIGH);
    delay(dly);
    digitalWrite(LEDPIN, LOW);
    delay(dly);
  }
}

void loop() {
  Serial.println("128Mb SPI FLASH MEMORY RFID Block READ test.....");
  spPrintRDID();
      delay(10);
      fmWriteEnable();    
      digitalWrite(CSMEM_PIN,LOW);
      SPI.transfer(0x20);                // sector Erase
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(0);
      digitalWrite(CSMEM_PIN,HIGH);
      delay(1000);
      Serial.println();
      Serial.println();
      Serial.println("SPI Flash Read 32bytes, starting at Address 0x20 (after sector erase, should be all FF's)");
      digitalWrite(CSMEM_PIN,LOW);
      SPI.transfer(0x13);//READ);
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(0x20);//20);
      for(int i=0x00; i<32; i++) {
        byte b = SPI.transfer(0);
        if(b<0x10) Serial.print("0");
        Serial.print(b,HEX);
        Serial.print(" ");
      }
      digitalWrite(CSMEM_PIN,HIGH);
      delay(10);
      Serial.println();
      Serial.println();
      Serial.println("SPI Flash pattern Write 32B at Address 0x20");
      fmWriteEnable();
      digitalWrite(CSMEM_PIN,LOW);
      SPI.transfer(0x12);//write;
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(0x20);//20);
      for(int i=0x00; i<32; i++) {
        byte b = (byte)i;
        SPI.transfer(b);
      }
      digitalWrite(CSMEM_PIN,HIGH);
      delay(10);
      Serial.println("SPI Flash Read back 32bytes at Address 0x20 (should be 00 to 1F)");
      digitalWrite(CSMEM_PIN,LOW);
      SPI.transfer(0x13);//READ);
      SPI.transfer(0);
      SPI.transfer(0);
      SPI.transfer(00);
      SPI.transfer(0x20);//20);
      for(int i=0x00; i<32; i++) {
        byte b = SPI.transfer(0);
        if(b<0x10) Serial.print("0");
        Serial.print(b,HEX);
        Serial.print(" ");
      }
      delay(10);

      Serial.println();

  Serial.println();
  Serial.print("memIdAvg: ");
  Serial.println(memIdAvg,HEX);
  
  Serial.print("mfgId=");
  Serial.println((uint8_t)fmGetMfgId(), HEX);    // Display Manufacturer Id  
  
  Serial.print("deviceId=");
  Serial.println((uint8_t)fmGetDeviceId(), HEX); // Display Device Id
  
  Serial.print("jedecId=");
  Serial.println((long)fmGetJedecId(), HEX);  
  Serial.println();

  //TODO: Fix read/write issues with RTC ram---check timing in specification and such
  //      remember 32bit is Wire buffer limit, so if you put len>32, the library will truncate
  //NOTE: This serves as simple demonstration of RD/WR of RTC ram
  Serial.println("Clear first 16bytes of RTC static ram, read back, write pattern/read back");
  writeRTCmem(0,16,0);
  printRTCmem(0,16);  
  writeRTCmem(0,16,-1);
  printRTCmem(0,16);  
  
  Serial.println();
  Serial.println("setting new sleep time: 5-sec.");
  clearAlarm();
  delay(20);
  setNewAlarm();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  
  sleep_enable();
  interrupts();
  attachInterrupt(1,sleepHandler, FALLING);
  Serial.println("going to sleep...");
  delay(20);
  sleep_mode();  //sleep now
  //--------------- ZZZZZZ sleeping here
  sleep_disable(); //fully awake now
  Serial.println("waking up...");
  detachInterrupt(1);//);

  int dly=0;
  if(memIdAvg>0x20 && memIdAvg<0xe0) {
    Serial.println("GOT HERE!!!  RTC alarm0 interrupt is working...............");
    dly=0x500;
  }else
    dly=0x50;
  while(1) {
    digitalWrite(LEDPIN, HIGH);
    delay(dly);
    digitalWrite(LEDPIN, LOW);
    delay(dly);
  }
}
