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
#include <WiFiClientSecure.h>

//=========================================================================================================
// Definicion de variables
//=========================================================================================================

//---------------------------------------------------------------------------------------------------------
// Variables para enviar informacion del serial de la maquina
const String SERIAL_ID = "CG-777";  // Serial de la maquina´
const char* mqtt_clientID = "CG-777";
IPAddress MACHINE_IP;

//---------------------------------------------------------------------------------------------------------
// Variables conexion MQTT
WiFiClientSecure esp32Client;
PubSubClient mqttClient(esp32Client);

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEVzCCAj+gAwIBAgIRALBXPpFzlydw27SHyzpFKzgwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n" \
"WhcNMjcwMzEyMjM1OTU5WjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
"RW5jcnlwdDELMAkGA1UEAxMCRTYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAATZ8Z5G\n" \
"h/ghcWCoJuuj+rnq2h25EqfUJtlRFLFhfHWWvyILOR/VvtEKRqotPEoJhC6+QJVV\n" \
"6RlAN2Z17TJOdwRJ+HB7wxjnzvdxEP6sdNgA1O1tHHMWMxCcOrLqbGL0vbijgfgw\n" \
"gfUwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcD\n" \
"ATASBgNVHRMBAf8ECDAGAQH/AgEAMB0GA1UdDgQWBBSTJ0aYA6lRaI6Y1sRCSNsj\n" \
"v1iU0jAfBgNVHSMEGDAWgBR5tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcB\n" \
"AQQmMCQwIgYIKwYBBQUHMAKGFmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0g\n" \
"BAwwCjAIBgZngQwBAgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVu\n" \
"Y3Iub3JnLzANBgkqhkiG9w0BAQsFAAOCAgEAfYt7SiA1sgWGCIpunk46r4AExIRc\n" \
"MxkKgUhNlrrv1B21hOaXN/5miE+LOTbrcmU/M9yvC6MVY730GNFoL8IhJ8j8vrOL\n" \
"pMY22OP6baS1k9YMrtDTlwJHoGby04ThTUeBDksS9RiuHvicZqBedQdIF65pZuhp\n" \
"eDcGBcLiYasQr/EO5gxxtLyTmgsHSOVSBcFOn9lgv7LECPq9i7mfH3mpxgrRKSxH\n" \
"pOoZ0KXMcB+hHuvlklHntvcI0mMMQ0mhYj6qtMFStkF1RpCG3IPdIwpVCQqu8GV7\n" \
"s8ubknRzs+3C/Bm19RFOoiPpDkwvyNfvmQ14XkyqqKK5oZ8zhD32kFRQkxa8uZSu\n" \
"h4aTImFxknu39waBxIRXE4jKxlAmQc4QjFZoq1KmQqQg0J/1JF8RlFvJas1VcjLv\n" \
"YlvUB2t6npO6oQjB3l+PNf0DpQH7iUx3Wz5AjQCi6L25FjyE06q6BZ/QlmtYdl/8\n" \
"ZYao4SRqPEs/6cAiF+Qf5zg2UkaWtDphl1LKMuTNLotvsX99HP69V2faNyegodQ0\n" \
"LyTApr/vT01YPE46vNsDLgK+4cL6TrzC/a4WcmF5SRJ938zrv/duJHLXQIku5v0+\n" \
"EwOy59Hdm0PT/Er/84dDV0CSjdR/2XuZM3kpysSKLgD1cKiDA+IRguODCxfO9cyY\n" \
"Ig46v9mFmBvyH04=\n" \
"-----END CERTIFICATE-----\n";

const char* serverMQTT = "mqtt.data24-7gaming.com";
const char* userMQTT = "data_24_7_@pp";
const char* passMQTT = "d4t@247#!$g4mming";
const int port = 8883;
const char* mqtt_topic_entrada = "Entrada";
const char* mqtt_topic_ping = "ping";

//---------------------------------------------------------------------------------------------------------
// Variables conexion BLE
BLEService serviceBLE("0880dd71-4dbd-474a-9101-7734ca3dd46e");
BLEStringCharacteristic characteristicBLE("9d0983d4-0c6c-45c4-b83b-14abaa6ba18a", BLERead | BLEWrite, 1024);
BLEStringCharacteristic characteristicCashOut("9d0983d4-0c6e-45c4-b83b-14abaa6ba17b", BLERead | BLEWrite | BLENotify, 1024);
BLEStringCharacteristic characteristicGPS("9d0983d4-0c65-45c4-b83b-14abaa6ba18c", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFIPass("9d0983d4-0c6f-45c4-b83b-14abaa6ba18d", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFISsid("9d0983d4-0c61-45c4-b83b-14abaa6ba18e", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicLocation("9d0983d4-0c64-45c4-b83b-14abaa6ba18b", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicCent("9d0983d5-0c64-45c4-b83b-14abaa6ba19a", BLERead | BLEWrite, 128);

//---------------------------------------------------------------------------------------------------------
// Variables conexion WIFI
String ssid;
String password;
String location;
String gps;
String backendURL = "http://backend.data24-7gaming.com/";
// String backendURL = "http://192.168.10.220:8000/";
String token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJjcmVhdGVkIjoiVFZSamVVMUVWVEZQVkdNeFRYYzlQUT09IiwiZXhwaXJlIjoiVFdwQmVrNVVhM2hQVkdNeFRYYzlQUT09IiwidXNlcklkIjoiVFZFOVBRPT0ifQ.lHQrQl7Ikf-U_-Ig7QuGrXjz2dNHZGZYFarbCoDsrXY";

//---------------------------------------------------------------------------------------------------------
// Variables relog y fecha
const char* ntpServer = "pool.ntp.org";
const int gmtOffset_sec = -14400;
struct tm initialTime;

RTC_DS3231 rtc;

//---------------------------------------------------------------------------------------------------------
// Variables lectura maquina, definicion de pines de entrada
String resultS = "";

int conteoCoin = 0;
int conteoKeyOut = 0;
byte conteoError = 0;
int memoriaCoin = 0;
float memoriaKeyOut = 0;
unsigned long currentMillisCoin;
unsigned long currentMillisKeyOut;
unsigned long previousMillisCoin = 0;
unsigned long previousMillisKeyOut = 0;
const unsigned int intervalCoin = 400;
const unsigned int intervalKeyOut = 800;

byte pinEntradaCoin = 35;
byte pinEntradaKeyOut = 34;
byte pinSalida = 2;
byte ledConexion = 13;
byte ledConectando = 14;
byte pinbuzzer = 32;
byte openDoor = 33;
byte pinDesconexionModulo = 12;

byte buzzerActivo;
byte estadoDesconexion; 

//---------------------------------------------------------------------------------------------------------
// Variables estados
int estadoAnteriorCoin = LOW;
int estadoAnteriorKeyOut = LOW;
int estadoAnteriorBuzzer = LOW;
byte estadoAnteriorDesconexion = LOW;

bool conecto = false;
bool wifiConnected = false;
bool synchronization = false;
bool registroCashOut = false;
bool registroBluetooth = false;
bool toggleBleOut = false;
String enviarBluettooth;

//---------------------------------------------------------------------------------------------------------
// Variable centaderia
int centavos = 0;

//---------------------------------------------------------------------------------------------------------
// Variables tiempo espera ejecucion de conectarse a WIFI
unsigned long tiempoAnteriorWifi;
unsigned int intervaloWifi = 20000; 
unsigned long currentMillisWifi;

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
void detectaSerialINGRESO() {
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
          if (conecto) {
            sendJson(String(memoriaCoin), "cashin");
          }
          conteoCoin = 0;
          if (!conecto) {
            informacion = "{\"v\": \"" + String(memoriaCoin) + "\",\"d\": \"" + actualizaFechaHora() + "\"},";
            saveSD("/cashInSyn.txt", informacion);
            synchronization = false;
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
  }
  estadoAnteriorCoin = estadoPinCoin;
}

void detectaSerialRETIRO() {

  currentMillisKeyOut = millis();
  if (currentMillisKeyOut - previousMillisKeyOut >= intervalKeyOut) {
    previousMillisKeyOut = currentMillisKeyOut;
    int acumuladoKeyOut = 0;
    if (conteoKeyOut != memoriaKeyOut) {
      memoriaKeyOut = conteoKeyOut;
    } else {
      if (conteoKeyOut == memoriaKeyOut && memoriaKeyOut != 0) {

        if (conteoError > 1) {
          float centavosConteo;

          if (centavos == 1) {        // 1 centavo
            centavosConteo = memoriaKeyOut / 100;
          } else if (centavos == 2) { // 5 centavos
            centavosConteo = memoriaKeyOut / 20;
          } else if (centavos == 3) { // 10 centavos
            centavosConteo = memoriaKeyOut / 10;
          } else if (centavos == 4) { // 25 centavos
            centavosConteo = memoriaKeyOut / 4;
          }else if (centavos == 5) { // 50 centavos
            centavosConteo = memoriaKeyOut / 2;
          }else {
            centavosConteo = memoriaKeyOut;
          }
          Serial.println("Pulsos:  " + String(memoriaKeyOut));
          Serial.println("Retiro USD:  " + String(centavosConteo));
          enviarBluettooth = "{\"value\":\"" + String(centavosConteo) + "\"}";
          String informacion = "{\"value\":\"" + String(centavosConteo) + "\", \"date\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/cashOUT.txt", informacion);
          registroCashOut = true;
          if (conecto) {
            sendJson(String(centavosConteo), "cashout");
          }
          conteoKeyOut = 0;

          if (!conecto) {
            informacion = "{\"v\":\"" + String(centavosConteo) + "\", \"d\": \"" + actualizaFechaHora() + "\"},";
            saveSD("/cashOutSyn.txt", informacion);
            synchronization = false;
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
  String nuevaOrden;

  if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt" || nombreArchivo == "/centaderia.txt" || nombreArchivo == "/gps.txt" ) {
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

  if (informacion != "") {
    String informacion64 = base64::encode(informacion);
    int len = informacion64.length();
    int partSize = len / 4;
    String parte1 = informacion64.substring(0, partSize);
    String parte2 = informacion64.substring(partSize, 2 * partSize);
    String parte3 = informacion64.substring(2 * partSize, 3 * partSize);
    String parte4 = informacion64.substring(3 * partSize, len);

    nuevaOrden = parte3 + parte1 + parte4 + parte2;
  } else {
    nuevaOrden = "";
  }

  if (archivo) {
    if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt" || nombreArchivo == "/centaderia.txt") {
      archivo.print(nuevaOrden);
    } else {
      if (nuevaOrden != "") {
        archivo.println(nuevaOrden);
      }
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
    return "";
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
    String nuevaOrden;
    String linea64 = archivo.readStringUntil('\n');

    int len = linea64.length();
    int partSize = len / 4;
    String parte1 = linea64.substring(0, partSize);
    String parte2 = linea64.substring(partSize, 2 * partSize);
    String parte3 = linea64.substring(2 * partSize, 3 * partSize);
    String parte4 = linea64.substring(3 * partSize, len);

    nuevaOrden = parte2 + parte4 + parte1 + parte3;
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
// Funcion leer alarma
void leerBuzzer() {
  
  buzzerActivo = digitalRead(openDoor);
  
  if (buzzerActivo == HIGH && estadoAnteriorBuzzer == LOW) {
    digitalWrite(pinbuzzer, HIGH);
    delay(100);
    sendJson("Open Door", "actionlog");
    String informacion = "{\"Oppen door\": \"" + actualizaFechaHora() + "\"}";
    saveSD("/openDoor.txt", informacion);
  } else if (buzzerActivo == LOW) {
    digitalWrite(pinbuzzer, LOW);
  }
  estadoAnteriorBuzzer = buzzerActivo;

  estadoDesconexion = digitalRead(pinDesconexionModulo);

  if (estadoDesconexion == LOW && estadoAnteriorDesconexion == HIGH) {
    digitalWrite(pinbuzzer, HIGH);
    delay(200);
    digitalWrite(pinbuzzer, LOW);
    sendJson("Module disconect", "actionlog");
    
  }
  estadoAnteriorDesconexion = estadoDesconexion;
  
}

//---------------------------------------------------------------------------------------------------------
// Funciones enviar datos HTTP
void sendJson(String valor, String endPoint) {

  DynamicJsonDocument jsonDoc(256);
  JsonObject json = jsonDoc.to<JsonObject>();

  json["machine_id"] = SERIAL_ID;

  if (endPoint == "cashin") {
    json["cashin_in"] = valor;
    json["action"] = "cashin";
  }
  if (endPoint == "cashout") {
    json["cashout_out"] = valor;
  }
  if (endPoint == "actionlog") {
    json["actionlog_type"] = "Physically";
    json["actionlog_event"] = valor;
    json["action"] = "event";
  }
  if (endPoint == "collect/received/") {
    json["action"] = valor;
  }
  

  String jsonString;
  serializeJson(json, jsonString);

  if (endPoint == "cashin" || endPoint == "actionlog") {
    const char* message = jsonString.c_str();
    mqttClient.publish(mqtt_topic_entrada, message);
  } else {
    sendHTTP(endPoint, jsonString);
  }
}

void sendHTTP(String endPoint, String json) {

  HTTPClient http;

  if (endPoint == "collect/received/") {
    http.begin(backendURL + endPoint + SERIAL_ID);
  } else {
    http.begin(backendURL + endPoint);
  }
  http.addHeader("Authorization", token);

  http.setTimeout(5000);

  int httpCode = http.POST(json);
  Serial.println(json);

  if (httpCode == HTTP_CODE_CREATED) {
    Serial.print(F("Respuesta 201:     :"));
    Serial.println(http.getString());
  } else if (httpCode == HTTP_CODE_OK) {
    Serial.print(F("Respuesta 200:    :"));
    Serial.println(http.getString());
  } else {
    Serial.print(F("Respuesta Error"));
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
  centavos = readSD("/centaderia.txt", "ultimo").toInt();
  gps = readSD("/gps.txt", "ultimo");

  password.trim();
  ssid.trim();
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  vTaskDelay(pdMS_TO_TICKS(10000));
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("Conectado a la red: " + ssid);
    Serial.println("Centaderia: " + String(centavos));
    MACHINE_IP = WiFi.localIP();

  } else {
    wifiConnected = false;
    Serial.println("\nError en la conexión WiFi.");
    digitalWrite(ledConectando, HIGH);
    digitalWrite(ledConexion, LOW);
  }
}

//---------------------------------------------------------------------------------------------------------
// FUncion conexxion BLE
//---------------------------------------------------------------------------------------------------------
void iniciarServicioBlue() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Conectado a central BLE: ");
    Serial.println(central.address());
    toggleBleOut = false;
    while (central.connected() && !toggleBleOut) {

      if (characteristicBLE.written()) {
        if (characteristicBLE.value() == "cashOUT") {
          String contenido = readSD("/cashOUT.txt", "todo");
          characteristicBLE.writeValue(contenido);
        }

        if (characteristicBLE.value() == "cashIN") {
          String contenido = readSD("/cashIN.txt", "todo");
          characteristicBLE.writeValue(contenido);
        }

        if (characteristicBLE.value() == "reset") {
          String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/reset.txt", informacion);
          Serial.println("Reinicio");
          String message = "{ \"machine_id\": \"" + SERIAL_ID + "\", \"action\" : \"reset\", \"actionlog_event\" : \"Reset\", \"actionlog_type\" : \"bluetooth\" }";
          mqttClient.publish(mqtt_topic_entrada, message.c_str());
          delay(2000);
          esp_restart();
        }

        if (characteristicBLE.value() == "wifi") {
          String contenido = readSD("/WIFIssid.txt", "ultimo");
          String contenido2 = readSD("/WIFIpass.txt", "ultimo");
          String contenido3 = readSD("/location.txt", "ultimo");
          String contenido4 = readSD("/centaderia.txt", "ultimo");
          characteristicBLE.writeValue("{\"ssid\":\"" + contenido + "\", \"pass\": \"" + contenido2 + "\" , \"location\": \"" + contenido3 + "\" , \"cent\": \"" + contenido4 + "\"},");
        }
      }
      if (characteristicCashOut.written()) {
        if (characteristicCashOut.value() == "retirarCashOUT") {
          toggleBleOut = true;

          registroBluetooth = false;
          registroCashOut = false;
          vTaskDelay(pdMS_TO_TICKS(1000));
          digitalWrite(pinSalida, HIGH);
          vTaskDelay(pdMS_TO_TICKS(50));
          digitalWrite(pinSalida, LOW);
          vTaskDelay(pdMS_TO_TICKS(2000));

          Serial.println(registroBluetooth);
          Serial.println(registroCashOut);

          while (!registroCashOut && registroBluetooth) {
            Serial.print(F("."));
            vTaskDelay(pdMS_TO_TICKS(200));
          }
          vTaskDelay(pdMS_TO_TICKS(4000));

          if (registroCashOut) {
            Serial.print("Se envia BLE:");
            Serial.println(enviarBluettooth);
            characteristicCashOut.writeValue(enviarBluettooth);
            enviarBluettooth = "";
          }
          if (!registroBluetooth) {
            Serial.println("No hay contenido BLE");
            characteristicCashOut.writeValue("No hay contenido");
          }

          registroBluetooth = false;
          registroCashOut = false;
          toggleBleOut = false;
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
      if (characteristicCent.written()) {
        String value = characteristicCent.value();
        saveSD("/centaderia.txt", value);
        characteristicCent.writeValue("CentOK");
        // iniciarWifiSD();
      }
      if (characteristicGPS.written()) {
        String value = characteristicGPS.value();
        saveSD("/gps.txt", value);
        characteristicGPS.writeValue("GpsOK");
        Serial.println("Se guardo el gps");
      }
    }
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }
}

//---------------------------------------------------------------------------------------------------------
// Funciones MQTT
void reconnect() {
  if (!mqttClient.connected()) {

    DynamicJsonDocument jsonDoc(256);
    JsonObject json = jsonDoc.to<JsonObject>();

    json["machine_serial"] = SERIAL_ID;
    json["action"] = "disconnect";

    String jsonString;
    serializeJson(json, jsonString);
    const char* message = jsonString.c_str();

    if (mqttClient.connect(mqtt_clientID, userMQTT, passMQTT, mqtt_topic_ping, 0, false, message, false)) {
      mqttClient.subscribe(mqtt_topic_entrada, 0);

      Serial.println("Conectado a servidor MQTT");

    } else {
      Serial.print("Fallo conexion a servidor MQTT");
      Serial.print(mqttClient.state());
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
    vTaskDelay(pdMS_TO_TICKS(1));

    if (toggleBleOut) {  // Pendiente si se esta retirando dinero, cuando BLE este activo
      detectaSerialRETIRO();
    }
    if (conteoError < 1) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      conteoError = 2;
      conteoCoin = 0;
      conteoKeyOut = 0;
    }
    
    if (wifiConnected && !toggleBleOut) {
      if (!mqttClient.connected()) {

        conecto = false;
        reconnect();
        enviarPingHTTP();
        vTaskDelay(pdMS_TO_TICKS(100));

        if (synchronization == false) {
          synchronization = true;
          syncData();
          
        }

      }

      mqttClient.loop();
      
    } else if (!wifiConnected && !toggleBleOut) {
      tiempoAnteriorWifi = millis();
      if (tiempoAnteriorWifi - currentMillisWifi >= intervaloWifi) {
        currentMillisWifi = tiempoAnteriorWifi;
        iniciarWifiSD();
      }
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
  json["gps"] = gps;

  String jsonString;
  serializeJson(json, jsonString);

  HTTPClient httpPING;
  httpPING.begin(backendURL + "machinelocations");
  httpPING.addHeader("Authorization", token);
  httpPING.setTimeout(10000);

  int httpCode = httpPING.POST(jsonString);
  Serial.print(F("Se envio:  :"));
  Serial.println(jsonString);
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    Serial.print(F("Respuesta 200: "));
    Serial.println(httpPING.getString());
    conecto = true;
    digitalWrite(ledConexion, HIGH);
    digitalWrite(ledConectando, LOW);
  } else if (httpCode == HTTP_CODE_CREATED) {
    Serial.print(F("Respuesta del servidor 201: "));
    Serial.println(httpPING.getString());
    conecto = true;
    digitalWrite(ledConexion, HIGH);
    digitalWrite(ledConectando, LOW);
  } else {
    Serial.print(F("Error en la solicitud HTTP, se obtuvo http.error:     :"));
    Serial.println(httpPING.getString());
    digitalWrite(ledConexion, LOW);
    digitalWrite(ledConectando, HIGH);
    conecto = false;
  }
  vTaskDelay(pdMS_TO_TICKS(100));

  httpPING.begin(backendURL + "machinelocations/ping");
  httpPING.addHeader("Authorization", token);
  DynamicJsonDocument jsonDoc2(128);
  JsonObject json2 = jsonDoc2.to<JsonObject>();
  json2["serial"] = SERIAL_ID;
  String jsonString2;
  serializeJson(json2, jsonString2);
  httpPING.POST(jsonString2);

  if (httpCode == HTTP_CODE_OK) {
    Serial.print(F("Respuesta 200: "));
    Serial.println(httpPING.getString());
    conecto = true;
    digitalWrite(ledConexion, HIGH);
    digitalWrite(ledConectando, LOW);
  } else if (httpCode == HTTP_CODE_CREATED) {
    Serial.print(F("Respuesta del servidor 201: "));
    Serial.println(httpPING.getString());
    conecto = true;
    digitalWrite(ledConexion, HIGH);
    digitalWrite(ledConectando, LOW);
  } else {
    Serial.print(F("Error en la solicitud HTTP, se obtuvo http.ping:     :"));
    Serial.println(httpPING.getString());
    digitalWrite(ledConexion, LOW);
    digitalWrite(ledConectando, HIGH);
    conecto = false;
  }
  httpPING.end();
}

void syncData() {

  Serial.println("Ingreso funcion Sync");
  HTTPClient httpSync;
  String urlIn = backendURL + "synchronization/cashInSyn";
  // String urlOut = backendURL + "synchronization/cashOutSyn";

  vTaskDelay(pdMS_TO_TICKS(100));
  DynamicJsonDocument combinedJson(1024);

  if (SD.exists("/cashInSyn.txt")) {
    File fileIn = SD.open("/cashInSyn.txt");
    String contenidoArchivoIn;
    while (fileIn.available()) {
      String line = fileIn.readStringUntil('\n');
      line.trim();  // Eliminar espacios en blanco alrededor
      contenidoArchivoIn += line + ",";
    }
    fileIn.close();

    JsonArray arrIn = combinedJson.createNestedArray("cashIn");
    arrIn.add(contenidoArchivoIn);
  }

  if (SD.exists("/cashOutSyn.txt")) {
    File fileOut = SD.open("/cashOutSyn.txt");
    String contenidoArchivoOut;
    while (fileOut.available()) {
      String line = fileOut.readStringUntil('\n');
      line.trim();  // Eliminar espacios en blanco alrededor
      contenidoArchivoOut += line + ",";
    }
    fileOut.close();

    // Convertir el contenido del archivo a un objeto JSON y agregarlo al JSON combinado
    JsonArray arrOut = combinedJson.createNestedArray("cashOut");
    arrOut.add(contenidoArchivoOut);
  }

  if (SD.exists("/cashOutSyn.txt") || SD.exists("/cashInSyn.txt")) {
    combinedJson["serial_id"] = SERIAL_ID;
    String jsonToSend;
    serializeJson(combinedJson, jsonToSend);

    Serial.println("Se envia sync" + jsonToSend);
    httpSync.begin(urlIn);
    httpSync.addHeader("Authorization", token);
    httpSync.addHeader("Content-Type", "application/json");
    // httpSync.setTimeout(5000);

    int ResponseCodeIn = httpSync.POST(jsonToSend);

    if (ResponseCodeIn == HTTP_CODE_OK) {
      Serial.printf("Respuesta del servidor: %d\n", ResponseCodeIn);
      Serial.println("Datos enviados correctamente");
      synchronization = true;

      if (SD.exists("/cashInSyn.txt")) {
        if (SD.remove("/cashInSyn.txt")) {
          Serial.println("Archivo cashInSyn borrado correctamente");
        } else {
          Serial.println("Error al borrar el archivo ");
        }
      }
      if (SD.exists("/cashOutSyn.txt")) {
        if (SD.remove("/cashOutSyn.txt")) {
          Serial.println("Archivo cashOutSyn borrado correctamente");
        } else {
          Serial.println("Error al borrar el archivo");
        }
      }
    } else {
      Serial.println("Error en la solicitud: sync");
      synchronization = false;
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  httpSync.end();
}

//=========================================================================================================
//Ejecucion primera, definicion de instancias
//=========================================================================================================
void setup() {

  delay(5000);
  Serial.begin(9600);
  Serial2.begin(9600);

  pinMode(pinEntradaCoin, INPUT);
  pinMode(pinEntradaKeyOut, INPUT);
  pinMode(openDoor, INPUT);        // Lectura si se abre la puerta
  pinMode(pinSalida, OUTPUT);      // Envia señal retiro dinero
  pinMode(ledConectando, OUTPUT);  // indicador espera de conexion
  pinMode(ledConexion, OUTPUT);    // Indicador conexion OK
  pinMode(pinbuzzer, OUTPUT);      // Indicador de sonido
  pinMode(pinDesconexionModulo, INPUT);
  
  digitalWrite(ledConectando, HIGH);
  digitalWrite(ledConexion, LOW);

  delay(200);
  if (!SD.begin(5)) {
    while (1) {
      digitalWrite(ledConexion, HIGH);
      digitalWrite(ledConectando, HIGH);
      delay(500);
      Serial.println("Error al inicializar la tarjeta microSD.");
      digitalWrite(ledConexion, LOW);
      digitalWrite(ledConectando, LOW);
      delay(500);
    }
  }
  Serial.println("Tarjeta microSD inicializada correctamente.");

  iniciarWifiSD();
  // Bluetooth
  if (!BLE.begin()) {
    while (1) {
      digitalWrite(ledConexion, HIGH);
      digitalWrite(ledConectando, LOW);
      delay(500);
      digitalWrite(ledConexion, LOW);
      digitalWrite(ledConectando, HIGH);
      delay(500);
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

  serviceBLE.addCharacteristic(characteristicBLE);
  serviceBLE.addCharacteristic(characteristicCashOut);
  serviceBLE.addCharacteristic(characteristicGPS);
  serviceBLE.addCharacteristic(characteristicWIFISsid);
  serviceBLE.addCharacteristic(characteristicWIFIPass);
  serviceBLE.addCharacteristic(characteristicLocation);
  serviceBLE.addCharacteristic(characteristicCent);
  BLE.addService(serviceBLE);

  characteristicBLE.writeValue(" ");
  characteristicCashOut.writeValue(" ");
  characteristicGPS.writeValue(" ");
  characteristicWIFISsid.writeValue(" ");
  characteristicWIFIPass.writeValue(" ");
  characteristicLocation.writeValue(" ");
  characteristicCent.writeValue(" ");
  delay(200);
  Serial.println("ESP32 Bluetooth iniciado");
  BLE.advertise();
  delay(100);

  saveSD("/cashIN.txt", "");
  saveSD("/cashOUT.txt", "");

  if (!rtc.begin()) {
    while (1) {
      digitalWrite(ledConexion, LOW);
      digitalWrite(ledConectando, HIGH);
      delay(500);
      digitalWrite(ledConexion, HIGH);
      digitalWrite(ledConectando, LOW);
      delay(1000);
      Serial.println(F("No se encuentra el RTC"));
    }
  }

  esp32Client.setCACert(ca_cert);
  mqttClient.setServer(serverMQTT, port);
  mqttClient.setCallback(callbackMQTT);

  if (getNTPTime()) {
    rtc.adjust(DateTime(initialTime.tm_year + 1900, initialTime.tm_mon + 1, initialTime.tm_mday,
                        initialTime.tm_hour, initialTime.tm_min, initialTime.tm_sec));
    Serial.print(F("Fecha y hora ajustadas desde Internet: "));
    Serial.println(actualizaFechaHora());
  } else {
    Serial.print("No se pudo obtener la hora de Internet. Usando la hora del RTC: ");
    Serial.println(actualizaFechaHora());
  }

  xTaskCreatePinnedToCore(mainSecond, "main", 12000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mqtt_http, "ping", 12000, NULL, 2, NULL, 0);
}

//---------------------------------------------------------------------------------------------------------
// Loop principal nucleo primario
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1));
  detectaSerialRETIRO();
  iniciarServicioBlue();
}
