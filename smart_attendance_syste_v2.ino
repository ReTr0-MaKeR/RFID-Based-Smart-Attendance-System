#include <SPI.h>
#include <MFRC522.h>
#include "WiFi.h"
#include <HTTPClient.h>

// RFID SS/SDA and Reset PINs
#define SS_PIN  5  
#define RST_PIN 0

// Button PIN
#define BTN_PIN 4

// WiFi credentials
const char* ssid = "Retro";            // Your WiFi name
const char* password = "Retro_Maker";   // Your WiFi password

// Google Script Web App URL
String Web_App_URL = "https://script.google.com/macros/s/AKfycbzuCWmCxMYJLryxYH6-6X5pIFsSiXlsK7_2Qa_DQ14cxboGf8J5M75f3fJ9bYi0Y7O8yA/exec";

// Global variables for handling responses and status
String reg_Info = "";
String atc_Info = "";
String atc_Name = "";
String atc_Date = "";
String atc_Time_In = "";
String atc_Time_Out = "";
int readsuccess;
char str[32] = "";
String UID_Result = "--------";
String modes = "atc";

// MFRC522 RFID object
MFRC522 mfrc522(SS_PIN, RST_PIN); 

// HTTP request function to send data to Google Sheets
void http_Req(String str_modes, String str_uid) {
  if (WiFi.status() == WL_CONNECTED) {
    String http_req_url = "";

    // URL creation based on mode
    if (str_modes == "atc") {
      http_req_url  = Web_App_URL + "?sts=atc";
      http_req_url += "&uid=" + str_uid;
    }
    if (str_modes == "reg") {
      http_req_url = Web_App_URL + "?sts=reg";
      http_req_url += "&uid=" + str_uid;
    }

    Serial.println("\n-------------");
    Serial.println("Sending request to Google Sheets...");
    Serial.print("URL : ");
    Serial.println(http_req_url);
    
    HTTPClient http;
    http.begin(http_req_url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET(); 
    Serial.print("HTTP Status Code : ");
    Serial.println(httpCode);

    String payload;
    if (httpCode > 0) {
      payload = http.getString();
      Serial.println("Payload : " + payload);  
    }
    
    Serial.println("-------------");
    http.end();

    String sts_Res = getValue(payload, ',', 0);

    // Handling responses based on the payload
    if (sts_Res == "OK") {
      if (str_modes == "atc") {
        atc_Info = getValue(payload, ',', 1);
        
        if (atc_Info == "TI_Successful") {
          atc_Name = getValue(payload, ',', 2);
          atc_Date = getValue(payload, ',', 3);
          atc_Time_In = getValue(payload, ',', 4);

          Serial.print("Name: ");
          Serial.println(atc_Name);
          Serial.println("Status: Posted");
          delay(1000);
        }

        if (atc_Info == "TO_Successful") {
          atc_Name = getValue(payload, ',', 2);
          atc_Date = getValue(payload, ',', 3);
          atc_Time_In = getValue(payload, ',', 4);
          atc_Time_Out = getValue(payload, ',', 5);

          Serial.print("Name: ");
          Serial.println(atc_Name);
          Serial.println("Status: Posted");
          delay(5000);
        }

        if (atc_Info == "atcInf01") {
          Serial.println("Attendance Recorded");
          delay(3000);
        }

        if (atc_Info == "atcErr01") {
          Serial.println("Error! Card not registered");
          delay(3000);
        }

        atc_Info = "";
        atc_Name = "";
        atc_Date = "";
        atc_Time_In = "";
        atc_Time_Out = "";
      }

      if (str_modes == "reg") {
        reg_Info = getValue(payload, ',', 1);
        
        if (reg_Info == "R_Successful") {
          Serial.println("UID was posted");
          delay(3000);
        }

        if (reg_Info == "regErr01") {
          Serial.println("Error! Already registered");
          delay(3000);
        }

        reg_Info = "";
      }
    }
  } else {
    Serial.println("Error! WiFi disconnected");
    delay(3000);
  }
}

// Helper function to extract values from strings
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// Function to get UID from RFID
int getUID() {  
  if(!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if(!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  
  byteArray_to_string(mfrc522.uid.uidByte, mfrc522.uid.size, str);
  UID_Result = str;
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  return 1;
}

// Convert byte array to string
void byteArray_to_string(byte array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++) {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len*2] = '\0';
}

// Setup function
void setup(){
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  pinMode(BTN_PIN, INPUT_PULLUP);
  delay(500);

  SPI.begin();      
  mfrc522.PCD_Init(); 
  delay(500);

  Serial.println("ESP32 RFID Attendance System");
  delay(3000);

  Serial.println("\nWIFI mode : STA");
  WiFi.mode(WIFI_STA);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int connecting_process_timed_out = 40;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
    
    if (connecting_process_timed_out > 0) connecting_process_timed_out--;
    if (connecting_process_timed_out == 0) {
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println("\nWiFi connected");
  delay(2000);
}

// Main loop
void loop() 
{
  
  if (getUID()) {
    Serial.print("UID: ");
    Serial.println(UID_Result);
    Serial.println(digitalRead(BTN_PIN));
    if (digitalRead(BTN_PIN) == LOW) {
      modes = "reg";
    } else {
      modes = "atc";
    }
    Serial.print("Mode: ");
    Serial.println(modes);
    
    http_Req(modes, UID_Result);
    delay(500); 
    Serial.println("Place your Card Now!");
  }
}
