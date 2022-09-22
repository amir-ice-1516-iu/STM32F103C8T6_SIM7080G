
#include<Arduino.h>

// #define TINY_GSM_MODEM_SIM5300E
#define TINY_GSM_MODEM_SIM7080

// Set serial for debug console (to the Serial Monitor, default speed 115200)
HardwareSerial SerialAT(PA3,PA2);
HardwareSerial SerialMon(PA10,PA9);

#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 1460
#endif

#define GSM_PIN ""
// data sending interval 20
const int DATA_SEND_INTERVAL = 500;
const int RETRY_TIMEOUT       = 30000;
uint32_t last_sent = millis();
// Your GPRS credentials, if any
//const char apn[]      = "jawalnet.com.sa";
const char apn[]      = "M2M";
const char gprsUser[] = "guest";
const char gprsPass[] = "guest";

// Your WiFi connection credentials, if applicable
const char wifiSSID[] = "";
const char wifiPass[] = "";

// Server details
const char server[]   = "3.22.213.65";
const char get_url_half[] = "/api/v1/";
const char get_url_last_half[] = "/attributes?sharedKeys=nfc_key_attr";
const char post_url_half[] = "/api/v1/";
const char post_url_last_half[] = "/telemetry";
const int  port       = 8080;
const char api_token[] = "gR8k9XWQLZXBVWGxgCbI";

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

// TinyGsm        modem(SerialAT);
TinyGsmClient client(modem);
HttpClient    http(client, server, port);

void initiate_sim_modlue(void); // #1 
bool connect_to_network_and_gprs(void); // #1
bool send_telemetry_data(String jsonAsString,
                         String token,
                         String contentType); // #2
String get_the_key(String token); // #3
void stop_http_and_disconnect(void); //#4
void go_to_sleep_mode(void); // #4
void wake_up_sim_module(void); // #5
 bool net_s = false;
void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(1000);
  pinMode(PB12,OUTPUT);
  digitalWrite(PB12,LOW);
  SerialMon.println("PB12 LOW");
  delay(1000);
  digitalWrite(PB12,HIGH);
  SerialMon.println("PB12 HIGH, powered on SIM7080G");
  delay(3000);
  pinMode(PA1,OUTPUT);
  digitalWrite(PA1,HIGH);
  SerialMon.println("PA1 HIGH");
  delay(1000);
  digitalWrite(PA1,LOW);
  SerialMon.println("PA1 LOW : enable triggered");
  delay(2000);
  digitalWrite(PA1,HIGH);
  SerialMon.println("PA1 HIGH . On ideal state");
  SerialMon.println("Wait...");
  initiate_sim_modlue();
  while(!net_s){  

  net_s = connect_to_network_and_gprs();
  SerialMon.println("Network for GPRS not connected");
  SerialMon.println(net_s);
  //delay(5000);
  //go_to_sleep_mode();
  delay(3000);
  //wake_up_sim_module();
  }
}

void loop() {
  if((millis()-last_sent) >= DATA_SEND_INTERVAL){
    //wake_up_sim_module();
    if(net_s){
      String Key_Got = get_the_key(String(api_token));
      if (Key_Got != "")
        SerialMon.println(Key_Got);
      send_telemetry_data("{\"temperature1\":25,\"temperature2\":26,\"gas1\":1,\"gas2\":1,\"lock_state\":1,\"door_state\":1}",
                      String(api_token),
                      "application/json");
      //go_to_sleep_mode();
      last_sent = millis();
    }
    else
      SerialMon.println("Network for GPRS not connected");
  delay(2000);
  }
  else{
    // collect your sensor data here
    SerialMon.println("Collecting sensor values here "+String(millis()));
    delay(2000);
  }
}

void initiate_sim_modlue(void){
  SerialMon.println("Initiating SIM module");
  // SerialAT.begin(9600);
  SerialAT.begin(115200);
  modem.sendAT(GF("+IPR=115200"));
  delay(3000);
  modem.init();
  String modemInfo = modem.getModemInfo();
  if (GSM_PIN && modem.getSimStatus() != 3) { modem.simUnlock(GSM_PIN); }
  
}

bool connect_to_network_and_gprs(void){
  SerialMon.println("Connection to Network");
  uint32_t st = millis();
  while((millis()-st) < RETRY_TIMEOUT){
    if (!modem.waitForNetwork()) {
      SerialMon.print('.');
      delay(1000);
      continue;
      // return false;
    }
    
     if (!modem.isNetworkConnected()) continue;
    // GPRS connection parameters are usually set after network registration
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      continue; //return false;
    } 
    if (modem.isGprsConnected()) return true;
    else continue;
  }
  return false;
}

bool send_telemetry_data(String jsonAsString, String token, String contentType ){
  SerialMon.print("Sending telemetry post data:");
  String post_url = String(post_url_half)+token+String(post_url_last_half);
  SerialMon.println(post_url);
  http.post(post_url, contentType, jsonAsString);
  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  if(statusCode == 200) return true;
  else return false;
}

String get_the_key(String token){
  SerialMon.print("Getting the Key: ");
  String resource = String(get_url_half)+String(token)+String(get_url_last_half);
  SerialMon.println(resource);
  http.stop();
  int err = http.get(resource.c_str());
  if (err != 0) {
    SerialMon.println(F("failed to connect"));
    delay(1000);
    return "";
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return "";
  }

  // SerialMon.println(F("Response Headers:"));
  // while (http.headerAvailable()) {
  //   String headerName  = http.readHeaderName();
  //   String headerValue = http.readHeaderValue();
  //   SerialMon.println("    " + headerName + " : " + headerValue);
  // }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  // SerialMon.println(F("Response:"));
  // SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());
  return body;
}

void stop_http_and_disconnect(void){
  http.stop();
  // SerialMon.println(F("Server disconnected"));
  modem.gprsDisconnect();
  // SerialMon.println(F("GPRS disconnected"));
}

void go_to_sleep_mode(void){
  SerialMon.println("Enabling Sleep mode for SIM5300E");
  stop_http_and_disconnect();
  modem.sleepEnable(true);
  modem.setPhoneFunctionality(4,false);
  // delay(5000);
  // digitalWrite(PB12,LOW); // replace this with DTR pin to HIGH
}

void wake_up_sim_module(void){
  // digitalWrite(PB12,HIGH); // replace this with DTR pin to LOW
  // SerialAT.begin(115200);
  // delay(500);
  SerialMon.println("Waiking up SIM5300E by uart data");
  // digitalWrite(PB12,LOW);
  // delay(100);
  // digitalWrite(PB12,HIGH);
  // initiate_sim_modlue(); // comment it out
  modem.sendAT(); // uncomment it out
  modem.waitResponse(); // uncomment it out
  modem.sendAT(); // uncomment it out
  modem.waitResponse(); // uncomment it out
  modem.sleepEnable(false);
  modem.setPhoneFunctionality(1,true); //
  modem.sendAT(); // uncomment it out
  modem.waitResponse(); // uncomment it out
  modem.sendAT(); // uncomment it out
  modem.waitResponse(); // uncomment it out
  modem.sendAT(GF("E0"));
  modem.waitResponse();
  modem.sendAT(GF("+CMEE=0"));
  modem.waitResponse();
}