#include <Console.h>    // NOTE This sketch will use the "Console" instead of "Setrial"
#include <Process.h>

#define SKETCH_VERSION "Tesla El Rapportering for Arduino YUN v0.1.0-beta"
#define DEBUG "TRUE"

/*
 * 
 * This sketch requires the Python script "/root/MyTesla/FileRW.py" and the two files "/root/MyTesla/TeslaCharging" and "/root/MyTesla/TeslaMeter". 
 * These datafiles are used for two issues:
 * 1) In case of power failure, the last known status is stored and used when restart.
 * 2) To start the syste with initial settings (e.g. the correct meter value can be added to TeslaMeter
 * 
 * TeslaMeter will holde the power meter value, which is updated everty time the meter is updated.
 * TeslaCharging will hold the state (charging or not charinge == true or false) the the chargine state.
 * 
 * MaxPulsChargeIntervalTime = 3600000.0 / (MIN_CHARGE_POWER_CONSUMPTION * NO_OF_PULSES_PER_KWh / 1000.00);
 * 
 * MIN_CHARGE_POWER_CONSUMPTION = 1000,00Wh, NO_OF_PULSES_PER_KWh 100,00 ==> MaxPulsChargeIntervalTime = 36.000,00 miliseconds
 * 
 * 
 */
#define MAX_PULS_CHARGE_INTERVAL_TIME 36000 //   
#define NO_OF_PULSES_PER_KWh 100

#define INTERRUPT_TRIGGER 1    // 1 for PIN 2 on Arduino YUN

#define CONTENT_LEN 10            // max antal forventgede karakterer gemt i datafiler
/*
 * ###################################################################################################
 *                       V  A  R  I  A  B  L  E      D  E  F  I  N  A  I  T  O  N  S
 */
boolean Debug = DEBUG;


Process python;                 // process used to get the date

boolean Charging_F = false;

volatile unsigned long PulseTimeStamp = 0;
volatile int PulseCounter = 0;  // For counting pulses 
volatile double KWh_total;
volatile unsigned long LastPulseTimeStamp = 0;
volatile unsigned long PulseIntervalTime = 0;


/*
######################################################################################################################################
                                       F  U  N  C  T  I  O  N        D  E  F  I  N  I  T  I  O  N  S
*/


/* Interrupt function "Count  pulses"
 * Triggered by a pulse on the powermeter and increase KWh counters
 */
void Pulse_Count() {
  PulseTimeStamp = millis();
  PulseCounter++;
  KWh_total = KWh_total + (1.00/double(NO_OF_PULSES_PER_KWh));
  
  if (LastPulseTimeStamp != 0) 
    PulseIntervalTime = PulseTimeStamp - LastPulseTimeStamp;
  
  LastPulseTimeStamp = PulseTimeStamp;

  }
/*
 * ###################################################################################################
 *                       S E T U P      B e g i n
 * ###################################################################################################
 * ###################################################################################################
 */

void setup() {
  char content[CONTENT_LEN] = "";
    
  // To visually verify booting. Initialize digital pin 13 as an output and set it high
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);  
   
  // Initialize Bridge and Console. Wait for port to open:
  Bridge.begin();
  Console.begin();

  // Wait for Console port to connect
  while (!Console);

                                                     if (Debug) Console.println("\nBridge and console initialized... Wait for connection");
  
  //Define interrupt action
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(INTERRUPT_TRIGGER, Pulse_Count, FALLING); 

  // "Reset interrupt initialization updates
  PulseCounter = 0;
  PulseTimeStamp = 0;
  LastPulseTimeStamp = 0;
  PulseIntervalTime = 0;

  
                                                     
                                                     if (Debug)  Console.println("Initialize Python");
   // Get stored Meter value stored 
  if (!python.running()) {
    python.begin("python");
    python.addParameter("/root/MyTesla/FileRW.py");
    python.addParameter("TeslaMeter");
    python.run();
  }
  
                                                     if (Debug)  Console.println("Read output from Python (Meter totals)\n");
  int ii = 0;
  while (python.available() > 0 and ii < CONTENT_LEN){
    content[ii++] = python.read();
  }

  KWh_total = atof(content);
                                                     if (Debug)  Console.println(KWh_total);
  

  while (python.available() > 0){
    python.read();
  }


                                                     if (Debug)  Console.println("Initialize Python");

   // Get stored charging state 
  if (!python.running()) {
    python.begin("python");
    python.addParameter("/root/MyTesla/FileRW.py");
    python.addParameter("TeslaCharging");
    python.run();
  }
                                                     if (Debug)  Console.println("Read output from Python (Charging)\n");
  
  // The first (and only) character is expected to be either 0 or 1 and sets the chargnig flag
  if (python.available() > 0) {
    if (python.read() == 48) {      // 48(int) represent ASCII '0'
      Charging_F = false;
    } else
      Charging_F = true;
  }
  //empty the input buffer for eventual other characters
  
                                                   if (Debug) {
                                                     if (Charging_F == false) {
                                                       Console.println("Not Charging");
                                                     } else {
                                                       Console.println("Charging");
                                                     }

                                                   }
  while (python.available() > 0){
    python.read();
  }

  digitalWrite(13, LOW);   
}


/*
 * ###################################################################################################
 *                       L O O P     B e g i n
 * ###################################################################################################
 * ###################################################################################################
 */

void loop() {
delay(100000000);

double KWh_Used;
  
  // put your main code here, to run repeatedly:
/* 
 *  To trigger when Charging Stops == next puls might no come before next charging stats, PulseIntervalTime is manipulated to indicate that charging has stoppend
 *  
 */
  if (millis() - LastPulseTimeStamp > MAX_PULS_CHARGE_INTERVAL_TIME && PulseIntervalTime < MAX_PULS_CHARGE_INTERVAL_TIME){ 
    PulseIntervalTime = millis() - LastPulseTimeStamp;
  }


/*  C H A R G E   /   S T A N D B Y   Reporting
 * 
 
    
  if (Charging_F && PulseIntervalTime >= MAX_PULS_CHARGE_INTERVAL_TIME){  //Charging ended
    digitalWrite(9, HIGH);
                                                                                                      if (Debug) Serial.println("Charging E N D E D...");
    KWh_Used = PulseCounter / double(NO_OF_PULSES_PER_KWh);
    PulseCounter = 0;
    Charging_F = false;
    
    EEPROM.write(CHARGING_F_ADDR,0);

    dtostrf( KWh_total, 4,2,buf);
    sprintf(DataString, "%s KWh Total. ", buf);
    dtostrf( KWh_Used, 4,2,buf);
    sprintf(DataString, "%s%s KWh brugt til opladning.", DataString, buf);
    send_message("Tesla El-Maalinge: Opladning afsluttet.", DataString);
                                                                                                      if (Debug) Serial.println(DataString);
    digitalWrite(9, LOW);
  } else if (!Charging_F && PulseIntervalTime > 0 && PulseIntervalTime < MAX_PULS_CHARGE_INTERVAL_TIME){
    digitalWrite(9, HIGH);
                                                                                                      if (Debug) Serial.println("Charging S T A R T E D...");
    Ethernet.maintain();
    KWh_Used = PulseCounter / double(NO_OF_PULSES_PER_KWh);
    PulseCounter = 0;
    Charging_F = true;

    EEPROM.write(CHARGING_F_ADDR,1);

    dtostrf( KWh_total, 4,2,buf);
    sprintf(DataString, "%s KWh Total. ", buf);
    dtostrf( KWh_Used, 4,2,buf);
    sprintf(DataString, "%s KWh brugt i Stand By.", DataString, buf);
    send_message("Tesla El-Maalinge: Opladning startet.", DataString);
                                                                                                      if (Debug) Serial.println(DataString);
    digitalWrite(9, LOW);
  }
*/
}
