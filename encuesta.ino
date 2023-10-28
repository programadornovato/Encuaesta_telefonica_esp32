#include <HTTPClient.h>
#include <WiFi.h>
#include <driver/dac.h>
//LLAMAMOS EL AUDIO QUE VAMOS A TRANSMITIR
#include "audio.h"
//CONEXIONES DE NUESTRO 
const char* ssid = "TotalplayDraLovera";
const char* password = "totalDraLove420207";
int pinApagador=35;
WiFiClient client;
//INICIALIZAMOS NUESTRO SERIAL
HardwareSerial sim800(1);
void setup() {
  Serial.begin(9600);
  //OBTENEMOS LA MAC DE NUESTRO ESP32
  String mac = WiFi.macAddress();
  pinMode(pinApagador, INPUT);
  //EXTRAEMOS LOS ULTIMOS 5 CARACTERES DE
  mac = mac.substring(mac.length() - 5);
  Serial.println("MAC address del ESP32: " + mac);
  //HABILITAMOS EL CANAL PARA TRASMITIR AUDIO
  dac_output_enable(DAC_CHANNEL_1);
  //CONFIGURAMOS EL SIM800 PARA CONECTARSE A LOS PINES 19 Y 18
  sim800.begin(9600, SERIAL_8N1, 19, 18);
  //HABILITAMOS LAS NOTIFICACIONES DE LLAMADAS
  sim800.println("AT+CLCC=1");
  delay(1000);
  //HABILITAMOS LAS DETECION DE PRESIONADO DE TECLAS
  sim800.println("AT+DDET=1");
  delay(1000);
  //CONSULTAMOS EL ESTADO DE LA TELEFONIA
  sim800.println("AT+CREG?");
  delay(2000);  // Da tiempo al módulo para responder
  // Verificar la intensidad de señal
  while (sim800.available()) {
      char c = sim800.read();
      Serial.write(c);  // Muestra la respuesta en el Monitor Serial
  }
  //DETECTA LA CALIDAD DE LA SEÑAL
  sim800.println("AT+CSQ");
  bool repeatRequest=true;
  //CICLO QUE RE REPITE HASTA QUE SE CONECTE EL WIFI
  do {
    Serial.print("digitalRead(pinApagador)=");
    Serial.println(digitalRead(pinApagador));
    if (digitalRead(pinApagador) == HIGH) {
      //VARIABLE QUE HACE QUE SE REPITA EL CICLO SI NO SE HAN OBTENIDO LOS DATOS
      repeatRequest = false;
      delay(2000);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Conectando al WiFi...");
      }
      Serial.println("WiFi CONECTADO");
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
  
      // Especificar la URL de la petición
      String url = "https://pastebin.com/raw/sAYWjjxA";
      http.begin(client,url);
  
      // Realizar la petición HTTP GET y obtener el código de estado HTTP
      int httpCode = http.GET();
  
      // Si la petición fue exitosa, procesar la respuesta
      if (httpCode > 0) {
          Serial.print("Código de respuesta: ");
          Serial.println(httpCode);
  
          // Obtener y mostrar la respuesta
          String payload = http.getString();
          Serial.println("Respuesta completa:");
          Serial.println(payload);
          // Extraer la segunda columna
          Serial.println("\nSegunda columna del CSV:");
          int index1 = 0;
          while(index1 < payload.length()) {
              int index2 = payload.indexOf('\n', index1); // Encuentra el final de la línea
              if (index2 == -1) {
                  index2 = payload.length();
              }
  
              String line = payload.substring(index1, index2); // Extrae una línea
              int commaIndex = line.indexOf(','); // Encuentra la coma en la línea
  
              if(commaIndex != -1 && commaIndex < line.length() - 1) {
                  String telefono = line.substring(commaIndex + 1); // Extrae el valor después de la coma
                  Serial.print("LLAMAR A");
                  Serial.println(telefono);
                  delay(1000);
                  makeCall(telefono);
                  delay(5000);
              }
  
              index1 = index2 + 1; // Mueve al inicio de la siguiente línea
          }
  
      } else {
          Serial.print("Error en la petición: ");
          Serial.println(http.errorToString(httpCode));
          // Establece la variable para repetir el proceso
          repeatRequest = true;
          delay(5000);
      }
  
      // Finalizar la conexión
      http.end();
    }
  } while(repeatRequest);
}

void makeCall(String number) {
  //INICIAMOS LA LLAMDA
  sim800.println("ATD+" + number + ";");
  //ESPERAMOS A QUE LA LLAMADA SE HAYA INICIADO
  delay(5000);
  unsigned long startTime = millis();
  bool answered = false;
  //REVISAMOS CADA X SEGUNDOS SI LA LLAMADA FUE CONTESTADA
  while (millis() - startTime < 60000) {
    if (isCallAnswered()) {
      answered = true;
      Serial.println("La llamada ha sido contestada!");
      break;  
    }
    delay(1000);
  }
  //SI SE CONTESTO LA LLAMADA TELEFONICA
  if (answered) {
    //ESPERAMOS 1 SEGUNDO ANTES DE REPRODUCIR EL AUDIO
    delay(1000);
    //REPRODUCIMOS EL AUDIO DE LA ENCUESTA
    audio();
    delay(5000);
    unsigned long dtmfStartTime = millis();
    //ESPERAMOS 2 SEGUNDO PARA ESTAR LEYENYO QUE TECLA PRECIONO
    while (millis() - dtmfStartTime < 20000) {
      String response = readResponse();
      if (response.length() > 0) {
        Serial.print("SIM800: ");
        Serial.println(response);
      }
      //SI SE PRECIONO UNA TECLA
      if (response.indexOf("+DTMF:") >= 0) {
        //CALCULAMOS LA TECLA QUE PRESIONO EL CLIENTE
        int position = response.indexOf("+DTMF:") + 6;
        char pressedKey = response[position];
        //VALIDAMOS LA TECLA QUE SE PRECIONO
        if (pressedKey != '\r' && pressedKey != '\n' && pressedKey != ' ') {
          Serial.print("Se ha presionado la tecla: ");
          Serial.println(pressedKey);
          //COLGAMOS LA LLAMADA
          sim800.println("ATH");
          // Dale un poco de tiempo al módulo para procesar el comando
          delay(1000);
          // Verifica el estado de la llamada
          sim800.println("AT+CLCC");
          break;
        }
      }
      delay(500);
    }
    sim800.println("ATH");
  }
  //SI LA LLAMADA NO FUE CONTESTADA
  else {
    Serial.println("La llamada no ha sido contestada o se ha perdido la conexión.");
    //COLGAMOS LA LLAMADA
    sim800.println("ATH");
  }
  delay(10000);
}

//FUNCION QUE DETECTA SI SE CONTESTO LA LLAMADA
bool isCallAnswered() {
  // CREA UNA VARIABLE DE TIPO STRING PARA ALMACENAR LA RESPUESTA DEL MÓDULO SIM800.
  String response = "";
  // MIENTRAS HAYA DATOS DISPONIBLES PARA LEER DEL MÓDULO SIM800...
  while (sim800.available()) {
    // ...LEE UN CARÁCTER DEL MÓDULO.
    char c = sim800.read();
    response += c;
    // ENVÍA EL CARÁCTER LEÍDO AL MONITOR SERIAL PARA SU VISUALIZACIÓN.
    Serial.write(c);
  }
  // VERIFICA SI LA CADENA DE RESPUESTA CONTIENE EL TEXTO "+CLCC: 1,0,0,0,0".
  return response.indexOf("+CLCC: 1,0,0,0,0") >= 0;
}

//ESTA FUNCIÓN LEE LA RESPUESTA DEL MÓDULO SIM800 Y LA DEVUELVE COMO UNA CADENA DE TEXTO.
String readResponse() {
  String response = "";
  while (sim800.available()) {
    char c = sim800.read();
    response += c;
  }
  return response;
}
//FUNCION QUE TRANSMITE AUDIO AL PIN 25
void audio() {
  int rawDataSize = sizeof(rawData) / sizeof(rawData[0]);
  for (int i = 0; i < rawDataSize; ++i) {
    dacWrite(25, rawData[i]);
    delayMicroseconds(6);
  }
}

void loop() {
  // Aquí podrías agregar más lógica o mantener el ESP32 en un estado de espera.
}
