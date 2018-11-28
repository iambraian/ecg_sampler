#include <ESP8266WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ESP8266HTTPClient.h>

//250 muestras por segundo - Señal de 40seg
//Delay: 4ms
//Sensores analógicos

const int NUM_GENERATED_SIGNALS = 1;
int doomsday_counter = 0;
const int DOOMSDAY = 50;

//NETWORK STATIC VARIABLES
const char *ssid = "wl-fmat-ccei";
const char *password = "";

//SAMPLING VARIABLES
const int SAMPLE_PERIOD_MILLIS = 5;
int captured_signals_counter = 0;
const int SAMPLING_TIME_SEC = 20;
const int MIN_YIELD_ITERS = 200;

//STORAGE STATIC VARIABLES
const int SD_digital_IO = 8;
int BUFFER_SIZE = 0;
const int MAX_MEM = 3000;

//SERVER VARIABLES
const String SERVER_URL = "/samples?";
String host = "192.168.226.192";
const int port = 3000;

void setup()
{
  Serial.begin(9600);
  begin_wifi_connection();
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

String format_request(String body, String isLast)
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
}

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

void save_ramp_signal()
{
  float window_size_millis = SAMPLING_TIME_SEC * 1000;
  int buffer_size = int(ceil(window_size_millis / SAMPLE_PERIOD_MILLIS));
  BUFFER_SIZE = buffer_size;
  int spike_magnitude = 100;
  int ramp_counter = 0;
  int yield_counter = 0;
  
  File dataFile = SD.open("ECG.txt", FILE_WRITE);
  if (dataFile.available())
  {
    for (int i = 0; i < buffer_size; ++i)
    {
      ramp_counter++;
      yield_counter++;
      if (ramp_counter < spike_magnitude / 2)
      {
        dataFile.print(String(float(0)) + ",");
      }
      else
      {
        dataFile.print(String(float(ramp_counter)) + ",");
      }
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
      if (ramp_counter >= spike_magnitude)
      {
        ramp_counter = 0;
      }
    }
    dataFile.close();
  }
  else
  {
    Serial.print("Couldn't open the specified txt file");
    hard_reset();
  }
}

void upload_stored_data()
{
  //SD.begin(SD_digital_IO);
  //File dataFile = SD.open("ECG.txt");
  //SD.remove("ECG.txt");
  //if (dataFile.available())
  //{
    String stored_data = "";
    int send_counter = 0;
    int yield_counter = 0;
    for (int file_index = 0; file_index < BUFFER_SIZE; ++file_index)
    {
      yield_counter++;
      send_counter++;
      stored_data = stored_data + dataFile.read();
      if (send_counter == MAX_MEM)
      {
        GET(stored_data, "false");
        stored_data = "";
        send_counter = 0;
      }
      if (yield_counter == MIN_YIELD_ITERS)
      {
        yield_counter = 0;
        yield();
      }
    }
    GET(stored_data, "true");
  //}
  //else
  //{
    //Serial.println("Couldn't open the specified textfile!");
  //}
  //dataFile.close();
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

float voltageToTemperature(float voltage){
   return voltage * 100;
}

float convertToVoltage(int bit_value){
  int max_ = 1023;
  float volt = 3.3;
  return (volt*bit_value)/max_;
}

void loop()
{
  if (captured_signals_counter <= NUM_GENERATED_SIGNALS)
  {
    int sensorValue = analogRead(A0);
    Serial.print(String(voltageToTemperature(convertToVoltage(sensorValue))));
    save_ramp_signal();
    upload_stored_data();
    Serial.print("Signal successfully uploaded...");
    captured_signals_counter++;
  }
}
