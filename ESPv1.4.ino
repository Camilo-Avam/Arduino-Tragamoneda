//=========================================================================================================
// Bibliotecas
//=========================================================================================================

#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Base64.h>
#include <FS.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>

//=========================================================================================================
// Definicion de variables
//=========================================================================================================

//---------------------------------------------------------------------------------------------------------
// Variables para enviar informacion del serial de la maquina

const String SERIAL_ID = "CG-777";  // Serial de la maquina´
IPAddress MACHINE_IP;

//---------------------------------------------------------------------------------------------------------
// Variables conexion MQTT
WiFiClient esp32Client;
PubSubClient mqttClient(esp32Client);

char* serverMQTT = "3.20.170.233";
char* userMQTT = "Brava@Bus!nnes";
char* passMQTT = "d4t@247#!$";
int port = 1883;
const char* mqtt_clientID = "CG-777";
const char* mqtt_topic_entrada = "Entrada";
const char* mqtt_topic_ping = "ping";

//---------------------------------------------------------------------------------------------------------
// Variables conexion BLE
BLEService serviceBLE("0880dd71-4dbd-474a-9101-7734ca3dd46e");  // Bluetooth® Low Energy LED Service
BLEStringCharacteristic characteristicBLE("9d0983d4-0c6c-45c4-b83b-14abaa6ba18a", BLERead | BLEWrite, 1024);
BLEStringCharacteristic characteristicGPS("9d0983d4-0c65-45c4-b83b-14abaa6ba18c", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFIPass("9d0983d4-0c6f-45c4-b83b-14abaa6ba18d", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFISsid("9d0983d4-0c61-45c4-b83b-14abaa6ba18e", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicLocation("9d0983d4-0c64-45c4-b83b-14abaa6ba18b", BLERead | BLEWrite, 128);

//---------------------------------------------------------------------------------------------------------
// Variables conexion WIFI
String ssid;
String password;
String location;
String backendURL = "https://backend.data24-7gaming.com/";
// String backendURL = "http://192.168.10.220:8000/";
String token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJjcmVhdGVkIjoiVFZSWk5VNVVUVFZPZW1jMVRXYzlQUT09IiwiZXhwaXJlIjoiVFZSbk1VMTZRVE5PZW1jMVRXYzlQUT09IiwidXNlcklkIjoiVFdjOVBRPT0ifQ.TPaUn1g5FpspFjf0x1nQ3ff4iNFDEBnx-RmcFEfFBAY";

//---------------------------------------------------------------------------------------------------------
// Variables relog y fecha
const char* ntpServer = "pool.ntp.org";
const byte gmtOffset_sec = -14400;
struct tm initialTime;

RTC_DS3231 rtc;

//---------------------------------------------------------------------------------------------------------
struct HttpRequestData { // pude eliminarse
  String endPoint;
  String json;
};

//---------------------------------------------------------------------------------------------------------
// Variables lectura maquina, definicion de pines de entrada
String resultS = "";

int conteoCoin = 0;
int conteoKeyOut = 0;
byte conteoError = 0;
int memoriaCoin = 0, memoriaKeyOut = 0;
unsigned long currentMillisCoin , currentMillisKeyOut;
unsigned long previousMillisCoin = 0;
unsigned long previousMillisKeyOut = 0;
const unsigned int intervalCoin = 300;
const unsigned int intervalKeyOut = 800;

byte pinEntradaCoin = 35;
byte pinEntradaKeyOut = 34;
byte pinSalida = 2;
byte ledConexion = 13;
byte ledConectando = 14;
byte pinbuzzer = 32;
byte openDoor = 33;
byte buzzerActivo;

//---------------------------------------------------------------------------------------------------------
// Variables estados
int estadoAnteriorCoin = LOW;
int estadoAnteriorKeyOut = LOW;
int estadoAnteriorBuzzer = LOW;

bool conecto = false;
bool wifiConnected = false;
bool synchronization = false;
bool registroCashOut = false;
bool registroBluetooth = false;
bool toggleBleOut = false;
String enviarBluettooth;

//---------------------------------------------------------------------------------------------------------
// Variables conteo indefinido CASH IN
bool estadoActual = false;
bool estadoPrevio = false;
unsigned long tiempoCambioEstado = 0;
const unsigned long INTERVALO = 20000; //--------------------------------- pruebas
int cashInSave = 0;

//=========================================================================================================
// Inicio funciones nucleo primario
//=========================================================================================================

//---------------------------------------------------------------------------------------------------------
// Funciones relog
String actualizaFechaHora() {
  DateTime now = rtc.now();
  char formattedTime[9];

  sprintf(formattedTime, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(String(now.month(), DEC) + "/" + String(now.day(), DEC) + "/" + String(now.year(), DEC) + " " + String(formattedTime));
}

bool getNTPTime() {
  configTime(gmtOffset_sec, 0, ntpServer);

  if (getLocalTime(&initialTime)) {
    return true;
  } else {
    return false;
  }
}

//-----------------------------------------------------------------------------------------------------
// Funciones leer dinero
void detectaSerialINGRESO() {  // detectaSerial() Detecta si hay cambios en el serial en la entra de ceros
  currentMillisCoin = millis();
  if (currentMillisCoin - previousMillisCoin >= intervalCoin) {

    previousMillisCoin = currentMillisCoin;
    int acumuladoCoin = 0;
    if (conteoCoin != memoriaCoin) {
      memoriaCoin = conteoCoin;
    } else {
      if (conteoCoin == memoriaCoin && memoriaCoin != 0) {
        if (conteoError > 1) {

          Serial.println("Valor Ingresado " + String(memoriaCoin));
          String informacion = "{\"value\": \"" + String(memoriaCoin) + "\",\"date\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/cashIN.txt", informacion);
          estadoActual = true;
          cashInSave = memoriaCoin + cashInSave;
          conteoCoin = 0;

          if (!conecto) {
            informacion = "{\"v\": \"" + String(memoriaCoin) + "\",\"d\": \"" + actualizaFechaHora() + "\"},";
            saveSD("/cashInSyn.txt", informacion);
          }
        }
      } else {
        memoriaCoin = conteoCoin;
      }
    }
  }

  int estadoPinCoin = digitalRead(pinEntradaCoin);  // Se almacenan los ceros del coin
  if (estadoPinCoin == HIGH && estadoAnteriorCoin == LOW) {
    conteoCoin++;
    estadoActual = false;
  }
  estadoAnteriorCoin = estadoPinCoin;
}

void detectaSerialRETIRO() {  // detectaSerial() Detecta si hay cambios en el serial en la entra de ceros

  currentMillisKeyOut = millis();
  if (currentMillisKeyOut - previousMillisKeyOut >= intervalKeyOut) {
    previousMillisKeyOut = currentMillisKeyOut;
    int acumuladoKeyOut = 0;
    if (conteoKeyOut != memoriaKeyOut) {
      memoriaKeyOut = conteoKeyOut;
    } else {
      if (conteoKeyOut == memoriaKeyOut && memoriaKeyOut != 0) {

        if (conteoError > 1) {

          Serial.println("Retiro USD:  " + String(memoriaKeyOut));
          enviarBluettooth = "{\"value\":\"" + String(memoriaKeyOut) + "\"}";
          String informacion = "{\"value\":\"" + String(memoriaKeyOut) + "\", \"date\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/cashOUT.txt", informacion);
          registroCashOut = true;
          if (conecto) {
            sendJson(String(memoriaKeyOut), "cashout");
          }
          conteoKeyOut = 0;

          if (!conecto) {
            informacion = "{\"v\":\"" + String(memoriaKeyOut) + "\", \"d\": \"" + actualizaFechaHora() + "\"},";
            saveSD("/cashOutSyn.txt", informacion);
          }
        }
      } else {
        memoriaKeyOut = conteoKeyOut;
      }
    }
  }
  int estadoPinKeyOut = digitalRead(pinEntradaKeyOut);  // Se almacenan los ceros del keyout
  if (estadoPinKeyOut == HIGH && estadoAnteriorKeyOut == LOW) {
    registroBluetooth = true;
    conteoKeyOut++;
  }
  estadoAnteriorKeyOut = estadoPinKeyOut;
}

//---------------------------------------------------------------------------------------------------------
// Funciones guardar en SD

void saveSD(String nombreArchivo, String informacion) {
  File archivo;

  if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt") {
    archivo = SD.open(nombreArchivo, FILE_WRITE);
  } else {
    archivo = SD.open(nombreArchivo, FILE_APPEND);
  }

  if (!archivo) {
    archivo = SD.open(nombreArchivo, FILE_WRITE);
    if (!archivo) {
      Serial.println("Error al abrir el archivo.");
    }
  }

  String informacion64 = base64::encode(informacion);
  int len = informacion64.length();
  int partSize = len / 4;
  String parte1 = informacion64.substring(0, partSize);
  String parte2 = informacion64.substring(partSize, 2 * partSize);
  String parte3 = informacion64.substring(2 * partSize, 3 * partSize);
  String parte4 = informacion64.substring(3 * partSize, len);

  String nuevaOrden = parte3 + parte1 + parte4 + parte2;

  if (archivo) {
    if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt") {
      archivo.print(nuevaOrden);
    } else {
      archivo.println(nuevaOrden);
    }
  } else {
    Serial.println("Error al abrir el archivo.");
  }
  archivo.close();
}

String readSD(String nombreArchivo, String accion) {

  String contenido = "";
  String ultimasLineas[10];

  File archivo = SD.open(nombreArchivo, FILE_READ);
  if (!archivo) {
    Serial.println("Error al abrir el archivo para lectura.");
    return "";
  }

  if (accion == "ultimo") {
    while (archivo.available()) {
      String linea64 = archivo.readStringUntil('\n');

      int len = linea64.length();
      int partSize = len / 4;
      String parte1 = linea64.substring(0, partSize);
      String parte2 = linea64.substring(partSize, 2 * partSize);
      String parte3 = linea64.substring(2 * partSize, 3 * partSize);
      String parte4 = linea64.substring(3 * partSize, len);

      String nuevaOrden = parte2 + parte4 + parte1 + parte3;
      String linea = base64::decode(nuevaOrden);
      archivo.close();
      return linea;
    }
  }

  archivo.seek(0, SeekEnd);
  int fileSize = archivo.position();

  int newlineCount = 0;
  int filePosition = fileSize;

  while (filePosition > 0 && newlineCount < 10) {
    filePosition--;

    archivo.seek(filePosition);
    char currentChar = archivo.read();

    if (currentChar == '\n') {
      newlineCount++;
    }
  }
  if (filePosition > 0) {
    filePosition++;
  }

  archivo.seek(filePosition);
  int indice = 0;

  while (archivo.available()) {

    String linea64 = archivo.readStringUntil('\n');
    int len = linea64.length();
    int partSize = len / 4;
    String parte1 = linea64.substring(0, partSize);
    String parte2 = linea64.substring(partSize, 2 * partSize);
    String parte3 = linea64.substring(2 * partSize, 3 * partSize);
    String parte4 = linea64.substring(3 * partSize, len);

    String nuevaOrden = parte2 + parte4 + parte1 + parte3;
    String linea = base64::decode(nuevaOrden);

    if (linea.length() > 0) {
      if (indice < 10) {
        ultimasLineas[indice] = linea;
        indice = (indice + 1) % 10;
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    contenido += String(ultimasLineas[i]) + "\n";
  }
  archivo.close();
  return contenido + "]";
}

//---------------------------------------------------------------------------------------------------------
// FUncion leer alarma
void leerBuzzer() {
  buzzerActivo = digitalRead(openDoor);
  if (buzzerActivo == HIGH && estadoAnteriorBuzzer == LOW) {
    digitalWrite(pinbuzzer, HIGH);
    delay(100);
    Serial.println(F("Se abrio la puerta"));
    sendJson("Open Door", "actionlog");
    String informacion = "{\"Oppen door\": \"" + actualizaFechaHora() + "\"}";
    saveSD("/openDoor.txt", informacion);
  } else if (buzzerActivo == LOW) {
    digitalWrite(pinbuzzer, LOW);
  }
  estadoAnteriorBuzzer = buzzerActivo;
}

//---------------------------------------------------------------------------------------------------------
// Funciones enviar datos HTTP

void sendJson(String valor, String endPoint) {

  DynamicJsonDocument jsonDoc(256);
  JsonObject json = jsonDoc.to<JsonObject>();

  json["machine_id"] = SERIAL_ID;

  if (endPoint == "cashin") {
    json["cashin_in"] = valor;
  }
  if (endPoint == "cashout") {
    json["cashout_out"] = valor;
  }
  if (endPoint == "actionlog") {
    json["actionlog_event"] = "Physically";
    json["actionlog_type"] = valor;
  }
  if (endPoint == "collect/received/") {
    json["action"] = valor;
  }

  String jsonString;
  serializeJson(json, jsonString);
  sendHTTP(endPoint, jsonString);
}

void sendHTTP(String endPoint, String json) {

  HTTPClient http;

  if (endPoint == "collect/received/") {
    http.begin(backendURL + endPoint + SERIAL_ID);
  } else {
    http.begin(backendURL + endPoint);
  }
    http.addHeader("Authorization", token);
  if (endPoint == "cashin"){
    http.setTimeout(2000);
  } else{
    http.setTimeout(5000);
  }

  int httpCode = http.POST(json);
  Serial.println(json);

  if (httpCode == HTTP_CODE_CREATED) {
    Serial.print(F("Respuesta del servidor 201:     :"));
    Serial.println(http.getString());
  } else if (httpCode == HTTP_CODE_OK) {
    Serial.print(F("Respuesta del servidor 200:    :"));
    Serial.println(http.getString());
  } else {
    Serial.print(F("Respuesta del servidor: Error"));
    Serial.println(http.getString());
    Serial.println(http.errorToString(httpCode));
  }
  http.end();
}

//---------------------------------------------------------------------------------------------------------
// Funcion iniciar WIFI
//---------------------------------------------------------------------------------------------------------
void iniciarWifiSD() {

  password = readSD("/WIFIpass.txt", "ultimo");
  ssid = readSD("/WIFIssid.txt", "ultimo");
  location = readSD("/location.txt", "ultimo");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  vTaskDelay(pdMS_TO_TICKS(5000));

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;

    MACHINE_IP = WiFi.localIP();

  } else {
    wifiConnected = false;
    Serial.println("\nError en la conexión WiFi.");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

//---------------------------------------------------------------------------------------------------------
// FUncion conexxion BLE
//---------------------------------------------------------------------------------------------------------
void iniciarServicioBlue() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Conectado a central BLE");
    Serial.println(central.address());
    toggleBleOut = false;
    while (central.connected() && !toggleBleOut) {

      if (characteristicBLE.written()) {
        if (characteristicBLE.value() == "cashOUT") {
          String contenido = readSD("/cashOUT.txt", "todo");
          characteristicBLE.writeValue(contenido);
        }

        if (characteristicBLE.value() == "retirarCashOUT") {
          toggleBleOut = true;

          registroBluetooth = false;
          registroCashOut = false;
          delay(10);
          digitalWrite(pinSalida, HIGH);
          delay(50);
          digitalWrite(pinSalida, LOW);
          vTaskDelay(pdMS_TO_TICKS(2000));

          Serial.println(registroBluetooth);
          Serial.println(registroCashOut);

          while (!registroCashOut && registroBluetooth) {
            Serial.print(F("......."));
            vTaskDelay(pdMS_TO_TICKS(100));
          }
          vTaskDelay(pdMS_TO_TICKS(4000));

          if (registroCashOut) {
            characteristicBLE.writeValue(enviarBluettooth);
            enviarBluettooth = "";
          }
          if (!registroBluetooth) {
            characteristicBLE.writeValue("No hay contenido");
          }

          registroBluetooth = false;
          registroCashOut = false;
          toggleBleOut = false;
        }

        if (characteristicBLE.value() == "ingreso") {
          String contenido = readSD("/cashIN.txt", "todo");
          characteristicBLE.writeValue(contenido);
        }

        if (characteristicBLE.value() == "reset") {
          String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/reset.txt", informacion);
          Serial.println("Reinicio");
          delay(4000);
          esp_restart();
        }

        if (characteristicBLE.value() == "wifi") {
          String contenido = readSD("/WIFIssid.txt", "ultimo");
          String contenido2 = readSD("/WIFIpass.txt", "ultimo");

          characteristicBLE.writeValue("{\"ssid\":\"" + contenido + "\", \"pass\": \"" + contenido2 + "\"},");
        }
      }
      if (characteristicWIFISsid.written()) {
        String value = characteristicWIFISsid.value();
        saveSD("/WIFIssid.txt", value);
        characteristicWIFISsid.writeValue("SSIDOK");
        iniciarWifiSD();
      }
      if (characteristicWIFIPass.written()) {
        String value = characteristicWIFIPass.value();
        saveSD("/WIFIpass.txt", value);
        characteristicWIFIPass.writeValue("PASSOK");
        iniciarWifiSD();
      }
      if (characteristicLocation.written()) {
        String value = characteristicLocation.value();
        saveSD("/location.txt", value);
        characteristicLocation.writeValue("LocationOK");
        iniciarWifiSD();
      }
      if (characteristicGPS.written()) {
        String value = characteristicGPS.value();
        saveSD("/gps.txt", value);
        characteristicGPS.writeValue("Ubicacion guardada: " + value);
      }
    }
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }
}

//---------------------------------------------------------------------------------------------------------
// Funciones MQTT
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conectarse MQTT...");

    DynamicJsonDocument jsonDoc(256);
    JsonObject json = jsonDoc.to<JsonObject>();

    json["machine_serial"] = SERIAL_ID;
    json["action"] = "disconnect";


    String jsonString;
    serializeJson(json, jsonString);
    const char* message = jsonString.c_str();

    if (mqttClient.connect(mqtt_clientID, userMQTT, passMQTT, mqtt_topic_ping, 0, false, message, false)) {
      mqttClient.subscribe(mqtt_topic_entrada, 0);
      mqttClient.subscribe(mqtt_topic_ping, 0);

      Serial.println("Conectado");

    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentar de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {

  char payload_string[length + 1];
  int resultI;

  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';
  resultI = atoi(payload_string);

  resultS = "";

  for (int i = 0; i < length; i++) {
    resultS = resultS + (char)payload[i];
  }
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, resultS);

  if (error) {
    Serial.print(F("No es una respuesta valida del servidor MQTT, no JSON: "));
    // Serial.println(error.c_str());
  }

  const String action = doc["action"];
  const String serial = doc["serial_id"];

  if (SERIAL_ID == serial) {
    Serial.print("Mensaje recibido: ");
    Serial.println(topic);
    Serial.println("Action = " + action);

    if (action == "COLLECT") {
      sendJson("received_message", "collect/received/");

      registroBluetooth = false;
      registroCashOut = false;

      digitalWrite(pinSalida, HIGH);
      delay(50);
      digitalWrite(pinSalida, LOW);
      vTaskDelay(pdMS_TO_TICKS(2000));

      Serial.println(registroBluetooth);
      Serial.println(registroCashOut);

      if (!registroBluetooth) {
        Serial.println("Envio cashout en cero");
        sendJson("0", "cashout");
      }

      registroBluetooth = false;
      registroCashOut = false;
    }
    if (action == "RESET") {
      Serial.println(F("Reinicio"));
      String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
      saveSD("/reset.txt", informacion);
      delay(100);
      String mensaje = "{\"message\": \"Reinicio enviado a tarjeta\" , \"ation\": \"RESET\"}";
      const char* messageChar = mensaje.c_str();
      mqttClient.publish(mqtt_topic_entrada, messageChar);
      esp_restart();
    }
    if (action == "SYNCHRONIZATION") {
      syncData();
    }
  }
}

//=========================================================================================================
// Inicio funciones nucleo secundario
//=========================================================================================================

//---------------------------------------------------------------------------------------------------------
// Funciones en Hilos
void mainSecond(void* parameter) {  // Hilo 1
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!wifiConnected) {
      detectaSerialINGRESO();
      leerBuzzer();
    }
    if (toggleBleOut) {
      detectaSerialRETIRO();
    }

    if (estadoActual == true && estadoPrevio == false) {
      tiempoCambioEstado = millis();
    }
    if (estadoActual == false && estadoPrevio == true) {
      tiempoCambioEstado = millis();
    }
    if (estadoActual == true && millis() - tiempoCambioEstado >= INTERVALO) {
      tiempoCambioEstado = millis();
      if (conecto) {
        sendJson(String(cashInSave), "cashin");
        cashInSave = 0;
      }
      estadoActual = false;
    }
    estadoPrevio = estadoActual;


    if (wifiConnected) {
      detectaSerialINGRESO();
      leerBuzzer();

      if (WiFi.status() == !WL_CONNECTED) {
        wifiConnected = false;
        synchronization = false;
        Serial.println(F("No hay conexion con red"));
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

//---------------------------------------------------------------------------------------------------------
void mqtt_http(void* parameter) {  // Hilo2
  while (true) {
    if (wifiConnected) {
      vTaskDelay(pdMS_TO_TICKS(1));
      if (!mqttClient.connected()) {
        conecto = false;
        reconnect();
        enviarPingHTTP();
        vTaskDelay(pdMS_TO_TICKS(100));
        syncData();
        if (conteoError < 1) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          Serial.printf("Entro en conteo error");
          conteoError = 2;
          conteoCoin = 0;
          conteoKeyOut = 0;
        }
        
      }
      mqttClient.loop();
    }
  }
}

//---------------------------------------------------------------------------------------------------------
// Funciones sincronizacion
void enviarPingHTTP() {


  DynamicJsonDocument jsonDoc(256);
  JsonObject json = jsonDoc.to<JsonObject>();

  json["machine_serial"] = SERIAL_ID;
  json["machine_ip"] = MACHINE_IP;
  json["location_id"] = location;
  json["action"] = "ping";

  String jsonString;
  serializeJson(json, jsonString);

  HTTPClient httpPING;
  httpPING.begin(backendURL + "machinelocations");
  httpPING.addHeader("Authorization", token);
  httpPING.setTimeout(20000);

  int httpCode = httpPING.POST(jsonString);
  Serial.print(F("Se envio:  :"));
  Serial.println(jsonString);
  Serial.print(F("Codigo respuesta: "));
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    Serial.print(F("Respuesta del servidor: "));
    Serial.println(httpPING.getString());
    conecto = true;
    digitalWrite(ledConexion, HIGH);
    digitalWrite(ledConectando, LOW);
  } else {
    Serial.print(F("Error en la solicitud HTTP, se obtuvo http.error:     :"));
    Serial.println(httpPING.getString());
    digitalWrite(ledConexion, LOW);
    digitalWrite(ledConectando, HIGH);
  }

  httpPING.begin(backendURL + "machinelocations/ping");
  httpPING.addHeader("Authorization", token);
  DynamicJsonDocument jsonDoc2(128);
  JsonObject json2 = jsonDoc2.to<JsonObject>();
  json2["serial"] = SERIAL_ID;
  String jsonString2;
  serializeJson(json2, jsonString2);
  httpPING.POST(jsonString2);
  Serial.print(F("Respuesta del servidor: "));
  Serial.println(httpPING.getString());
  httpPING.end();
}

void syncData() {
  HTTPClient httpSync;
  String urlIn = backendURL + "synchronization/cashInSyn";
  String urlOut = backendURL + "synchronization/cashOutSyn";

  vTaskDelay(pdMS_TO_TICKS(100));

  if (SD.exists("/cashInSyn.txt")) {

    File fileIn = SD.open("/cashInSyn.txt");
    String contenidoArchivo = fileIn.readString();

    fileIn.close();

    httpSync.begin(urlIn);
    httpSync.addHeader("Authorization", token);
    httpSync.addHeader("Content-Type", "text/plain");
    httpSync.setTimeout(5000);

    int ResponseCodeIn = httpSync.POST(contenidoArchivo);
    if (ResponseCodeIn == HTTP_CODE_OK) {
      Serial.printf("Respuesta del servidor: %d\n", ResponseCodeIn);
      Serial.println("Archivo enviado correctamente");
      if (SD.remove("/cashInSyn.txt")) {
        Serial.println("Archivo cashInSyn borrado correctamente");
      } else {
        Serial.println("Error al borrar el archivo");
      }

    } else {
      Serial.printf("Error en la solicitud: %s\n", httpSync.errorToString(ResponseCodeIn).c_str());
    }
  }
  httpSync.end();
  vTaskDelay(pdMS_TO_TICKS(100));

  if (SD.exists("/cashOutSyn.txt")) {

    File fileOut = SD.open("/cashOutSyn.txt");
    String contenidoArchivo = fileOut.readString();

    fileOut.close();

    httpSync.begin(urlOut);
    httpSync.addHeader("Authorization", token);
    httpSync.addHeader("Content-Type", "text/plain");
    httpSync.setTimeout(5000);

    int ResponseCodeOut = httpSync.POST(contenidoArchivo);
    if (ResponseCodeOut == HTTP_CODE_OK) {
      Serial.printf("Respuesta del servidor: %d\n", ResponseCodeOut);
      Serial.println("Archivo enviado correctamente");
      if (SD.remove("/cashOutSyn.txt")) {
        Serial.println("Archivo cashOutSyn borrado correctamente");
      } else {
        Serial.println("Error al borrar el archivo");
      }
    } else {
      Serial.printf("Error en la solicitud: %s\n", httpSync.errorToString(ResponseCodeOut).c_str());
    }
  }
  httpSync.end();
}

//=========================================================================================================
//Ejecucion primera, definicion de instancias
//=========================================================================================================
void setup() {

  Serial.begin(9600);  // Enviar mensajes
  Serial2.begin(9600);

  pinMode(pinEntradaCoin, INPUT);  // Lectura keyOut Metter
  pinMode(pinEntradaKeyOut, INPUT);
  pinMode(openDoor, INPUT);        // Lectura si se abre la puerta
  pinMode(pinSalida, OUTPUT);      // Envia señal retiro dinero
  pinMode(ledConectando, OUTPUT);  // indicador espera de conexion
  pinMode(ledConexion, OUTPUT);    // Indicador conexion OK
  pinMode(pinbuzzer, OUTPUT);      // Indicador de sonido

  digitalWrite(ledConectando, HIGH);
  digitalWrite(ledConexion, LOW);

  delay(200);
  if (!SD.begin(5)) {
    while (1) {
      delay(1000);
      Serial.println("Error al inicializar la tarjeta microSD.");
    }
  }
  Serial.println("Tarjeta microSD inicializada correctamente.");

  iniciarWifiSD();
  // Bluetooth
  if (!BLE.begin()) {
    while (1) {
      delay(1000);
      Serial.println("Error al iniciar el servicio Bluetooth");
    }
  }
  conteoError = 0;
  char bufferBLE[25];
  strcpy(bufferBLE, "IGS-MC: ");
  strcat(bufferBLE, SERIAL_ID.c_str());

  BLE.setLocalName(bufferBLE);

  Serial.println(bufferBLE);
  BLE.setAdvertisedService(serviceBLE);

  // add the characteristic to the service
  serviceBLE.addCharacteristic(characteristicBLE);
  serviceBLE.addCharacteristic(characteristicGPS);
  serviceBLE.addCharacteristic(characteristicWIFISsid);
  serviceBLE.addCharacteristic(characteristicWIFIPass);
  serviceBLE.addCharacteristic(characteristicLocation);
  BLE.addService(serviceBLE);

  characteristicBLE.writeValue(" ");
  characteristicGPS.writeValue(" ");
  characteristicWIFISsid.writeValue(" ");
  characteristicWIFIPass.writeValue(" ");
  characteristicLocation.writeValue(" ");
  delay(200);
  Serial.println("ESP32 Bluetooth iniciado");
  BLE.advertise();
  delay(100);

  if (!rtc.begin()) {
    while (1) {
      delay(1000);
      Serial.println(F("No se encuentra el RTC"));
    }
  }

  mqttClient.setServer(serverMQTT, port);
  mqttClient.setCallback(callbackMQTT);

  // Configura el servidor NTP
  if (getNTPTime()) {
    rtc.adjust(DateTime(initialTime.tm_year + 1900, initialTime.tm_mon + 1, initialTime.tm_mday,
                        initialTime.tm_hour, initialTime.tm_min, initialTime.tm_sec));
    Serial.print(F("Fecha y hora ajustadas desde Internet: "));
    Serial.println(actualizaFechaHora());
  } else {
    Serial.print("No se pudo obtener la hora de Internet. Usando la hora del RTC: ");
    Serial.println(actualizaFechaHora());
  }

  xTaskCreatePinnedToCore(mainSecond, "main", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mqtt_http, "ping", 10000, NULL, 2, NULL, 0);
}

//---------------------------------------------------------------------------------------------------------
// Loop principal nucleo primario
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1));
  detectaSerialRETIRO();
  iniciarServicioBlue();
}
