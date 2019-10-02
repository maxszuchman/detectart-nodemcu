#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <Adafruit_NeoPixel.h>

#define PinLED 2

// INDICADOR SONORO //////////
#define BUZZER D2
#define NOTE 440
//////////////////////////////

// INDICADOR LUMÍNICO ////////
#define NEOPIXEL D1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, NEOPIXEL, NEO_RGB + NEO_KHZ800);
bool ledEncendido;

// COLORES /////
#define ROJO 0
#define AMARILLO 1
#define VERDE 2

// BLINK ///////
unsigned long blinkInterval = 250;
unsigned long currentMillis;
unsigned long previousMillis;
//////////////////////////////

// ALARMA SONORA /////////////
const unsigned long UN_MINUTO = 60000;
unsigned long millisAlEncenderAlarma;
unsigned long previousMillisAlarmaSonora;
bool alarmaSonoraEncendida = false;
//////////////////////////////

// ACCESS POINT //////////////
#define APSSID "Experta_Seguros"
#define APPSK  "123456789"  // 8 CARACTERES COMO MINIMO!!!
const char *APssid = APSSID;
const char *APpassword = APPSK;
//////////////////////////////

// DATOS DE RED A CONECTARSE ////
String SSID_TO_CONNECT;
String PASSWORD;
/////////////////////////////////

// ENDPOINT A ENVIAR DATOS //////
String URL;
String completeURL;

const String ENDPOINT = "/reverse/";
const String DATA = "12345";
/////////////////////////////////

// CONSTANTES DE ESTADO /////////
bool estadoNormal;
bool estadoSinConexion;
bool estadoApareamiento;
bool estadoDefectuoso;
bool estadoAlarma;
/////////////////////////////////

ESP8266WebServer server(80);
WiFiClient client;
HTTPClient http;

void setup() {
  // INICIALIZAR INDICADOR LUMÍNICO ////
  strip.setBrightness(255);
  strip.begin();
  pinMode(PinLED, OUTPUT);
  //////////////////////////////////////
  
  Serial.begin(115200);
    
  delay(4000);
  configAsAccessPoint();
  
  // setEstadoDefectuoso();
  // setEstadoAlarma();
}

void configAsAccessPoint() {

    setEstadoApareamiento();
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
        ledAmarillo();
        delay(250);
        Serial.print(WiFi.status()); 
        ledApagado();
        delay(250);
    }
    Serial.println("");
    Serial.println("WiFi conectada");

    completeURL = URL + ENDPOINT + DATA;

    setEstadoNormal();
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

  if (estadoApareamiento) {

    titilar(AMARILLO);
    server.handleClient();  
  } 
  
  else if (estadoNormal) {
    
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
        setEstadoSinConexion();
      }
  
      http.end();
  
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  
    digitalWrite(PinLED, HIGH); // Está al revés para prender el led interno
    delay(10000);
  }

  else if (estadoAlarma) {

    titilar(ROJO);
    sonarAlarmaPor(UN_MINUTO);
  }

  else if (estadoSinConexion) {
    
  }
  
}

// INDICADOR SONORO //////////////////////////////////////////

// Esto es para poder hacer sonar la alarma por más de 20 segundos sin bloquear el thread, ya que tone no funciona con más de este tiempo
void sonarAlarmaPor(int time) {

    if (!alarmaSonoraEncendida) {
      millisAlEncenderAlarma = millis();
      alarmaSonoraEncendida = true;
    } else {
      
      if ((unsigned long)(currentMillis - millisAlEncenderAlarma) >= time) {
        
        noTone(BUZZER);
        
      } else {
        
        tone(BUZZER, NOTE);
        previousMillisAlarmaSonora = millis();
      }
    }
}

//////////////////////////////////////////////////////////////

// ESTADOS INDICADOR LUMÍNICO ////////////////////////////////

// TITILADO NO BLOQUEANTE ////////
void titilar(int color) {
    
    currentMillis = millis();
    if ((unsigned long)(currentMillis - previousMillis) >= blinkInterval) {
      switchLed(color);
      previousMillis = millis();
    }
}

void switchLed(int color) {
  
  if (ledEncendido) { 
    ledApagado();
  } else {

    switch (color) {
      case ROJO: ledRojo(); break;
      case AMARILLO: ledAmarillo(); break;
      case VERDE: ledVerde(); break;
    }
    
  }
}

//////////////////////////////////

void ledRojo() {
  strip.setPixelColor(0, 0, 255, 0);    // Rojo
  strip.show();
  ledEncendido = true;
}

void ledAmarillo() {
  strip.setPixelColor(0, 200, 255, 0);  // Amarillo
  strip.show();  
  ledEncendido = true;
}

void ledVerde() {
  strip.setPixelColor(0, 255, 0, 0);    // Verde
  strip.show();
  ledEncendido = true;
}

void ledApagado() {
  strip.setPixelColor(0, 0, 0, 0);
  strip.show();  
  ledEncendido = false;
}

//////////////////////////////////////////////////////////////

// SETTERS PARA LOS ESTADOS DEL DISPOSITIVO //////////////////

void setEstadoNormal() {
  estadoNormal = true;
  estadoSinConexion = false;
  estadoApareamiento = false;
  estadoDefectuoso = false;
  estadoAlarma = false;

  // CU 11
  ledVerde();
}

void setEstadoSinConexion() {
  estadoNormal = false;
  estadoSinConexion = true;
  estadoApareamiento = false;
  estadoDefectuoso = false;
  estadoAlarma = false;

  // CU 12
  ledAmarillo();
  
  // CU 16
  tone(BUZZER, NOTE, 5000);
}

void setEstadoApareamiento() {
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = true;
  estadoDefectuoso = false;
  estadoAlarma = false;
}

void setEstadoDefectuoso() {
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = false;
  estadoDefectuoso = true;
  estadoAlarma = false;

  // CU 14
  ledRojo();
    
  // CU 17
  tone(BUZZER, NOTE, 10000);
}

void setEstadoAlarma() {
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = false;
  estadoDefectuoso = false;
  estadoAlarma = true;
}

///////////////////////////////////////////////////////////////
