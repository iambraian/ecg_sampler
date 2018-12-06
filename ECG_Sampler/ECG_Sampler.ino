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
const int MAX_MEM = 3000;
String buffer_ = "";

//SERVER VARIABLES
const String SERVER_URL = "/sample?";
String host = "192.168.228.251";
const int port = 3000;

void setup()
{
  Serial.begin(9600);
  begin_wifi_connection();
  float window_size_millis = SAMPLING_TIME_SEC * 1000;
  int buffer_size = int(ceil(window_size_millis / SAMPLE_PERIOD_MILLIS));
  BUFFER_SIZE = buffer_size;
  Serial.println("MAX_MEM=" + MAX_MEM);
  Serial.println("NUM_GENERATED_SIGNALS=" + NUM_GENERATED_SIGNALS);
  Serial.println("SAMPLE_PERIOD_MILLIS=" + SAMPLE_PERIOD_MILLIS);
  Serial.println("SAMPLING_TIME_SEC=" + SAMPLING_TIME_SEC);
  Serial.println("MESSAGE_SENT_EVERY=" + +"ms");
  Serial.println("BUFFER_SIZE=" + BUFFER_SIZE);
  Serial.println("SERVER=" + host + ":" + port + SERVER_URL);
}

void begin_wifi_connection()
{
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    //Espera a que se conecte a Red WIFI
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

/* String format_request(String body, String isLast)
{
  String request;
  char quotationMark = '"';
  request.concat("data=");
  request.concat("{");
  request.concat(quotationMark);
  request.concat("signal");
  request.concat(quotationMark);
  request.concat(":");
  request.concat(quotationMark);
  request.concat(body);
  request.concat(quotationMark);
  request.concat(",");
  request.concat(quotationMark);
  request.concat("last");
  request.concat(quotationMark);
  request.concat(":");
  request.concat(quotationMark);
  request.concat(isLast);
  request.concat(quotationMark);
  request.concat("}");
  return request;
} */

void GET(String stored_data, String is_last)
{
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

    //Espera respuesta del servidor
    while (client.available() == 0)
    {
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
    }

    // Lee la respuesta del servidor y la muestra por el puerto serie
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

/* void upload_signal(int captured_value)
{
  if (captured_value != MAX_MEM * 3)
  {
    GET(buffer_, "false");
  }
  else
  {
    GET(buffer_, "true");
  }
} */

void save_and_upload_signal(float value)
{
  send_counter++;
  buffer_ = buffer_ + String(value) + ",";

  if (send_counter == MAX_MEM)
  {
    GET(buffer_, "false");
    buffer_ = "";
    send_counter = 0;
  }

  if (captured_values_counter >= BUFFER_SIZE)
  {
    GET(buffer_, "true");
    buffer_ = "";
    send_counter = 0;
    Serial.print("Signal successfully uploaded.\n");
    captured_values_counter = 0;
    captured_signals_counter++;
  }

}

/* void save_signal(String value_to_concatenate)
{
  buffer_ += value_to_concatenate + ",";
} */

float convertToVoltage(int bit_value)
{
  int max_ = 1023;
  float volt = 3.3;
  return (volt * bit_value) / max_;
  // return (max_/volt)*bit_value;
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
    /* save_signal(sensorValue);
    if(captured_values_counter % MAX_MEM == 0)
    {
      if (captured_values_counter != 0)
      {
        upload_signal(captured_values_counter);
        buffer_ = "";
      }
      
      if (captured_values_counter == (MAX_MEM*TOTAL_MESSAGES))
      {
        captured_signals_counter++;
        captured_values_counter = 0;
      }
    } */
    delay(SAMPLE_PERIOD_MILLIS);
  }
}
