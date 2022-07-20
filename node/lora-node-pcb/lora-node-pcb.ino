#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <stdlib.h>
#include <EEPROM.h>
#include "LowPower.h"

//define the pins used by the transceiver module
//These settings will work with the Pro-Mini DIYCON pcb board
//Other wise, define your own.

#define ss 10 // 10 for both
#define rst 5 // 5 for both
#define dio0 7 // 7 for ray, 2 for mine

#define SerialMonitor Serial
#define FAILSAFE_TIMEOUT  30 // seconds

// Device ID of this Device.  Must start with a 'T' followed by a number
#define deviceID "T2"

// Device ID of the Gateway. I'm leaving room open here for multiple
// gateways.  Not sure why, haven't thought about it that much.
#define gatewayID "G1"

// Easy way to deal with Lat/Lng coordiants
struct GPSData{
	float lat;
	float lng;
};

// I found making a struct was the best way 
// of dealing with the device configuration as well
struct cfgObject {
  char bw[8];//BW
  int sf; //SF
  int cr; //CR
  int gps; //GPS Transmit Interval
};


// Setup TinyGPS++
// Only RX is strictly necessary. We're not sending commands to the GPS.
static const int RXPin = 6, TXPin = 0;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial sws(RXPin, TXPin);

// Where to store the config file in the EEPROM
int eeAddress = 0;   //Location for config data

// Create two configuration objects
cfgObject default_cfg;
cfgObject current_cfg;

// Define some other global variables
char last_packetID[8] = "";
long int startTime = 0;
float lastlat = 0;
float lastlng = 0;


//declare reset function at address 0
void(* resetFunc) (void) = 0;

//Message recieved are broken into comma separated fields, there are 5 of them.
char* fields[5];  

void setup() {
	//Get a "random" seed startup value by reading Analog 3 pin
   int a3value = analogRead(A3);
   randomSeed(a3value);

    // Set your Default BW/SF/CR here
    
    strcpy(default_cfg.bw,"125E3");
    default_cfg.sf = 12;
    default_cfg.cr = 8;
    default_cfg.gps = 13;
    
	
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

	// Start out with the default config settings
    setDefaultConfig();
    enableConfig();
    printConfig();
    
	// Failsafe waits for instructions from the mothership
	// before setting the configuration to the stored values
	failsafeMode();
    
    // Load and enable the stored configuraiton
    readConfig();
    enableConfig();
    printConfig();
  
    reportConfig("Online");



	//GPS STARTUP
	sws.begin(GPSBaud);
	Serial.println("GPS Enabled");

	//Start the Clock
	startTime = millis();


}

void loop(){

	// Continuously read for Incoming Lora
	// Read and report GPS every gpstimer ms

	readLora();
	readGPS();


}

long generatePacketID() {
	// Generates and returns a random number between 1000 and 9999
    long randNumber = random(1000,9999);
    return randNumber;
}

int payloadToFields(char msg[]) {
	// Split up an incoming message into the fields array
    char incoming[64] ="";
    fields[0] = '\0';
    strcpy(incoming,msg); // original msg will get deconstructed so copy to a new variable and use that
    char* token;
    char* rest = incoming;
    int index = 0;

    // Store Comma Seperated values in field[]
    while ((token = strtok_r(rest, ",", &rest)))
    {
      if(token == NULL) {
        Serial.println("Null Value");
      }
      fields[index] = token;
      index++;
    }
    return index;
}

void reportConfig(char prefix[]) {
	// Just what it says
	// This creates the "message" part of what is going to be sent out
	// <Prefix>-<BW>/<SF>/<CR>/<seconds beween sending GPS location>
    char tmp[64] = "";
    char flt[16];
    strcat(tmp, prefix);
    strcat(tmp," - ");
    strcat(tmp, current_cfg.bw);
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
	// Here is where we just wait and listen for the mothership
    Serial.println("Failsafe Mode");
    reportConfig("Failsafe");
    long int starttime = millis();    

    while(millis() < starttime + FAILSAFE_TIMEOUT*1000) {
        readLora();
    }
}

bool newMessage() {
	// Deterimines if the message  recieved is the same as the last one received
    if(strcmp(fields[4], last_packetID) == 0) {
        Serial.println("Duplicate Message");
        return false;
    } else {
        return true;
        strcpy(last_packetID, fields[4]);
    }
 
}

bool myMessage(){
	// Dtermines if the message is for this device
    if(strcmp(deviceID,fields[1]) == 0) {
        return true;
    } else {
        return false;
    }
    
}

void readLora(){
	// Reads the LoRa buffer
	char incoming[64] = "";
    int index = 0;
    int total_fields=0;

	// Check to see if there is packet ready
    int packetSize = LoRa.parsePacket();
	if (packetSize > 0) {
	// received a packet
		for(int i = 0; i < packetSize; i++){
	//Load packet into the 'incoming' char array		
            while (LoRa.available()) {
                incoming[index] = LoRa.read();
                index++;
           }
        }
        incoming[index] = '\0';
        Serial.println(incoming);
        
	// convert the message to a fields array
        total_fields = payloadToFields(incoming); // Populate Fields Array
    
    // Do some checks and respond appropiately
        if (newMessage()) {
            if (myMessage()) {
				// Sends the message part of the packet to be processed
                parseIncoming(fields[2]);
            }
        }
	}
	
}


void readGPS() {
	// Check to see if there is GPS info to process if it's time to do so
    while (sws.available() > 0){ 
        if (gps.encode(sws.read())){
            if (millis() > (startTime + current_cfg.gps*1000)) {
                processGPS();
                startTime = millis();
            }
        }
    }
            
}

void processGPS() {
	//This takes the current gps reading and turns it into
	//a message and sends it off
	float lat = gps.location.lat();
	float lng = gps.location.lng();

	if (gps.location.isValid()) {
		if ((lastlat != lat) || (lastlng != lng)) {

			char tmp[21] = "";
			char latlng[21] = "";
            dtostrf(lat,10,6,tmp);
            strcat(latlng,tmp);
            strcat(latlng,"&");
            dtostrf(lng,10,6,tmp);
            strcat(latlng,tmp);

            Serial.println(latlng);
            writeLora(latlng);

            lastlat = lat;
            lastlng = lng;
		}

	}
}

void writeLora(char message[]) {
	// This will take the 'message' char array and turn it into a packet
	// that has some other info with it:
	// PACKET = <this device id>,<destinations id>,<THE MESSAGE>,<routing info>,<packetid>
	// ex. T1,G1, 33.856117&-86.812945,99,6984

	
    char packetID[7];
    long pid = generatePacketID();
    itoa(pid,packetID,10);

// Returns a standard format string <ID>,<Dest>,<data>,<route>,<packetID>
    char msg[128] = "";
    strcat(msg,deviceID);
    strcat(msg,",");
    strcat(msg,gatewayID);
    strcat(msg,",");
    strcat(msg, message);
    strcat(msg,",99,"); // No Routing Info
    strcat(msg,packetID);
    
    Serial.print("Sending : ");
    Serial.println(msg);

    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();

}

void parseIncoming(char msg[]) {
	// This takes the message part of a packet and determines if there
	// is somethig that needs to be done.
	// ex. the message could be "delay 20"
    char* words[3];
    char* token;
    char* rest = msg;
    int index = 0;

	// Split the message up by spaces
    while ((token = strtok_r(rest, " ", &rest)))
    {
      words[index] = token;
      index++;
    }
	
	// Start checking to see if we can make sense of the message
	// and respond appropriately
	// Available command:
	// rxset, default, reset, report, sleep, delay
	
	if(strcmp(words[0],"rxset") == 0) {
		if(!setRxValues(words[1])){
			writeLora("Error Setting new Rx Values");
	}
		}

	if(strcmp(words[0],"default") == 0) {
    writeLora("Setting Default Rx");
		setDefaultConfig();
		writeConfig();

	}

	if(strcmp(words[0], "reset") == 0){
		resetFunc();
	}
			
	if(strcmp(words[0],"report") == 0) {
		  reportConfig("Report");
	}

	if(strcmp(words[0],"sleep") == 0) {
        if( isInt(words[1])) {
    		  sleep(atoi(words[1]));
        } else {
            writeLora("Failed. Try: <trackerid> <sleep> <minutes>");
        }
    }

    if(strcmp(words[0],"delay") == 0) {
        if( isInt(words[1])) {
            current_cfg.gps = atoi(words[1]);
            Serial.println("Attempt to write config");
            writeConfig();
            writeLora("New Reporting Interval Enabled");

        } else {
            writeLora("Failed. Try: <trackerid> <delay> <seconds>");
        }
    }
}

boolean setRxValues(char msg[]){
	// Changes the transmit/recieve settings and update
	// the store config file
    char *words[3];
    char* token;
    char* rest = msg;
    int index = 0;
    bool success = true;
    
    Serial.print("setRx:");
    Serial.println(msg);
    while ((token = strtok_r(rest, ":", &rest)))
    {
      words[index] = token;
      index++;
    }
    char bw[8];
    strcpy(bw,words[0]);
    
    if(!isInt(words[1])) {
		Serial.println("SF Not INT");
		return false;
	}
	int sf = atoi(words[1]);
	
	if(!isInt(words[2])) {
		Serial.println("CR Not INT");
		return false;
	}
	int cr = atoi(words[2]);

	if(!validBW(bw))
		success = false;
	if(!validSF(sf))
		success = false;
	if(!validCR(cr))
		success = false;

	if(success) {
		writeLora("OK : Resetting");
        strcpy(current_cfg.bw, bw);
        current_cfg.sf = sf;
        current_cfg.cr = cr;
		writeConfig();
    delay(1000);
		resetFunc();

	} else {
		return false;
	}
	return true;
}

// Various validity checks in the next bit.
// Should be self explanitory

bool validBW(char bw[]) {
    char* bwvalues[10] = {"7.8E3", "10.4E3", "15.6E3", "20.8E3", "31.25E3", "41.7E3", "62.5E3", "125E3", "250E3", "500E3"};
    for(int i = 0; i < 10; i++) {
        if (strcmp(bw,bwvalues[i]) == 0) {
            return true;
        }
    }
    Serial.println("Invalid BW value");
    return false;   
}

bool validSF(int sf) {
	if((sf > 5) && (sf < 13))
		return true;
    Serial.println("Invalid SF value");
	return false;
}

bool validCR(int cr) {
	if((cr > 4) && (cr < 9))
		return true;
    Serial.println("Invalid CR value");
	return false;

}



void readConfig(){

	// Load current_cfg from eeprom

    Serial.println("Reading Config");
	EEPROM.get(eeAddress,current_cfg);

	// If there is a problem, set to default config and 
	// store that into eeprom
	
    if(current_cfg.sf == -1) {
        setDefaultConfig();
        writeConfig();
    }

}

void enableConfig() {
	
	// start using the current_config
	
    float floatbw = atof(current_cfg.bw);
    Serial.println("Setting LoRa: "+String(current_cfg.bw)+","+String(current_cfg.sf)+","+String(current_cfg.cr));
    LoRa.setSignalBandwidth(floatbw);
    LoRa.setSpreadingFactor(current_cfg.sf);
    LoRa.setCodingRate4(current_cfg.cr);
    LoRa.setSyncWord(0xE3);
    LoRa.setTxPower(20);

    delay(500);
}

void writeConfig(){
	
	// Stores the current_cfg structure into eeprom
    Serial.println("Writing Config");
	EEPROM.put(eeAddress, current_cfg);
}

void printConfig(){
	Serial.println("Bandwidth : " + String(current_cfg.bw));
	Serial.println("Spread Factor : " + String(current_cfg.sf));
	Serial.println("Coding Rate : " + String(current_cfg.cr));
    Serial.println("GPS Delay : " + String(current_cfg.gps) + "Sec.");
}

void setDefaultConfig(){

    strcpy(current_cfg.bw, default_cfg.bw);
    current_cfg.sf = default_cfg.sf;
    current_cfg.cr = default_cfg.cr;
    current_cfg.gps = default_cfg.gps;
    
}

boolean isInt(char str[]) {
	int l = strlen(str);
	bool number = true;
	bool t;
	for(int i = 0;i < l;i++){
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
	for(int i = 0;i < l;i++){
		if(str[i] == '.')  {
			decimals++;
		} else	
			if(!isDigit(str[i])) {
				return false;
			}
		}
	if (decimals < 2) {
		return true;
	} else {
		return false;
    }
}

void sleep(int mins) { // DOES NOT WORK WITH GPS ACTIVE.. DAMN

  writeLora("Sleeping");

  long int cycles = (mins*60)/8;

  LoRa.sleep();
	for (int i = 0 ; i < cycles; i++){
	    LowPower.powerDown(SLEEP_8S, ADC_OFF,BOD_OFF) ;
        delay(10);
    }
   
	LoRa.idle();
  Serial.println("Awake");
}
