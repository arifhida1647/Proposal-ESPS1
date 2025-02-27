#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int TRIG_PINS[] = {14, 33, 26};
const int ECHO_PINS[] = {27, 32, 25};
const int NUM_SENSORS = sizeof(TRIG_PINS) / sizeof(TRIG_PINS[0]);

#define SCREEN_WIDTH 128 // OLED width, in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels

// Create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// WiFi credentials
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *baseURL = "https://service-proposal.vercel.app/update-status-iot-s1";

void setup()
{
  Serial.begin(9600);

  // Set all TRIG pins as OUTPUT and ECHO pins as INPUT
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
  }

  // Initialize OLED display with I2C address 0x3C
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Failed to start SSD1306 OLED"));
    while (1)
      ;
  }

  delay(2000);         // Wait two seconds for initializing
  oled.clearDisplay(); // Clear display

  oled.setTextSize(1);        // Set text size
  oled.setTextColor(WHITE);   // Set text color
  oled.setCursor(0, 2);       // Set position to display (x,y)
  oled.println("Robotronix"); // Set text
  oled.display();             // Display on OLED

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

long getDistance(int trigPin, int echoPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10); // Changed to 10 microseconds for better accuracy
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2; // Calculate distance in cm
}

void displayDataOnOLED(int bothZeroCount, int oneOneOtherZeroCount, int bothOneCount)
{
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("API Data:");
  oled.setCursor(0, 16);
  oled.print("Both Zero: ");
  oled.println(bothZeroCount);
  oled.setCursor(0, 32);
  oled.print("One1Other0: ");
  oled.println(oneOneOtherZeroCount);
  oled.setCursor(0, 48);
  oled.print("Both One: ");
  oled.println(bothOneCount);
  oled.display();
}

void getDataFromAPI()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin("https://service-proposal.vercel.app/compare-status");

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0)
    {
      String payload = http.getString();
      Serial.println(payload);

      // Parsing JSON data
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error)
      {
        displayDataOnOLED(doc["bothZeroCount"], doc["oneOneOtherZeroCount"], doc["bothOneCount"]);
      }
      else
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      }
    }
    else
    {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}

// Function to send JSON data via POST request
void sendJsonData(int statusArray[])
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Specify the URL
    http.begin(baseURL);

    // Specify content-type header
    http.addHeader("Content-Type", "application/json");

    // Construct JSON string manually
    String jsonString = "{ \"statusArray\": [";
    for (int i = 0; i < NUM_SENSORS; i++)
    {
      jsonString += String(statusArray[i]);
      if (i < NUM_SENSORS - 1)
      {
        jsonString += ", "; // Add a comma between values
      }
    }
    jsonString += "] }";

    // Send the POST request
    int httpResponseCode = http.POST(jsonString);

    // Check the response
    if (httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.println("Response Code: " + String(httpResponseCode));
      Serial.println("Response Body: " + response);
    }
    else
    {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    // Close connection
    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}

void loop()
{
  int statusArray[NUM_SENSORS];

  // Read and store data for each sensor
  // Baca dan simpan data dari tiap sensor
  // Baca dan simpan data dari tiap sensor
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    long distance = getDistance(TRIG_PINS[i], ECHO_PINS[i]);

    if (distance == 0 || distance > 400)
    {
      statusArray[i] = 2; // Sensor tidak terhubung
    }
    else
    {
      statusArray[i] = (distance < 5) ? 1 : 0; // 1 jika < 5cm, 0 jika >= 5cm
    }
  }

  // Print the JSON-like structure to Serial
  String jsonOutput = "{\n    \"statusArray\": [";
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    jsonOutput += String(statusArray[i]);
    if (i < NUM_SENSORS - 1)
    {
      jsonOutput += ", "; // Tambahkan koma antar nilai
    }
  }
  jsonOutput += "]\n}";

  Serial.println(jsonOutput); // Print the complete JSON structure

  // Send the status array as JSON to the specified URL
  sendJsonData(statusArray);

  delay(1000); // Delay between readings

  // Update data from main API
  getDataFromAPI();

  delay(1000); // Fetch data every 1 second
}