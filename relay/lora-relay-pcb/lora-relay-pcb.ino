#include <SPI.h>
#include <LoRa.h>
#include <stdlib.h>
#include <EEPROM.h>
#include <LowPower.h>
>

extern unsigned int __bss_end;
extern void *__brkval;


struct payload {
    char origin[4];
    char dest[4];
    char data[49];
    char route[4];
    char packetid[5];
    
};


/* config DELAY:
 *  
 *  This is either a 0 or a 1
 *  
 *  0 = delay of transmit time
 *  1 = 2 x transmit time
 *  
 *  0's are really just waiting for an ACK from gateway otherwise could resend
 *  immediately. Therefore, any 0 that is not next to the gateway could send right away.
 *  *  
 *  Only messages that have the message route = deviceID (without the R)
 *  get sent without a delay to listen for others dealing with the message
 *  
 *  Most repeaters should only be able to hear one other repeater but you never know
 *  so assign 1 and 0 appropriately

*/
struct cfgObject {
    char bw[8];//BW
    int sf; //SF
    int cr; //CR
    int delay; //How long to listen for other relays before repeating (transmit cycles)
    int scan; // How long to scan for signals (seconds)
    int sleep; // How long to sleep between scans (minutes)
    int route; //Next Stop on the Route to Gateway (GW = 0)
    int neargw; // Next to gateway 0 = no, 1 = yes
};

// Constants //

const int ss = 10;  // Lora NSS pin
const int rst = 5;  // Lora Reset Pin
const int dio0 = 7; // Lora DIO0 Pin
const char deviceID[] = "R2";
const char gatewayID[] = "G1";
const int fields_in_payload = 5;
const int sleeps_before_reboot = 15; // Number of Sleep Cycles before Rebooting
const int transmit_time = 3000; // Time to transmit standard length message in milliseconds
const char ACK[4] = "ACK"; // Acknowledgment burst from Gateway on reciept
const int ack_wait_time = 900; //How long to wait for an ACK response in milliseconds

// Global Variables //

cfgObject default_cfg;
cfgObject current_cfg;

int eeAddress = 0; // Location for Config Data
char incoming[64]; // Payload from LoRa Buffer goes here
char queued_message[64]; // Extra message buffer

long int startTime = 0; // Global Timer
bool signal_detected = false;
bool duplicate_payload = false;
bool ACK_received = false;
int sleep_counter = 0;

const int wait_for_signal = 4; // Time to wait for more signals before sleeping (minutes)

// Function to Reset proMini
void(* resetFunc) (void) = 0;//declare reset function at address 0


void setup() {

    //Set Random Seed for PacketIDs
    randomSeed(analogRead(A0));
    
    //initialize Serial Monitor
    Serial.begin(115200);
    //while (!Serial);
    
    // Define Default Config
    strcpy(default_cfg.bw,"125E3");
    default_cfg.sf = 12;
    default_cfg.cr = 8;
    default_cfg.delay = 0;
    default_cfg.scan = 30;
    default_cfg.sleep = 2;
    default_cfg.route= 0;
    default_cfg.neargw = 1;
    
    Serial.println("------L O R A   R E L A Y-------");
    
    //Initialize LoRa Transceiver Module
    LoRa.setPins(ss, rst, dio0);
    Serial.print("Enabling LoRa Module..");
    while (!LoRa.begin(915E6)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("");


    //setDefaultConfig();
    //writeConfig();

    readConfig();
    enableConfig();

    // Start the Clock
    resetTimer();

}

void loop() {
    if(loraRead()) {
        processIncoming();
    }
    if (signal_detected == true and (millis() > (startTime+wait_for_signal*60*1000L))){
        Serial.println("Signal Lost");
        signal_detected = false;
    }
    
    if (signal_detected == false and (millis() > (startTime + current_cfg.scan*1000L))){
        sleep(current_cfg.sleep);
    }
    
}

// -------------------- LoRa Read and Write Functions -------------------------
bool loraRead(){
    int index = 0;
    int packetSize = LoRa.parsePacket();
    
    if (packetSize > 0) {
        // received a packet
        // read packet and report
        for(int i = 0; i < packetSize; i++){
            while (LoRa.available()) {
                incoming[index] = LoRa.read();
                index++;
            }
        }
        incoming[index] = '\0';
        Serial.print("Received : ");
        Serial.println(incoming);

        signal_detected = true;
        resetTimer();
        return true;
    } else {
          // Nothing to see here
          return false;
    }
    
}

void loraWrite(char message[]) {

    char packetID[7];
    long pid = generatePacketID();
    itoa(pid,packetID,10);


    // Returns a standard format string <ID>,<Dest>,<data>,<route>,<packetID>
    char msg[64] = "";
    char myrte[4] = "";
    strcat(msg,deviceID);
    strcat(msg,",");
    strcat(msg,gatewayID);
    strcat(msg,",");
    strcat(msg, message);
    strcat(msg,",");
    // Convert route to string
    itoa(current_cfg.route,myrte,10);
    strcat(msg,myrte);
    strcat(msg,",");
    strcat(msg,packetID);
    
    
    Serial.print("Sending : ");
    Serial.println(msg);
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();

}

void loraRelay(payload outgoing) {
    // Basically sends the same message but changes
    // the route value if the destination is gateway
    char newmsg[64] = "";
    char newrte[4] = "";
    itoa(current_cfg.route,newrte,10);

    // Reconstruct message from payload

    strcat(newmsg, outgoing.origin);
    strcat(newmsg,",");
    strcat(newmsg, outgoing.dest);
    strcat(newmsg,",");
    strcat(newmsg, outgoing.data);
    strcat(newmsg,",");
    if(strcmp(outgoing.dest, gatewayID) == 0) {
        strcat(newmsg, newrte);
    } else {
        strcat(newmsg, outgoing.route);
    }
    strcat(newmsg,",");
    strcat(newmsg, outgoing.packetid);
    
    // Send Payload
    Serial.print("Relaying Payload : ");
    Serial.println(newmsg);
    LoRa.beginPacket();
    LoRa.print(newmsg);
    LoRa.endPacket();
    
}

//------------------------------- Incoming Signal Functions ----------------------

void processIncoming() { 
    char current_message[64];
    
    strcpy(current_message,incoming);
    // Only called from main loop
    
    // Is this an ACK message?
    if(strcmp(incoming, ACK) == 0) {
        // We can ignore these unless called for
        return;
    }

    payload current_payload;
    int index = messageToPayload(incoming,current_payload);

    Serial.print("Free Memory :");
    Serial.println(freeMemory());


    if (index == fields_in_payload){ 
        // Payload is good, let's deal with it.
        
        if(strcmp(deviceID, current_payload.dest) == 0 ) {
            Serial.println("For Me");
            // Send the data part to be parsed
            parseDataField(current_payload.data);
            return;
        }
        /* 
         *  Fist check if we are in the route field.
         *  The only way we can be in the route is if 
         *  someone put us there so we can retransmit 
         *  immediately if we are.       
        */
        
        if(inRoute(current_payload.route)) {
            Serial.println("In Route");
            loraRelay(current_payload);
            return;
        }
        /* If relay is not within range of gateway AND it has a delay of 0
         *  you can safely retransmit immediately since you can't hear any ACK
         *  from gateway and relays around you should only be delay = 1
         */

        if(current_cfg.neargw == 0 && current_cfg.delay == 0) {
            loraRelay(current_payload);
            return;
        }
        
        if(!newSignal()){
            // If you hear nothing back...relay it
            Serial.println("No Reply..Relaying");
            loraRelay(current_payload);
            return;
        } 
        Serial.print("---> New Messages : ");
        /* This is more complicated since multiple things could be happing.
           1. ACK from gateway (if config.neargw = 1)
           2. Another repeater has relayed message
           3. Reply to a command from the gateway
           4. Totally new message

        */
        if(strcmp(incoming, ACK) == 0) {
            // Message recieved by gateway. Ignore.
            Serial.println(" [Received by GW]");
            return;
        }
        
        payload new_payload;
        index = messageToPayload(incoming,new_payload);
        
        if(index == fields_in_payload) {
            if(strcmp(current_payload.packetid, new_payload.packetid) == 0) {
                // Message has been retransmitted by someone else. Ignore
                Serial.println(" [Duplicate]");
                return;
            }
            
            if(strcmp(current_payload.origin, gatewayID) == 0) {
                // Message is from gateway we should listen for a reply
                if(strcmp(current_payload.dest, new_payload.origin) == 0) {
                    // New message is from target. Sorted. Ignore.
                    Serial.println(" [Reply from Target]");
                    return;
                }
            }
            
            /* If you get here it's a totally new message. 
             * so we need to send both the original and new messages
             */
             loraRelay(current_payload);
             delay(500);
             loraRelay(new_payload);
             return;
        }

    } else {
        // Something weird about this payload
        Serial.print("Weird Payload : ");
        Serial.println(incoming);
        /* Could repackage it and send it on but packet size might
         *  get too large.
         */
        
    }
    
}

int messageToPayload(char message[], payload &new_payload) {

    char* fields[fields_in_payload];
    char* token;
    char* rest = message;
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
    strcpy(new_payload.origin, fields[0]);
    strcpy(new_payload.dest, fields[1]);
    strcpy(new_payload.data, fields[2]);
    strcpy(new_payload.route, fields[3]);
    strcpy(new_payload.packetid, fields[4]);

    return index;
}

bool newSignal() {
    // This reads the lora buffer and waits for more payloads

    // Make a copy of incoming
    Serial.print("Waiting for new signal");
    char original_message[64];
    strcpy(original_message,incoming);
    
    bool newsignal = false;
    int wait_time = ((((current_cfg.delay * transmit_time) + transmit_time)));
    Serial.print("Waiting : ");
    Serial.print(wait_time);
    Serial.println(" ms");
    long int now = millis();
    while(millis() < now + wait_time) {
        if(loraRead()) {
            return true;
        }
    }
    return false;
}

// ----------------------------- Config Functions -------------------------

void readConfig(){

    Serial.println("Reading Config");
    cfgObject config;
    EEPROM.get(eeAddress,config);
    strcpy(current_cfg.bw,config.bw);
    //current_cfg.bw = config.bw;
    current_cfg.sf = config.sf;
    current_cfg.cr = config.cr;
    current_cfg.sleep = config.sleep;
    current_cfg.scan = config.scan;
    current_cfg.delay = config.delay;
    current_cfg.route = config.route;
    current_cfg.neargw = config.neargw;

    if(current_cfg.scan == -1) {
        setDefaultConfig();
        writeConfig();   
    }
}

void enableConfig() {
    Serial.println("Setting LoRa: "+String(current_cfg.bw)+","+String(current_cfg.sf)+","+String(current_cfg.cr));
    float floatbw = atof(current_cfg.bw);
    LoRa.setSignalBandwidth(floatbw);
    LoRa.setSpreadingFactor(current_cfg.sf);
    LoRa.setCodingRate4(current_cfg.cr);
    LoRa.setSyncWord(0xE3);
    LoRa.setTxPower(20);

    delay(500);
}

void writeConfig(){
    Serial.println("Writing Config");
    EEPROM.put(eeAddress, current_cfg);
}

void setDefaultConfig(){

    Serial.print("default route :");
    Serial.println(default_cfg.route);

    strcpy(current_cfg.bw, default_cfg.bw);
    current_cfg.sf = default_cfg.sf;
    current_cfg.cr = default_cfg.cr;
    current_cfg.delay = default_cfg.delay;
    current_cfg.scan = default_cfg.scan;
    current_cfg.sleep = default_cfg.sleep;
    current_cfg.route = default_cfg.route;
    current_cfg.neargw = default_cfg.neargw;
}

void reportConfig() {

    char tmp[64] = "";
    char flt[16];
    strcat(tmp, current_cfg.bw);
    strcat(tmp, "/");
    itoa(current_cfg.sf, flt, 10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.cr, flt, 10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.delay, flt, 10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.scan,flt,10);
    strcat(tmp, flt);
    strcat(tmp, "/");
    itoa(current_cfg.sleep,flt,10);
    strcat(tmp, flt); 
    strcat(tmp, "/");
    itoa(current_cfg.neargw,flt,10);
    strcat(tmp, flt);  

    loraWrite(tmp);
}
// ---------------------------------- Process Incoming Commands From Gateway -----------------------

void parseDataField(char data_field[]) {

    char command[49] = "";
    char* words[3];
    char* token;
    char* rest = data_field;
    int index = 0;
    char buffer[32] = "";

    // preserve data field
    strcat(command,data_field);

    // Command is broken up into words seperated by spaces
    // Split them up

    while ((token = strtok_r(rest, " ", &rest)))
    {
      words[index] = token;
      index++;
    }
    
    if(strcmp(words[0],"rxset") == 0) {
        if(!setRxValues(words[1])){
            loraWrite("Error Setting new Rx Values");
        }
    }
    if(strcmp(words[0],"default") == 0) {
        setDefaultConfig();
        writeConfig();
        loraWrite("Defaults Restored");
    }

    
    if(strcmp(words[0],"scan") == 0) {
        if( isInt(words[1])) {
              loraWrite("Scan Period Updated");
              current_cfg.scan = atoi(words[1]);
              writeConfig();
        } else {
            loraWrite("Error. Try: scan <seconds>");
        }
    }
    if(strcmp(words[0],"neargw") == 0) {
        if( isInt(words[1])) {
            int ng = atoi(words[1]);
            if(ng == 0 || ng == 1) {
                current_cfg.neargw = atoi(words[1]);
                writeConfig();
                strcpy(buffer,"neargw now = ");
                strcat(buffer,words[1]);
                loraWrite(buffer);
            } else {
                loraWrite("Error : Try neargw <0/1>");
            }
        } else {
            loraWrite("Error : Try neargw <0/1>");
        }
    }
    if(strcmp(words[0],"sleep") == 0) {
        if( isInt(words[1])) {
              strcpy(buffer,"New Sleep Period: ");
              strcat(buffer, words[1]);
              strcat(buffer, " min");
              current_cfg.sleep = atoi(words[1]);
              writeConfig();
              loraWrite(buffer);
        } else {
            loraWrite("Error. Try: sleep <minutes>");
        }
    }
    if(strcmp(words[0],"report") == 0) {
        reportConfig();    
    }
    if(strcmp(words[0],"delay") == 0) {
        if( isInt(words[1])) {
              loraWrite("Updating Relay Delay");
              setRelayDelay(atoi(words[1]));
        }
    }

    if(strcmp(words[0],"route") == 0) {
        if(isInt(words[1])) {
              current_cfg.route = atoi(words[1]);
              writeConfig();
              loraWrite("Routing Updated");
        } else {
              loraWrite("Error Updating Route!");
        }
   }
    

}

boolean setRxValues(char msg[]){

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
        loraWrite("OK : Resetting");
        strcpy(current_cfg.bw, bw);
        //current_cfg.bw = bw;
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

void setRelayDelay(int sec) {
    
    current_cfg.delay = sec;
    writeConfig();
    Serial.println(current_cfg.delay);
    
}

void setRoute(char rte[]) {
    
    strcpy(current_cfg.route,rte);
    writeConfig();
   
}


// ---------------------------------- Validity Check Functions ------------------------------
boolean inRoute(char msgrte[]) {
    // Drop R from deviceID
    char * myroute = deviceID + 1;
    if(strcmp(myroute, msgrte)==0) {
        return true;
    }
    return false;

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

// -------------------------------------- MISC Functions ----------------------------------------
void resetTimer(){
    startTime = millis();
}

void sleep(int mins) {

    sleep_counter++;
    
    // Decided to reboot once and a while to keep things fresh.
    
    if (sleep_counter > sleeps_before_reboot) {
      resetFunc();
    }


    
    LoRa.sleep();
    int cycles = (mins*60)/8;
    Serial.println("Sleeping");
    delay(1000);
    for (int i = 0 ; i < cycles; i++){
     LowPower.powerDown(SLEEP_8S, ADC_OFF,BOD_OFF) ;
    }
    LoRa.idle();
    Serial.println("Awake");
    Serial.print("Free Memory :");
    Serial.println(freeMemory());
    resetTimer();

}

long generatePacketID() {
    long randNumber = random(1234,9999);
    return randNumber;
}

//
// Returns the current amount of free memory in bytes.
//
int freeMemory() {
    int free_memory;
    if ((int) __brkval)
        return ((int) &free_memory) - ((int) __brkval);
    return ((int) &free_memory) - ((int) &__bss_end);
}
