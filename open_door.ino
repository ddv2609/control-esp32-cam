#include <WiFi.h> // Include WIFi Library for ESP32
#include <WebServer.h> // Include WebSever Library for ESP32
#include <WebSocketsServer.h>  // Include Websocket Library
#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"


const char* ssid = "POCO";  // Your SSID
const char* password = "26092002"; // Your Password
#define LED 15
#define relay_pin 2 // pin 12 can also be used
unsigned long door_opened_millis = 0;
long interval = 10000;           // open lock for ... milliseconds
bool isUserControl = false;

// WebServer server(80);  // create instance for web server on port "80"
WebSocketsServer webSocket = WebSocketsServer(60);  //create instance for webSocket server on port"60"

void setup() {
  // put your setup code here, to run once:
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
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  digitalWrite(LED, LOW);
  pinMode(LED, OUTPUT); // Set PIN22 As output(LED Pin)
  digitalWrite(relay_pin, LOW);
  pinMode(relay_pin, OUTPUT);
  Serial.begin(115200); // Init Serial for Debugging.
  WiFi.begin(ssid, password); // Connect to Wifi 
  while (WiFi.status() != WL_CONNECTED) { // Check if wifi is connected or not
    delay(3000);
    Serial.print(".");
  }
  Serial.println();
  // Print the IP address in the serial monitor windows.
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // server.begin(); // init the server
  webSocket.begin();  // init the Websocketserver
  webSocket.onEvent(webSocketEvent);  // init the webSocketEvent function when a websocket event occurs 
}

// This function gets a call when a WebSocket event occurs
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: // enum that read status this is used for debugging.
      break;
    case WStype_CONNECTED:  // Check if a WebSocket client is connected or not
      break;
    case WStype_TEXT: // check responce from client
      Serial.println(payload[0] - 48);  // the payload variable stores teh status internally
      if (payload[0] - 48 == 9) {
        digitalWrite(LED, HIGH);
        digitalWrite(relay_pin, HIGH);
        door_opened_millis = millis(); // time relay closed and door opened
      } else {
        if (payload[0] - 48 == 1) {
          isUserControl = true;
        } else {
          isUserControl = false;
        }
        digitalWrite(LED, payload[0] - 48);
        digitalWrite(relay_pin, payload[0] - 48);
      }
      break;
  }
}


void loop() {
  // server.handleClient();  // webserver methode that handles all Client
  webSocket.loop(); // websocket server methode that handles all Client
  if (!isUserControl && millis() - interval > door_opened_millis) { // current time - face recognised time > 5 secs
    digitalWrite(LED, LOW); //open relay
    digitalWrite(relay_pin, LOW);
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {

    // Gửi mảng binary qua WebSocket
    webSocket.broadcastBIN(fb->buf, fb->len);

    esp_camera_fb_return(fb);
  }
}
