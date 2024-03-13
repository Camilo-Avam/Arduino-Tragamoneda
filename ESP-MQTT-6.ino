#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Base64.h>
#include <FS.h>  // Librería para manejar el sistema de archivos
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <SD.h>   // Librería para manejar la tarjeta SD
#include <SPI.h>  // Librería para la comunicación SPI (Serial Peripheral Interface)
#include <time.h>
#include <WiFi.h>

//---------------------------------------------------------------------------------------------------------
// Variables para enviar informacion del serial de la maquina
const String SERIAL_ID = "3112848849";  // Serial de la maquina´
IPAddress MACHINE_IP;

//------------------------------------------------------------------------------------------------------------
// Variables conexion
WiFiClient esp32Client;
PubSubClient mqttClient(esp32Client);

char* serverMQTT = "192.168.10.220";
int port = 1883;
const char* mqtt_clientID = "3112848849";
const char* mqtt_topic_entrada = "Entrada";
const char* mqtt_topic_salida = "Salida";
const char* mqtt_topic_ping = "ping";
// const char* mqtt_last_msg = "3112848849 Disconect";

String resultS = "";
char datos[40];
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 20000;

BLEService serviceBLE("0880dd71-4dbd-474a-9101-7734ca3dd46e");  // Bluetooth® Low Energy LED Service
BLEStringCharacteristic characteristicBLE("9d0983d4-0c6c-45c4-b83b-14abaa6ba18a", BLERead | BLEWrite, 1024);
BLEStringCharacteristic characteristicGPS("9d0983d4-0c65-45c4-b83b-14abaa6ba18c", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFIPass("9d0983d4-0c6f-45c4-b83b-14abaa6ba18d", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFISsid("9d0983d4-0c61-45c4-b83b-14abaa6ba18e", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicLocation("9d0983d4-0c64-45c4-b83b-14abaa6ba18b", BLERead | BLEWrite, 128);

String ssid;
String password;
String location;
// WebServer server(80);
String backendURL = "http://192.168.10.220:8000/";
String token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJjcmVhdGVkIjoiVFZSWk5VNVVUVFZPZW1jMVRXYzlQUT09IiwiZXhwaXJlIjoiVFZSbk1VMTZRVE5PZW1jMVRXYzlQUT09IiwidXNlcklkIjoiVFdjOVBRPT0ifQ.TPaUn1g5FpspFjf0x1nQ3ff4iNFDEBnx-RmcFEfFBAY";

//---------------------------------------------------------------------------------------------------------
// Variables relog y fecha
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
struct tm initialTime;

RTC_DS3231 rtc;

//---------------------------------------------------------------------------------------------------------
// Variables lectura maquina
int dataCoin[250], dataKeyOut[250];
int conteoCoin = 0;
int conteoKeyOut = 0;
int memoriaCoin = 0, memoriaKeyOut = 0;
unsigned long currentMillis;
unsigned long previousMillis = 0;
const unsigned int interval = 350;

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

bool conecto_backend = true;  // Variable para controlar si conecto a MQTT
bool conecto_mqtt = false;
bool wifiConnected = false;
bool synchronization = false;
bool registroCashOut = false;
bool registroBluetooth = false;
String enviarBluettooth;

//=========================================================================================================
// Inicio funciones hilo principal
//=========================================================================================================

String actualizaFechaHora() {
  DateTime now = rtc.now();
  char formattedTime[9];  // Suficiente para "hh:mm:ss" y el carácter nulo

  // Formatea la hora, minuto y segundo con dos dígitos y coloca en formattedTime
  sprintf(formattedTime, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(String(now.month(), DEC) + "/" + String(now.day(), DEC) + "/" + String(now.year(), DEC) + " " + String(formattedTime));
}

//---------------------------------------------------------------------------------------------------------
bool getNTPTime() {
  configTime(gmtOffset_sec, 0, ntpServer);

  if (getLocalTime(&initialTime)) {
    return true;
  } else {
    return false;
  }
}

//---------------------------------------------------------------------------------------------------------
void sumaCeros() {  // sumaCero() Cuenta la cantidad de ceros (0) ingresados en los puertos del ESP32
  previousMillis = currentMillis;
  // Lectura COIN-----------------------------------------------------------------------
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
        if (conecto_backend) {
          sendJson(String(memoriaCoin), "cashin");
        }
        conteoCoin = 0;
        String informacion = "{\"value\": \"" + String(memoriaCoin) + "\",\"date\": \"" + actualizaFechaHora() + "\"},";
        saveSD("/cashIN.txt", informacion);
        if (!conecto_backend) {
          informacion = "{\"v\": \"" + String(memoriaCoin) + "\",\"d\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/cashInSyn.txt", informacion);
        }
      }
      limpiarData();
    } else {
      memoriaCoin = acumuladoCoin;
    }
  }

  // Lectura KEYOUT------------------------------------------------------------------------
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
        enviarBluettooth = "{\"value\":\"" + String(memoriaKeyOut) + "\", \"date\": \"" + actualizaFechaHora() + "\"},";
        registroCashOut = true;

        if (conecto_backend) {
          sendJson(String(memoriaKeyOut), "cashout");
        }

        conteoKeyOut = 0;
        String informacion = "{\"value\":\"" + String(memoriaKeyOut) + "\", \"date\": \"" + actualizaFechaHora() + "\"},";

        saveSD("/cashOUT.txt", informacion);

        if (!conecto_backend) {
          informacion = "{\"v\":\"" + String(memoriaKeyOut) + "\", \"d\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/cashOutSyn.txt", informacion);
        }
      }
      limpiarData();
    } else {
      memoriaKeyOut = acumuladoKeyOut;
    }
  }
}

//---------------------------------------------------------------------------------------------------------
void limpiarData() {  // limpiarData() Limpia la el arreglo donde se almacena la cantidad de ceros
  for (int i = 0; i < 250; i++) {
    dataCoin[i] = 0;
    dataKeyOut[i] = 0;
  }
}

//---------------------------------------------------------------------------------------------------------
void detectaSerial() {  // detectaSerial() Detecta si hay cambios en el serial en la entra de ceros
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    sumaCeros();
  }
  int estadoPinCoin = digitalRead(pinEntradaCoin);  // Se almacenan los ceros del coin
  if (estadoPinCoin == HIGH && estadoAnteriorCoin == LOW) {
    conteoCoin++;
    dataCoin[conteoCoin] = 1;
  }
  estadoAnteriorCoin = estadoPinCoin;

  int estadoPinKeyOut = digitalRead(pinEntradaKeyOut);  // Se almacenan los ceros del keyout
  if (estadoPinKeyOut == HIGH && estadoAnteriorKeyOut == LOW) {
    registroBluetooth = true;
    conteoKeyOut++;
    dataKeyOut[conteoKeyOut] = 1;
  }
  estadoAnteriorKeyOut = estadoPinKeyOut;
}

//---------------------------------------------------------------------------------------------------------
void saveSD(String nombreArchivo, String informacion) {
  File archivo;

  if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt") {
    archivo = SD.open(nombreArchivo, FILE_WRITE);
  } else {
    archivo = SD.open(nombreArchivo, FILE_APPEND);
  }

  if (!archivo) {
    // Si el archivo no existe, créalo en modo de escritura
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
    Serial.println("Datos escritos en el archivo.");
  } else {
    Serial.println("Error al abrir el archivo.");
  }
  archivo.close();
}

//---------------------------------------------------------------------------------------------------------
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
    filePosition--;  // Retroceder un carácter

    archivo.seek(filePosition);         // Mover el puntero de lectura a la nueva posición
    char currentChar = archivo.read();  // Leer el carácter en la nueva posición

    if (currentChar == '\n') {
      newlineCount++;  // Incrementar el contador de líneas nuevas encontradas
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
void sendJson(String valor, String endPoint) {  // sendHttpPOST() Envia la peticion POST al dashboard

  DynamicJsonDocument jsonDoc(256); // Json enviado al http
  JsonObject json = jsonDoc.to<JsonObject>();


  DynamicJsonDocument jsonDoc2(256); // Json enviado a MQTT
  JsonObject json2 = jsonDoc2.to<JsonObject>();

  json["machine_id"] = SERIAL_ID;


  json2["machine_id"] = SERIAL_ID;
  json2["action"] = endPoint;

  if (endPoint == "cashin") {
    json["cashin_in"] = valor;
    json2["cashin_in"] = valor;
  }
  if (endPoint == "cashout") {
    json["cashout_out"] = valor;
    json2["cashout_out"] = valor;
  }
  if (endPoint == "actionlog") {
    json["actionlog_event"] = "Physically";
    json["actionlog_type"] = valor;
  }

  String jsonString2;
  serializeJson(json2, jsonString2);
  const char* message = jsonString2.c_str();
  mqttClient.publish(mqtt_topic_salida, message);

  String jsonString;
  serializeJson(json, jsonString);
  sendHTTP(endPoint, jsonString);
  
}

//---------------------------------------------------------------------------------------------------------
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
void sendHTTP(String endPoint, String json) {

  HTTPClient http;
  http.begin(backendURL + endPoint);
  http.addHeader("Authorization", token);
  if (endPoint == "cashout") {
    http.setTimeout(20000);
  }
  if (endPoint == "cashin") {
    http.setTimeout(20000);
  }

  int httpCode = http.POST(json);
  Serial.println(json);
  Serial.println("enviar http send");
  Serial.println(httpCode);
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_CREATED) {
      Serial.print(F("Respuesta del servidor HTTP_CODE_CREATED:     :"));
      Serial.println(http.getString());
    }
    if (httpCode == HTTP_CODE_OK) {
      Serial.print(F("Respuesta del servidor HTTP_CODE_OK:    :"));
      Serial.println(http.getString());
    }else{
      Serial.print(F("Respuesta del servidor:    :"));
      Serial.println(http.getString());
    }

  } else {
    Serial.print(F("Error en la solicitud HTTP, se obtuvo http.error:    :"));
    Serial.println(http.errorToString(httpCode));
  }
  http.end();
}

//---------------------------------------------------------------------------------------------------------
void iniciarWifiSD() {

  password = readSD("/WIFIpass.txt", "ultimo");
  ssid = readSD("/WIFIssid.txt", "ultimo");
  location = readSD("/location.txt", "ultimo");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  vTaskDelay(pdMS_TO_TICKS(5000));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conexión Wi-Fi establecida. Dirección IP del ESP32: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;

    MACHINE_IP = WiFi.localIP();

  } else {
    wifiConnected = false;
    Serial.println("\nError en la conexión WiFi.");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

//---------------------------------------------------------------------------------------------------------
void iniciarServicioBlue() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Conectado a central BLE");
    Serial.println(central.address());

    while (central.connected()) {

      if (characteristicBLE.written()) {
        if (characteristicBLE.value() == "cashOUT") {
          String contenido = readSD("/cashOUT.txt", "todo");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "retirarCashOUT") {

          registroBluetooth = false;
          registroCashOut = false;

          delay(50);
          digitalWrite(pinSalida, HIGH);
          delay(50);
          digitalWrite(pinSalida, LOW);
          Serial.println(F("KeyOut enviado a la tarjeta"));
          vTaskDelay(pdMS_TO_TICKS(2000));

          Serial.println(registroBluetooth);
          Serial.println(registroCashOut);

          while (!registroCashOut && registroBluetooth) {
            Serial.print(F("......."));
            vTaskDelay(pdMS_TO_TICKS(100));
          }

          vTaskDelay(pdMS_TO_TICKS(500));

          if (registroCashOut) {
            Serial.print(F("......."));
            Serial.println(enviarBluettooth);
            characteristicBLE.writeValue(enviarBluettooth);
            enviarBluettooth = "";
          }
          if (!registroBluetooth) {
            Serial.println("no hay contenido");
            characteristicBLE.writeValue("No hay contenido");
          }
          registroBluetooth = false;
          registroCashOut = false;
        }

        if (characteristicBLE.value() == "ingreso") {
          Serial.println("Ingreso en cash in");
          String contenido = readSD("/cashIN.txt", "todo");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "conectado") {
          Serial.println(wifiConnected);
          characteristicBLE.writeValue(String(wifiConnected));
        }
        if (characteristicBLE.value() == "reset") {
          characteristicBLE.writeValue("RESET OK");
          Serial.println("Reinicio");

          delay(100);
          String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/reset.txt", informacion);

          delay(4000);
          esp_restart();
        }
      }
      if (characteristicWIFISsid.written()) {
        String value = characteristicWIFISsid.value();
        saveSD("/WIFIssid.txt", value);
        characteristicWIFISsid.writeValue("SSID OK");
        iniciarWifiSD();
      }
      if (characteristicWIFIPass.written()) {
        String value = characteristicWIFIPass.value();
        saveSD("/WIFIpass.txt", value);
        characteristicWIFIPass.writeValue("PASS OK");
        iniciarWifiSD();
      }
      if (characteristicLocation.written()) {
        String value = characteristicLocation.value();
        saveSD("/location.txt", value);
        characteristicLocation.writeValue("Location OK");
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

//------------------------------------------------------------------------------------------------------
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

    if (mqttClient.connect(mqtt_clientID, "camilo", "12345678", mqtt_topic_ping, 0, false, message, false)) {
      mqttClient.subscribe(mqtt_topic_entrada, 0);
      mqttClient.subscribe(mqtt_topic_ping, 0);
      mqttClient.subscribe(mqtt_topic_salida, 0);

      Serial.println("Conectado");

    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentar de nuevo en 5 segundos");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//---------------------------------------------------------------------------------------------------------
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
    Serial.print("Mensaje recibido [");
    Serial.print(topic);
    Serial.print("] ");

    Serial.println("Action = " + action);

    if (action == "COLLECT") {

      Serial.println("Ingreso = collect");

      registroBluetooth = false;
      registroCashOut = false;

      delay(50);
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
      vTaskDelay(pdMS_TO_TICKS(500));

      if (registroCashOut) {
        Serial.println("Cash out success");
        String mensaje = "{\"message\": \"Cash out success\" , \"ation\": \"Cash out\"}";
        const char* messageChar = mensaje.c_str();
        mqttClient.publish(mqtt_topic_entrada, messageChar);
      }
      if (!registroBluetooth) {
        Serial.println("no hay contenido");
        String mensaje = "{\"message\": \"NO CONTENT\" , \"ation\": \"Cash out\"}";
        const char* messageChar = mensaje.c_str();
        mqttClient.publish(mqtt_topic_entrada, messageChar);
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
    if (action == "ping_response") {
      conecto_mqtt = true;
      String mensaje = "{\"message\": \"ESP32 -> MQTT -> Backend\" , \"ation\": \"ping_response\"}";
      const char* messageChar = mensaje.c_str();
      mqttClient.publish(mqtt_topic_ping, messageChar);
    }
    if (action == "SYNCHRONIZATION") {
      // syncData();
      String mensaje = "{\"message\": \"Sincronizacion\" , \"ation\": \"SYNCHRONIZATION\"}";
      mqttClient.publish("Salida", "Se sincronizo ");
    }
  }
}

//=========================================================================================================
// Inicio funciones hilo secundario
//=========================================================================================================
void mainSecond(void* parameter) {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1));
    // Serial.println("punto 1");
    if (!wifiConnected) {
      detectaSerial();
      leerBuzzer();
    }
    if (wifiConnected) {
      detectaSerial();
      leerBuzzer();

      if (WiFi.status() == !WL_CONNECTED) {
        wifiConnected = false;
        synchronization = false;
        Serial.println("No hay conexion con red");
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

//---------------------------------------------------------------------------------------------------------
void syncData() {
  HTTPClient httpSync;
  String url = backendURL + "synchronization";

  File fileOUT = SD.open("/cashOutSyn.txt", FILE_READ);
  File fileIN = SD.open("/cashInSyn.txt", FILE_READ);
  int sizeIN = fileIN.size();
  int sizeOUT = fileOUT.size();

  int sizeFile = (sizeIN + sizeOUT) * 2;
  Serial.println(sizeFile);
  String jsonString;
  DynamicJsonDocument jsonDoc(sizeFile);
  JsonObject json = jsonDoc.to<JsonObject>();
  json["machine_id"] = SERIAL_ID;
  vTaskDelay(pdMS_TO_TICKS(100));

  if (fileIN) {
    String fileContent = fileIN.readString();
    fileIN.close();
    // SD.remove("/cashInSyn.txt");
    json["sync cashIN"] = "[" + fileContent + "]";
  }

  if (fileOUT) {
    String fileContent = fileOUT.readString();
    fileOUT.close();
    // SD.remove("/cashOutSyn.txt");
    json["sync cashOut"] = "[" + fileContent + "]";
  }

  serializeJson(json, jsonString);

  vTaskDelay(pdMS_TO_TICKS(100));

  httpSync.begin(url);
  httpSync.addHeader("Authorization", token);
  httpSync.setTimeout(5000);

  int httpCode = httpSync.POST(jsonString);
  Serial.print("Se envio:  :");
  Serial.println(jsonString);
  Serial.print("Codigo respuesta: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.print("Respuesta del servidor: ");
      Serial.println(httpSync.getString());
      synchronization = true;
    }
  } else {
    Serial.print("Error en la solicitud HTTP, se obtuvo http.error:     :");
    Serial.println(httpSync.errorToString(httpCode));
  }
  httpSync.end();
}

//------------------------------------------------------------------------------------------------------
void mqtt_http(void* parameter) {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!mqttClient.connected()) {
      reconnect();
    }
    mqttClient.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastPingTime >= pingInterval) {
      lastPingTime = currentMillis;
      enviarPingHTTP();
    }
    if (currentMillis - (lastPingTime * 3) >= pingInterval && !conecto_mqtt) {
      digitalWrite(ledConexion, LOW);
      digitalWrite(ledConectando, HIGH);
    }
  }
}

//------------------------------------------------------------------------------------------------------
void enviarPingHTTP() {
  // mqttClient.publish("pingEsp", jsonString); // Codigo para enviar a mqtt
  if (wifiConnected) {

    DynamicJsonDocument jsonDoc(516);
    JsonObject json = jsonDoc.to<JsonObject>();

    json["machine_serial"] = SERIAL_ID;
    json["machine_ip"] = MACHINE_IP;
    json["location_id"] = location;
    json["action"] = "ping";


    String jsonString;
    serializeJson(json, jsonString);
    const char* message = jsonString.c_str();

    mqttClient.publish(mqtt_topic_ping, message);

    // HTTPClient httpPING;
    // httpPING.begin(backendURL + "machinelocations");
    // httpPING.addHeader("Authorization", token);
    // httpPING.setTimeout(20000);

    // int httpCode = httpPING.POST(jsonString);
    // Serial.print("Se envio:  :");
    // Serial.println(jsonString);
    // Serial.print("Codigo respuesta: ");
    // Serial.println(httpCode);

    // if (httpCode > 0) {
    //   if (httpCode == HTTP_CODE_OK) {
    //     Serial.print("Respuesta del servidor: ");
    //     Serial.println(httpPING.getString());
    //   }
    // } else {
    //   Serial.print("Error en la solicitud HTTP, se obtuvo http.error:     :");
    //   Serial.println(httpPING.errorToString(httpCode));
    //   digitalWrite(ledConexion, LOW);
    //   digitalWrite(ledConectando, HIGH);
    // }
    // httpPING.end();
  }
}

//=========================================================================================================
//Ejecucion
//=========================================================================================================
void setup() {

  Serial.begin(9600);
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
      Serial.println("starting Bluetooth® Low Energy module failed!");
    }
  }

  char bufferBLE[20];
  strcpy(bufferBLE, "ESP32: ");
  strcat(bufferBLE, SERIAL_ID.c_str());

  BLE.setLocalName(bufferBLE);
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
  Serial.println("ESP32 Bluetooth");
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
    Serial.print("Fecha y hora ajustadas desde Internet: ");
    Serial.println(actualizaFechaHora());
  } else {
    Serial.print("No se pudo obtener la hora de Internet. Usando la hora del RTC: ");
    Serial.println(actualizaFechaHora());
  }

  xTaskCreatePinnedToCore(mainSecond, "main", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mqtt_http, "ping", 10000, NULL, 2, NULL, 0);
  // xTaskCreatePinnedToCore(callbackMQTT, "mqtt", 10000, NULL, 2, NULL, 0);
}

//---------------------------------------------------------------------------------------------------------
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
  iniciarServicioBlue();
}
