#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

// WiFi credentials
const char* ssid = "Raf's Wifi";
const char* password = "pizzaroll";
const char* SERVER_NAME = "http://www.intelliegg.site/webpages/sensordata.php";

// API Key
String PROJECT_API_KEY = "hello world";

// Data placeholders
String humidityData = "";
String tempData = "";

// LED or Relay control
const int RELAY_PIN = D1; // D1 pin on NodeMCU
bool relayState = false;

// Relay Type (NO or NC)
const char* RELAY_TYPE = "NO"; // Set to "NO" or "NC"

unsigned long previousMillis = 0;
const long interval = 1000; // Interval to send data every 1 second

// Create a web server object
ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  Serial.println("NodeMCU Starting...");

  // Set relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Initially turn off the relay

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up server routes
  server.on("/candling", HTTP_GET, []() {
    String state = server.arg("state");
    if (state == "on") {
      Serial.println("Relay turning on..."); // Debugging
      // Check if relay is NO or NC type
      if (RELAY_TYPE == "NO") {
        digitalWrite(RELAY_PIN, HIGH);
      } else if (RELAY_TYPE == "NC") {
        digitalWrite(RELAY_PIN, LOW);
      }
      relayState = true;
      server.send(200, "text/plain", "Relay turned on");
    } else if (state == "off") {
      Serial.println("Relay turning off..."); // Debugging
      // Check if relay is NO or NC type
      if (RELAY_TYPE == "NO") {
        digitalWrite(RELAY_PIN, LOW);
      } else if (RELAY_TYPE == "NC") {
        digitalWrite(RELAY_PIN, HIGH);
      }
      relayState = false;
      server.send(200, "text/plain", "Relay turned off");
    } else {
      server.send(400, "text/plain", "Invalid request");
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
  });
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle client requests
  server.handleClient();

  // Check if 1 second has passed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (Serial.available()) {
      String incomingData = Serial.readStringUntil('\n');
      incomingData = cleanData(incomingData);
      processSensorData(incomingData);
    }
  }
}

// Function to clean the incoming data by removing non-printable characters
String cleanData(String data) {
  String clean = "";
  for (int i = 0; i < data.length(); i++) {
    if (isPrintable(data[i])) {
      clean += data[i];
    }
  }
  return clean;
}

// Process the sensor data
void processSensorData(String incomingData) {
  Serial.print("Received data: ");
  Serial.println(incomingData);

  int humIndex = incomingData.indexOf("Humidity:");
  int tempIndex = incomingData.indexOf("Temp:");

  if (humIndex != -1 && tempIndex != -1) {
    // Extract humidity and temperature values
    humidityData = incomingData.substring(humIndex + 9, incomingData.indexOf("%", humIndex));
    tempData = incomingData.substring(tempIndex + 5, incomingData.indexOf("C", tempIndex));

    // Convert to float for validation
    float humidity = humidityData.toFloat();
    float temperature = tempData.toFloat();

    // Validate the data range before sending
    if (humidity >= 0 && humidity <= 100 && temperature >= -50 && temperature <= 100) {
      String formattedData = "";
      for (int i = 0; i < 10; i++) {
          formattedData += "temperature=" + String(temperature) + "&humidity=" + String(humidity) + "&";
      }
      formattedData.remove(formattedData.length() - 1); // Remove the last '&' character
      uploadData(formattedData);
    } else {
      Serial.println("Error: Invalid humidity or temperature value.");
    }
  } else {
    Serial.println("Error: Invalid data format. Expected 'Humidity:' and 'Temp:'");
  }
}

// Function to upload sensor data to the server
void uploadData(String data) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String postData = "api_key=" + PROJECT_API_KEY + "&" + data;
    Serial.println("Sending data: " + postData);

    http.begin(client, SERVER_NAME);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(5000); // Reduce timeout for faster response
    client.setNoDelay(true); // Disable Nagle's algorithm

    int httpResponseCode = http.POST(postData);
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected.");
  }
}