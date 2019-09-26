#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"

#define PinLED 2

#define APSSID "Experta_Seguros"
#define APPSK  "123456789"  // 8 CARACTERES COMO MINIMO!!!

const char *APssid = APSSID;
const char *APpassword = APPSK;
  
String SSID_TO_CONNECT;
String PASSWORD;
String URL;
String completeURL;

const String ENDPOINT = "/reverse/";
const String DATA = "12345";

bool isAccessPoint;
bool isClient = false;

ESP8266WebServer server(80);
WiFiClient client;
HTTPClient http;

void setup() {
    pinMode(PinLED, OUTPUT);
    Serial.begin(115200);
    
    delay(2000);

    configAsAccessPoint();
}

void configAsAccessPoint() {

    isAccessPoint = true;    
    Serial.println("Configuring access point...");
    WiFi.softAP(APssid, APpassword);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access Point IP address: ");
    Serial.println(myIP);
    
    server.on("/connectionData/", HTTP_POST, handleConnectionData);
    server.begin();
    Serial.println("HTTP server started");
}

void handleConnectionData() {
  String ssid, password;
  
  if (server.args() != 2) {
    return returnFail("BAD ARGS");
  }

  if (server.argName(0) == "ssid") {
    ssid = server.arg(0);  
  } else {
    return returnFail("BAD ARGS");
  }

  if (server.argName(1) == "password") {
    password = server.arg(1);
  } else {
    return returnFail("BAD ARGS");
  }
  
  server.send(200, "text/plain", "OK");
  saveConfig(ssid, password);
  
  Serial.println();
  Serial.println("Data received, switching to Client Mode...");

  WiFi.softAPdisconnect(true);
  isAccessPoint = false;
  connectToWiFi();
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

void connectToWiFi() {

    if (!loadConfig()) {
      Serial.println("Unable to read config.json");
      return;
    }
    // showAvailableNetworks();
        
    Serial.println();
    Serial.printf("SSID to connect to: %s\n", SSID_TO_CONNECT.c_str());
    Serial.printf("Password: %s\n", PASSWORD.c_str());
    Serial.printf("Sending data to: %s\n", URL.c_str());

    WiFi.disconnect(true);
    WiFi.begin(SSID_TO_CONNECT.c_str(), PASSWORD.c_str());
  
    Serial.printf("\nConectando a la red: %s\n", WiFi.SSID().c_str());
    
    while (WiFi.status() != WL_CONNECTED) { 
        digitalWrite(PinLED, LOW);
        delay(250);
        Serial.print(WiFi.status()); 
        digitalWrite(PinLED, HIGH);
        delay(250);
    }
    Serial.println("");
    Serial.println("WiFi conectada");

    completeURL = URL + ENDPOINT + DATA;

    isClient = true;
}

bool saveConfig(String ssid, String password) {
  Serial.println("Mounting File System...");

  if (!SPIFFS.begin()) {
    Serial.println("Unable to mount File System.");
    return false;
  }
  
  StaticJsonDocument<200> json;
  
  json["ssid"] = ssid;
  json["password"] = password;
  json["url"] = "http://tranquil-headland-60104.herokuapp.com";
  
  Serial.println("\nSaving connection data into memory:");
  serializeJsonPretty(json, Serial);

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  serializeJson(json, configFile);
  return true;
}

bool loadConfig() {

  Serial.println("Mounting File System...");

  if (!SPIFFS.begin()) {
    Serial.println("Unable to mount File System.");
    return false;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file.");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file is too large.");
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.print("Failed to parse json file, with error code ");
    Serial.println(error.c_str());
    return false;
  }

  // Workaround para el tema de manejo de arrays de caracteres y strings
  const char* ssid = doc["ssid"];
  const char* password = doc["password"];
  const char* url = doc["url"];
  SSID_TO_CONNECT = String(ssid);
  PASSWORD = String(password);
  URL = String(url);
  
  return true;
}

void showAvailableNetworks() {

    Serial.println("Scanning for available networks... ");
    int n = WiFi.scanNetworks();
    
    Serial.print(n);
    Serial.println(" network(s) found");
    Serial.println();
    
    for (int i = 0; i < n; i++) {
        Serial.println(WiFi.SSID(i));
    }
    
    Serial.println();
}

void loop() 
{

  if (isAccessPoint) {
    server.handleClient();  
  } 
  else if (isClient) {
    
    digitalWrite(PinLED, LOW); // Está al revés para prender el led interno
    
    if (http.begin(client, completeURL.c_str())) {  // HTTP
  
      Serial.println("\n[HTTP] Sending a GET request to " + completeURL);
      
      // start connection and send HTTP header
      int httpCode = http.GET();
  
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] Response Status Code: %d\n", httpCode);
  
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println("[HTTP] Response: " + payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
  
      http.end();
  
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  
    digitalWrite(PinLED, HIGH); // Está al revés para prender el led interno
    delay(10000);
  }
}
