#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#define SERVICE_UUID            "ebc53bc1-60e6-4a1b-85c1-1d3d616f4c86"
#define CHARACTERISTIC_UUID        "86d017a8-5ba5-4eba-b145-3caf2cd42fa4"
#define APIKEY_CHARACTERISTIC_UUID "12345678-1234-5678-1234-567812345678"
#define MAC_CHARACTERISTIC_UUID "12346578-1234-5678-1234-567812345678"
#define CHANNEL_CHARACTERISTIC_UUID "12345678-1324-5678-1234-567812345678"



const char* ssid = "DungTien";      // Replace with your SSID
const char* password = "234567890"; // Replace with your Wi-Fi password
String apiKeyWrite = "66H4NBOIARR1XADT";
String apiKeyRead = "W2KRZHJZJ0K8CTXC"; // Assuming the read API key is different from the write key
String thingSpeakChannel = "2622135"; // Your ThingSpeak channel ID

const int ledPin = 12; // LED điều khiển qua BLE và ThingSpeak
const int buttonPin = 15; 
const int ledCheckBle = 33; // LED kiểm tra quảng bá
const int checkFirstConnect = 26; // Đèn kiểm tra kết nối đầu tiên
bool ledState = false;
// Global variables
BLEServer* pServer = nullptr;
BLEAdvertising* pAdvertising = nullptr;
BLECharacteristic* pCharacteristic;
BLECharacteristic* pApiKeyCharacteristic;
BLECharacteristic* pMacCharacteristic;
BLECharacteristic* pChannelCharacteristic;

IPAddress local_IP(192,168,43,174);  // Địa chỉ IP tĩnh mong muốn
IPAddress gateway(192, 168, 1, 13);     // Địa chỉ gateway
IPAddress subnet(255, 255, 255, 0);    // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);      // DNS server chính (Google DNS)
IPAddress secondaryDNS(8, 8, 4, 4);    // DNS server phụ (Google DNS)

AsyncWebServer server(80);

bool deviceConnected = false;
String led = "OFF";
String pairedDeviceMac = "";
bool allowPairing = false;

void updateThingSpeak(String ledState1) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + apiKeyWrite + "&field3=" + ledState1;
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.println("HTTP Response code: " + String(httpResponseCode));
    } else {
      Serial.println("Error on HTTP request");
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}





class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
    String deviceMac = BLEAddress(param->connect.remote_bda).toString().c_str();
    digitalWrite(ledCheckBle, LOW);
    digitalWrite(checkFirstConnect, LOW);
    
    if (allowPairing) {
      // Lưu địa chỉ MAC thiết bị đầu tiên kết nối
      pairedDeviceMac = deviceMac;
      Serial.println("Paired with device: " + pairedDeviceMac);
      allowPairing = false;
    } else if (deviceMac != pairedDeviceMac ) {
      // Ngắt kết nối nếu thiết bị không phải là thiết bị đã được ghép đôi
      pServer->disconnect(param->connect.conn_id);
      Serial.println("Disconnected unauthorized device: " + deviceMac);
    } else {
      deviceConnected = true;
      Serial.println("Device connected: " + deviceMac);
    }
  }

  void onDisconnect(BLEServer* pServer) override {
    digitalWrite(ledCheckBle, HIGH);
    deviceConnected = false;
    pAdvertising->start();
    Serial.println("Device disconnected");
  }
};



void setup() {
  Serial.begin(115200);
  
  // Pin setup
  pinMode(ledPin, OUTPUT);
  pinMode(ledCheckBle, OUTPUT);
  pinMode(checkFirstConnect, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(ledCheckBle, HIGH);
  // Initialize BLE
  BLEDevice::init("MyESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // BLE Characteristic for controlling LED
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );

  // BLE Characteristic for setting ThingSpeak API Key
  pApiKeyCharacteristic = pService->createCharacteristic(
    APIKEY_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );

  pMacCharacteristic = pService->createCharacteristic(
    MAC_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );

    pChannelCharacteristic = pService->createCharacteristic(
    CHANNEL_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  
  pService->start();
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Waiting for pairing...");


  server.on("/led/on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, HIGH);
    ledState = true;
    updateThingSpeak("1"); // Send LED ON to ThingSpeak
    Serial.println("LED turned OFF");
    request->send(200, "text/plain", "LED on");
  });

  server.on("/led/off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, LOW);
    ledState = false;
    updateThingSpeak("0"); // Send LED OFF to ThingSpeak
    Serial.println("LED turned ON");
    request->send(200, "text/plain", "LED off");
  });

  
  Serial.println("\nConnected to WiFi");

  String ipAddress = WiFi.localIP().toString();
  Serial.println(ipAddress);

  pApiKeyCharacteristic->setValue(apiKeyRead.c_str());
  pApiKeyCharacteristic->notify();

  pMacCharacteristic->setValue(ipAddress.c_str());
  pMacCharacteristic->notify();

  pChannelCharacteristic->setValue(thingSpeakChannel.c_str());
  pChannelCharacteristic->notify();


  server.begin();
}

void loop() {
  if (digitalRead(buttonPin) == LOW) {
    // Khi nút được nhấn, cho phép ghép đôi với thiết bị mới
    allowPairing = true;
    Serial.println("Pairing mode enabled. Waiting for device to connect...");
    digitalWrite(checkFirstConnect, HIGH);
  }

  if (deviceConnected) {
    // Kiểm tra trạng thái LED thông qua đặc tính BLE
    String value = pCharacteristic->getValue().c_str();
    if (value == "ON" && !ledState) {
      digitalWrite(ledPin, HIGH);
      ledState = true;
      Serial.println("LED turned ON");
      updateThingSpeak("1"); // Send LED ON to ThingSpeak
    } else if (value == "OFF" && ledState) {
      digitalWrite(ledPin, LOW);
      ledState = false;
      Serial.println("LED turned OFF");
      updateThingSpeak("0"); // Send LED OFF to ThingSpeak
    }


    delay(1000); // Adjust delay as needed
  }

}

// void readThingSpeak() {
//   if (WiFi.status() == WL_CONNECTED) {
//     HTTPClient http;
//     String url = "http://api.thingspeak.com/channels/" + thingSpeakChannel + "/fields/3.json?api_key=" + apiKeyRead + "&results=1";
    
//     http.begin(url);
//     int httpResponseCode = http.GET();
    
//     if (httpResponseCode > 0) {
//       // Sử dụng Stream để đọc trực tiếp từ HTTP
//       String payload = http.getString();

//       // Phân tích JSON trực tiếp từ stream
//       DynamicJsonDocument doc(1024);
//       DeserializationError error = deserializeJson(doc, payload);

//       if (!error) {
//         JsonArray feeds = doc["feeds"];
//         if (!feeds.isNull()) {
//           JsonObject firstFeed = feeds[0];
//           String field3Value = firstFeed["field3"];

//           if (field3Value == "1" && !ledState) {
//             digitalWrite(ledPin, HIGH);
//             ledState = true;
//             Serial.println("LED turned ON (ThingSpeak)");
//           } else if (field3Value == "0" && ledState) {
//             digitalWrite(ledPin, LOW);
//             ledState = false;
//             Serial.println("LED turned OFF (ThingSpeak)");
//           }
//           Serial.println(field3Value);
//           delay(1000);
//         }
//       } else {
//         Serial.println("Failed to parse JSON");
//       }
//     } else {
//       Serial.println("Error on HTTP request");
//     }

//     http.end(); // Kết thúc kết nối HTTP
//   } else {
//     Serial.println("WiFi Disconnected");
//   }

// }