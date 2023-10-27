#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <FS.h>  // Librería para manejar el sistema de archivos
#include <HTTPClient.h>
#include <SD.h>   // Librería para manejar la tarjeta SD
#include <SPI.h>  // Librería para la comunicación SPI (Serial Peripheral Interface)
#include <time.h>
#include <TinyGPSPlus.h>  //incluimos TinyGPS
#include <WebServer.h>
#include <WiFi.h>

BLEService serviceCashOUT("0880dd71-4dbd-474a-9101-7734ca3dd46e");  // Bluetooth® Low Energy LED Service
BLEStringCharacteristic characteristicCashOUT("9d0983d4-0c6c-45c4-b83b-14abaa6ba18a", BLERead | BLEWrite, 512);
BLEStringCharacteristic characteristicCashIN("9d0983d4-0c6d-45c4-b83b-14abaa6ba18b", BLERead | BLEWrite, 512);
BLEStringCharacteristic characteristicReset("9d0983d4-0c6e-45c4-b83b-14abaa6ba18c", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFIPass("9d0983d4-0c6f-45c4-b83b-14abaa6ba18d", BLERead | BLEWrite, 128);
BLEStringCharacteristic characteristicWIFISsid("9d0983d4-0c61-45c4-b83b-14abaa6ba18e", BLERead | BLEWrite, 128);

TinyGPSPlus gps;
String guardarGPS;

String ssid;
String password;
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
int blueConteoIN;
int blueConteoOUT;

int estadoAnteriorCoin = LOW;
int estadoAnteriorKeyOut = LOW;
int estadoAnteriorBuzzer = LOW;

boolean envioDatos = true;
boolean conecto = false;
boolean wifiConnected = false;
//---------------------------------------------------------------------------------------------------------
// Inicio funciones hilo principal
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
        // sendHttpMoney(String(memoriaCoin), "cashin");
        sendJson(String(memoriaCoin), "cashin");
        conteoCoin = 0;
        String informacion = "Valor Ingresado: " + String(memoriaCoin) + "USD fecha: " + actualizaFechaHora();
        saveSD("/cashIN.txt", informacion);
        blueConteoIN = memoriaCoin;
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
        // sendHttpMoney(String(memoriaKeyOut), "cashout");
        sendJson(String(memoriaKeyOut), "cashout");
        conteoKeyOut = 0;
        String informacion = "Valor retirado: " + String(memoriaKeyOut) + "USD, fecha: " + actualizaFechaHora();
        saveSD("/cashOUT.txt", informacion);
        blueConteoOUT = memoriaKeyOut;
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
  if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt") {
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
    if (nombreArchivo == "/WIFIpass.txt" || nombreArchivo == "/WIFIssid.txt") {
      archivo.print(informacion);
    } else {
      archivo.println(informacion);
    }
    archivo.close();
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
        contenido = contenido + ultimasLineas[indice];
        indice = (indice + 1) % 8;
      }
      if (accion == "ultimo") {
        contenido = linea;
      }
    }
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
  if (endPoint == "machinelocations") {
    json["machine_serial"] = SERIAL_ID;
    json["machine_ip"] = MACHINE_IP;
    json["location_id"] = 1;
  }
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
    String informacion = "Puerta abierta: " + actualizaFechaHora();
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
  http.setTimeout(20000);

  byte httpCode = http.POST(json);
  Serial.print(F("Se envio:  :"));
  Serial.println(json);
  delay(100);
  Serial.print(F("Codigo respuesta: "));
  Serial.println(httpCode);
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_CREATED) {
      Serial.print(F("Respuesta del servidor:     :"));
      Serial.println(http.getString());
    }
    if (httpCode == HTTP_CODE_OK) {
      Serial.print(F("Respuesta del servidor:    :"));
      Serial.println(http.getString());
      if (endPoint == "machinelocations") {
        digitalWrite(ledConexion, HIGH);
        conecto = true;
      }
    }
  } else {
    Serial.print(F("Error en la solicitud HTTP, se obtuvo http.error:    :"));
    Serial.println(http.errorToString(httpCode));
    if (endPoint == "machinelocations") { digitalWrite(ledConectando, HIGH); }
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
          delay(100);
          String informacion = "Tarjeta reiniciada: " + actualizaFechaHora();
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
          File file = SD.open("/" + filename, FILE_READ);
          if (file) {
            server.streamFile(file, "text/plain");
            file.close();
          } else {
            server.send(404, "text/plain", "Archivo no encontrado");
          }
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
    // Serial.println(password);
  }
  myFile.close();


  File myFile2 = SD.open("/WIFIssid.txt", FILE_READ);
  if (!myFile2) {
    Serial.println("Error al abrir el archivo para lectura.");
  }
  if (myFile2.available()) {
    ssid = myFile2.readString();
    // Serial.println(ssid);
  }
  myFile2.close();
}

void iniciarServicioBlue() {
  BLEDevice central = BLE.central();
  if (central) {
    Serial.print("Conectado a central: ");
    Serial.println(central.address());

    while (central.connected()) {
      if (characteristicCashOUT.written()) {
        if (characteristicCashOUT.value() == "todo") {  // any value other than 0
          String contenido = readSD("/cashOUT.txt", "todo");
          Serial.println(contenido);
          characteristicCashOUT.writeValue(contenido);
        }
        if (characteristicCashOUT.value() == "retirar") {  // any value other than 0
          digitalWrite(pinSalida, HIGH);
          delay(300);
          digitalWrite(pinSalida, LOW);
          Serial.println(F("KeyOut enviado a la tarjeta"));
        }
        if (characteristicCashOUT.value() == "ultimo") {  // any value other than 0
          String contenido = readSD("/cashOUT.txt", "ultimo");
          Serial.println(contenido);
          characteristicCashOUT.writeValue(contenido);
        }
      }
      if (characteristicCashIN.written()) {
        if (characteristicCashIN.value() == "todo") {  // any value other than 0
          String contenido = readSD("/cashIN.txt", "todo");
          Serial.println(contenido);
          characteristicCashIN.writeValue("" + contenido);
        }
        if (characteristicCashIN.value() == "ultimo") {  // any value other than 0
          String contenido = readSD("/cashIN.txt", "ultimo");
          Serial.println(contenido);
          characteristicCashIN.writeValue(contenido);
        }
      }
      if (characteristicReset.written()) {
        if (characteristicReset.value() == "reset") {  // any value other than 0
          Serial.println(F("Reinicio"));
          delay(100);
          String informacion = "Tarjeta reiniciada: " + actualizaFechaHora();
          saveSD("/reset.txt", informacion);
          esp_restart();
        }
      }
      if (characteristicWIFISsid.written()) {
        String value = characteristicWIFISsid.value();
        saveSD("/WIFIssid.txt", value);
        characteristicWIFISsid.writeValue("Nombre red cambiado: " + value);
      }
      if (characteristicWIFIPass.written()) {
        String value = characteristicWIFIPass.value();
        saveSD("/WIFIpass.txt", value);
        characteristicWIFIPass.writeValue("Clave red cambiado: " + value);
      }
    }
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }
}

//---------------------------------------------------------------------------------------------------------
// Inicio funciones hilo secundario
//---------------------------------------------------------------------------------------------------------
void mainSecond(void* parameter) {
  while (1) {
    while (envioDatos) {
      digitalWrite(ledConectando, HIGH);
      delay(500);
      digitalWrite(ledConectando, LOW);
      delay(100);
      sendJson("", "machinelocations");
      if (conecto) {
        envioDatos = false;
      }
    }
    leerBuzzer();
    server.handleClient();
    detectaSerial();
  }
}

// void enviarPingHTTP(void* parameter) {
//   vTaskDelay(pdMS_TO_TICKS(10000));  // Espera 30 segundos antes de la próxima ejecución
//   while (true) {

//     HTTPClient httpPING;
//     String url = backendURL + "machinelocations/ping";
//     httpPING.begin(url);
//     httpPING.addHeader("Authorization", token);
//     httpPING.setTimeout(20000);
//     DynamicJsonDocument jsonDoc(1024);
//     JsonObject json = jsonDoc.to<JsonObject>();
//     json["serial"] = SERIAL_ID;
//     String jsonString;
//     serializeJson(json, jsonString);
//     int httpCode = httpPING.POST(jsonString);
//     Serial.print("Se envio:  :");
//     Serial.println(jsonString);
//     if (httpCode > 0) {
//       if (httpCode == HTTP_CODE_OK) {
//         Serial.print("Codigo respuesta: ");
//         Serial.println(httpCode);
//         Serial.print("Respuesta del servidor: ");
//         Serial.println(httpPING.getString());
//       }
//     } else {
//       Serial.print("Codigo respuesta: ");
//       Serial.println(httpCode);
//       Serial.print("Error en la solicitud HTTP, se obtuvo http.error:     :");
//       Serial.println(httpPING.errorToString(httpCode));
//     }
//     httpPING.end();
//     xSemaphoreGive(xSemaphore);  // Release the semaphore
//     delay(10000);
//   }
// }
//---------------------------------------------------------------------------------------------------------
//Ejecucion
//---------------------------------------------------------------------------------------------------------
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

  if (!SD.begin(5)) {
    Serial.println("Error al inicializar la tarjeta microSD.");
    return;
  }
  delay(200);
  Serial.println("Tarjeta microSD inicializada correctamente.");
  delay(200);

  iniciarWifiSD();

  delay(200);
  // Bluetooth
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }
  BLE.setLocalName("ESP32 BLE");
  BLE.setAdvertisedService(serviceCashOUT);

  // add the characteristic to the service
  serviceCashOUT.addCharacteristic(characteristicCashOUT);
  serviceCashOUT.addCharacteristic(characteristicCashIN);
  serviceCashOUT.addCharacteristic(characteristicReset);
  serviceCashOUT.addCharacteristic(characteristicWIFISsid);
  serviceCashOUT.addCharacteristic(characteristicWIFIPass);
  BLE.addService(serviceCashOUT);

  characteristicCashOUT.writeValue(" ");
  characteristicCashIN.writeValue(" ");
  characteristicReset.writeValue(" ");
  characteristicWIFISsid.writeValue(" ");
  characteristicWIFIPass.writeValue(" ");
  delay(200);
  Serial.println("ESP32 Bluetooth");
  BLE.advertise();

  delay(300);
  do {

    iniciarServicioBlue();
    delay(300);
    iniciarWifiSD();
    // Lectura WIFI
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);
    if (WiFi.status() != WL_CONNECTED) {
      for (int i = 0; i < 30; i++) {
        Serial.print(".");
        delay(100);
      }
    }
    delay(200);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("Conexión Wi-Fi establecida. Dirección IP del ESP32: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true;
    } else {
      wifiConnected = false;
      Serial.println("\nError en la conexión WiFi. Verifica el nombre o la clave de red.");
      // Puedes agregar un retraso antes de intentar nuevamente para evitar una conexión continua.
      delay(5000);  // Espera 5 segundos antes de intentar nuevamente.
    }

  } while (!wifiConnected);
  MACHINE_IP = WiFi.localIP();

  // Configura el servidor NTP
  configTime(gmtOffset_sec, 0, ntpServer);
  if (getLocalTime(&initialTime)) {
    Serial.print("Fecha y hora inicial obtenidas de Internet: ");
    Serial.println(&initialTime, "%A, %B %d %Y %H:%M:%S");
  }
  // Servidor
  server.on("/machine/cash_out", sendPostHTTP);
  server.begin();
  Serial.println("Servidor iniciado");

  // xSemaphore = xSemaphoreCreateBinary();  // Set the semaphore as binary
  // xTaskCreatePinnedToCore(enviarPingHTTP, "EnviarPing", 2000, NULL, 1, NULL, 0);
  // xTaskCreatePinnedToCore(task2,"2000", 1000, NULL, 1, NULL, 0 );
  xTaskCreatePinnedToCore(mainSecond, "blue", 5000, NULL, 1, NULL, 0);
}
//---------------------------------------------------------------------------------------------------------
void loop() {
  iniciarServicioBlue();
  delay(100);
}