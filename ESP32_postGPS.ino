#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>  // Librería para la comunicación SPI (Serial Peripheral Interface)
#include <SD.h>   // Librería para manejar la tarjeta SD
#include <FS.h>   // Librería para manejar el sistema de archivos
#include <time.h>

#include <TinyGPS.h>//incluimos TinyGPS
#include <Adafruit_GPS.h>

TinyGPS gps;
#define GPSSerial Serial2
Adafruit_GPS serialgps(&GPSSerial);

String guardarGPS;

const char* ssid = "Z";
const char* password = "Z";
WebServer server(80);
String backendURL = "http://192.168.10.190:8000/";
String token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJjcmVhdGVkIjoiVFZSWk5VNVVUVFZPZW1jMVRXYzlQUT09IiwiZXhwaXJlIjoiVFZSbk1VMTZRVE5PZW1jMVRXYzlQUT09IiwidXNlcklkIjoiVFdjOVBRPT0ifQ.TPaUn1g5FpspFjf0x1nQ3ff4iNFDEBnx-RmcFEfFBAY";

//---------------------------------------------------------------------------------------------------------
// Variables para enviar informacion del serial de la maquina
const String SERIAL_ID = "123456789";  // Serial de la maquina´
IPAddress MACHINE_IP;

//---------------------------------------------------------------------------------------------------------
// Variables relog y fecha
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
struct tm initialTime;

//---------------------------------------------------------------------------------------------------------
// Variables lectura maquina
int dataCoin[250], dataKeyOut[250];
int conteoCoin = 0;
int conteoKeyOut = 0;
int memoriaCoin = 0, memoriaKeyOut = 0;
unsigned long currentMillis;
unsigned long previousMillis = 0;
const unsigned long interval = 300;

int pinEntradaCoin = 35;
int pinEntradaKeyOut = 34;
int pinSalida = 2;
int ledConexion = 13;
int ledConectando = 14;
int pinbuzzer = 32;
int openDoor = 33;
int buzzerActivo;

int estadoAnteriorCoin = LOW;
int estadoAnteriorKeyOut = LOW;
int estadoAnteriorBuzzer = LOW;

String envioDatos = "activo";

//---------------------------------------------------------------------------------------------------------
void setup() {

  Serial.begin(9600);
  // Serial2.begin(9600, 16);         // Lectura COIN
  serialgps.begin(9600);//Iniciamos el puerto serie del gps
  serialgps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  serialgps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); 
  serialgps.sendCommand(PGCMD_ANTENNA);

  pinMode(pinEntradaCoin, INPUT);      // Lectura keyOut Metter
  pinMode(pinEntradaKeyOut, INPUT);
  pinMode(openDoor, INPUT);        // Lectura si se abre la puerta
  pinMode(pinSalida, OUTPUT);      // Envia señal retiro dinero
  pinMode(ledConectando, OUTPUT);  // indicador espera de conexion
  pinMode(ledConexion, OUTPUT);    // Indicador conexion OK
  pinMode(pinbuzzer, OUTPUT);      // Indicador de sonido

  // Lectura WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    digitalWrite(ledConectando, HIGH);
    delay(100);
    digitalWrite(ledConectando, LOW);
  }
  Serial.print("Conexión Wi-Fi establecida. Dirección IP del ESP32: ");
  Serial.println(WiFi.localIP());
  MACHINE_IP = WiFi.localIP();

  // Configura el servidor NTP
  configTime(gmtOffset_sec, 0, ntpServer);
  if (getLocalTime(&initialTime)) {
    Serial.println("Fecha y hora inicial obtenidas de Internet:");
    Serial.println(&initialTime, "%A, %B %d %Y %H:%M:%S");
  }
  // Servidor
  server.on("/machine/cash_out", keyOut);
  server.on("/machine/gps", postGPS);
  server.begin();
  Serial.println("Servidor iniciado");

  if (!SD.begin(5)) {
    Serial.println("Error al inicializar la tarjeta microSD.");
    return;
  }
  Serial.println("Tarjeta microSD inicializada correctamente.");
}
//---------------------------------------------------------------------------------------------------------
void loop() {

  if (envioDatos == "activo") {
    sendPOSTserial_ID();
    envioDatos = "desactivo";
  }
  leerBuzzer();
  server.handleClient();
  detectaSerial();
}
//---------------------------------------------------------------------------------------------------------
String actualizaFechaHora() {
  unsigned long elapsedMillis = millis();
  struct tm currentTime;
  memcpy(&currentTime, &initialTime, sizeof(currentTime));
  currentTime.tm_sec += elapsedMillis / 1000;
  mktime(&currentTime);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%A, %B %d %Y %H:%M:%S", &currentTime);

  return String(buffer);
}
//---------------------------------------------------------------------------------------------------------
void sumaCeros() {  // sumaCero() Cuenta la cantidad de ceros (0) ingresados en los puertos del ESP32
  previousMillis = currentMillis;

  // Lectura COIN
  int acumuladoCoin = 0;
  for (int i = 0; i < 250; i++) {
    if (dataCoin[i] == 1) {
      acumuladoCoin++;
    }
  }
  if (acumuladoCoin != memoriaCoin) {
    memoriaCoin = acumuladoCoin;
  } else {
    if (acumuladoCoin == memoriaCoin && memoriaCoin != 0) {
      if (memoriaCoin > 1) {
        Serial.println("Valor Ingresado " + String(memoriaCoin));
        sendHttpMoney(String(memoriaCoin), "cashin");
        conteoCoin = 0;
        String informacion= "Valor Ingresado: " + String(memoriaCoin) + "USD fecha: " + actualizaFechaHora();
        saveSD("/cashIN.txt", informacion);
      }
      limpiarData(1);
    } else {
      memoriaCoin = acumuladoCoin;
    }
  }

  // Lectura KEYOUT
  int acumuladoKeyOut = 0;
  for (int i = 0; i < 250; i++) {
    if (dataKeyOut[i] == 1) {
      acumuladoKeyOut++;
    }
  }
  if (acumuladoKeyOut != memoriaKeyOut) {
    memoriaKeyOut = acumuladoKeyOut;
  } else {
    if (acumuladoKeyOut == memoriaKeyOut && memoriaKeyOut != 0) {
      if (memoriaKeyOut > 1) {
        Serial.println("Retiro USD:  " + String(memoriaKeyOut));
        sendHttpMoney(String(memoriaKeyOut), "cashout");
        conteoKeyOut = 0;
        String informacion= "Valor retirado: " + String(memoriaKeyOut) + "USD, fecha: " + actualizaFechaHora();
        saveSD("/cashOUT.txt", informacion);
      }
      limpiarData(2);
    } else {
      memoriaKeyOut = acumuladoKeyOut;
    }
  }
}
//---------------------------------------------------------------------------------------------------------
void limpiarData(int num1) {  // limpiarData() Limpia la el arreglo donde se almacena la cantidad de ceros
  if (num1 == 1) {
    for (int i = 0; i < 250; i++) {
      dataCoin[i] = 0;
    }
  }
  if (num1 == 2) {
    for (int i = 0; i < 250; i++) {
      dataKeyOut[i] = 0;
    }
  }
}
//---------------------------------------------------------------------------------------------------------
void detectaSerial() {  // detectaSerial() Detecta si hay cambios en el serial en la entra de ceros para coin IN y key OUT
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    sumaCeros();
  }

  // if (Serial2.available() > 0) {  // Se almacenan los ceros del coin
  //   conteoMeter++;
  //   int in = Serial2.read();
  //   if (in == 0) {
  //     dataMeter[conteoMeter] = 1;
  //   }
  // }
  int estadoPinCoin = digitalRead(pinEntradaCoin);  // Se almacenan los ceros del coin
  if (estadoPinCoin == HIGH && estadoAnteriorCoin == LOW) {
    conteoCoin++;
    dataCoin[conteoCoin] = 1;
  }
  estadoAnteriorCoin = estadoPinCoin;


  int estadoPinKeyOut = digitalRead(pinEntradaKeyOut);  // Se almacenan los ceros del keyout
  if (estadoPinKeyOut == HIGH && estadoAnteriorKeyOut == LOW) {
    conteoKeyOut++;
    dataKeyOut[conteoKeyOut] = 1;
  }
  estadoAnteriorKeyOut = estadoPinKeyOut;
}
//---------------------------------------------------------------------------------------------------------
void leerBuzzer() {
  buzzerActivo = digitalRead(openDoor);
  if (buzzerActivo == HIGH && estadoAnteriorBuzzer == LOW) {
    digitalWrite(pinbuzzer, HIGH);
    delay(1000);

    DynamicJsonDocument jsonDoc(1024);
    JsonObject json = jsonDoc.to<JsonObject>();
    json["actionlog_event"] = "Physically";
    json["actionlog_type"] = "Open door";
    json["machine_id"] = SERIAL_ID;
    String jsonString;
    serializeJson(json, jsonString);
    Serial.println("Se abrio la puerta");
    sendHTTP("actionlog", jsonString);

    String informacion = "Puerta abierta: " + actualizaFechaHora();
    saveSD("/openDoor.txt", informacion);

  } else if (buzzerActivo == LOW) {
    digitalWrite(pinbuzzer, LOW);
  }
  estadoAnteriorBuzzer = buzzerActivo;
}
void saveSD(String nombreArchivo, String informacion) {

  File archivo = SD.open(nombreArchivo, FILE_APPEND);
  if (!archivo) {
    // Si el archivo no existe, créalo en modo de escritura
    archivo = SD.open(nombreArchivo, FILE_WRITE);
    if (!archivo) {
      Serial.println("Error al abrir el archivo.");
    }
  }
  if (archivo) {
    archivo.println(informacion);
    archivo.close();
    Serial.println("Datos escritos en el archivo.");

  } else {
    Serial.println("Error al abrir el archivo.");
  }
}
//---------------------------------------------------------------------------------------------------------
void sendPOSTserial_ID() {  //  Se envia informacion de la placa, serial e ip de conexion

  DynamicJsonDocument jsonDoc(1024);
  JsonObject json = jsonDoc.to<JsonObject>();
  json["machine_serial"] = SERIAL_ID;
  json["machine_ip"] = MACHINE_IP;
  json["location_id"] = 1;
  String jsonString;
  serializeJson(json, jsonString);

  sendHTTP("machinelocations", jsonString);
}
//---------------------------------------------------------------------------------------------------------
void sendHttpMoney(String valor, String endPoint) {  // sendHttpPOST() Envia la peticion POST al dashboard donde muestra la cantidad ingresa y cant salida
  HTTPClient http;

  DynamicJsonDocument jsonDoc(1024);
  JsonObject json = jsonDoc.to<JsonObject>();
  if (endPoint == "cashin") {
    json["machine_id"] = SERIAL_ID;
    json["cashin_in"] = valor;
  }
  if (endPoint == "cashout") {
    json["machine_id"] = SERIAL_ID;
    json["cashout_out"] = valor;
  }
  String jsonString;
  serializeJson(json, jsonString);

  sendHTTP(endPoint, jsonString);
}
//---------------------------------------------------------------------------------------------------------
void keyOut() {  // keyOut() Envia al ESP32 un Json que activa segun la condicion la salida de USD
  if (server.method() == HTTP_POST) {

    if (server.hasArg("plain") == false) {
      server.send(400, "text/plain", "Bad request!");
    } else {
      String message = server.arg("plain");
      message += "\n";

      DynamicJsonDocument doc(1024);
      // Parse JSON object
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      const String action = doc["action"];
      const String serial = doc["serial_id"];

      Serial.println("Action = " + action);
      Serial.println("Serial id = " + serial);

      if (SERIAL_ID == serial) {
        if (action == "COLLECT") {
          digitalWrite(pinSalida, HIGH);
          delay(300);
          digitalWrite(pinSalida, LOW);
          Serial.println("Enciende");
          server.send(200, "application/json", "{\"success\":\"true\"}");
        }
      }
    }
  }
}
void sendHTTP(String endPoint, String json) {
  HTTPClient http;
  String url = backendURL + endPoint;

  http.begin(url);
  http.addHeader("Authorization", token);
  http.setTimeout(30000);

  int httpCode = http.POST(json);
  Serial.println("Se envio:");
  Serial.println(json);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_CREATED) {
      Serial.println(httpCode);
      Serial.println("Respuesta del servidor:");
      Serial.println(http.getString());
    }
    if (httpCode == HTTP_CODE_OK) {
      Serial.println(httpCode);
      Serial.println("Respuesta del servidor:");
      Serial.println(http.getString());
      if(endPoint=="machinelocations"){digitalWrite(ledConexion, HIGH);}
    }
    
  } else {
    Serial.println("Error en la solicitud HTTP");
    Serial.println("Se obtuvo http.error:");
    Serial.println(http.errorToString(httpCode));
    if(endPoint=="machinelocations"){digitalWrite(ledConectando,HIGH);}
  }
  http.end();
}

void postGPS() {  // keyOut() Envia al ESP32 un Json que activa segun la condicion la salida de USD
  searchGPS();
  if (server.method() == HTTP_POST) {

    if (server.hasArg("plain") == false) {
      server.send(400, "text/plain", "Bad request!");
    } else {
      String message = server.arg("plain");
      message += "\n";

      DynamicJsonDocument doc(1024);
      // Parse JSON object
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      const String action = doc["action"];
      const String serial = doc["serial_id"];

      Serial.println("Action = " + action);
      Serial.println("Serial id = " + serial);

      if (SERIAL_ID == serial) {
        if (action == "GPS") {
          Serial.println(guardarGPS);
          server.send(200, "application/json", "{\"success\":\"true\"}");
        }
      }
    }
  }
}

void searchGPS(){
  while(serialgps.available()){
    int c = serialgps.read();
    if(gps.encode(c)){
      float latitude, longitude;
      gps.f_get_position(&latitude, &longitude);
      guardarGPS= "Latitud/Longitud: " + String(latitude,9) + ", " + String(longitude,9);
    }
  }
}
