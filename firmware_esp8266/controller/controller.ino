#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "config.h"

/////////////////////
// Pin Definitions //
/////////////////////
#define PO_ENERGIZE   D3
#define PO_DIRECTION  D7
#define PI_OPEN       D2
#define PI_CLOSED     D1
#define PO_IND        D4

#define getLightLevel() analogRead(A0)
#define isOpen() (digitalRead(PI_OPEN) == 0)
#define isClosed() (digitalRead(PI_CLOSED) == 0)
#define indOn() digitalWrite(PO_IND, 0)
#define indOff() digitalWrite(PO_IND, 1)

#define DBG_OUTPUT_PORT Serial

const char WiFiAPPSK[] = WIFI_AP_SECRET;
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* host       = HOSTNAME;

unsigned int cnt = 34000000;
int isIndOn = 0;

ESP8266WebServer server(SERVER_PORT);

int openDoor() {
  int timeout = 4000;
  
  if(isOpen())
    return 0;

  digitalWrite(PO_DIRECTION, 0);
  digitalWrite(PO_ENERGIZE, 1);
  while( !isOpen() && timeout-- ) {
    delay(1);  
  }
  digitalWrite(PO_ENERGIZE, 0);
  digitalWrite(PO_DIRECTION, 0);  

  return (timeout > 0 ? 0 : 1);
}

int closeDoor() {
  int timeout = 4000;
  
  if(isClosed())
    return 0;

  digitalWrite(PO_DIRECTION, 1);
  digitalWrite(PO_ENERGIZE, 1);
  while( !isClosed() && timeout-- ) {
    delay(1);  
  }
  digitalWrite(PO_ENERGIZE, 0);
  digitalWrite(PO_DIRECTION, 0);  

  return (timeout > 0 ? 0 : 1);
}

void handleOpen() {
  DBG_OUTPUT_PORT.println("Open Door....");
  openDoor();
  DBG_OUTPUT_PORT.println("Done.");
  handleGetStatus();
}

void handleClose() {
  DBG_OUTPUT_PORT.println("Close Door....");
  closeDoor();
  DBG_OUTPUT_PORT.println("Done.");
  handleGetStatus();
}

void handleGetLightLevel() {
  DBG_OUTPUT_PORT.println("handleGetLightLevel");
  handleGetStatus();
}

void handleGetStatus() {
  String callbackFn = server.hasArg("callback") ? server.arg("callback") : "";
 
  DBG_OUTPUT_PORT.println("handleGetStatus");
  String output = "";

  // JSONP function wrapper
  if (callbackFn.length()) {
    output += callbackFn;
    output += "(";
  }

  // Response body
  output += "{ \"lightlevel\": ";
  output += getLightLevel();
  output += ", \"door\": ";
  output += (isOpen() ? "\"open\"" : (isClosed() ? "\"closed\"" : "\"unknown\"") ); 
  output += ", \"tmr\": ";
  output += cnt;
  output += " }";
  
  // JSONP function wrapper
  if (callbackFn.length()) {
    output += ");";
  }

  output += "\n";
  
  server.send(200, "text/json", output);
}


void setupWiFi()
{
  //WIFI INIT
  WiFi.mode(WIFI_STA);
  delay(300);
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
  }
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

  server.on("/open",   HTTP_GET, handleOpen);
  server.on("/close",  HTTP_GET, handleClose);
  server.on("/level",  HTTP_GET, handleGetLightLevel);
  server.on("/stat",   HTTP_GET, handleGetStatus);
  server.onNotFound([](){
      server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
}

void initHardware()
{
  DBG_OUTPUT_PORT.begin(115200);
  pinMode(PO_DIRECTION,     OUTPUT);
  digitalWrite(PO_DIRECTION, 0);
  pinMode(PO_ENERGIZE, OUTPUT);
  digitalWrite(PO_ENERGIZE, 0);
  pinMode(PO_IND, OUTPUT);
  indOff();
  
  pinMode(PI_OPEN,     INPUT);
  pinMode(PI_CLOSED,   INPUT);
}

void setup()
{
  initHardware();
  setupWiFi();
}

void loop()
{
  server.handleClient();

  // Test light level every 5 minutes
  if (cnt-- == 0) {
    cnt = 34000000;

    if (isIndOn) {
      indOff();
      isIndOn = 0;
    } else {
      indOn();
      isIndOn = 1;
    }
    
    if (getLightLevel() > 20) {
      openDoor();
    } else {
      closeDoor();
    }

  }
}




