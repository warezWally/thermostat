#include <dummy.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


//INIT

bool CONTROLLER;
bool MASTER_ALIVE = false;
int masterCheck = 0;
bool masterSaysRelay = false;

//MEASUREMENT
int readingPin = A0;
float reading = 0;
float temp_float = 0;
byte temp_int = 0;
byte secondDelay = 60;
unsigned long timeOut = 0;
unsigned long count = 0;
int targetTemp = 21;
float meanTemp = 0;

byte howManyValid = 0;
const unsigned int deviceMax = 8;
float readings[9]; //+1 for null pointer -- required.
byte addresses[9];
float bias[9];
byte alive[9];
byte lives = 3; //HOW MANY MINUTES BEFORE MEASUREMENT IS DELETED;

//RELAY
int relayPin = 2;
bool relayOn = false;
unsigned long relayTimeout = 0;

//SCREEN

U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, 14, 13, 15, 12 , 16);
//U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 15, /* dc=*/ 12, /* reset=*/ 16);

char tempStr[15];

//WiFi/Connectivity

WiFiUDP Udp;
IPAddress broadcast(192, 168, 0, 255);
const int UDP_PORT = 3141;

unsigned int deviceID;

char incomingPacket[255];  // buffer for incoming packets

//ROTARY ENCODER

bool dtLow = false;
bool clLow = false;
bool roSw = false;

int ro_dir = 0;
unsigned long roSwTimeout = 0;

//TIMERS

unsigned long timeNow;
unsigned long timePrev;
unsigned long *timers[] = {&roSwTimeout, &relayTimeout, &timeOut};




/*

	Serial.println(timeOut); //PRINT CURRENT timeOut VALUE;
	Serial.println(*timers[2]); //PRINT CURRENT timeOut VALUE USING A POINTER;
	timeOut--; //MANIPULATE CODE BLOCK USING VAR;
	Serial.println(timeOut); //UPDATED VALUE;
	Serial.println(*timers[2]); //SHOW UPDATED VALUE USING ARRAY
	timers[2] = *timers[2] + 1; //MANIPULATE CODE BLOCK USING POINTER
	Serial.println(timeOut); //UPDATED VAR




*/



//END INIT

//ENCODER FUNCTIONS
void ro_dt () { //DT GOES LOW

  if (!dtLow) {

    if (clLow) { //IF CLOCK WAS ALREADY LOW

      ro_dir = -1; //CCW

    }

    dtLow = true;
  }
}

void ro_cl () { //CLK GOES LOW

  if (!clLow) {

    if (dtLow) { //IF DT WAS ALREADY LOW

      ro_dir = 1; //CW

    }

    clLow = true;
  }
}

void ro_sw () {

  roSw = false;

  if (timeNow > roSwTimeout) {

    roSw = true;
    roSwTimeout = timeNow + 500;

  }

}

void setup()
{

  Serial.begin(115200);
  pinMode(5, INPUT); //Clock
  pinMode(4, INPUT); //Mr DT AFC
  pinMode(0, INPUT); //Switch

  attachInterrupt(digitalPinToInterrupt(5), ro_cl, FALLING);
  attachInterrupt(digitalPinToInterrupt(4), ro_dt, FALLING);
  attachInterrupt(digitalPinToInterrupt(0), ro_sw, FALLING);

  pinMode(relayPin, INPUT);

  delay(500);

  if (digitalRead(relayPin)) {

    //THIS IS A CONTROLLER
    CONTROLLER = true;

    //RELAY INIT
    Serial.println("This is a controller");
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, 1);


  }

  //CONNECT WiFi

  WiFi.begin("Hello Neighbour!", "whiffy999!");

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  deviceID = WiFi.localIP()[3];

  if (Udp.begin(UDP_PORT)) {
    Serial.println("UDP Port established");
  };

  //SCREEN INIT
  u8g2.begin();
  u8g2.enableUTF8Print();    // enable UTF8 support for the Arduino print() function

  delay(500);
}

void loop() {

  timeNow = millis();

  if (timeNow < timePrev) { //ROLLOVER HAS HAPPENED

    for (int i = 0; i < sizeof(timers) / 2; i++) {

      *timers[i] = *timers[i] > timePrev ? (*timers[i] - timePrev) : 0;

    }

sendPacket("L_ROLLOVER OCCURED");

  }
  timePrev = timeNow;


  if (CONTROLLER) {
    if (roSw) { //  && timeNow > roSwTimeout) {
      Serial.print("BUTTON");
      if (relayOn) {

        Serial.println("Relay is on, going off");

        relaySw(false);
      } else if (timeNow >= relayTimeout) {

        //relayTimeout = timeNow + (1000 * 60 );//* 60 * 3);

        Serial.print("Relay Off In: ");
        Serial.println(relayTimeout);
        relaySw(true);


      }
	roSw = false;
    } else if (!MASTER_ALIVE) {

      //   Serial.println("No Master");

      if (timeNow > relayTimeout) {

        relaySw((meanTemp < targetTemp));
      }

    } else {

      //  Serial.print("Master In Control: ");
      //  Serial.println(masterSaysRelay);

      relaySw(masterSaysRelay);

    }
  }
  if (ro_dir) {
    targetTemp += ro_dir;
    writeTempsToScreen(meanTemp);
  }

  dtLow = !digitalRead(4);
  clLow = !digitalRead(5);

  //roSw = false;

  ro_dir = 0;

  // roSw = timeNow > roSwTimeout ? false : roSw;

int x_one = 2;
int x_two = 1000;
int s_three = 500;
int d_four = 10;

  reading = float(analogRead(readingPin)) / 1023; //Convert to Voltage Float 0.00 - 1.00
  reading = ((reading * x_one * x_two) - s_three) / d_four; //Curve on specs of TGZ
  temp_float += reading;
  count++;



  if (timeNow >= timeOut) {

    masterCheck--;
    if (masterCheck <= 0) {

      MASTER_ALIVE = false;
      masterCheck = 0;
    }

    timeOut = timeNow + (secondDelay * 1000);
    //Udp.stop();
    temp_float /= count;
    // temp_int = int(temp_float);

    addMeasurement(temp_float, deviceID);

	String str = "M";
	str.concat(tempStr);
	sendPacket(str);

    // writeTempsToScreen() ;
    if (Udp.beginPacketMulticast(broadcast, UDP_PORT, WiFi.localIP())) {

      Serial.println("Writing Packet");

      dtostrf(temp_float, 5, 2, tempStr);

      Udp.write("M");

      Udp.write(tempStr, sizeof(tempStr));

      Udp.endPacket();

      delay(100);

    }



    meanTemp = 0;
    howManyValid = 0;
    //CALCULATE MEAN TEMP

    float totalBias = 0;

    for (byte i = 0; i < deviceMax; i++) {

      if (alive[i] <= 0) {

        readings[i] = 0;
        addresses[i] = 0;
        bias[i] = 0;
        alive[i] = 0;

      } else {

        alive[i]--;

      }

      totalBias += (bias[i] * !!(readings[i]));

    }

  bool biasMade = false;

    if (!totalBias) { //CREATE A DEFAULT BIAS
      for ( byte i = 0; i < deviceMax; i++) {

        totalBias += bias[i] = readings[i] ? 12.5 : 0;



      }

      biasMade = true;
    }
    for (byte i = 0; i < deviceMax ; i++) {

      meanTemp += ((bias[i] / totalBias) * readings[i]);


      howManyValid += (!!readings[i] * !!bias[i]);

      bias[i] = biasMade ? 0 : bias[i];
    }

    Serial.print("Mean Temp: ");
    Serial.println(meanTemp);

    Serial.print("Valid: ");
    Serial.println(howManyValid);

    count = 0;
    temp_float = 0;

  }

  meanTemp = meanTemp / howManyValid;

  writeTempsToScreen(int(meanTemp));

  int packetSize = Udp.parsePacket();

  if (packetSize)
  {

    int len = Udp.read(incomingPacket, 255);
    Serial.print("LEN: ");
    Serial.println(len);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }

    switch (incomingPacket[0])   {
      case 'C' :

        Serial.println("Command Recieved");

        switch (incomingPacket[1]) {

          case 'A' :


            MASTER_ALIVE = incomingPacket[2] - 48;

            Serial.print("Master ");
            Serial.print(incomingPacket[1]);
            Serial.print(": ");
            Serial.println(MASTER_ALIVE);

            masterCheck = 3;

            break;

          case 'R' :

            masterSaysRelay = incomingPacket[2] - 48;

            Serial.print("Master Says Relay: ");
            Serial.println(masterSaysRelay);
            break;

          case 'B' :

            char biasBuild[8];
            byte buildIndex = 0;
            byte gotAddr = 0;
            bool biasBuilt = false;

            Serial.println(incomingPacket);

            for (byte f = 2; f < len; f++) {

              if (!gotAddr) {
                gotAddr = asciiToHex(incomingPacket[f], incomingPacket[(f + 1)]);
                f++;
                f++;
              }
              if (incomingPacket[f] == '-' || incomingPacket[f] == '.' || (incomingPacket[f] - 48 < 10)) {

                biasBuild[buildIndex++] = incomingPacket[f];

              }

              biasBuilt = (incomingPacket[f] == '_') ? true : false;

              if (biasBuilt) {

                Serial.print("BIAS: ");
                Serial.println(atof(biasBuild));
                Serial.print("ADDR: ");
                Serial.println(gotAddr);

                for (byte i = 0; i < deviceMax; i++) {

                  if (addresses[i] == gotAddr) {
                    bias[i] = atof(biasBuild);
                    break;
                  }

                }
                buildIndex = 0;
                gotAddr = 0;
                biasBuilt = false;
              }

            }


            break;
        }

        break;

      //case 'M':
      default:

        incomingPacket[0] = 0x20;
        addMeasurement(atof(incomingPacket), Udp.remoteIP()[3]);

        break;
    }
    //yield();
    //writeTempsToScreen(String(incomingPacket[0]).toInt());
  }

  // yield();
  delay(50);


}
void sendPacket(String str){

if (Udp.beginPacketMulticast(broadcast, UDP_PORT, WiFi.localIP())) {

      Serial.println("Writing Packet");

      dtostrf(temp_float, 5, 2, tempStr);

      Udp.write(tr, sizeof(tempStr));

      Udp.endPacket();

      delay(100);

    }


}
void writeTempsToScreen(int temp) {

  u8g2.setFont(u8g2_font_logisoso54_tf);

  if (temp < -9) temp = -9;
  if (temp > 99) temp = 99;

  //TURN NUMBER IN TO CHAR
  char tempChar[4];
  String tempStr = String(temp);
  tempStr.toCharArray(tempChar, 4);

  byte width = u8g2.getStrWidth(tempChar);

  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  u8g2.setCursor(64 - width, 64);
  u8g2.print(temp);

  u8g2.print("°");

  u8g2.setFont(u8g2_font_rosencrantz_nbp_tf);

  width = u8g2.getStrWidth("TARGET");
  u8g2.setCursor(128 - width, 64 - 20);
  u8g2.print("TARGET");

  tempStr = (String(targetTemp));
  tempStr.concat("°");
  tempStr.toCharArray(tempChar, 4);
  width = u8g2.getStrWidth(tempChar);

  u8g2.setCursor(128 - width - 10, 64);
  u8g2.print(tempChar);

  u8g2.sendBuffer();
  //delay(1000);

}

void writeMsgToScreen(String msg) {

  u8g2.setFont(u8g2_font_logisoso54_tf);

  Serial.println(msg);

  char msgChar[4];
  String msgStr = String(msg);
  msgStr.toCharArray(msgChar, 4);
  byte width = u8g2.getStrWidth(msgChar);

  u8g2.clearBuffer();
  u8g2.setCursor((128 / 2) - (width / 2), 64 - 5);
  u8g2.print(msg);

  u8g2.sendBuffer();
}

void relaySw(bool relay) {

  if (relay != relayOn) {

    if (relay) {

      writeMsgToScreen("ON");

    } else {

      writeMsgToScreen("OFF");
    }
    relayOn = relay;
    digitalWrite(relayPin, !relay);
    delay(1000);
  }
}

void addMeasurement(float measurement, byte address) {

  bool done = false;

  Serial.println("\n\n");
  Serial.println("--------------------------------");
  Serial.println("  ADDR  |  MEAS  | BIAS | ALIVE ");
  Serial.println("--------------------------------");

  for (byte i = 0; i < deviceMax; i++) {

    if (addresses[i] == address) {

      alive[i] = lives;
      readings[i] = measurement;
      done = true;
    }


    Serial.print("   ");
    Serial.print(addresses[i]);
    Serial.print("   ");


    Serial.print("|");

    Serial.print(" ");
    Serial.print(readings[i]);
    Serial.print(" ");


    Serial.print("|");
    Serial.print("  ");
    Serial.print(bias[i]);

    Serial.print(" ");
    Serial.print("|");
    Serial.print("  ");
    Serial.print(alive[i]);

    Serial.println("");

  }

  if (done) {
    return;
  }
  Serial.println("----------------");
  Serial.println("\n\n");
  //IF YOU'RE HERE, NO MEASUREMENT HAS BEEN ADDED TO THE ARRAY PREVIOUSLY

  for (byte i = 0; i < deviceMax; i++) {

    if (!addresses[i]) {
      Serial.println("New Device");
      //ADDRESS SLOT FREE
      addresses[i] = address;

      addMeasurement(measurement, address);
      return;
    }

  }

  Serial.println("Array's are full");
  //IF YOU GET HERE, THE ADDRESS ARRAY(S) ARE FULL

}

byte asciiToHex(byte upper, byte lower) {

  Serial.println(upper);
  Serial.println(lower);

  upper -= upper > 96 ? 32 : 0;
  upper += upper < 58 ? 7 : 0;
  upper -= 55;

  lower -= lower > 96 ? 32 : 0;
  lower += lower < 58 ? 7 : 0;
  lower -= 55;

  return ((upper << 4) | lower );
}

