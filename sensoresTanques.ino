#include <OneWire.h>
#include <DallasTemperature.h>
/*
  Define variables to send SMSs
*/
int8_t answer;
int onModulePin= 2;
int xtime = 900000; // 15 mintutes to delay after send SMSs
char aux_string[30];
char *phones[]={"+526462236390"};   // is the number to send SMS
char pin[] = ""; // number pin of the sim card
String sms_text="";     
/*
 Define variables to init One Wire Sensors
 */
// Data wire is plugged into port 10 on the Arduino
#define ONE_WIRE_BUS 10
#define TEMPERATURE_PRECISION 9
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
int numberOfDevices; // Number of temperature devices found
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address 
void setup(void){
  // start serial port
  Serial.begin(9600);
  // Start up the library
  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();  
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");
  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++){
      // Search the wire for address
      if(sensors.getAddress(tempDeviceAddress, i)){
        Serial.print("Found device ");
        Serial.print(i, DEC);
        Serial.print(" with address: ");
        printAddress(tempDeviceAddress);
        Serial.println();
        Serial.print("Setting resolution to ");
        Serial.println(TEMPERATURE_PRECISION, DEC);
        // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
        sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
        Serial.print("Resolution actually set to: ");
        Serial.print(sensors.getResolution(tempDeviceAddress), DEC); 
        Serial.println();
    }else{
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }

}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress, int num){
    num = num +1;
    float tempC = sensors.getTempC(deviceAddress);
    String stringToSend;
    Serial.println("Sensor #"+(String)num+"    Temp C: "+(String)tempC);
    if(tempC >25 ){
        sms_text +="La temperatura del sensor: "+(String)num+" es mayor a 24C, la temperatura es de: "+(String)tempC+"\n";  
    }
    else if(tempC < 14){
         sms_text +="La temperatura del sensor: "+(String)num+" es menor a 14C, la temperatura es de: "+(String)tempC+"\n";  
  
    }
}

void loop(void){  
  sms_text="";     
  // call sensors.requestTemperatures() to issue a global temperature 
  sensors.requestTemperatures(); // Send the command to get temperature
  // Loop through each device, print out temperature data
  for(int i=0;i<numberOfDevices; i++)
  {
     // Search the wire for address
       if(sensors.getAddress(tempDeviceAddress, i)){
        // It responds almost immediately. Let's print out the data
        printTemperature(tempDeviceAddress, i); // Use a simple function to print out the data
      }   
  }
  if(sms_text.length()> 0){
   Serial.println(sms_text);
   setupShieldSms();
  }

  delay(xtime);
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress){
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
/// Init shield sms


void setupShieldSms(){
    pinMode(onModulePin, OUTPUT);
    Serial.println("Starting... Shield");
    power_on();
    delay(3000);
    // sets the PIN code
    sprintf(aux_string, "AT+CPIN=%s", pin);
    sendATcommand(aux_string, "OK", 2000);
    delay(3000);
    Serial.println("Connecting to the network...");
    while( (sendATcommand("AT+CREG?", "+CREG: 0,1", 500) || 
            sendATcommand("AT+CREG?", "+CREG: 0,5", 500)) == 0 );
    Serial.print("Setting SMS mode...");
    sendATcommand("AT+CMGF=1", "OK", 1000);    // sets the SMS mode to text
    for(int i = 0; i<2; i++){
          Serial.println("Sending SMS to "+(String)phones[i]);

        sprintf(aux_string,"AT+CMGS=\"%s\"",phones[i] );
        answer = sendATcommand(aux_string, ">", 2000);    // send the SMS number
        if (answer == 1){
            Serial.println(sms_text);
            Serial.write(0x1A);
            answer = sendATcommand("", "OK", 20000);
            if (answer == 1){
                Serial.print("Sent ");   
                xtime = 900000; 
            }
            else{
                Serial.print("error ");
            }
        }
        else{
            Serial.print("error ");
            Serial.println(answer, DEC);
        }
   }

}
// Power On the shield
void power_on(){
    uint8_t answer=0;
    // checks if the module is started
    answer = sendATcommand("AT", "OK", 2000);
    if (answer == 0){
        // power on pulse
        digitalWrite(onModulePin,HIGH);
        delay(3000);
        digitalWrite(onModulePin,LOW);
        // waits for an answer from the module
        while(answer == 0){     // Send AT every two seconds and wait for the answer
            answer = sendATcommand("AT", "OK", 2000);    
        }
    }
}

// Setup ATCommand to send SMS protocols
int8_t sendATcommand(char* ATcommand, char* expected_answer, unsigned int timeout){
    uint8_t x=0,  answer=0;
    char response[100];
    unsigned long previous;
    memset(response, '\0', 100);    // Initialice the string
    delay(100);
    while( Serial.available() > 0) Serial.read();    // Clean the input buffer
    Serial.println(ATcommand);    // Send the AT command 
    x = 0;
    previous = millis();
    // this loop waits for the answer
    do{
        // if there are data in the UART input buffer, reads it and checks for the asnwer
        if(Serial.available() != 0){    
            response[x] = Serial.read();
            x++;
            // check if the desired answer is in the response of the module
            if (strstr(response, expected_answer) != NULL){
                answer = 1;
            }
        }
    // Waits for the asnwer with time out
    }while((answer == 0) && ((millis() - previous) < timeout));    
    return answer;
}

