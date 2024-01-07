// Libraries
#include <WiFi.h>
#include <PubSubClient.h>
#include <DFRobot_ENS160.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <lvgl.h>
#include "UI/ui.h"


// Sensor Variables
DFRobot_ENS160_I2C  ENS160(&Wire, 0x53); 
uint16_t            ENS160_AQI;
uint16_t            ENS160_eCO2;
uint16_t            ENS160_TVOC;


// Wi-Fi and MQTT variables
WiFiClient    WiFi_Client;
PubSubClient  MQTT_Client(WiFi_Client);
IPAddress     WiFi_IP       (192, 168, 1, x);
IPAddress     WiFi_Gateway  (192, 168, 1, 1);
IPAddress     WiFi_Subnet   (255, 255, 0, 0);
const char*   WiFi_SSID     = "";
const char*   WiFi_Password = "";
const char*   MQTT_Server   = "";
const char*   MQTT_User     = "";                                 
const char*   MQTT_Password = "";  


// Needs to be 600, with default value HA MQTT Auto Discovery message is not send
#define MQTT_MAX_PACKET_SIZE 600;


// Home Assistant Discovery Variables
const char*         ESP_Device_Model        = "ESP32-S2";                            
const char*         ESP_Software_Version    = "1.0";                                      
const char*         ESP_Device_Manufacturer = "Diggs";                               
String              ESP_Device_Name         = "Air Quality Monitor";                            
String              ESP_Device_Abbreviation = "AQM";                            
String              MQTT_Topic_Status       = "ESP32/" + ESP_Device_Abbreviation;     
byte                MQTT_Expire_After_Time  = 20;
String              ESP_Unique_ID;


// Time variables
unsigned long Time_Current      = 0;
unsigned long Time_To_Connect   = 10000;  // 10 seconds
unsigned long Time_Update       = 15000;  // 15 seconds


// Message variables
StaticJsonDocument<200> MQTT_JSON;
String MQTT_JSON_to_String;


// Functions
void HomeAssistant_Discovery();
void MQTT_Callback(char* topic, byte* inFrame, unsigned int length); 
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p );

// TFT variables
static const uint16_t TFT_Width  = 160;
static const uint16_t TFT_Height = 128;

// LVGL variables
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ TFT_Width * 10 ];
TFT_eSPI tft = TFT_eSPI(TFT_Width, TFT_Height); 


// TFT Screen start
void TFT_Start()
{
  // Start TFT
  tft.begin();          

  // Set rotation
  tft.setRotation( 3 );

  // Clean screen
  tft.fillScreen(TFT_WHITE); 
}

// LVGL start
void LVGL_Start()
{
  // Code is exported from SquareLine Studio
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, TFT_Width * 10 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = TFT_Width;
  disp_drv.ver_res = TFT_Height;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  ui_init();
}

// TFT display flush
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  // Code is exported from SquareLine Studio

  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  tft.endWrite();

  lv_disp_flush_ready( disp );
}

// Starting Wi-Fi
void WiFi_Start() 
{
  // Device MAC Address
  byte MAC_Address[6];  
  
  Serial.println("Starting Wi-Fi !");
  
  // Wi-Fi with static IP, faster connection | If error, restart ESP
  if(!WiFi.config(WiFi_IP, WiFi_Gateway, WiFi_Subnet))
  {
    Serial.println("Error configuring Wi-Fi !");
    Serial.println("ESP restarting !");

    // Restart
    ESP.restart();
  }

  // Start Wi-Fi
  WiFi.begin(WiFi_SSID, WiFi_Password);

  // Wi-Fi MAC Address for ESP_Unique ID
  WiFi.macAddress(MAC_Address);

  //Conversion
  ESP_Unique_ID =  String(MAC_Address[0],HEX) +String(MAC_Address[1],HEX) +String(MAC_Address[2],HEX) +String(MAC_Address[3],HEX) + String(MAC_Address[4],HEX) + String(MAC_Address[5],HEX);

  // Print Unique ID
  Serial.print("ESP Unique ID: ");
  Serial.println(ESP_Unique_ID);  

  // Time variable for cycle
  unsigned long Connection_Time = millis();

  // Cycle while Wi-Fi is not connected
  while (WiFi.status() != WL_CONNECTED) 
  {
    // If time is over and Wi-Fi is not connected, restart ESP
    if ((millis() - Connection_Time >= Time_To_Connect) &&  (WiFi.status() != WL_CONNECTED)) 
    {
      Serial.println("Error connecting to Wi-Fi !");
      Serial.println("ESP restarting !");

      // Restart
      ESP.restart();
    }
  }

  Serial.println("Wi-Fi connected !");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Starting MQTT
void MQTT_Start()
{
  // MQTT server IP and port
  MQTT_Client.setServer(MQTT_Server, 1883);

  // Time variable for cycle
  unsigned long Connection_Time = millis();

  // Cycle while there is no MQTT connection
  while (!MQTT_Client.connected()) 
  {
    // MQTT Connection
    MQTT_Client.connect(ESP_Device_Abbreviation.c_str(), MQTT_User, MQTT_Password);

    // If time is over and Wi-Fi is not connected, restart ESP
    if ((millis() - Connection_Time >= Time_To_Connect) &&  (!MQTT_Client.connected())) 
    {
      Serial.println("Error connection to MQTT server !");
      Serial.println("ESP restarting !");

      // Restart
      ESP.restart();
    }
  }

  Serial.println("MQTT connected !");

  // Callback function
  MQTT_Client.setCallback(MQTT_Callback);

  // Subscribe topic home assistant, important in case of HA restart
  MQTT_Client.subscribe("homeassistant/status");
}

// MQTT reconnect function
void MQTT_Reconnect() 
{
 // Loop while ESP is not connected with MQTT server
  while (!MQTT_Client.connected())
  {
    Serial.print("Reconnecting with MQTT server ... ");
    
    // Trying
    if (MQTT_Client.connect(ESP_Device_Abbreviation.c_str(), MQTT_User, MQTT_Password)) 
    {
      Serial.println("Connected");

      // If connected, subscribe home assistant topic again
      MQTT_Client.subscribe("homeassistant/status");
    } 
    else 
    {
      Serial.println("\nFailed");

      // Delay before trying again
      delay(1000);
    }
  }  
}

// Function to handle messages received
void MQTT_Callback(char* topic, byte* inFrame, unsigned int length) 
{
  // Variable to store received message
  String Message_Received;
  
  // Convert message to string
  for (int i = 0; i < length; i++) 
  {
    Message_Received += (char)inFrame[i];
  }

  // If topic is correct
  if(String(topic) == String("homeassistant/status")) 
  {
    // If message is online
    if(Message_Received == "online")
    {
      // Send discovery configuration again
      HomeAssistant_Discovery();
    }
  }
}

// Function for Home Assistant Discovery configuration
void HomeAssistant_Discovery()
{
  // If cliente is connected with MQTT server
  if(MQTT_Client.connected())
  {
    // Variables needed
    String Discovery_Topic;
    String JSON_to_String;

    // JSON variables
    StaticJsonDocument<600> JSON_Variables;
    JsonObject JSON_Nest_Device;
    JsonArray JSON_Nest_Identifier;

    // TVOC Configuration

    // Topic to publish discovery 
    Discovery_Topic = "homeassistant/sensor/ESP32/" + ESP_Device_Abbreviation + "_tvoc" + "/config";
    
    // JSON Configuration
    JSON_Variables["name"]                = "TVOC";
    JSON_Variables["unique_id"]           = ESP_Unique_ID + "_tvoc";
    JSON_Variables["device_class"]        = "volatile_organic_compounds_parts";
    JSON_Variables["state_topic"]         = MQTT_Topic_Status;
    JSON_Variables["value_template"]      = "{{ value_json.TVOC | is_defined }}";
    JSON_Variables["unit_of_measurement"] = "ppb";
    JSON_Variables["expire_after"]        = MQTT_Expire_After_Time;

    // JSON nest device and identifier configuration
    JSON_Nest_Device                  = JSON_Variables.createNestedObject("device");
    JSON_Nest_Device["name"]          = ESP_Device_Name;
    JSON_Nest_Device["model"]         = ESP_Device_Model;
    JSON_Nest_Device["sw_version"]    = ESP_Software_Version;
    JSON_Nest_Device["manufacturer"]  = ESP_Device_Manufacturer;
    JSON_Nest_Identifier              = JSON_Nest_Device.createNestedArray("identifiers");
    JSON_Nest_Identifier.add(ESP_Unique_ID);

    // Convert from JSON to String
    serializeJson(JSON_Variables, JSON_to_String);
    
    // Publish
    MQTT_Client.publish(Discovery_Topic.c_str(), JSON_to_String.c_str());

    // Clear data
    JSON_Variables.clear();
    JSON_Nest_Device.clear();
    JSON_Nest_Identifier.clear();
    JSON_to_String.clear();

    // AQI Configuration

    // Topic to publish discovery 
    Discovery_Topic = "homeassistant/sensor/ESP32/" + ESP_Device_Abbreviation + "_aqi" + "/config";
    
    // JSON Configuration
    JSON_Variables["name"]            = "AQI";
    JSON_Variables["unique_id"]       = ESP_Unique_ID + "_aqi";
    JSON_Variables["device_class"]    = "aqi";
    JSON_Variables["state_topic"]     = MQTT_Topic_Status;
    JSON_Variables["value_template"]  = "{{ value_json.AQI | is_defined }}";
    JSON_Variables["expire_after"]    = MQTT_Expire_After_Time;
    
    // JSON nest device and identifier configuration
    JSON_Nest_Device                  = JSON_Variables.createNestedObject("device");
    JSON_Nest_Device["name"]          = ESP_Device_Name;
    JSON_Nest_Device["model"]         = ESP_Device_Model;
    JSON_Nest_Device["sw_version"]    = ESP_Software_Version;
    JSON_Nest_Device["manufacturer"]  = ESP_Device_Manufacturer;
    JSON_Nest_Identifier              = JSON_Nest_Device.createNestedArray("identifiers");
    JSON_Nest_Identifier.add(ESP_Unique_ID);

    // Convert JSON to string
    serializeJson(JSON_Variables, JSON_to_String);

    // Publish
    MQTT_Client.publish(Discovery_Topic.c_str(), JSON_to_String.c_str());

    // Clear data
    JSON_Variables.clear();
    JSON_Nest_Device.clear();
    JSON_Nest_Identifier.clear();
    JSON_to_String.clear();

    // eCO2 Configuration

    // Topic
    Discovery_Topic = "homeassistant/sensor/ESP32/" + ESP_Device_Abbreviation + "_eco2" + "/config";
    
    // JSON configuration
    JSON_Variables["name"]                = "eCO2";
    JSON_Variables["unique_id"]           = ESP_Unique_ID + "_eco2";
    JSON_Variables["device_class"]        = "carbon_dioxide";
    JSON_Variables["state_topic"]         = MQTT_Topic_Status;
    JSON_Variables["value_template"]      = "{{ value_json.eCO2 | is_defined }}";
    JSON_Variables["unit_of_measurement"] = "ppm";
    JSON_Variables["expire_after"]        = MQTT_Expire_After_Time;
    
    // JSON nest device and identifier configuration
    JSON_Nest_Device                  = JSON_Variables.createNestedObject("device");
    JSON_Nest_Device["name"]          = ESP_Device_Name;
    JSON_Nest_Device["model"]         = ESP_Device_Model;
    JSON_Nest_Device["sw_version"]    = ESP_Software_Version;
    JSON_Nest_Device["manufacturer"]  = ESP_Device_Manufacturer;
    JSON_Nest_Identifier              = JSON_Nest_Device.createNestedArray("identifiers");
    JSON_Nest_Identifier.add(ESP_Unique_ID);

    // Convert from JSON to string
    serializeJson(JSON_Variables, JSON_to_String);

    // Publish
    MQTT_Client.publish(Discovery_Topic.c_str(), JSON_to_String.c_str());

    // Clear data
    JSON_Variables.clear();
    JSON_Nest_Device.clear();
    JSON_Nest_Identifier.clear();
    JSON_to_String.clear();

    // Status configuration

    // Topic to publish discovery 
    Discovery_Topic = "homeassistant/binary_sensor/ESP32/" + ESP_Device_Abbreviation + "_state" + "/config";
    
    // JSON configuration
    JSON_Variables["name"] = "State";
    JSON_Variables["unique_id"] = ESP_Unique_ID + "_state";
    JSON_Variables["device_class"] = "connectivity";
    JSON_Variables["entity_category"] = "diagnostic";
    JSON_Variables["payload_on"] = "online";
    JSON_Variables["state_topic"] = MQTT_Topic_Status;
    JSON_Variables["value_template"] = "{{ value_json.State | is_defined }}";
    JSON_Variables["expire_after"] = MQTT_Expire_After_Time;
    
    // JSON nest device and identifier configuration
    JSON_Nest_Device = JSON_Variables.createNestedObject("device");
    JSON_Nest_Device["name"] = ESP_Device_Name;
    JSON_Nest_Device["model"] = ESP_Device_Model;
    JSON_Nest_Device["sw_version"] = ESP_Software_Version;
    JSON_Nest_Device["manufacturer"] = ESP_Device_Manufacturer;
    JSON_Nest_Identifier = JSON_Nest_Device.createNestedArray("identifiers");
    JSON_Nest_Identifier.add(ESP_Unique_ID);

    // Convert form JSON to string
    serializeJson(JSON_Variables, JSON_to_String);
    
    // Publish
    MQTT_Client.publish(Discovery_Topic.c_str(), JSON_to_String.c_str());
  }
}

// Start ENS160 sensor
void ENS160_Start()
{
  // Start
  Wire.begin();

  // Time variable for cycle
  unsigned long Connection_Time = millis();

  // Cycle while there is no connection
  while (ENS160.begin() != NO_ERR) 
  {
    // If time is over and Wi-Fi is not connected, restart ESP
    if ((millis() - Connection_Time >= Time_To_Connect)) 
    {
      Serial.println("ESP restarting !");
      
      // Restart
      ESP.restart();
    }

    Serial.println("Error connecting with ENS160 !");
    
    // Small delay before trying again
    delay(10);
  }

  // Set mode
  ENS160.setPWRMode(ENS160_STANDARD_MODE);

  Serial.println("ENS160 working !");
}

// ENS160 Readings
void ENS160_Readings()
{
  // If is not connected then restart
  if(ENS160.getENS160Status() == 3)
  {
    Serial.println("Error with ENS160 !");

    Serial.println("ESP restarting !");

    // Restart ESP
    ESP.restart();
  }
  else
  {
    // Read values
    ENS160_AQI  = ENS160.getAQI();
    ENS160_eCO2 = ENS160.getECO2();
    ENS160_TVOC = ENS160.getTVOC();

    // If one value is 0, it means there was an error
    if((ENS160_AQI ||ENS160_eCO2 || ENS160_TVOC) == 0)
    {
      Serial.println("Error with ENS160 !");

      Serial.println("ESP restarting !");

      // Restart ESP
      ESP.restart();
    }
  }
}

// Setup
void setup() 
{
  // Start Serial, needed with ESP32-S2, without code does not work
  Serial.begin(115200);

  // Delay because of USB serial, 2 seconds before everything is working
  delay(2000);
                                                              
  Serial.println("ESP32-S2 | Air Quality Monitor");

  // Start ENS160 sensor
  ENS160_Start();
  
  // Start Wi-Fi and MQTT
  WiFi_Start();
  MQTT_Start();

  // Send Home Assistant discovery
  HomeAssistant_Discovery();

  // Start TFT display
  TFT_Start();

  // Start LVGL interface
  LVGL_Start();

  // Time variable with negative number, needed to send on first loop cycle
  Time_Current = -Time_Update;
}

// Loop
void loop() 
{
  // LVGL handler
  lv_timer_handler();

  // Delay for not overloading
  delay(1);

  // If Wi-Fi is working
  if(WiFi.status() == WL_CONNECTED)
  {
    // If MQTT is not connected
    if(!MQTT_Client.connected())
    {
      // Reconnect
      MQTT_Reconnect();
    }
    else
    {
      // Normal loop to mantain connection with server
      MQTT_Client.loop();    
    }
  }
  else
  {
    // Time variable for cycle
    unsigned long Connection_Time = millis();

    // Disconnect Wi-Fi
    WiFi.disconnect();

    // Reconnect Wi-Fi
    WiFi.reconnect();
    
    Serial.print("Reconnecting to Wi-Fi ... ");

    // Cycle Wi-Fi is not connected
    while(WiFi.status() != WL_CONNECTED)
    {
      // If time is over and Wi-Fi is not connected, restart ESP
      if((WiFi.status() != WL_CONNECTED) && (millis() - Connection_Time >=Time_To_Connect))
      {
        Serial.println("\nRestarting ESP !");

        // Restart
        ESP.restart();
      }
    }

    Serial.println("Reconnected !");
  }

  // Publish data every time cycle 
  if(millis() - Time_Current >= Time_Update)
  {
    // Current time
    Time_Current = millis();

    // Clear variables
    MQTT_JSON.clear();
    MQTT_JSON_to_String.clear();

    // Get sensor values
    ENS160_Readings();

    // JSON MQTT variables
    MQTT_JSON["State"]  = "online";
    MQTT_JSON["TVOC"]   = ENS160_TVOC;
    MQTT_JSON["eCO2"]   = ENS160_eCO2;
    MQTT_JSON["AQI"]    = ENS160_AQI;

    // Convert JSON to string
    serializeJson(MQTT_JSON, MQTT_JSON_to_String);

    // Print values to serial monitor
    Serial.println(MQTT_JSON_to_String);

    // If client is connected to MQTT server
    if(MQTT_Client.connected())
    {
      // Publish
      MQTT_Client.publish(MQTT_Topic_Status.c_str(), MQTT_JSON_to_String.c_str());
    }

    // Update AQI arrow
    lv_obj_set_y(ui_Arrow_AQI, (45 - ((ENS160_AQI-1)*20)));   

    // Update eC02 and AQI values
    lv_label_set_text(ui_eCO2_Value,String(ENS160_eCO2).c_str());
    lv_label_set_text(ui_AQI_Value,String(ENS160_AQI).c_str());

    // Update eCO2 arrow
    if(ENS160_eCO2 >= 400 && ENS160_eCO2 < 600)
    {
      lv_obj_set_y(ui_Arrow_eC02, 45);
    }
    else if (ENS160_eCO2 >= 600 && ENS160_eCO2 < 800)
    {
      lv_obj_set_y(ui_Arrow_eC02, 25);      
    }
    else if (ENS160_eCO2 >= 800 && ENS160_eCO2 < 1000)
    {
      lv_obj_set_y(ui_Arrow_eC02, 5);      
    }
    else if (ENS160_eCO2 >= 1000 && ENS160_eCO2 < 1500)
    {
      lv_obj_set_y(ui_Arrow_eC02, -15);      
    }
    else if (ENS160_eCO2 >= 1500)
    {
      lv_obj_set_y(ui_Arrow_eC02, -35);      
    }
  }


}