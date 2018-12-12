#include <ESP8266WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ESP8266HTTPClient.h>

const int NUM_GENERATED_SIGNALS = 1;
const int TOTAL_MESSAGES = 3;
int doomsday_counter = 0;
const int DOOMSDAY = 50;

//NETWORK STATIC VARIABLES
const char *ssid = "wl-fmat-ccei";
const char *password = "";

//SAMPLING VARIABLES
int send_counter = 0;
int captured_values_counter = 0;
int captured_signals_counter = 0;
const int SAMPLE_PERIOD_MILLIS = 4;
const int SAMPLING_TIME_SEC = 45;
const int MIN_YIELD_ITERS = 200;

//STORAGE STATIC VARIABLES
const int SD_digital_IO = 8;
int BUFFER_SIZE = 0;
const int MAX_MEM = 1000;
String buffer_ = "";

//SERVER VARIABLES
const String SERVER_URL = "/sample?";
String host = "192.168.228.84";
const int port = 3000;

float window_size_millis = 0;
int buffer_size = 0;

void setup()
{
  Serial.begin(9600);
  begin_wifi_connection();

  //Variables initialization
  window_size_millis = SAMPLING_TIME_SEC * 1000;
  buffer_size = int(ceil(window_size_millis / SAMPLE_PERIOD_MILLIS));
  BUFFER_SIZE = buffer_size;
}

void begin_wifi_connection()
{
  //Connects to a WiFi network (SSID was specified in variable "ssid")
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    //Waits until WiFi is connected
    delay(500);
    Serial.print(".");
    doomsday_counter++;
    if (doomsday_counter == DOOMSDAY)
    {
      hard_reset();
    }
  }
  Serial.println("");
  Serial.println("WiFi conectado.");
}

void GET(String stored_data, String is_last)
{
  //GET Method to send data to server
  String server_response = "";
  int yield_counter = 0;
  WiFiClient client;
  while (true)
  {
    while (!client.connect(host, port))
    {
      Serial.println("Couldnt connect to the server");
      yield_counter++;
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
    }
    Serial.println("Connected to server!");
    String host_dir = SERVER_URL + "&signal=" + stored_data + "&last=" + is_last;
    client.print(String("GET ") + host_dir + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();

    //Waits for server response
    while (client.available() == 0)
    {
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
    }

    // Reads server response and prints it in the serial port
    while (client.available())
    {
      server_response = client.readStringUntil('\r');
      yield_counter++;
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
    }
    Serial.println("Server_response = " + server_response);
    if (server_response.equals("\nok"))
    {
      break;
    }
    else
    {
      Serial.println("Failed request");
      Serial.println("Trying again...\n\n");
    }
    server_response = "";
    if (yield_counter == MIN_YIELD_ITERS)
    {
      yield_counter = 0;
      yield();
    }
  }
}

void hard_reset()
{
  Serial.println('\n');
  Serial.println("Got some bad news for ya'...");
  Serial.println("\nDoomsday counter has already ticked " + String(DOOMSDAY) + " times");
  Serial.println("There seems to be a problem with either the WiFi connection,server connectivity or the sd module...");
  Serial.println("\nIssuing a hard reset...");
  Serial.println("Good Bye.");
  ESP.reset();
}

void save_and_upload_signal(float value)
{
  send_counter++;
  buffer_ = buffer_ + String(value) + ",";

  if (send_counter == MAX_MEM)
  {
    //Sends a message with a part of the signal (=3000 elements)
    GET(buffer_, "false");
    buffer_ = "";
    send_counter = 0;
  }

  if (captured_values_counter >= BUFFER_SIZE)
  {
    //Sends the last message with the last part of the signal (<3000 elements)
    GET(buffer_, "true");
    buffer_ = "";
    send_counter = 0;
    Serial.print("Signal successfully uploaded.\n");
    captured_values_counter = 0;
    captured_signals_counter++;
  }

}


float convertToVoltage(int bit_value)
{
  //Not used
  int max_ = 1023;
  float volt = 3.3;
  return bit_value * (volt / max_);
}

void loop()
{

  if (captured_signals_counter < NUM_GENERATED_SIGNALS)
  {
    int sensorValue = analogRead(A0);
    //int sensorValue = convertToVoltage(sensorValue);
    Serial.println(String(sensorValue));
    captured_values_counter++;

    save_and_upload_signal(sensorValue);
    delay(SAMPLE_PERIOD_MILLIS);
  }
}