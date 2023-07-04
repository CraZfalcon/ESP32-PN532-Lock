#include <fs.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <Arduino.h>
#include <HttpClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
HTTPClient http;
WiFiManager wfm;

#include <Wire.h>
#include <PN532.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);

bool shouldSaveConfig = false;

String KeyWord = "";
String serverPath = "";
char Door_Name[100] = "Door_Config";
char Door_Pass[100] = "123456789";
char api_key[100];
char api_port[8];
char Door[5];
char Location[4];
char Open[8];

WiFiManagerParameter Door_Name_WFM("Door_Name", "Door Name:", Door_Name, 100);
WiFiManagerParameter Door_Pass_WFM("Door_Pass", "Door Password (min 8 char):", Door_Pass, 100);
WiFiManagerParameter api_key_Adress("api_Adress", "API key:", api_key, 100);
WiFiManagerParameter api_port_Adress("api_port", "API port:", api_port, 8);
WiFiManagerParameter Door_Adress("Door", "Door:", Door, 5);
WiFiManagerParameter Location_Adress("Location", "Location:", Location, 4);
WiFiManagerParameter Open_Duration("Open_Duration", "Open Duration (ms):", Open, 4);

void saveConfigCallback () { shouldSaveConfig = true; }
void menubtn() {
  if ( digitalRead(20) == LOW ) {
    delay(1000);
    if ( digitalRead(20) == LOW ) {
      wfm.setSaveConfigCallback(saveConfigCallback);
      wfm.startConfigPortal(Door_Name, Door_Pass);
      strcpy(Door_Name, Door_Name_WFM.getValue());
      strcpy(api_key, api_key_Adress.getValue());
      strcpy(api_key, api_key_Adress.getValue());
      strcpy(api_port, api_port_Adress.getValue());
      strcpy(Door, Door_Adress.getValue());
      strcpy(Location, Location_Adress.getValue());
      strcpy(Open, Open_Duration.getValue());

      DynamicJsonDocument doc(1024);
      doc["DoorName"] = Door_Name;
      doc["DoorPass"] = Door_Pass;
      doc["apikey"] = api_key;
      doc["apiport"] = api_port;
      doc["Door"] = Door;
      doc["Location"] = Location;
      doc["Open"] = Open;
      File configFile = SPIFFS.open("/config.json", "w");
      serializeJson(doc, configFile);
      configFile.close();
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(5, OUTPUT);
  pinMode(20, INPUT_PULLUP);

  if(digitalRead(20) == LOW){
    delay(1000);
    if(digitalRead(20) == LOW){
      wfm.resetSettings();
    }
  }

  if(SPIFFS.begin()){
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, buf.get());
        strcpy(Door_Name, doc["DoorName"]);
        strcpy(Door_Pass, doc["DoorPass"]);
        strcpy(api_key, doc["apikey"]);
        strcpy(api_port, doc["apiport"]);
        strcpy(Door, doc["Door"]);
        strcpy(Location, doc["Location"]);
        strcpy(Open, doc["Open"]);
        configFile.close();
        Serial.println("Successfully read config file");
      }
    }
  }
  else if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    Serial.println("Formatting...");
    SPIFFS.format();
    delay(1000);
    ESP.restart();
    delay(5000);
    return;
  }

  wfm.setSaveConfigCallback(saveConfigCallback);
  wfm.addParameter(&Door_Name_WFM);
  wfm.addParameter(&Door_Pass_WFM);
  wfm.addParameter(&api_key_Adress);
  wfm.addParameter(&api_port_Adress);
  wfm.addParameter(&Door_Adress);
  wfm.addParameter(&Location_Adress);
  wfm.addParameter(&Open_Duration);
  std::vector<const char *> menu = {"wifi"};
  wfm.setMenu(menu);
  wfm.setConnectTimeout(180);
  wfm.autoConnect(Door_Name, Door_Pass);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  if (shouldSaveConfig) {
    strcpy(Door_Name, Door_Name_WFM.getValue());
    strcpy(Door_Pass, Door_Pass_WFM.getValue());
    strcpy(api_key, api_key_Adress.getValue());
    strcpy(api_port, api_port_Adress.getValue());
    strcpy(Door, Door_Adress.getValue());
    strcpy(Location, Location_Adress.getValue());
    strcpy(Open, Open_Duration.getValue());
    DynamicJsonDocument doc(1024);
    doc["DoorName"] = Door_Name;
    doc["DoorPass"] = Door_Pass;
    doc["apikey"] = api_key;
    doc["apiport"] = api_port;
    doc["Door"] = Door;
    doc["Location"] = Location;
    doc["Open"] = Open;
    File configFile = SPIFFS.open("/config.json", "w");
    serializeJson(doc, configFile);
    configFile.close();
  }
  nfc.begin();
}

void loop() {
  menubtn();
  if (nfc.tagPresent()) {

    NfcTag KeyContents = nfc.read();
    KeyWord = "";
    KeyWord = KeyContents.getUidString();
    KeyWord.replace(" ", "");
    KeyWord.toUpperCase();

    if(KeyContents.hasNdefMessage()){
      NdefMessage message = KeyContents.getNdefMessage();
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++) {
        NdefRecord record = message.getRecord(i);
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);
        KeyWord = "";
        for (int c = 0; c < payloadLength; c++) {
          KeyWord += (char)payload[c];
        }

        if(KeyWord == "OVERRIDE"){
          digitalWrite(5, HIGH);
          Serial.println("OPEN");
          delay(atoi(Open));
          digitalWrite(5, LOW);
          Serial.println("CLOSE");
          delay(500);
          return;
        }

        if(KeyWord == "RESET"){
          wfm.resetSettings();
          DynamicJsonDocument doc(1024);
          doc["DoorName"] = "";
          doc["DoorPass"] = "";
          doc["apikey"] = "";
          doc["apiport"] = "";
          doc["Door"] = "";
          doc["Location"] = "";
          doc["Open"] = "";
          File configFile = SPIFFS.open("/config.json", "w");
          serializeJson(doc, configFile);
          configFile.close();
          delay(1000);
          ESP.restart();
          delay(5000);
          return;
        }

        if(KeyWord == "CONFIG"){
          wfm.setSaveConfigCallback(saveConfigCallback);
          wfm.setConfigPortalTimeout(300);
          wfm.startConfigPortal(Door_Name, Door_Pass);
          strcpy(Door_Name, Door_Name_WFM.getValue());
          strcpy(api_key, api_key_Adress.getValue());
          strcpy(api_key, api_key_Adress.getValue());
          strcpy(api_port, api_port_Adress.getValue());
          strcpy(Door, Door_Adress.getValue());
          strcpy(Location, Location_Adress.getValue());
          strcpy(Open, Open_Duration.getValue());

          DynamicJsonDocument doc(1024);
          doc["DoorName"] = Door_Name;
          doc["DoorPass"] = Door_Pass;
          doc["apikey"] = api_key;
          doc["apiport"] = api_port;
          doc["Door"] = Door;
          doc["Location"] = Location;
          doc["Open"] = Open;
          File configFile = SPIFFS.open("/config.json", "w");
          serializeJson(doc, configFile);
          configFile.close();
          return;
        }
      }
    }

    Serial.println(KeyWord);
    
    serverPath = "https://" + String(api_key) + ":" + String(api_port) + "/api/DoorAccess?AccessTag_ID=" + KeyWord + "&Id_Door=" + String(Door) + "&Location=" + String(Location);

    http.begin(serverPath.c_str());
    http.GET();
    if (http.getString() == "true") {
      digitalWrite(5, HIGH);
      Serial.println("Door opened");
      delay(atoi(Open));
      digitalWrite(5, LOW);
      Serial.println("Door closed");
      delay(500);
    }
    http.end();
  }
}