#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include "Adafruit_APDS9960.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <math.h>
#include "MAX30100.h"
#include "Adafruit_MCP9808.h"

#define REPORTING_PERIOD_MS     1000

#define NUMPIXELS               24
#define PIN                     (PIN_PD4)
#define LED_RED                 (PIN_PD0)
#define LED_YELLOW              (PIN_PD1)
#define LED_GREEN               (PIN_PD2)
#define LED_BLUE                (PIN_PD3)
#define LIGHTSENSORPIN          (PIN_PD5)
#define MCP9808_ADDR            (0x18)

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

MAX30100* pulseOxymeter;

Adafruit_APDS9960 apds;


uint32_t tsLastReport = 0;
uint32_t lastTemp = 0;
uint32_t lastOnboardSensor = 0;
char ssid[] = "YOUR_WIFI_SSID";        
char pass[] = "YOUR WIFI_PASSWORD";    
int keyIndex = 0;  
int status = WL_IDLE_STATUS;
int mode = 0;

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
WiFiClient wificlient;
PubSubClient mqttclient(wificlient);

void setup() {
  //Wire.begin();
  Serial2.begin(115200);
  Serial2.println("Initialized...");  

  pixels.begin();
  pixels.show();

  lightAll(150,0,0,500);
  lightAll(0,150,0,500);
  lightAll(0,0,150,500);
  
  mlx.begin();  
  
  if(!apds.begin()){
    Serial2.println("failed to initialize device! Please check your wiring.");
  }
  else Serial2.println("Device initialized!");
  
  apds.enableProximity(true);
  apds.enableGesture(true);

  pulseOxymeter = new MAX30100();

  if (!tempsensor.begin(0x18)) {
    Serial2.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
  }
  Serial2.println("MCP9808 Initialized.");

  tempsensor.setResolution(0);
  
  WiFi.setPins(PIN_PA7, PIN_PF2, PIN_PA1, PIN_PF3);
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial2.println("WiFi shield not present");
    // don't continue:
    while (true);
  }
  while (status != WL_CONNECTED) {
    Serial2.print("Attempting to connect to SSID: ");
    Serial2.println(ssid);
    status = WiFi.begin(ssid, pass);
  }
  Serial2.println("Connected to wifi");
  printWiFiStatus();
  mqttclient.setServer("YOUR_MQTT_SERVER_IP", 1883);
  mqttclient.connect("MedicalDevice001");
  Serial2.println("Ready....");
  chaser(150,0,0,3,50);
}

void loop() {
  uint8_t gesture = apds.readGesture();
  
  pulseoxymeter_t result = pulseOxymeter->update();
  
  if(gesture == APDS9960_DOWN) {
    Serial2.println("v On");
    mode = 2;
  }
  if(gesture == APDS9960_UP){
    Serial2.println("^ Off");
    mode = 0;
    clearAll();
  }
  if(gesture == APDS9960_LEFT){
    Serial2.println("< Temp");
    mode = 1;
    clearAll();
    chaser(0,255,0,3,50);
  }
  if(gesture == APDS9960_RIGHT){
    Serial2.println("> Beat");
    mode = 3;
    clearAll();
    chaser(0,0,150,3,50);
    lightOn(0,0,255);
  }

  if(mode==1){
    if(millis() - lastTemp > 1000){
      lightOn(0,255,0);
      if (mqttclient.connect("MedicalDevice001")) {
        Serial2.print("Object = "); Serial2.print(mlx.readObjectTempC()); Serial2.println("*C");
        String str_temp = (String) mlx.readObjectTempC();
        char char_temp[5];
        str_temp.toCharArray(char_temp, 5);
        mqttclient.publish("temp", char_temp);
        //delay(500);
      } 
      lastTemp = millis();
    }   
  }
  if(mode==2){
    lightOn(255,255,255);
  }

  if(mode==3){
    if( result.pulseDetected == true )
    {
      Serial2.println("BEAT");
      
      Serial2.print( "BPM: " );
      Serial2.print( result.heartBPM );
      Serial2.print( " | " );
    
      Serial2.print( "SaO2: " );
      Serial2.print( result.SaO2 );
      Serial2.println( "%" );
      if (mqttclient.connect("MedicalDevice001")) {
        String str_bpm = (String) result.heartBPM;
        char char_bpm[5];
        str_bpm.toCharArray(char_bpm, 5);
        mqttclient.publish("bpm", char_bpm);
        
        String str_spo2 = (String) result.SaO2;
        char char_spo2[5];
        str_spo2.toCharArray(char_spo2, 5);
        mqttclient.publish("spo2", char_spo2);
        //delay(500);
      } 
    }
    delay(10);
  }

  if(millis() - lastOnboardSensor > 1000){
    // Onboard temperature sensor
    tempsensor.wake(); 
    float c = tempsensor.readTempC();
    float f = tempsensor.readTempF();
    Serial2.print("Temp: "); 
    Serial2.print(c, 4); Serial2.print("*C\t and "); 
    Serial2.print(f, 4); Serial2.println("*F.");
    if (mqttclient.connect("MedicalDevice001")) {
      String str_ontemp = (String) c;
      char char_ontemp[5];
      str_ontemp.toCharArray(char_ontemp, 5);
      mqttclient.publish("ontemp", char_ontemp);
    }
    tempsensor.shutdown_wake(1);
    
    // Light sensor
    int reading = analogRead(LIGHTSENSORPIN);
    float light_value = reading / 1023.0 * 100;
    if (mqttclient.connect("MedicalDevice001")) {
      String str_light = (String) light_value;
      char char_light[5];
      str_light.toCharArray(char_light, 5);
      mqttclient.publish("light", char_light);
    }
    lastOnboardSensor = millis();
  }
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial2.print("SSID: ");
  Serial2.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial2.print("IP Address: ");
  Serial2.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial2.print("signal strength (RSSI):");
  Serial2.print(rssi);
  Serial2.println(" dBm");
}

// Make all light on
void lightAll(int r, int g, int b, int d){
  for(int i=0; i<NUMPIXELS; i++) { 
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
  delay(d);
  clearAll();
}
// Light chaser
void chaser(int r, int g, int b, int c, int d){
  clearAll();
  for(int a=0; a<c; a++){
    for(int i=0; i<NUMPIXELS; i++) { 
      pixels.setPixelColor(i, pixels.Color(r, g, b));
      pixels.setPixelColor(i-1, pixels.Color(r*.75, g*.75, b*.75));
      pixels.setPixelColor(i-2, pixels.Color(r*.5, g*.5, b*.5));
      pixels.setPixelColor(i-3, pixels.Color(r*.25, g*.25, b*.25));
      pixels.setPixelColor(i-4, pixels.Color(r*0, g*0, b*0));
      pixels.show();
      delay(d);
    }
    clearAll();
  }
  clearAll(); 
}
// Turn off all light
void clearAll(){
  for(int i=0; i<NUMPIXELS; i++) { 
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
}
// Light fade effect
void fade(int c){
  for(int a=0; a<c; a++){
    for(int j=0; j<256; j++) { 
        for(int i=0; i<NUMPIXELS; i++) { 
          pixels.setPixelColor(i, pixels.Color(j, 0, 0));
        }
        pixels.show();
        delay(10);
    }
    for(int p=0; p<256; p++) { 
        for(int q=0; q<NUMPIXELS; q++) { 
          pixels.setPixelColor(q, pixels.Color(255-p, 0, 0));
        }
        pixels.show();
        delay(10);
    }
  }
  clearAll();
}
// On all lights without delay
void lightOn(int r, int g, int b){
  clearAll();
  for(int i=0; i<NUMPIXELS; i++) { 
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void onBoardLED(int c){
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  if(c==1){
    digitalWrite(LED_RED, LOW);
    }
  if(c==3){
    digitalWrite(LED_YELLOW, LOW);
    }
  if(c==2){
    digitalWrite(LED_GREEN, LOW);
    }
  if(c==4){
    digitalWrite(LED_BLUE, LOW);
    }
}
void onBeatDetected()
{
    Serial2.println("Beat!");
}
