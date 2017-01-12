#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "config.h"
#include <TimeLib.h>
#include <WiFiUdp.h>

// NTP Servers:
static const char ntpServerName[] = "0.au.pool.ntp.org";
const int timeZone = 0;   // GMT

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

/////////////////////
// Pin Definitions //
/////////////////////
#define PO_ENERGIZE   D2
#define PO_OPEN       D7
#define PI_OPEN       D0
#define PI_CLOSED     D1

#define getLightLevel() analogRead(A0)
#define isOpen() (digitalRead(PI_OPEN) == 0)
#define isClosed() (digitalRead(PI_CLOSED) == 0)


#define DBG_OUTPUT_PORT Serial


const char WiFiAPPSK[] = WIFI_AP_SECRET;
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* host       = HOSTNAME;

ESP8266WebServer server(SERVER_PORT);

void openDoor() {
  int timeout = 4000;
  
  if(isOpen())
    return;

  digitalWrite(PO_OPEN, 1);
  digitalWrite(PO_ENERGIZE, 1);
  while( !isOpen() && timeout-- ) {
    delay(1);  
  }
  digitalWrite(PO_ENERGIZE, 0);
  digitalWrite(PO_OPEN, 0);  
}

void closeDoor() {
  int timeout = 4000;
  
  if(isClosed())
    return;

  digitalWrite(PO_OPEN, 0);
  digitalWrite(PO_ENERGIZE, 1);
  while( !isClosed() && timeout-- ) {
    delay(1);  
  }
  digitalWrite(PO_ENERGIZE, 0);
  digitalWrite(PO_OPEN, 0);  
}

void handleOpen() {
  DBG_OUTPUT_PORT.println("Open Door....");
  openDoor();
  DBG_OUTPUT_PORT.println("Done.");
  server.send(200, "text/json", "OK");
}

void handleClose() {
  DBG_OUTPUT_PORT.println("Close Door....");
  closeDoor();
  DBG_OUTPUT_PORT.println("Done.");
  server.send(200, "text/json", "OK");
}

void handleGetLightLevel() {
  DBG_OUTPUT_PORT.println("handleGetLightLevel");
  String output = "{ \"level\": ";
  output += getLightLevel();
  output += " }";
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
  server.onNotFound([](){
      server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(1800);

}


void initHardware()
{
  DBG_OUTPUT_PORT.begin(115200);
  pinMode(PO_OPEN,     OUTPUT);
  digitalWrite(PO_OPEN, 0);
  pinMode(PO_ENERGIZE, OUTPUT);
  digitalWrite(PO_ENERGIZE, 0);

  pinMode(PI_OPEN,     INPUT_PULLUP);
  pinMode(PI_CLOSED,   INPUT_PULLUP);
}

void setup()
{
  initHardware();
  setupWiFi();
}

void loop()
{
  server.handleClient();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}



