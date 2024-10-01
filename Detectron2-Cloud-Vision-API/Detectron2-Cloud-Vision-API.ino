#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <esp_camera.h>
#include <SPI.h>

#include <FS.h>
#include <FSImpl.h>
#include <vfs_api.h>
#include <SD.h>
#include <sd_defines.h>
#include <sd_diskio.h>
#include <base64.h>

// Define camera model & pinout
#define CAMERA_MODEL_XIAO_ESP32S3  // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter API endpoint info
// ===========================
const char* myDomain = "<INSERT-API-ENDPOINT-DOMAIN-HERE>";
String myResource = "<INSERT-API-ENDPOINT-RESOURCE-HERE>";
String myApiKey = "<INSERT-API-ENDPOINT-APIKEY-HERE>";

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "<INSERT-WIFI-SSID-HERE>";
const char *password = "<INSERT-WIFI-PASSWORD-HERE>";

char *imageFileName = "";

//receive api response
float apiResponse;
bool touch1detected = true;

// Ref:
// https://randomnerdtutorials.com/esp32-https-requests/
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_Validation/BearSSL_Validation.ino
// https://github.com/fustyles/Arduino/blob/master/ESP32-CAM_openAI_vision/ESP32-CAM_openAI_vision.ino
// https://api.openweathermap.org/data/2.5/weather?lat=44.34&lon=10.99&appid=f9ef6538c50791760fea7f21430f1dcd
 
// Camera status variable
bool camera_status = false;
 
// MicroSD status variable
bool sd_status = false;
 
// Save pictures to SD card
String photo_save(const char *fileName) {
  // Take a photo
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if(!fb) {     
      delay(1000);
      ESP.restart();
      return "Camera capture failed";
    }

    String imageFile = base64::encode(fb->buf, fb->len);
    // Save photo to file - uncomment to save to SD card
    //writeFile(SD, fileName, fb->buf, fb->len);

    esp_camera_fb_return(fb);

    return imageFile;
}

String create_request_body(){
// Make sure the camera and MicroSD are ready
  String base64Image = "";
  if (camera_status) {
    // Take a picture
    sprintf(imageFileName, "/image%d.jpg", 1);
    base64Image = photo_save(imageFileName);
  }

  return "{\"body\":\"" + base64Image + "\"}"; 
}

// Camera Parameters for setup
void CameraParameters() {
  // Define camera parameters
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; //YUV422,GRAYSCALE,RGB565,JPEG
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;// UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA, QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_CIF);
  
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  delay(5000);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  //
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Define Camera Parameters and Initialize
  CameraParameters();
  // Camera is good, set status
  camera_status = true;

  Serial.println("");
  Serial.println("WiFi connected");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (touch1detected) {

    Serial.println();
    Serial.println("Start request");
    HTTPClient http;
    http.begin(myDomain);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", myApiKey);

    // Send the POST request
    int httpResponseCode = http.POST(create_request_body());

    // If we receive a valid response, parse it
    if (httpResponseCode > 0) {
      
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
      Serial.println("------------");
      JsonObject obj;
      DynamicJsonDocument doc(200000);
      //StaticJsonDocument<196608> doc();
      //SpiRamJsonDocument doc(196608);
      DeserializationError error = deserializeJson(doc, response);
      Serial.println("Error:");
      Serial.println(error.c_str());
      obj = doc.as<JsonObject>();
      //Serial.println(obj["pred_boxes"].as<String>());
      Serial.println(obj);
      //Serial.println(obj[0]["pred_boxes"].as<String>());
      Serial.println("Done.");
      apiResponse = response.toFloat();
    }
    else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    // End the connection
    http.end();
  
    // Reset the touch variable
    touch1detected = false;
  }

  delay(80);
}
