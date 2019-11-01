#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <Adafruit_NeoPixel.h>

#define PinLED 2

// CONSTANTES TEMPORALES /////
const unsigned long UN_MINUTO = 60000;
const unsigned long CINCO_SEGUNDOS = 5000;
const unsigned long TRES_SEGUNDOS = 3000;
//////////////////////////////

// PULSADOR //////////////////
#define PULSADOR D6
bool pulsadorEncendido;
unsigned long tiempoInicioPulsadorEncendido;
//////////////////////////////

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
unsigned long millisAlEncenderAlarma;
unsigned long previousMillisAlarmaSonora;
bool alarmaSonoraEncendida = false;
//////////////////////////////

// ACCESS POINT //////////////
#define APSSID "EXPERTA_"
#define APPSK  "123456789"  // 8 CARACTERES COMO MINIMO!!!
char APssid[40];
const char *APpassword = APPSK;
byte mac[6];
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

// MULTIPLEXOR //////////////////
const int muxSIG = A0;
const int muxS0 = D5;
const int muxS1 = D6;
const int muxS2 = D7;
const int muxS3 = D8;
const int SENSORS_LENGTH = 2;

int sensors[] = {0,0};
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
  ledVerde();   // Inicializamos el led en el color "todo está bien"
  //////////////////////////////////////

  // INICIALIZAR MULTIPLEXOR PARA LEER SENSORES //
  pinMode(muxS0, OUTPUT);
  pinMode(muxS1, OUTPUT);
  pinMode(muxS2, OUTPUT);
  pinMode(muxS3, OUTPUT);
  ////////////////////////////////////////////////
   
  pinMode(PULSADOR, INPUT);

  Serial.begin(115200);
  
  delay(1000);
  
  setEstadoApareamiento();
  // setEstadoDefectuoso();
  // setEstadoAlarma();
  // setEstadoSinConexion();
  // setEstadoNormal();
  
}

void loop() {

  if (estadoApareamiento) {

    titilar(AMARILLO);
    server.handleClient();  
  } 
  
  else if (estadoNormal) {

    if (pulsadorEncendidoPor(TRES_SEGUNDOS)) {
      setEstadoApareamiento();
      return; // Salimos del loop para que tome el cambio de estado y entre en el flujo de estadoApareamiento
    }

    leerSensores();
        
    digitalWrite(PinLED, LOW); // Está al revés para prender el led interno
    enviarDatosAlServidor();
    digitalWrite(PinLED, HIGH); // Está al revés para prender el led interno
    delay(10000);
    
  }

  else if (estadoAlarma) {

    titilar(ROJO);
    sonarAlarmaPor(UN_MINUTO);
    
    if (pulsadorEncendidoPor(TRES_SEGUNDOS)) {
      setEstadoNormal();
    }
  }

  else if (estadoSinConexion) {

    if (pulsadorEncendidoPor(TRES_SEGUNDOS)) {
      noTone(BUZZER);
      setEstadoApareamiento();
      return; // Salimos del loop para que tome el cambio de estado y entre en el flujo de estadoApareamiento
    }
  }
  
}

void enviarDatosAlServidor() {
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
      Serial.printf("[HTTP] Unable to connect\n");
      
      // CU 16
      tone(BUZZER, NOTE, 5000);
      setEstadoSinConexion();
    }
  
}

void configAsAccessPoint() {

    WiFi.macAddress(mac);
    String macString = macToString(mac);
    char macCharArr[19];
    macString.toCharArray(macCharArr, 19);
    
    strcpy(APssid, APSSID);
    strcat(APssid, macCharArr);
    
    Serial.println("Configuring access point...");
    WiFi.softAP(APssid, APpassword);

    Serial.print("Access Point SSID: ");
    Serial.println(APssid);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access Point IP address: ");
    Serial.println(myIP);
    
    Serial.print("MAC Address: ");
    Serial.print(mac[5],HEX);
    Serial.print(":");
    Serial.print(mac[4],HEX);
    Serial.print(":");
    Serial.print(mac[3],HEX);
    Serial.print(":");
    Serial.print(mac[2],HEX);
    Serial.print(":");
    Serial.print(mac[1],HEX);
    Serial.print(":");
    Serial.println(mac[0],HEX);
    
    server.on("/connectionData/", HTTP_POST, handleConnectionData);
    server.begin();
    Serial.println("HTTP server started");
}

String macToString(byte ar[]) {
  String s;
  for (int i = 5; i >= 0; --i)
  {
    char buf[3];
    sprintf(buf, "%2X", ar[i]);
    s += buf;
    if (i > 0) s += ':';
  }
  return s;
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
  json["url"] = "https://still-shelf-00010.herokuapp.com";
  
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


// MULTIPLEXOR ///////////////////////////////////////////////

void setMuxChannel(byte channel) {
   digitalWrite(muxS0, bitRead(channel, 0));
   digitalWrite(muxS1, bitRead(channel, 1));
   //digitalWrite(muxS2, bitRead(channel, 2));
   //digitalWrite(muxS3, bitRead(channel, 3));
}

void leerSensores() {
    
    for (byte i = 0; i < SENSORS_LENGTH; i++) {
        setMuxChannel(i);
        sensors[i] = analogRead(muxSIG);
        Serial.print(sensors[i]);
        Serial.print("\t");
    }
    Serial.print("\n");
}

int getCO() {
  return sensors[0];
}

int getGas() {
  return sensors[1];
}
//////////////////////////////////////////////////////////////

// PULSADOR //////////////////////////////////////////////////

boolean pulsadorEncendidoPor(int time) {

  if (digitalRead(PULSADOR) == LOW) {
    pulsadorEncendido = false;
    return pulsadorEncendido;
  }

  if (!pulsadorEncendido) {
    // Se encendió el pulsador por primera vez
    tiempoInicioPulsadorEncendido = millis();
    pulsadorEncendido = true;

  } else {

    // Chequeamos si el tiempo que el pulsador estuvo apretado es igual o mayor al tiempo que queremos medir
    if ((unsigned long)(millis() - tiempoInicioPulsadorEncendido) >= time) {
      
      pulsadorEncendido = false;  // Reseteamos la variable aún si la persona está apretando el botón, por consistencia
      return true;
    }
  }

  return false;
}

//////////////////////////////////////////////////////////////

// INDICADOR SONORO //////////////////////////////////////////

void apagarAlarmaSonora() {
  alarmaSonoraEncendida = false;  
}

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
  noTone(BUZZER);   // Apagar alarma si suena
}

void setEstadoSinConexion() {
  estadoNormal = false;
  estadoSinConexion = true;
  estadoApareamiento = false;
  estadoDefectuoso = false;
  estadoAlarma = false;

  // CU 12
  ledAmarillo();  
}

void setEstadoApareamiento() {
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = true;
  estadoDefectuoso = false;
  estadoAlarma = false;

  // Ya hacemos titilar el led, ya que configurar como access point lleva un tiempo, y sino obligamos al usuario a seguir apretando el pulsador
  titilar(AMARILLO);
  tone(BUZZER, NOTE, 200);
  configAsAccessPoint();
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
