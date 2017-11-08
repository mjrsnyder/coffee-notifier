#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

//for talking to IFTTT
#include <IFTTTMaker.h>           //https://github.com/witnessmenow/arduino-ifttt-maker

//for getting the time
#include <time.h>

//for the oled
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

//for LED status
#include <Ticker.h>
Ticker ticker;

int freshSwitchPin = D1;
int emptySwitchPin = D2;

volatile byte freshInterruptCounter = 0;
volatile byte emptyInterruptCounter = 0;
int numberOfInterrupts = 0;

unsigned long lastFreshTime = 0;
unsigned long timeSinceFresh = 0;
bool isEmpty = true;

#define KEY ""
WiFiClientSecure client; //For ESP8266 boards
IFTTTMaker ifttt(KEY, client);

SSD1306  display(0x3c, D6, D5);

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  display.clear();
  display.drawString(DISPLAY_WIDTH/2, 0, "Entering");
  display.drawString(DISPLAY_WIDTH/2, 16, "config mode");
  display.drawString(DISPLAY_WIDTH/2, 32, myWiFiManager->getConfigPortalSSID());
  display.display();

  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

time_t getCurrentTime() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  time_t now = time(nullptr);
  return now;
}

byte handleButtonInterrupt(byte counter) {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    counter++;
  }
  last_interrupt_time = interrupt_time;
  return counter;
}

void handleFreshInterrupt() {
  freshInterruptCounter = handleButtonInterrupt(freshInterruptCounter);
}

void handleEmptyInterrupt() {
  emptyInterruptCounter = handleButtonInterrupt(emptyInterruptCounter);
}

void displayStartNotification(){
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, 16, "Sending");
  display.drawString(DISPLAY_WIDTH/2, 32, "notification");
  display.drawString(DISPLAY_WIDTH/2, 48, "...");
  display.display();
}

void displaySuccessNotification(){
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(DISPLAY_WIDTH/2, 0, "Thanks!");
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, 32, "Notification");
  display.drawString(DISPLAY_WIDTH/2, 48, "sent!");
  display.display();
}

void displayFailedNotification(){
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, 0, "Notification");
  display.drawString(DISPLAY_WIDTH/2, 16, "failed :(");
  display.drawString(DISPLAY_WIDTH/2, 48, "Thanks anyway!");
  display.display();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 16, "Connecting");
  display.display();

  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);

  pinMode(freshSwitchPin, INPUT_PULLUP);
  pinMode(emptySwitchPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(freshSwitchPin), handleFreshInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(emptySwitchPin), handleEmptyInterrupt, FALLING);

  display.clear();
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 16, "Connected!");
  display.display();
  delay(2000);
}

void loop() {
  // put your main code here, to run repeatedly:

  if(freshInterruptCounter>0){
    isEmpty = false;
    freshInterruptCounter--;
    numberOfInterrupts++;

    Serial.print("Fresh :) Total: ");
    Serial.println(numberOfInterrupts);

    displayStartNotification();

    if (ifttt.triggerEvent("coffee_full")) {
      Serial.println("Trigger sent to IFTTT");

      displaySuccessNotification();

    } else {
      Serial.println("Failed triggering IFTTT");

      displayFailedNotification();
    }
    lastFreshTime = millis();

    delay(3000);
  }

  if(emptyInterruptCounter>0){
    isEmpty = true;
    emptyInterruptCounter--;
    numberOfInterrupts++;

    Serial.print("Empty :( Total: ");
    Serial.println(numberOfInterrupts);

    displayStartNotification();
    if (ifttt.triggerEvent("coffee_empty")) {
      Serial.println("Trigger sent to IFTTT");

      displaySuccessNotification();
    } else {
      Serial.println("Failed triggering IFTTT");

      displayFailedNotification();
    }

    delay(3000);
  }


  if (lastFreshTime != 0) {
    timeSinceFresh = millis() - lastFreshTime;
  }

  if (numberOfInterrupts != 0 && isEmpty == true) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(DISPLAY_WIDTH/2, 0, "Empty!");
    display.setFont(ArialMT_Plain_16);
    display.drawString(DISPLAY_WIDTH/2, 24, "Please make");
    display.drawString(DISPLAY_WIDTH/2, 40, "more :)");
    display.display();
  } else if (timeSinceFresh > 1000 * 60 * 60 * 4 && isEmpty == false) {
    // if the coffee is > 4 hours old we probably don't want it
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(DISPLAY_WIDTH/2, 0, "Old!");
    display.setFont(ArialMT_Plain_16);
    display.drawString(DISPLAY_WIDTH/2, 24, "Coffee made a");
    display.drawString(DISPLAY_WIDTH/2, 40, "long time ago.");
    display.display();
  } else if (lastFreshTime != 0 && isEmpty == false){
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(DISPLAY_WIDTH/2, 0, "Coffee made");
    display.setFont(ArialMT_Plain_24);
    display.drawString(DISPLAY_WIDTH/2, 16, String(round(timeSinceFresh/60000)));
    display.setFont(ArialMT_Plain_16);
    display.drawString(DISPLAY_WIDTH/2, 40, "minutes ago");
    display.display();
  } else {
    // We don't know when coffee was made last
    // the program was probably restarted
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(DISPLAY_WIDTH/2, 16, "Not sure when");
    display.drawString(DISPLAY_WIDTH/2, 32, "coffee was made");
    display.display();
  }
}
