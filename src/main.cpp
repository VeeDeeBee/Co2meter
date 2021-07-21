#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Credentials.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define MH_Z19_RX D6
#define MH_Z19_TX D5

SoftwareSerial co2Serial(MH_Z19_RX, MH_Z19_TX); // define MH-Z19

ESP8266WebServer server(80);



String ip;

bool online;

int count;

int oldMillis = 0;

int readCO2() {
  // From original code https://github.com/jehy/arduino-esp8266-mh-z19-serial
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9]; 

  co2Serial.write(cmd, 9); //request PPM CO2

  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (co2Serial.available() > 0 && (unsigned char)co2Serial.peek() != 0xFF) {
    co2Serial.read();
  }

  memset(response, 0, 9);
  co2Serial.readBytes(response, 9);

  if (response[1] != 0x86)
  {
    Serial.println("Invalid response from co2 sensor!");
    Serial.println(response[1]);
    return -1;
  }

  byte crc = 0;
  for (int i = 1; i < 8; i++) {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc) {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
  } else {
    Serial.println("CRC error!");
    return -1;
  }
}

void handle_co2() {
  Serial.print("Requesting CO2 concentration...");
  int ppm = readCO2();
  Serial.println("  PPM = " + String(ppm));
  server.send(200, "text/html", String(ppm)); 
}


String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}

void handle_screen() {
  String message = "";
  message += "\
    <html> \
      <head> \
      <title>Screens toggle</title> \
    </head> \
    <body> \
  ";

  if (server.arg("state")== ""){     //Parameter not found
    message += "Please provide a state</br>";
  } else {     //Parameter found
    message += "state = ";
    message += server.arg("state");     //Gets the value of the query parameter
    message += "</br>";
    if (server.arg("state").toInt() == 0) {
      display.dim(true);
    } else {
      display.dim(false);
    }
  }
    
    message += "\
    </body> \
    </html> \
    ";
 
    server.send(200, "text/html", message); 
}

void setup() {
  Serial.begin(9600);

  Serial.println("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextWrap(false);

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Connecting");
  int i = 0;
  while ( i < 60 && WiFi.status() != WL_CONNECTED ) {
    display.print(".");
    display.display();
    i++;
    delay(500);
  }
  display.clearDisplay();
  display.setCursor(0,0);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("Connected");
    ip = IpAddress2String(WiFi.localIP());
    display.println("IP address: ");
    display.println(ip);
    display.print("Starting up...");
    online = true;
  }
  else {
    display.println("Could not connect");
    ip = IpAddress2String(WiFi.localIP());
    display.println("Continuing offline");
    display.print("Starting up...");  
    online = false;  
  }
  display.display();
  display.startscrollleft(0x00, 0x07);

  count = 0;

  Serial.println("");
  Serial.println("WiFi connected!");

  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/co2", handle_co2);

  server.on("/screen", handle_screen);

  server.begin();
  Serial.println("HTTP server started");
  
  co2Serial.begin(9600); //Init sensor MH-Z19(14)

  oldMillis = millis();

  delay(30000);

  int ppm = readCO2();
  while (ppm == -1 || ppm == 500) {
    ppm = readCO2();
    delay(1000);
    Serial.println(ppm);
  }
  display.stopscroll();
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  //display.print("CO2 ppm");
  display.setTextSize(2);
  display.setCursor(20,9);
  display.print(ppm);
  display.println(" ppm");
  display.display();
  oldMillis = millis();
}

void loop() {
  if (online) {
    if (count == 7200) {
      int i = 0;
      while (i < 60 && WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        i++;
      }
      count = 0;
    } else {
      count++;
    }
    server.handleClient();
  }

  delay(500); 

  if(millis() - oldMillis > 10000){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    //display.print("CO2 ppm");
    display.setTextSize(2);
    display.setCursor(20,9);
    int ppm = readCO2();
    display.print(ppm);
    display.println(" ppm");
    display.display();
    oldMillis = millis();
  }
}

