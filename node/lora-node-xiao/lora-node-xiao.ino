#include <LoRa.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <stdlib.h>
#include <FlashStorage_SAMD.h>                                                                  


// Flashrom setup
const int WRITTEN_SIGNATURE = 0xBEEFDEED;
const int START_ADDRESS     = 0;
int eeAddress = START_ADDRESS + sizeof(WRITTEN_SIGNATURE);   //Location we want the data to be put.



//define the pins used by the transceiver module
#define ss 1
#define rst 0 
#define dio0 2

#define SerialMonitor Serial
#define FAILSAFE_TIMEOUT  30 // seconds

#define GPSDATA 0
#define MSGDATA 1

#define deviceID "T1"
#define gatewayID "G1"
struct GPSData {
  float lat;
  float lng;
};


uint32_t CurrentTime;    
uint32_t CurrentDate;

char* fields[5];  //Message recieved are broken into comma separated fields

//float bwarray[10] = {7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, 250E3, 500E3}

struct cfgObject {
  int sf; //SF
  int cr; //CR
  int gps; //GPS Transmit Interval
  float bw;//BW
};

// TinyGPS++
static const int RXPin = 7, TXPin = 6; 
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial sws(RXPin,TXPin);


cfgObject default_cfg;
cfgObject current_cfg;

float lastlat = 0.00;
float lastlng = 0.00;

long int startTime = 0;

  char* bw_char_values[10] = {"7.8E3", "10.4E3", "15.6E3", "20.8E3", "31.25E3", "41.7E3", "62.5E3", "125E3", "250E3", "500E3"};
  float bw_float_values[10] = {7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, 250E3, 500E3};


void setup() {


    // Good fallback settings
    default_cfg.bw = 125E3;
    default_cfg.sf = 12;
    default_cfg.cr = 8;
    default_cfg.gps = 11;

	
	//initialize Serial Monitor
	Serial.begin(115200);
    //while(!Serial);
    
	Serial.println("LoRa Node");

	//setup LoRa transceiver module
	LoRa.setPins(ss, rst, dio0);

	while (!LoRa.begin(915E6)) {
		Serial.println(".");
		delay(500);
	}
	Serial.println("LoRa Initialized");

// With the XIAO you need to do this after every time you
// upload code to the XIAO since it clears the EEPROM when you do
    setDefaultConfig();
    writeConfig();
    enableConfig();
    printConfig();
// -----------------------------------------------------
   
    readConfig();
    enableConfig();  
    reportConfig();

    failsafeMode();
    
	//GPS STARTUP
	sws.begin(GPSBaud);
    
    //waitForSats(4);
    getGPSDatetime();

	//Start the Clock
	startTime = millis();
    randomSeed(analogRead(A0));


}

void loop() {

	// Continuously read for Incoming Lora
	// Read and report GPS every gpstimer ms

	readLora();
	readGPS();

}

long generatePacketID() {
	long randNumber = random(1000,9999);
	return randNumber;
}




void reportConfig() {

    char tmp[32] = "";
    char flt[11];

    for (int i = 0; i < sizeof(bw_float_values); i++) {
        if(current_cfg.bw == bw_float_values[i]) {
            strcat(tmp,bw_char_values[i]);
        }
    }
    strcat(tmp, "/");
    itoa(current_cfg.sf, flt, 10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.cr, flt, 10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.gps, flt, 10);
    strcat(tmp, flt);
    writeLora(tmp);

}

void failsafeMode() {
    Serial.println("Failsafe Mode");
    writeLora("Failsafe Mode");
    //reportConfig();
    long int starttime = millis();    

    while(millis() < starttime + FAILSAFE_TIMEOUT*1000) {
        readLora();
    }
}

int payloadToFields(char msg[]) {
    char* token;
    char* rest = msg;
    int index = 0;

   
    // Store Comma Seperated values in field[]
    while ((token = strtok_r(rest, ",", &rest)))
    {
      fields[index] = token;
      index++;
    }
    return index;
}



bool myMessage(){

    if(strcmp(deviceID,fields[1]) == 0) {
        return true;
    } else {
        return false;
    }
    
}

void readLora() {
    
	char incoming[64] = "";
	int index = 0;
	byte b;
    int total_fields = 0;

	int packetSize = LoRa.parsePacket();
	if (packetSize > 0) {
		// received a packet
		// read packet and report
		for (int i = 0; i < packetSize; i++) {
			while (LoRa.available()) {
				b = LoRa.read();
				incoming[index] = b;
				index++;
			}
		}
		incoming[index] = '\0';
        Serial.print("Received: ");
        Serial.println(incoming);
        total_fields = payloadToFields(incoming); // Populate Fields Array
        if(myMessage()) {
            parseIncoming(fields[2]);
            
        }

	}
}


void readGPS() {

    while (sws.available() > 0) {
        if (gps.encode(sws.read())) {
            if (millis() > (startTime + current_cfg.gps*1000)) {
                processGPS();
                startTime = millis();
            }
        }
    }
}


void getGPSDatetime() {

    bool datetime_acquired = false;
    //char timenow[10];
    //char datenow[10];
    while (datetime_acquired == false) {
        while (sws.available() > 0) {
            if (gps.encode(sws.read())) {
                CurrentTime = gps.time.value(); // Raw time in HHMMSSCC format (u32)
                CurrentDate = gps.date.value(); // Raw date in DDMMYY format (u32)
                Serial.print("TIME:");
                Serial.println(CurrentTime);
                Serial.print("DATE:");
                Serial.println(CurrentDate);
                datetime_acquired = true;
                //writeLora(timenow);
            }
        }
    }
}
void processGPS() {

  float lat = gps.location.lat();
  float lng = gps.location.lng();

  if (gps.location.isValid()) {

    if ((lastlat != lat) || (lastlng != lng)) {

      char tmp[21] = "";
      char latlng[21] = "";
      
//      if (millis() > (startTime + current_cfg.gps*1000)) {
        dtostrf(lat, 10, 6, tmp);
        strcat(latlng, tmp);
        strcat(latlng, "&");
        dtostrf(lng, 10, 6, tmp);
        strcat(latlng, tmp);

        writeLora(latlng);

//      }
      lastlat = lat;
      lastlng = lng;
    }

  }
}


void writeLora(char message[]) {

  // Returns a standard format string <ID>,<Dest>,<datatype>,<message>,<route>
    char msg[128] = "";
    char buff[5];
    long packetID = generatePacketID();
    itoa(packetID,buff,10);
    
    strcat(msg, deviceID);
    strcat(msg, ",");
    strcat(msg, gatewayID);
    strcat(msg, ",");
    strcat(msg, message);
    strcat(msg, ",99,"); // Route set to 99 since from tracker
    strcat(msg,buff);
    strcat(msg,"\0");
    Serial.print("Sending : ");
    Serial.println(msg);
    
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket(true);
}

void parseIncoming(char incoming[]) {

    char msg[64] = "";
    char tmp[32] = "";



    char* words[3];
    char* token;
    char* rest = incoming;
    int index = 0;
    
    while ((token = strtok_r(rest, " ", &rest))) {
        words[index] = token;
        index++;
    }

  // Put other Commands Here

    Serial.println(words[0]);
    if (strcmp(words[0], "rxset") == 0) {
        if (strcmp(words[1], "default") == 0) {
            setDefaultConfig();
            writeConfig();
            writeLora("New Values Applied and Saved\0");
            return;
        } else {
            if (!setRxValues(words[1])) {
                writeLora("Error Setting new Rx Values\0");
                return;
            }
        }
    }
    
    if (strcmp(words[0], "reset") == 0) {
        writeLora("Not implemented on XIAO");
        delay(10000);
        // Not sure how to do this with the xiao    
    }

    if (strcmp(words[0], "delay") == 0) {
        if (isInt(words[1])) {
            current_cfg.gps = atoi(words[1]);
            tmp[0] = '\0';
            strcat(tmp, "New Delay : ");
            strcat(tmp, words[1]);
            strcat(tmp, " Sec");
            writeLora(tmp);
            writeConfig();
        } else {
            writeLora("Delay Value must be an Integer");
        }

    }
    if (strcmp(words[0], "sleep") == 0) {
        if (isInt(words[1])) {
            int sleepmin = atoi(words[1]);
            int sleepsec = sleepmin * 60;
            strcat(msg, "Sleeping : ");
            itoa(sleepmin, tmp, 10);
            strcat(msg, tmp);
            strcat(msg, " Minutes");
            writeLora(msg);
            sleepXiao(sleepsec); // Not working with XIAO yet
        
        } else {

            writeLora("Sleep value must be an Integer");
        }
    }
    if (strcmp(words[0], "report") == 0) {
        reportConfig();
    }
  
}

boolean setRxValues(char msg[]) {

  char* words[3];
  char* token;
  char* rest = msg;
  int index = 0;
  bool success = true;
  char bw[8];

  Serial.print("setRx:");
  Serial.println(msg);
  while ((token = strtok_r(rest, ":", &rest)))
  {
    words[index] = token;
    index++;
  }
  strcpy(bw,words[0]);

  if (!isInt(words[1])) {
    Serial.println("SF Not INT");
    return false;
  }
  int sf = atoi(words[1]);

  if (!isInt(words[2])) {
    Serial.println("CR Not INT");
    return false;
  }
  int cr = atoi(words[2]);

  if (!validBW(bw))
    success = false;
  if (!validSF(sf))
    success = false;
  if (!validCR(cr))
    success = false;

  if (success) {
    writeLora("New Values Saved");

    current_cfg = default_cfg;

    for (int i = 0; i < sizeof(bw_char_values); i++) {
        if(bw == bw_char_values[i]) {
            current_cfg.bw = bw_float_values[i];
        }
    }
    
    current_cfg.sf = sf;
    current_cfg.cr = cr;
    writeConfig();
    enableConfig();

  } else {
    return false;
  }
  return true;
}

bool validBW(char bw[]) {

  for (int i = 0; i < 10; i++) {
    if (strcmp(bw,bw_char_values[i]) == 0) {
      return true;
    }
  }
  Serial.println("Invalid BW value");
  return false;

}

bool validSF(int sf) {
  if ((sf > 5) && (sf < 13))
    return true;
  Serial.println("Invalid SF value");
  return false;
}

bool validCR(int cr) {
  if ((cr > 4) && (cr < 9))
    return true;
  Serial.println("Invalid CR value");
  return false;

}


void readConfig() {

    Serial.println("Reading Config");
    EEPROM.get(eeAddress, current_cfg);
    printConfig();
    if(current_cfg.sf == -1) {
        Serial.println("Creating New Config File");
        setDefaultConfig();
        writeConfig();
    }
}

void writeConfig() {
    

    Serial.println("Writing Config");
    EEPROM.put(eeAddress, (cfgObject) current_cfg);
    if (!EEPROM.getCommitASAP()) {
        Serial.println("CommitASAP not set. Need commit()");
        EEPROM.commit();
    }

}

void enableConfig() {
  Serial.println("Setting LoRa: " + String(current_cfg.bw) + "," + String(current_cfg.sf) + "," + String(current_cfg.cr));
  LoRa.setSignalBandwidth(current_cfg.bw);
  LoRa.setSpreadingFactor(current_cfg.sf);
  LoRa.setCodingRate4(current_cfg.cr);
  LoRa.setSyncWord(0xE3);
  LoRa.setTxPower(20);

}

void printConfig() {
    Serial.println("---= Current Settings =---");    
    Serial.println("Bandwidth : " + String(current_cfg.bw));
    Serial.println("Spread Factor : " + String(current_cfg.sf));
    Serial.println("Coding Rate : " + String(current_cfg.cr));
    Serial.println("GPS Interval : " + String(current_cfg.gps));
}

void setDefaultConfig() {

    current_cfg.bw = default_cfg.bw;
    current_cfg.sf = default_cfg.sf;
    current_cfg.cr = default_cfg.cr;
    current_cfg.gps = default_cfg.gps;

}

void sendPacket(String message) {
  Serial.print("Sending : ");
  Serial.println(message);
  String tempmsg = message;
  LoRa.beginPacket();
  LoRa.print(tempmsg);
  LoRa.endPacket(false);
}

boolean isInt(char str[]) {
  int l = strlen(str);
  bool number = true;
  bool t;
  for (int i = 0; i < l; i++) {
    t = isDigit(str[i]);
    if (!t) {
      number = false;
    }
  }
  return number;
}

boolean isFloat(char str[]) {
  int l = strlen(str);
  int decimals = 0;
  for (int i = 0; i < l; i++) {
    if (str[i] == '.')  {
      decimals++;
    } else if (!isDigit(str[i])) {
      return false;
    }
  }
  if (decimals < 2) {
    return true;
  } else {
    return false;
  }
}

void sleepXiao(int sec) {
  writeLora("Not Implemented on XIAO");

}
