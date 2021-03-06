/**
 * Program: ChristmasDiorama
 * Purpose:
 *   A sketch to control a DFRobot mp3 player to play Christmas carols and to control the street/house lights etc.
 *   At the moment, I'm reckoning to use an ESP12F set up with a low power voltage regulator so that it can:
 *   * use the Internet to check date and time;
 *   * go into deep sleep overnight;
 *   * have active times in the day;
 *   * stay on all day on Christmas Day;
 *   * respond to "lights on/off" events (via an LDR);
 *   * do anything else I think of while developing the project.
 *   NOTE: Rules should be -
 *   a) IF "someone comes near" (defined as: LDR dims)
 *      - Lights on in church
 *      - IF it's Christmas Eve/Day/Boxing Day, play peal of bells
 *      - Play 1 veerse of carol
 *      - Play walking through snow
 *      - Lights on round tree
 *      - Play entire christmas carol
 *   b) IF it's after 11pm
 *      - deep sleep until 6am
 *   c) IF it goes completely dark AND it's between 4pm and 11pm
 *      - Trigger sequence in (a) (assume someone's turned the lights off to see model lit up)
 * @author: David Argles, d.argles@gmx.com
 */

/* Program identification */ 
#define PROG    "ChristmasDiorama"
#define VER     "2.0"
#define BUILD   "14Dec2021 @23:17h"

/* Necessary includes */
#include "flashscreen.h"

// For the DFPlayer, we need the following
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
SoftwareSerial mySoftwareSerial(D2, D1); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// For time, we need a working Internet connection. The next line works with the ESP8266, change if necessary
#include <ESP8266WiFi.h>
#include "myinfo.h"
const char *ssid     = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// The next line sets up the time server (ezTime)
#include <ezTime.h>

/* Global "defines" - may have to look like variables because of type */
#define BAUDRATE  115200    // Baudrate for serial output
#define INTERVAL  3000      // Time between loops in mS
// The next lines define the connections to the DFPlayer
#define SOFT_TX   D1
#define SOFT_RX   D2
#define DFP_CLEAR D4 // Note: It's the 'busy' line, but high means available
// These lines define the connections to the LDR and the various LED lighting units
#define LDR       A0
#define CHURCH    D5
#define TREE      D6
#define HOUSES    D7
#define STREET    D8
// These lines define how the program responds to the LDR
#define LDR_DARK  10        // Reading from the LDR when it's considered "dark"
#define LDR_STEP  40        // How much the LDR must change by before it's considered a "change"
// These lines define when the unit sleeps
#define SLEEPTIME 23        // 24h clock - goes to sleep after 11pm
#define WAKETIME  6         // ditto - but defines wake time as 6am

/* ----- Initialisation ------------------------------------------------- */

/* Global stuff that must happen outside setup() */
flashscreen flash;
bool      sound = false;          // For DFPlayer
bool      wifi = false;           // For...
bool      timer = false;          // ...time server
int       prev_ldrValue = 0;      // So we know if it's lighter or darker
uint64_t  deepSleepTime = 3600e6; // Deep sleep delay (millionths of sec)


void setup() {
  Serial.begin(BAUDRATE);           // Start up the serial output port
  flash.message(PROG, VER, BUILD);  // Send program details to serial output

  // *** Set the various LED lines as outputs ***
  pinMode(CHURCH, OUTPUT);
  pinMode(TREE, OUTPUT);
  pinMode(HOUSES, OUTPUT);
  pinMode(STREET, OUTPUT);
  // Set the Player Busy line to input
  pinMode(DFP_CLEAR, INPUT);

  // *** Get the DFPlayer going ***
  connectDFPlayer();
  
  // Get the WiFi going
  connectWifi();

  // Get the time client going
  connectTime();

  // *** Set up the lights & sound as we wish to begin ***
  // Get the initial LDR reading
  
  // Set the lights for start up
  digitalWrite(CHURCH, HIGH);
  digitalWrite(TREE, HIGH);
  digitalWrite(HOUSES, HIGH);
  digitalWrite(STREET, HIGH);
  // Play the first mp3 - NO don't do this, it will play every hour during the night!
  // myDFPlayer.playFolder(2,1);   // Bells; i.e. 001.mp3
}

void connectDFPlayer(){
  // *** Start up the DFPlayer ***
  mySoftwareSerial.begin(9600); // We need to start up the software serial link to the DFPlayer
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  if (!myDFPlayer.begin(mySoftwareSerial,true,false)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to start DFPlayer"));
  }
  else {
    Serial.println(F("DFPlayer Mini online."));
    sound = true;  
    myDFPlayer.volume(20);  //Set volume value. From 0 to 30
  }
  return;
}

void connectWifi(){
  // *** Start up the WiFi ***
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int tries = 20;
  while ( (WiFi.status() != WL_CONNECTED) && (tries-- > 0) ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  if(WiFi.status() == WL_CONNECTED){
    wifi = true;
    Serial.println("connected");
  }
  else Serial.println("FAILED");
  return;
}

void connectTime(){
  // *** Start up the time client ***
  if(wifi){
    Serial.print("Syncing up time: ");
    waitForSync(7);
    if(timeStatus()==timeSet){
      timer = true;
      Serial.println("time synced");
    }
    else Serial.println("failed to sync time");
  }  
  return;
}

void waitforSoundEnd(){
  // Wait for sound to get playing
  delay(1000);
  // Now keep waiting until the DFPlayer indicates it's done
  while(!digitalRead(DFP_CLEAR)){
    Serial.print(".");
    delay(1000); // Let it finish      
  }
  Serial.println("");
  return;
}

void runSequence(){
  // *** Run the lights and sounds sequence ***
  Serial.println("Entering Sequence");
  digitalWrite(CHURCH, HIGH);  // Lights on in church
  
  // IF it's Christmas Eve/Day/Boxing Day, play peal of bells
  if((day()>23 && day()<27) && (month()==12)){
    myDFPlayer.playFolder(2,1);     // Play peal of bells
    Serial.print("Merry Christmas!");
    waitforSoundEnd();
  }

  // Play 1 verse of (random) carol
  myDFPlayer.playFolder(1,random(8));
  Serial.print("Playing verse of carol");
  waitforSoundEnd();
  
  // Play walking through snow
  myDFPlayer.playFolder(2,2);// Walking through snow, i.e. 002.mp3
  Serial.print("Playing snow walking");
  waitforSoundEnd();      
  
  // Lights on tree and play a whole carol
  digitalWrite(TREE, HIGH);   // Lights on round tree
  digitalWrite(CHURCH, LOW);  // Lights off in church
  // Play entire christmas carol
  myDFPlayer.play(random(15));// myDFPlayer.playFolder(1,random(5));
  Serial.print("Playing entire carol");
  waitforSoundEnd();

  // Finished, back to defaults
  digitalWrite(TREE, LOW);   // Lights off round tree
  Serial.println("Turning lights off round tree");
  digitalWrite(CHURCH, HIGH);
  return;
}

void loop() {
  // Try again if we've not got WiFi or time
  if(!wifi) connectWifi();
  if(!timer) connectTime();
  
  // IF it's after 11pm or before 6am: deep sleep until 6am
  if(timer && (hour()>SLEEPTIME-1) || (hour()<WAKETIME)){
    // Deep sleep until 6am - or as long as possible (max sleep is about 3 hours I think)
    Serial.print("It's ");
    Serial.print(dateTime("g:ia"));
    Serial.println("! Nighty nighty! zzz...");
    ESP.deepSleep(deepSleepTime);  // Currently set to one hour
  }

  // IF mp3 is still playing, let it finish. Otherwise...
  if(sound && digitalRead(DFP_CLEAR)){
    int ldrValue = analogRead(LDR); // Get the current LDR reading
    Serial.printf("%d", ldrValue);
    // IF {"someone comes near"} OR { "it goes completely dark" OR "it's on the hour" }: trigger sequence
    if(((prev_ldrValue-ldrValue)>LDR_STEP) || (ldrValue<LDR_DARK) || (minute()==0)){
      // Something's happening - trigger "the sequence"
      runSequence();
    }  
    prev_ldrValue = ldrValue;
  }

  // Report the current time once a minute
  if(second()==0){
    Serial.print("Time is now ");
    Serial.print(dateTime("g:ia"));
    Serial.print(" on ");
    Serial.println(dateTime("l jS F Y"));
    //Serial.print("Hour is: ");
    //Serial.println(hour());
  }
  else Serial.print(".");

  // Pause a bit. Note, this could be a light sleep to save power
  /*Serial.print("Now pausing for ");
  Serial.print(INTERVAL/1000);
  Serial.println(" seconds");*/
  delay(INTERVAL);
}
