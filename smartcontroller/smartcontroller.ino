

// SmartGrow Controller - Firmware version 1.3 
// Designed by irovagna
// 1 relay / oled screen / moisture sensor / arduino pump 5v / Telegram bot comunication

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>  // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <ESP32Time.h>
ESP32Time rtc;

#include <DHT.h>
#include <DHT_U.h>
DHT dht(19, DHT11);

// OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <EEPROM.h>
#define EEPROM_SIZE 64

// Reemplazar con los datos de tu red wifi
const char* ssid = "OneLove 2.4GHz";
const char* password = "facilyrapido";

// Telegram Bot ADMIN
String ADMIN_ID = "1382056401";
// Max registered telegram IDs
#define MAX_IDS 5
String telegram_ids[MAX_IDS];
int idIndex = 0;

//Token de Telegram BOT se obtenienen desde Botfather en telegram
#define BOTtoken "5864187645:AAGdh5JqR0qH21OGck9E0pG88GdO-AVrdHA"  // your Bot Token (Get from Botfather)
#define CHAT_ID "100708637663"
//#define CHAT_ID "1382056401"

// Config Server NTP - Actualizar hora
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

float temp, hume;

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PUMP_PIN 32

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

// Sensor Humedad Tierra
const int AirValue = 3580;    //you need to replace this value with Value_1
const int WaterValue = 1295;  //you need to replace this value with Value_2
const int SensorPin = 36;
int soilMoistureValue = 0;
int soilmoisturepercent = 0;

int relay = 4;  // RELAY PIN for ESP32 microcontroller

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

const int ledPin = 2;
bool ledState = LOW;
bool luzState;

int Direcion = 0;

static int hora1;
static int minuto1;
static int hora0;
static int minuto0;
static int timerLampara;



void setup() {

  Serial.begin(115200);
  String chat_id = CHAT_ID;
  dht.begin();

  pinMode(relay, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);  // Set pin as OUTPUT

  //Init EEPROM
  EEPROM.begin(EEPROM_SIZE);

  hora1 = EEPROM.read(0);
  minuto1 = EEPROM.read(2);
  hora0 = EEPROM.read(5);
  minuto0 = EEPROM.read(7);
  timerLampara = EEPROM.read(10);
  luzState = EEPROM.read(13);


   Serial.println(" lampara " + String(luzState));
   Serial.println(EEPROM.read(13));



  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    Serial.println(F("SSD1306 allocation correctly"));
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000);  // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!

  display.setTextColor(WHITE);
  testdrawline();
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(11, 25);
  display.println("SmartGrow");
  display.display();
  delay(2000);
  display.invertDisplay(true);
  delay(2000);
  display.invertDisplay(false);
  delay(2000);
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  


#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  client.setTrustAnchors(&cert);     // Add root certificate for api.telegram.org
#endif

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
#endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("Connecting to WiFi.. \n");
    display.setCursor(0, 35);
    display.print(ssid);
    display.display();
    delay(3000);
  }

  // Print ESP32 Local IP Address

  if (WiFi.status() != WL_CONNECTED) {  // Connect to Wi-Fi fails
    display.clearDisplay();
    display.setCursor(0, 15);
    display.print("Error en conexion WIFI");
    display.setCursor(0, 35);
    display.print(ssid);
    display.display();
    delay(3000);
  } else {
    Serial.println(WiFi.localIP());
    display.clearDisplay();
    display.setCursor(10, 15);
    display.print(ssid);
    display.setCursor(10, 25);
    display.print("WIFI CONECTADO");
    display.setCursor(15, 40);
    display.println(WiFi.localIP());

    display.display();
    delay(3000);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }

  // --------------- Alarma Lampara ON / OFF -----------------------

  setTime(rtc.getHour(), rtc.getMinute(), rtc.getSecond(), rtc.getMonth(), rtc.getDay(), rtc.getYear());

  if (timerLampara == 1) {
    Alarm.alarmRepeat(hora1, minuto1, 0, lamparaOn);   // 8:30am every day
    Alarm.alarmRepeat(hora0, minuto0, 0, lamparaOff);  // 5:45pm every day
  }

  //----------------------------------------------------------------------------

  bot.sendMessage(chat_id, "SmartGrow iniciado. \n Escribe /menu para ver las funciones", "");

  display.setCursor(20, 20);
  display.setTextSize(1);
  display.clearDisplay();
  display.print("Telegram Bot ");
  display.setCursor(32, 40);
  display.print("ACTIVADO");
  display.display();
  delay(3000);

  hume = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(temp) || isnan(hume)) {
    display.clearDisplay();
    display.setCursor(10, 18);
    display.println("Conexion sensor Temperatura fallida");
    bot.sendMessage(chat_id, "Conexion sensor Temperatura fallida");
    display.display();
    delay(2000);
  } else {
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 18);
    display.print("Conexion establecida");
    display.setCursor(0, 38);
    display.print("Sensor Temperatura");
    display.display();
    delay(2000);
  }


  soilMoistureValue = analogRead(SensorPin);

  if (soilMoistureValue < 900) {
    display.clearDisplay();
    display.setCursor(18, 18);
    display.print("Error conexion");
    display.setCursor(0, 38);
    display.print("Sensor Humedad suelo");
    display.display();

    delay(3000);
  } else {
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 18);
    display.print("Conexion establecida");
    display.setCursor(0, 38);
    display.print("Sensor Humedad suelo");
    display.display();
    delay(3000);
  }

  if (luzState == HIGH){
    lamparaOn();
  }

}


void loop() {


  hume = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(temp) || isnan(hume)) {
    display.clearDisplay();
    display.setCursor(15, 10);
    display.println("Revisar conexion Sensor Temperatura");
    display.display();
    delay(3000);
  }

  leermsg();

  infoGral();

  if (timerLampara == 1) {
    Alarm.delay(1000);
  }

  //testdrawline();

  verTimerLamp();

  leermsg();

  if (timerLampara == 1) {
    Alarm.delay(1000);
  }
}


// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);


    float hume0;
    float temp0;
    hume0 = dht.readHumidity();
    temp0 = dht.readTemperature();

    soilMoistureValue = analogRead(SensorPin);  //put Sensor insert into soil
    Serial.println(soilMoistureValue);
    soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);


    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    display.setCursor(10, 15);
    display.clearDisplay();
    display.print(text);
    display.display();
    delay(2000);

    String from_name = bot.messages[i].from_name;

    String estado;

    String timerEstado;
    if (timerLampara == 1) {
      timerEstado = "ACTIVADO";
    } else {
      timerEstado = "DESACTIVADO";
    }


    if (digitalRead(relay)) {
      estado = "ENCENDIDA";
    } else {
      estado = "APAGADA";
    }

    String est_riego;

    if (digitalRead(PUMP_PIN)) {
      est_riego = "ACTIVADO";
    } else {
      est_riego = "DESACTIVADO";
    }


    if (text == "/menu") {
      String welcome = "Hi! " + from_name + ". Welcome to \n";
      welcome += "SmartGrow Menu \n";
      welcome += "Utiliza los siguiente comandos para interactuar con el controlador.\n\n";
      welcome += "/estado Ver SmartGrow information. \n";
      welcome += "/luz_on Para encender la lampara. \n";
      welcome += "/luz_off Para apagar la lampara. \n";
      welcome += "/luz Ver el estado de lampara. \n";
      welcome += "/horarios_lampara Modificar el timer de la lampara. \n";
      welcome += "/horarios_guardados Ver los horarios establecidos de la lampara. \n";
      welcome += "/timer_lampara Ver si el timer de la lampara esta activado o no. \n";
      welcome += "/riego_on Encender el riego. \n";
      welcome += "/riego_off Apagar el riego. \n";
      welcome += "/riego Ver el estado del riego. \n";


      bot.sendMessage(chat_id, welcome, "");
      display.setCursor(7, 34);
      display.clearDisplay();
      display.print("Welcome, " + from_name + ".\n");
      display.display();
      delay(2000);
    }

    if (text == "/luz_on") {
      lamparaOn();
    }

    if (text == "/luz_off") {
      lamparaOff();
    }

    if (text == "/luz") {
      if (digitalRead(relay)) {
        bot.sendMessage(chat_id, "Lampara Encendida", "");
        display.setCursor(0, 15);
        display.clearDisplay();
        display.print("Lampara Encendida by:" + from_name + ".\n");
      } else {
        bot.sendMessage(chat_id, "Lampara Apagada", "");
        display.setCursor(0, 15);
        display.clearDisplay();
        display.print("Lampara Apagada by: " + from_name + ".\n");
      }
    }

    if (text == "/estado") {

      String estado0 = " - SmartGrow information - \n";
      estado0 += "Temperatura: " + String(temp0) + " *C \n";
      estado0 += "Humedad: " + String(hume0) + " % \n";
      estado0 += "Humedad de suelo: " + String(soilmoisturepercent) + " % \n";
      estado0 += "Lampara: " + String(estado) + ".\n";
      estado0 += "Timer Lampara: " + String(timerEstado) + ".\n";
      estado0 += "Riego: " + String(est_riego) + ".\n";

      bot.sendMessage(chat_id, estado0, "");

      display.setCursor(0, 10);
      display.clearDisplay();
      display.print("Temperatura: " + String(temp0) + " *C \n");
      display.print("Humedad: " + String(hume0) + " % \n");
      display.print("Humedad Suelo: " + String(soilmoisturepercent) + " % \n");
      display.print("Lampara: " + String(estado) + ".\n");
      display.print("Printed by:  " + from_name + ".\n");
      display.display();
      delay(2000);
    }

    if (text == "/riego_on") {
      digitalWrite(PUMP_PIN, HIGH);  // Set pin HIGH
      Serial.println("The Water Pump is ON!");
      display.setCursor(0, 15);
      display.clearDisplay();
      display.print("Riego encendido by: " + from_name + ".\n");
      display.display();
      bot.sendMessage(chat_id, "Riego encendido", "");
      delay(3000);  // Wait 3 seconds
    }

    if (text == "/riego_off") {
      digitalWrite(PUMP_PIN, LOW);  // Set pin LOW
      Serial.println("The Water Pump is OFF!");
      display.setCursor(0, 15);
      display.clearDisplay();
      display.print("Riego apagado by: " + from_name + ".\n");
      display.display();
      bot.sendMessage(chat_id, "Riego apagado", "");
      delay(3000);  // Wait 3 seconds
    }

    if (text == "/riego") {
      if (digitalRead(PUMP_PIN)) {
        bot.sendMessage(chat_id, "Riego Encendido", "");
        display.setCursor(0, 15);
        display.clearDisplay();
        display.print("Riego Encendido by:" + from_name + ".\n");
      } else {
        bot.sendMessage(chat_id, "Riego Apagadao", "");
        display.setCursor(0, 15);
        display.clearDisplay();
        display.print("Riego Apagadao by: " + from_name + ".\n");
      }
    }

    if (text == "/horarios_lampara") {
      bot.sendMessage(chat_id, "Para cambiar el horario de encendido de la lampara, escribir en el siguiente mensaje: \n/hl1 HHMM  ---> dos digitos para hora seguido de dos digitos para los minutos. \nEj, /hl1 0930 ---> la lampara se enciende a las 9:30 de la mañana.");
      bot.sendMessage(chat_id, "Para cambiar el horario que se apaga la lampara, escribir en el siguiente mensaje: \n/hl0 HHMM  ---> dos digitos para hora seguido de dos digitos para los minutos. \nEj, /hl0 2330 ---> la lampara se apaga a las 23:30 de la noche.");
      bot.sendMessage(chat_id, "Para ver el horario guardado escribir: \n/horarios_guardados .");
    }

    if (text.indexOf("/hl1") > -1) {

      //extract from the string the ID and the nick. We know that there's a space after "/add", and a space after the ID
      //float horarioLimpio = text.length();
      //int secondSpace = text.indexOf(" ", firstSpace + 1);
      float horarioFlo = text.length();
      if (horarioFlo == 9) {
        int firstSpace = text.indexOf(" ");
        String sHora1 = text.substring(firstSpace + 1, 7);
        String sMinuto1 = text.substring(7, 9);
        hora1 = sHora1.toInt();
        minuto1 = sMinuto1.toInt();
        EEPROM.put(0, hora1);
        EEPROM.put(2, minuto1);
        EEPROM.commit();
        if (timerLampara == 1) {
          Alarm.alarmRepeat(hora1, minuto1, 0, lamparaOn);
        }
        bot.sendMessage(chat_id, from_name + " a modificado el horario de encendido de la lampara a: " + String(hora1) + ":" + String(minuto1) + " hs. \nEscribir /horarios_guardados para ver los horarios establecidos.");
      } else {
        bot.sendMessage(chat_id, "No se a ingresado una fecha correcta, proceso cancelado.");
      }
    }
    // Horario OFF
    if (text.indexOf("/hl0") > -1) {

      float horarioFlo = text.length();
      if (horarioFlo == 9) {
        int firstSpace = text.indexOf(" ");
        String sHora0 = text.substring(firstSpace + 1, 7);
        String sMinuto0 = text.substring(7, 9);
        hora0 = sHora0.toInt();
        minuto0 = sMinuto0.toInt();
        EEPROM.put(5, hora0);
        EEPROM.put(7, minuto0);
        EEPROM.commit();
        if (timerLampara == 1) {
          Alarm.alarmRepeat(hora0, minuto0, 0, lamparaOff);
        }
        bot.sendMessage(chat_id, from_name + " a modificado el horario de apagado de la lampara a: " + String(hora0) + ":" + String(minuto0) + " hs. \nEscribir /horarios_guardados para ver los horarios establecidos.");
      } else {
        bot.sendMessage(chat_id, "No se a ingresado una fecha correcta, proceso cancelado.");
      }
    }
    // Ver horarios Lampara
    if (text.indexOf("/horarios_guardados") > -1) {

      bot.sendMessage(chat_id, from_name + " , estos son los horarios de la lampara guardados: \nEncendido: " + (hora1) + ":" + (minuto1) + " hs. \nApagado: " + (hora0) + ":" + (minuto0) + " hs.");
    }

    if (text == "/timer_lampara") {
      bot.sendMessage(chat_id, "El timer de la Lampara está: " + String(timerEstado) + ".\n");
      bot.sendMessage(chat_id, "Para encender el timer escribir en el siguiente mensaje:\n /tm1 1 ---> La lampara se enciende o apaga segun el horario guardado.\n Para apagar el timer escribir en el siguiente mensaje:\n /tm1 0 ---> La lampara solo se puede encender o apagar con las comandos /luz_on y /luz_off ");
    }

    if (text.indexOf("/tm1") > -1) {

      int firstSpace = text.indexOf(" ");
      String tEstado = text.substring(firstSpace + 1, text.length());
      if (tEstado == "1") {
        timerLampara = 1;
        EEPROM.put(10, timerLampara);
        EEPROM.commit();
        Alarm.alarmRepeat(hora0, minuto0, 0, lamparaOff);
        Alarm.alarmRepeat(hora1, minuto1, 0, lamparaOn);
        
        bot.sendMessage(chat_id, from_name + " El timer de la lampara se activó.\n El horario de encendido es: " + String(hora1) + ":" + String(minuto1) + " hs. \nEl horario de apagado es: " + String(hora0) + ":" + String(minuto0) + " hs. ");
      }
      if (tEstado == "0") {
        timerLampara = 0;
        EEPROM.put(10, timerLampara);
        EEPROM.commit();
        bot.sendMessage(chat_id, from_name + " El timer de la lampara se desactivó.\n---> La lampara solo se puede encender o apagar con las comandos /luz_on y /luz_off");
      }
    }
  }
}



void leermsg() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      display.setTextSize(1);
      Serial.println("Consultando...");
      display.setCursor(0, 15);
      display.clearDisplay();
      display.print("Conectando con BOT de TELEGRAM");
      display.display();
      delay(1000);
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void infoGral() {

  String estado;

  if (digitalRead(relay)) {
    estado = "ENCENDIDA";
  } else {
    estado = "APAGADA";
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(5, 10);
  display.print("Temperatura: " + String(temp) + " C \n");
  display.setCursor(5, 25);
  display.print("Humedad: " + String(hume) + " % \n");
  display.setCursor(5, 40);
  display.print("Humedad Suelo: " + String(soilmoisturepercent) + " % \n");
  display.setCursor(5, 55);
  display.print("Lampara: " + String(estado));
  display.display();
  delay(5000);
}

void lamparaOn() {
  luzState = HIGH;
  digitalWrite(relay, luzState);
  EEPROM.put(13, luzState);
  EEPROM.commit();
            Serial.println(luzState);
        Serial.println(EEPROM.read(13));
  display.setTextSize(1.5);
  display.setCursor(3, 30);
  display.clearDisplay();
  display.print("LAMPARA: ON");
  display.display();
  bot.sendMessage(CHAT_ID, "Se encendio la lampara. Hora: " + String(rtc.getTime()), "");
  delay(3000);
}

void lamparaOff() {
  luzState = LOW;
  digitalWrite(relay, luzState);
  EEPROM.put(13, luzState);
  EEPROM.commit();
          Serial.println(luzState);
        Serial.println(EEPROM.read(13));
  display.setTextSize(1.5);
  display.setCursor(3, 30);
  display.clearDisplay();
  display.print("LAMPARA: OFF");
  display.display();
  bot.sendMessage(CHAT_ID, "Se apagó la lampara. Hora: " + String(rtc.getTime()), "");
  delay(3000);
}


void sensorTierra() {
  soilMoistureValue = analogRead(SensorPin);  //put Sensor insert into soil
  Serial.println(soilMoistureValue);
  soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  display.clearDisplay();
  if (soilmoisturepercent > 100) {
    Serial.println("100 %");

    display.setCursor(20, 0);  //oled display
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Humedad");
    display.setCursor(27, 15);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Suelo");

    display.setCursor(40, 40);  //oled display
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("100 %");
    display.display();

    delay(5000);
    display.clearDisplay();
  } else if (soilmoisturepercent < 0) {
    Serial.println("0 %");

    display.setCursor(20, 0);  //oled display
    display.setTextSize(2);
    display.println("Humedad");
    display.setCursor(26, 15);
    display.println("Suelo");

    display.setCursor(37, 40);  //oled display
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("0 %");
    display.display();

    delay(5000);
    display.clearDisplay();
  } else if (soilmoisturepercent >= 0 && soilmoisturepercent <= 100) {
    Serial.print(soilmoisturepercent);
    Serial.println("%");

    display.setCursor(26, 7);  //oled display
    display.setTextSize(2);
    display.println("Humedad");
    display.setCursor(35, 25);
    display.println("Suelo");
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(48, 50);
    display.print(soilmoisturepercent);
    display.setCursor(75, 50);
    display.print("%");
    display.display();

    delay(5000);
    display.clearDisplay();
  }
}


void testdrawline() {
  int16_t i;

  for (i = 0; i < display.width(); i += 4) {
    display.drawLine(0, 0, i, display.height() - 1, WHITE);
    display.display();  // Update screen with each newly-drawn line
    delay(1);
  }
  for (i = 0; i < display.height(); i += 4) {
    display.drawLine(0, 0, display.width() - 1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);
  display.clearDisplay();
}

void verTimerLamp() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.print("T.Lamp ON: " + String(hora1) + ":" + String(minuto1) + " hs.\n");
  display.setCursor(0, 20);
  display.print("T.Lamp OFF: " + String(hora0) + ":" + String(minuto0) + " hs.");
  display.setTextSize(2);
  display.setCursor(18, 40);
  display.print(String(rtc.getTime()));
  display.display();
  delay(4000);
}
