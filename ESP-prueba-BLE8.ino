#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <FS.h>  // Librería para manejar el sistema de archivos
#include <HTTPClient.h>
#include <RTClib.h>
#include <SD.h>   // Librería para manejar la tarjeta SD
#include <SPI.h>  // Librería para la comunicación SPI (Serial Peripheral Interface)
#include <time.h>
#include <TinyGPSPlus.h>  //incluimos TinyGPS
#include <WebServer.h>
#include <WiFi.h>

BLEService serviceBLE("0880dd71-4dbd-474a-9101-7734ca3dd46e");  // Bluetooth® Low Energy LED Service
BLEStringCharacteristic characteristicBLE("9d0983d4-0c6c-45c4-b83b-14abaa6ba18a", BLERead | BLEWrite, 1024);
BLEStringCharacteristic characteristicGPS("9d0983d4-0c65-45c4-b83b-14abaa6ba18c", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFIPass("9d0983d4-0c6f-45c4-b83b-14abaa6ba18d", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFISsid("9d0983d4-0c61-45c4-b83b-14abaa6ba18e", BLERead | BLEWrite, 128);

TinyGPSPlus gps;
String guardarGPS;

String ssid;
String password;
WebServer server(8080);
String backendURL = "http://192.168.10.220:8000/";
String token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJjcmVhdGVkIjoiVFZSWk5VNVVUVFZPZW1jMVRXYzlQUT09IiwiZXhwaXJlIjoiVFZSbk1VMTZRVE5PZW1jMVRXYzlQUT09IiwidXNlcklkIjoiVFdjOVBRPT0ifQ.TPaUn1g5FpspFjf0x1nQ3ff4iNFDEBnx-RmcFEfFBAY";

//---------------------------------------------------------------------------------------------------------
// Variables para enviar informacion del serial de la maquina
const String SERIAL_ID = "3204555291";  // Serial de la maquina´
IPAddress MACHINE_IP;

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

bool conecto = false;
bool wifiConnected = false;
bool synchronization = false;

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
        if (conecto) {
          sendJson(String(memoriaCoin), "cashin");
        }
        conteoCoin = 0;
        String informacion = "{\"value\": \"" + String(memoriaCoin) + "\",\"date\": \"" + actualizaFechaHora() + "\"},";
        saveSD("/cashIN.txt", informacion);
        if (!conecto) {
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
        if (conecto) {
          sendJson(String(memoriaKeyOut), "cashout");
        }
        conteoKeyOut = 0;
        String informacion = "{\"value\":\"" + String(memoriaKeyOut) + "\", \"date\": \"" + actualizaFechaHora() + "\"},";
        saveSD("/cashOUT.txt", informacion);
        if (!conecto) {
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

  if (archivo) {
    if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt" || nombreArchivo == "/location.txt") {
      archivo.print(informacion);
    } else {
      archivo.println(informacion);
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
  String ultimasLineas[8];
  File archivo = SD.open(nombreArchivo, FILE_READ);
  if (!archivo) {
    Serial.println("Error al abrir el archivo para lectura.");
    return "";
  }

  int indice = 0;
  while (archivo.available()) {
    String linea = archivo.readStringUntil('\n');
    if (linea.length() > 0) {
      if (accion == "todo") {
        ultimasLineas[indice] = linea;
        indice = (indice + 1) % 8;
      }
      if (accion == "ultimo") {
        contenido = linea;
      }
    }
  }
  for (int i = 0; i < 8; i++) {
    int j = (indice + i) % 8;
    contenido += ultimasLineas[j] + "\n";
  }
  archivo.close();
  return contenido;
}
//---------------------------------------------------------------------------------------------------------
void sendJson(String valor, String endPoint) {  // sendHttpPOST() Envia la peticion POST al dashboard

  DynamicJsonDocument jsonDoc(512);
  JsonObject json = jsonDoc.to<JsonObject>();

  if (endPoint == "cashin") {
    json["machine_id"] = SERIAL_ID;
    json["cashin_in"] = valor;
  }
  if (endPoint == "cashout") {
    json["machine_id"] = SERIAL_ID;
    json["cashout_out"] = valor;
  }
  // if (endPoint == "machinelocations") {
  //   json["machine_serial"] = SERIAL_ID;
  //   json["machine_ip"] = MACHINE_IP;
  //   json["location_id"] = 1;
  // }
  if (endPoint == "actionlog") {
    json["actionlog_event"] = "Physically";
    json["actionlog_type"] = valor;
    json["machine_id"] = SERIAL_ID;
  }
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
  http.setTimeout(10000);

  byte httpCode = http.POST(json);
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_CREATED) {
      Serial.print(F("Respuesta del servidor:     :"));
      Serial.println(http.getString());
    }
    if (httpCode == HTTP_CODE_OK) {
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
void sendPostHTTP() {  // keyOut() Envia al ESP32 un Json que activa segun la condicion la salida de USD
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain") == false) {
      server.send(400, "text/plain", "Bad request!");
    } else {
      String message = server.arg("plain");
      message += "\n";
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }
      const String action = doc["action"];
      const String serial = doc["serial_id"];
      Serial.println("Action = " + action);

      if (SERIAL_ID == serial) {
        if (action == "COLLECT") {
          digitalWrite(pinSalida, HIGH);
          delay(300);
          digitalWrite(pinSalida, LOW);
          Serial.println(F("KeyOut enviado a la tarjeta"));
          server.send(200, "application/json", "{\"success\":\"true\"}");
        }
        if (action == "RESET") {
          Serial.println(F("Reinicio"));
          String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/reset.txt", informacion);
          delay(100);
          server.send(200, "application/json", "{\"success\":\"true\"}");
          esp_restart();
        }
        if (action == "GPS") {
          Serial.print(F("Cordenadas:  "));
          searchGPS();
          Serial.println(guardarGPS);
          server.send(200, "application/json", "{\"success\":\"true\"}");
        }
        if (action == "GET_FILE") {
          const String filename = doc["filename"];
          File file = SD.open("/" + filename + ".txt", FILE_READ);
          if (file) {
            server.streamFile(file, "text/plain");
            file.close();
          } else {
            server.send(404, "text/plain", "Archivo no encontrado");
          }
        }
        if (action == "SYNCHRONIZATION") {
          syncData();
          server.send(200, "application/json", "{\"success\":\"true\",\"message\":\"synchronization send\"}");
        }
      }
    }
  }
}
//---------------------------------------------------------------------------------------------------------
void searchGPS() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      Serial.print(F("Location: "));
      if (gps.location.isValid()) {
        guardarGPS = "Latitud/Longitud: " + String(gps.location.lat(), 9) + ", " + String(gps.location.lng(), 9);
      } else {
        Serial.print(F("INVALID"));
      }
    }
  }
}
//---------------------------------------------------------------------------------------------------------
void iniciarWifiSD() {

  File myFile = SD.open("/WIFIpass.txt", FILE_READ);
  if (!myFile) {
    Serial.println("Error al abrir el archivo para lectura.");
  }
  if (myFile.available()) {
    password = myFile.readString();
  }
  myFile.close();
  File myFile2 = SD.open("/WIFIssid.txt", FILE_READ);
  if (!myFile2) {
    Serial.println("Error al abrir el archivo para lectura.");
  }
  if (myFile2.available()) {
    ssid = myFile2.readString();
  }
  myFile2.close();

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  vTaskDelay(pdMS_TO_TICKS(5000));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conexión Wi-Fi establecida. Dirección IP del ESP32: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;

    server.on("/machine/esp", sendPostHTTP);
    server.begin();
    Serial.println("Servidor iniciado");

    MACHINE_IP = WiFi.localIP();
  } else {
    wifiConnected = false;
    Serial.println("\nError en la conexión WiFi.");

    vTaskDelay(pdMS_TO_TICKS(3000));
    // Puedes agregar un retraso antes de intentar nuevamente para evitar una conexión continua.
  }
}

void iniciarServicioBlue() {
  BLEDevice central = BLE.central();
  if (central) {
    Serial.print("Conectado a central BLE");
    Serial.println(central.address());

    while (central.connected()) {
      if (characteristicBLE.written()) {
        if (characteristicBLE.value() == "cashOUT") {  // any value other than 0
          String contenido = readSD("/cashOUT.txt", "todo");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "retirarCashOUT") {  // any value other than 0
          digitalWrite(pinSalida, HIGH);
          delay(300);
          digitalWrite(pinSalida, LOW);
          Serial.println(F("KeyOut enviado a la tarjeta"));
        }
        if (characteristicBLE.value() == "ultimoCashOUT") {  // any value other than 0
          String contenido = readSD("/cashOUT.txt", "ultimo");
          Serial.println("ultimo cash out");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "cashIN") {  // any value other than 0
          String contenido = readSD("/cashIN.txt", "todo");
          Serial.println("todo cash in");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "ultimoCashIN") {  // any value other than 0
          String contenido = readSD("/cashIN.txt", "ultimo");
          Serial.println("ultimo cash in");
          Serial.println(contenido);
          characteristicBLE.writeValue(contenido);
        }
        if (characteristicBLE.value() == "conectado") {  // any value other than 0
          Serial.println(wifiConnected);
          characteristicBLE.writeValue(String(wifiConnected));
        }
        if (characteristicBLE.value() == "reset") {  // any value other than 0
          Serial.println(F("Reinicio"));
          delay(100);
          String informacion = "{\"reset\": \"" + actualizaFechaHora() + "\"},";
          saveSD("/reset.txt", informacion);
          characteristicBLE.writeValue("Tarjeta reiniciada");
          delay(2000);
          esp_restart();
        }
      }
      if (characteristicWIFISsid.written()) {
        String value = characteristicWIFISsid.value();
        saveSD("/WIFIssid.txt", value);
        characteristicWIFISsid.writeValue("Nombre red cambiado: " + value);
        iniciarWifiSD();
      }
      if (characteristicWIFIPass.written()) {
        String value = characteristicWIFIPass.value();
        saveSD("/WIFIpass.txt", value);
        characteristicWIFIPass.writeValue("Clave red cambiado: " + value);
        iniciarWifiSD();
      }
      if (characteristicGPS.written()) {
        String value = characteristicGPS.value();
        saveSD("/location.txt", value);
        characteristicGPS.writeValue("Ubicacion guardada: " + value);
      }
    }
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
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
      server.handleClient();
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

//---------------------------------------------------------------------------------------------------------
void enviarPingHTTP(void* parameter) {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(30000));
    if (conecto && !synchronization) {
      syncData();
    }
    if (wifiConnected) {
      HTTPClient httpPING;
      String url;
      if (!conecto) {
        url = backendURL + "machinelocations";
      } else {
        url = backendURL + "machinelocations/ping";
      }

      httpPING.begin(url);
      httpPING.addHeader("Authorization", token);
      httpPING.setTimeout(10000);

      DynamicJsonDocument jsonDoc(1024);
      JsonObject json = jsonDoc.to<JsonObject>();
      if (!conecto) {
        json["machine_serial"] = SERIAL_ID;
        json["machine_ip"] = MACHINE_IP;
        json["location_id"] = 1;
      } else {
        json["serial"] = SERIAL_ID;
      }

      String jsonString;
      serializeJson(json, jsonString);
      int httpCode = httpPING.POST(jsonString);
      Serial.print("Se envio:  :");
      Serial.println(jsonString);
      Serial.print("Codigo respuesta: ");
      Serial.println(httpCode);
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          Serial.print("Respuesta del servidor: ");
          Serial.println(httpPING.getString());
          if (!conecto) {
            conecto = true;
            digitalWrite(ledConexion, HIGH);
            digitalWrite(ledConectando, LOW);
          }
        }
      } else {
        Serial.print("Error en la solicitud HTTP, se obtuvo http.error:     :");
        Serial.println(httpPING.errorToString(httpCode));
        digitalWrite(ledConexion, LOW);
        digitalWrite(ledConectando, HIGH);
        conecto = false;
      }
      httpPING.end();
    }
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
    Serial.println("Error al inicializar la tarjeta microSD.");
    return;
  }
  Serial.println("Tarjeta microSD inicializada correctamente.");

  iniciarWifiSD();
  // Bluetooth
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
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
  BLE.addService(serviceBLE);

  characteristicBLE.writeValue(" ");
  characteristicGPS.writeValue(" ");
  characteristicWIFISsid.writeValue(" ");
  characteristicWIFIPass.writeValue(" ");
  delay(200);
  Serial.println("ESP32 Bluetooth");
  BLE.advertise();
  delay(100);

  if (!rtc.begin()) {
    Serial.println(F("No se encuentra el RTC"));
    while (1)
      ;
  }
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
  xTaskCreatePinnedToCore(enviarPingHTTP, "ping", 10000, NULL, 2, NULL, 0);
}
//---------------------------------------------------------------------------------------------------------
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
  iniciarServicioBlue();
}
