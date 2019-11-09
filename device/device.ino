#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <Adafruit_NeoPixel.h>
#include "WifiLocation.h"

#define PinLED 2

// CONSTANTES TEMPORALES /////
const unsigned long UN_MINUTO = 60000;
const unsigned long CINCO_SEGUNDOS = 5000;
const unsigned long TRES_SEGUNDOS = 3000;
//////////////////////////////

// DELAY NO BLOQUEANTE ///////
bool esperando;
unsigned long tiempoInicioEspera;
//////////////////////////////

// PULSADOR //////////////////
#define PULSADOR D3
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
bool alarmaSonoraCanceladaPorUsuario = false;
//////////////////////////////

// ACCESS POINT //////////////
#define APSSID "EXPERTA_"
#define APPSK  "123456789"  // 8 CARACTERES COMO MINIMO!!!
#define CHANNEL 10

char APssid[40];
const char *APpassword = APPSK;
byte mac[6];
String macString;
//////////////////////////////

// DATOS DE RED A CONECTARSE ////
String SSID_TO_CONNECT;
String PASSWORD;
/////////////////////////////////

// CONFIGURACIÓN CLIENTE   //////
const String URL = "http://still-shelf-00010.herokuapp.com/deviceData/";
const int MAX_NUM_OF_CONNECTING_RETRIES = 30;
const int CONNECTING_RETRIES_EN_ESTADO_SIN_CONEXION = 12;
const unsigned long ESPERA_ENTRE_ENVIO_DE_DATOS = 10000;
/////////////////////////////////

// API GEOLOCALIZACIÓN GOOGLE ///
// IMPORTANTE: Versión gratiuta admite 2500 consultas x día.
// https://console.cloud.google.com/apis/credentials?project=experta-c43ae para configurar la api key
String GOOGLE_API_KEY = "AIzaSyB25W77r8umo8iaBvYktNLFfoqr74SxFaw";
/////////////////////////////////

// MULTIPLEXOR //////////////////
const int muxSIG = A0;
const int muxS0 = D5;
const int muxS1 = D6;
const int muxS2 = D7;
const int muxS3 = D8;
const int SENSORS_LENGTH = 2;
/////////////////////////////////

// POSICION /////////////////////
  double latitude = 0.0;
  double longitude = 0.0;
  double accuracy = 0.0;
/////////////////////////////////

// SENSORES //////////////////////////
int sensors[] = {0,0};
#define S_HUMO D4

// UMBRALES ///////////////
const int umbralCO = 1000;
const int umbralGAS = 950;

// CONSTANTES DE ESTADO DE SENSORES //
const char *NORMAL = "NORMAL";
const char *ALARM = "ALARM";
//////////////////////////////////////

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
  // PinMode HUMO
  pinMode(S_HUMO, INPUT);

  // CONFIGURAR NOMBRE DE SSID DINÁMICAMENTE Y TOMAR LA MAC COMO STRING
  configSSID();
  
  Serial.begin(115200);
  
  delay(1000);

  beep();
// Cargamos json con los datos de conexión de la memoria persistente e intentamos conectar a Internet
  loadJsonAndConnectToWiFi();

  // setEstadoApareamiento();
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
    if (getGeneralStatus().equals(ALARM)) {
      setEstadoAlarma();
      return;
    }
        
    digitalWrite(PinLED, LOW); // Está al revés para prender el led interno
    if (!enviarDatosAlServidor()) {
      setEstadoSinConexion();
      return;
    }
    digitalWrite(PinLED, HIGH); // Está al revés para prender el led interno

    while (esperar(ESPERA_ENTRE_ENVIO_DE_DATOS)) {
      delay(10);  // Breve delay para no saturar el micro
      
      if (pulsadorEncendidoPor(TRES_SEGUNDOS)) {
        setEstadoApareamiento();
        esperando = false;
        break;
      }
    }
  }

  else if (estadoAlarma) {

    leerSensores();
    if (getGeneralStatus().equals(NORMAL)) {
      setEstadoNormal();
      return;
    }

    digitalWrite(PinLED, LOW); // Está al revés para prender el led interno
    enviarDatosAlServidor();
    digitalWrite(PinLED, HIGH); // Está al revés para prender el led interno

    while (esperar(ESPERA_ENTRE_ENVIO_DE_DATOS)) {
      delay(10);  // Breve delay para no saturar el micro

      titilar(ROJO);

      if (!alarmaSonoraCanceladaPorUsuario) {
        tone(BUZZER, NOTE);
      }
      
      if (pulsadorEncendidoPor(CINCO_SEGUNDOS)) {
        alarmaSonoraCanceladaPorUsuario = true;
        noTone(BUZZER);
      }
    }
  }

  else if (estadoSinConexion) {

    if (pulsadorEncendidoPor(TRES_SEGUNDOS)) {
      noTone(BUZZER);
      setEstadoApareamiento();
      return; // Salimos del loop para que tome el cambio de estado y entre en el flujo de estadoApareamiento
    }

    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi(CONNECTING_RETRIES_EN_ESTADO_SIN_CONEXION, false);
    } else {
      
      if (enviarDatosAlServidor()) {
        setEstadoNormal();
      }

      delay(TRES_SEGUNDOS);
    }
  }
}

void loadJsonAndConnectToWiFi() {
  ledAmarillo();  // Sin conexión
  
  if (loadConfig() && connectToWiFi(MAX_NUM_OF_CONNECTING_RETRIES, false)) {
    getGoogleGeolocation();
    setEstadoNormal();  
  } else {
    setEstadoApareamiento();
  }
}

boolean enviarDatosAlServidor() {

    Serial.println("\n[HTTP] Sending a POST request to " + URL);
    
    if (http.begin(client, URL.c_str())) {  // HTTP
  
      http.addHeader("Content-Type", "application/json");
      String body = generarJson();

      Serial.println(body);
      
      // start connection and send HTTP header
      int httpCode = http.POST(body);
  
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] Response Status Code: %d\n", httpCode);
  
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println("[HTTP] Response: " + payload);
        } else if (httpCode == HTTP_CODE_NOT_FOUND) {
          Serial.println("[HTTP] POST Devolvió HTTP Status 404 Not Found, el dispositivo no fue vinculado a un User todavía.");
        }
        
      } else {

        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        return false;
      }
  
      http.end();
  
    } else {
      Serial.printf("[HTTP] Unable to connect\n");
      
      return false;
    }

    return true;
}

void configAsAccessPoint() {

    WiFi.disconnect(true);
    
    Serial.println("Configuring access point...");
    WiFi.softAP(APssid, APpassword, CHANNEL, false);

    Serial.print("Access Point SSID: ");
    Serial.println(APssid);
  
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access Point IP address: ");
    Serial.println(myIP);
    
    Serial.print("MAC Address: ");
    for (int i = 0; i <= 5; i++) {
      Serial.print(mac[i],HEX);  
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    
    server.on("/connectionData/", HTTP_POST, handleConnectionData);
    server.begin();
    Serial.print("HTTP server started. POST to ");
    Serial.print(myIP);
    Serial.println("/connectionData/");
}

String macToString(byte ar[]) {
  String s;
  for (int i = 0; i <= 5; ++i)
  {
    char buf[3];
    sprintf(buf, "%2X", ar[i]);
    s += buf;
    if (i < 5) s += ':';
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

  loadJsonAndConnectToWiFi();
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

boolean connectToWiFi(int retries, boolean titilar) {

    showAvailableNetworks();
        
    Serial.println();
    Serial.printf("SSID to connect to: %s\n", SSID_TO_CONNECT.c_str());
    Serial.printf("Password: %s\n", PASSWORD.c_str());
    Serial.printf("Sending data to: %s\n", URL.c_str());

    WiFi.setPhyMode( WIFI_PHY_MODE_11B ); // WIFI_PHY_MODE_11G / WIFI_PHY_MODE_11N 
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.begin(SSID_TO_CONNECT.c_str(), PASSWORD.c_str());
  
    Serial.printf("\nConectando a la red: %s\n", WiFi.SSID().c_str());

    int retryNum = 1;
    while (WiFi.status() != WL_CONNECTED && retryNum <= retries) { 
        if (titilar) {
          ledAmarillo();
        }

        delay(250);
        Serial.print(WiFi.status()); 

        if (titilar) {
          ledApagado();
        }
        delay(250);

        retryNum++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi conectada");
      Serial.println(WiFi.SSID().c_str());
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.print("\nERROR conectándose a la red ");
      Serial.println(SSID_TO_CONNECT.c_str());
      return false;
    }
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
    Serial.println("\nFailed to open config file.");
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
    
  return true;
}

void configSSID() {
    WiFi.macAddress(mac);
    macString = macToString(mac);
    char macCharArr[19];
    macString.toCharArray(macCharArr, 19);
    
    strcpy(APssid, APSSID);
    strcat(APssid, macCharArr);
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


// SENSORES ///////////////////////////////////////////////

void setMuxChannel(byte channel) {
   digitalWrite(muxS0, bitRead(channel, 0));
   digitalWrite(muxS1, bitRead(channel, 1));
   //digitalWrite(muxS2, bitRead(channel, 2));
   //digitalWrite(muxS3, bitRead(channel, 3));
}

void leerSensores() {

    Serial.println("\nLeyendo sensores: ");
    for (byte i = 0; i < SENSORS_LENGTH; i++) {
        setMuxChannel(i);
        sensors[i] = analogRead(muxSIG);
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(sensors[i]);
        Serial.print("\t");
    }
    Serial.print(getSmoke());
    Serial.println();
}

int getCO() {
  return sensors[0];
}

int getGas() {
  return sensors[1];
}

int getSmoke() {
  return digitalRead(S_HUMO);
}

String getCOStatus() {
  if (getCO() >= umbralCO) {
    return ALARM;
  } else {
    return NORMAL;
  }
}

String getGasStatus() {
  if (getGas() >= umbralGAS) {
    return ALARM;
  } else {
    return NORMAL;
  }
}

String getSmokeStatus() {
  if (getSmoke() == LOW) {
    return ALARM;
  } else {
    return NORMAL;
  }
}

String getGeneralStatus() {
  if (getCOStatus().equals(NORMAL) && getGasStatus().equals(NORMAL) && getSmokeStatus().equals(NORMAL)) {
    return NORMAL;
  }

  return ALARM;
}

String generarJson() {
  String salida;

  StaticJsonDocument<512> pos;
  pos["latitude"] = latitude;
  pos["longitude"] = longitude;
  pos["accuracy"] = accuracy;
  
  StaticJsonDocument<512> sensor1;
  sensor1["type"] = "CO";
  sensor1["status"] = getCOStatus();
  sensor1["level"] = getCO();

  StaticJsonDocument<512> sensor2;
  sensor2["type"] = "GAS";
  sensor2["status"] = getGasStatus();
  sensor2["level"] = getGas();

  StaticJsonDocument<512> sensor3;
  sensor3["type"] = "SMOKE";
  sensor3["status"] = getSmokeStatus();
  sensor3["level"] = getSmoke() == 1? 0 : 1;

  StaticJsonDocument<1024> json;
  json["macAddress"] = macString;
  json["status"] = getGeneralStatus();
  json["position"] = pos;
  json["sensor1"] = sensor1;
  json["sensor2"] = sensor2;
  json["sensor3"] = sensor3;
  
  serializeJsonPretty(json, salida);

  return salida;
}
//////////////////////////////////////////////////////////////

// API GEOLOCALIZACIÓN GOOGLE ////////////////////////////////

void getGoogleGeolocation() {

  Serial.println("\nTomando datos de localización de la api de Google...");
  
  WifiLocation location(GOOGLE_API_KEY);
  location_t loc = location.getGeoFromWiFi();
  latitude = loc.lat;
  longitude = loc.lon;
  accuracy = loc.accuracy;
  
  Serial.print("Latitud: ");
  Serial.println(latitude);
  Serial.print("Longitud: ");
  Serial.println(longitude);
  Serial.print("Precisión: ");
  Serial.println(accuracy);
  Serial.println();
}
//////////////////////////////////////////////////////////////

// DELAY NO BLOQUEANTE ///////////////////////////////////////

boolean esperar(int tiempo) {

  if (!esperando) {
    Serial.print("\nComienzo de la ESPERA de ");
    Serial.print(tiempo);
    Serial.println("ms.");
    
    esperando = true;
    tiempoInicioEspera = millis();

  } else {
  
    if ((unsigned long)(millis() - tiempoInicioEspera) >= tiempo) {
  
        Serial.println("\nESPERA finalizada.");
        esperando = false;
    }
  }

  return esperando;
}

//////////////////////////////////////////////////////////////

// PULSADOR //////////////////////////////////////////////////

boolean pulsadorEncendidoPor(int tiempo) {

  if (digitalRead(PULSADOR) == HIGH) {
    pulsadorEncendido = false;
    return pulsadorEncendido;
  
  } else if (digitalRead(PULSADOR) == LOW) {
      
    if (!pulsadorEncendido) {
      Serial.println("\nSe apretó el PULSADOR.");
      
      // Se encendió el pulsador por primera vez
      tiempoInicioPulsadorEncendido = millis();
      pulsadorEncendido = true;
  
    } else {
  
      // Chequeamos si el tiempo que el pulsador estuvo apretado es igual o mayor al tiempo que queremos medir
      if ((unsigned long)(millis() - tiempoInicioPulsadorEncendido) >= tiempo) {
  
        Serial.print("\nPaso un tiempo mayor a ");
        Serial.print(tiempo);
        Serial.println("ms con el PULSADOR apretado.");
        
        pulsadorEncendido = false;  // Reseteamos la variable aún si la persona está apretando el botón, por consistencia
        return true;
      }
    }
  }

  return false;
}

//////////////////////////////////////////////////////////////

// INDICADOR SONORO //////////////////////////////////////////

void beep() {
  tone(BUZZER, NOTE, 200);
}

void apagarAlarmaSonora() {
  alarmaSonoraEncendida = false;  
}

// Esto es para poder hacer sonar la alarma por más de 20 segundos sin bloquear el thread, ya que tone no funciona con más de este tiempo
void sonarAlarmaPor(int tiempo) {

    if (!alarmaSonoraEncendida) {
      millisAlEncenderAlarma = millis();
      alarmaSonoraEncendida = true;

      // Prueba de encender la alarma desde el principio
      tone(BUZZER, NOTE);
    } else {
      
      if ((unsigned long)(currentMillis - millisAlEncenderAlarma) >= tiempo) {
        
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
  beep();
}

void setEstadoSinConexion() {
  Serial.println("\n---------------------------------------");
  Serial.println("          ESTADO SIN CONEXIÓN          ");
  Serial.println("---------------------------------------");
  
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
  Serial.println("\n---------------------------------------");
  Serial.println("          ESTADO APAREAMIENTO          ");
  Serial.println("---------------------------------------");
  
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = true;
  estadoDefectuoso = false;
  estadoAlarma = false;

  // Ya hacemos titilar el led, ya que configurar como access point lleva un tiempo, y sino obligamos al usuario a seguir apretando el pulsador
  titilar(AMARILLO);
  beep();
  configAsAccessPoint();
}

void setEstadoDefectuoso() {
  Serial.println("\n---------------------------------------");
  Serial.println("           ESTADO DEFECTUOSO           ");
  Serial.println("---------------------------------------");
  
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
  Serial.println("\n---------------------------------------");
  Serial.println("             ESTADO ALARMA             ");
  Serial.println("---------------------------------------");
  
  estadoNormal = false;
  estadoSinConexion = false;
  estadoApareamiento = false;
  estadoDefectuoso = false;
  estadoAlarma = true;

  // Reseteamos por si el usuario canceló la alarma anteriormente
  alarmaSonoraCanceladaPorUsuario = false;
}

///////////////////////////////////////////////////////////////
