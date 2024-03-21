#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

/* START CUSTOM PARAMS */

//Define parameters for the http firmware update
const char* host = "GarageESP";
const char* update_path = "/WebFirmwareUpgrade";
const char* update_username = "admin";
const char* update_password = "adminpassword";

//Define the pins
const int DOOR_PIN = 5;
const int RELAY_PIN = 4;

/* END CUSTOM PARAMS */

// Setup the web server for http OTA updates. 
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
void handleRoot();
void handleNotFound();  
WiFiClient espClient;

// Setup Variables
String strPayload;
String door_state = "UNDEFINED";
String last_state = "";

// Wifi Manager will try to connect to the saved AP. If that fails, it will start up as an AP
WiFiManager wifiManager;
long lastMsg = 0;

void setup() {
  // Set Relay(output) and Door(input) pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(DOOR_PIN, INPUT);

  Serial.begin(115200);

  // Set the wifi config portal to only show for 3 minutes, then continue.
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect(host, update_password);

  // setup http firmware update page.
  MDNS.begin(host);
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);

  // setup http end points
  httpServer.on("/", HTTP_GET, handleRoot); 
  httpServer.on("/status", HTTP_GET, handleStatus);
  httpServer.on("/activate", HTTP_POST, handleActivate); 
  httpServer.onNotFound(handleNotFound);
  
  httpServer.begin();
  Serial.println("HTTP server started");
  
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and your password\n", host, update_path, update_username);
}

void loop() {
  checkDoorState();
  httpServer.handleClient();
}

void handleRoot() {
  String txt = "";
  txt += "<!DOCTYPE html>\n";
  txt += "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Garage Door Status</title>\n";
  txt += "<style>\n";
  txt += "  body {\n";
  txt += "    font-family: Arial, sans-serif;\n";
  txt += "    text-align: center;\n";
  txt += "  }\n";
  txt += "  button {\n";
  txt += "    background-color: lightgray;\n";
  txt += "    cursor: pointer;\n";
  txt += "    padding: 15px 20px;\n";
  txt += "    font-size: 16px;\n";
  txt += "  }\n";
  txt += "</style>\n";
  txt += "<script>\n";
  txt += "var refresh;\n";
  txt += "function updateStatus() { \n";
  txt += "  fetch('/status') \n";
  txt += "  .then(response => response.json())\n";
  txt += "  .then(msg => {\n";
  txt += "    document.getElementById('status').textContent = msg.door_state;\n";
  txt += "    if (msg.door_state == 'OPEN') { \n";
  txt += "      document.getElementById('closed-path').style.display = 'none';\n";
  txt += "    } else { \n";
  txt += "      document.getElementById('closed-path').style.display = 'block';\n";
  txt += "    }\n";
  txt += "  })\n";
  txt += "  .catch(error => { \n";
  txt += "    document.getElementById('status').textContent = 'Status Error';\n";
  txt += "  });\n";
  txt += "}\n";
  txt += "function activateDoor() { \n";
  txt += "  clearInterval(refresh);\n";
  txt += "  fetch('/activate', { method: 'POST' }) \n";
  txt += "  .then(response => response.json()) \n";
  txt += "  .then(msg => { \n";
  txt += "    document.getElementById('status').textContent = msg.door_state; \n";
  txt += "  }) \n";
  txt += "  .catch(error => { \n";
  txt += "    document.getElementById('status').textContent = 'Activate Error'; \n";
  txt += "  }); \n";
  txt += "  refresh = setInterval(updateStatus, 15000);\n";
  txt += "}; \n";
  txt += "updateStatus(); \n";
  txt += "refresh = setInterval(updateStatus, 15000); \n";
  txt += "</script></head><body>\n";
  txt += "<h1>Garage Door Status</h1>\n";
  txt += "<div id='status-container'>\n";
  txt += "  <svg id='garage-door' style='width: 200px;' role='img' viewBox='0 0 24 24'><g><path id='closed-path' style='display:block;' class='primary-path' d='M22 9V20H20V11H4V20H2V9L12 5L22 9M19 12H5V14H19V12M19 18H5V20H19V18M19 15H5V17H19V15Z'></path>\n";
  txt += "<path id='open-path' style='display:block;' class='primary-path' d='M22 9V20H20V11H4V20H2V9L12 5L22 9M19 12H5V14H19V12Z'></path></g></svg>\n";
  txt += "<p id='status' style='font-size: 16px;margin: 20px 10px;'>Loading...</p>\n";
  txt += "<button id='activate-btn' onclick='activateDoor();' style='width: 200px;'>Activate Door</button>\n";
  txt += "</div></body></html>";

  httpServer.send(200, "text/html", txt);
}

void handleStatus() {
  httpServer.send(200, "application/json", "{\"door_state\":\"" + door_state + "\"}");
}

void handleActivate() {
  last_state = door_state;
  activateDoor();
  if (last_state == "OPEN") {
    door_state = "CLOSING";
  } else if (last_state == "CLOSED") {
    door_state = "OPENING";
  } else {
    door_state = "UNKNOWN";
  }
  httpServer.send(200, "application/json", "{\"door_state\":\"" + door_state + "\"}");
}

void handleNotFound(){
  httpServer.send(404, "text/plain", "404: Uh oh, there is nobody home at this endpoint");
}

void checkDoorState() {
  //Checks if the door state has changed
  last_state = door_state;
  if (digitalRead(DOOR_PIN) == 0) {
    door_state = "OPEN";
  } else if (digitalRead(DOOR_PIN) == 1) {
    door_state = "CLOSED"; 
  }
    
  if (last_state != door_state) {
    Serial.println(door_state);
  }
}

void activateDoor() {
  // 'click' the relay
  Serial.println("Activating door");
  digitalWrite(RELAY_PIN, HIGH);
  delay(600);
  digitalWrite(RELAY_PIN, LOW);
}
