
#include <Arduino.h>
#include <PS2Kbd.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>

#include <HttpClient.h>
#include <WiFi.h>

#include <inttypes.h>

#include <stdio.h>

#include "esp_system.h"

#include "freertos/FreeRTOS.h"

#include "freertos/task.h"

#include <deque>


char* ssid = "Alex's Galaxy S23+";
char* pass = "maivxkx8jtsaqkt";

#define KEYBOARD_DATA_PIN 33
#define KEYBOARD_CLK_PIN  32
PS2Kbd keyboard(KEYBOARD_DATA_PIN, KEYBOARD_CLK_PIN);


#define BUZZER_PIN 15

// Some nice key defines.
#define KEY_ENTER 10
#define KEY_BACKSPACE 8
#define KEY_TOGGLE_HIDE 252

#define POP_BUTTON_PIN 2
bool pop_pressed = false;

#define NEW_FINGERS_BUTTON_PIN 15
int time_to_fingerprint_timeout = 0;
bool fingerprinting_flash_state = false;

#define RED_LED_PIN 12
#define GREEN_LED_PIN 13

bool access_state = false;

bool hide_mode = false;

std::string input_buffer;
std::string display_string;
std::deque<std::string> queue;

#define PASSWORD "53rocks"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

TFT_eSPI tft = TFT_eSPI();

bool should_clear_screen = false;

void clear_screen() {
  tft.setCursor(0,0);
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TFT_BLACK);
}

std::string build_queue_string() {
  std::string output = "/?queue=";
  if (queue.empty()) {
    output += "empty";
    return output;
  }
  int queue_print_iter = 0;
  for (auto entry : queue) {
    output += std::to_string(queue_print_iter);
    output += ":";
    output += entry;
    output += "<br/>";
    queue_print_iter += 1;
  }
  Serial.printf("Body: \"%s\"", output.c_str());
  return output;
}

void update_queue() {
  WiFiClient c;
  HttpClient http(c);

  http.get("18.223.112.58", 5000, build_queue_string().c_str(), NULL);
}

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("COnnected to wifi\n");

  keyboard.begin();
  pinMode(POP_BUTTON_PIN, INPUT_PULLUP);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT); 

  tft.begin();
  tft.setRotation(3);
    
  tft.setCursor(0,0);
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TFT_BLACK);
  tft.setTextSize(2);

  input_buffer = "";
  display_string = "";
}

void loop() {
  tft.setCursor(2, 10);
  tft.print(">");
  if (!input_buffer.empty()) {
    //tft.print(input_buffer.c_str());
    tft.print(display_string.c_str());
  }

  tft.setCursor(220, 10);
  if (hide_mode) {
  tft.print('*');
  }


  if (!queue.empty()) {
    int queue_print_iter = 0;
    for (auto entry : queue) {
      tft.setCursor(2, 30 + queue_print_iter * 20);
      tft.print(queue_print_iter + 1);
      tft.print(": ");
      tft.print(entry.c_str());
      if (++queue_print_iter == 5) {
        break;
      }
    }
  } else {
    tft.setCursor(2, 30);
    tft.print("QUEUE EMPTY");
  }

  if (access_state) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
  }

  if (keyboard.available()) {
    auto key = keyboard.read();
    Serial.printf("Key pressed: %d\n", key);
    if (isalnum(key)) {
      input_buffer.push_back(key);
      if (hide_mode) {
        display_string.push_back('*');
      } else {
        display_string.push_back(key);
      }
    } else if (KEY_ENTER == key) {
      if (!input_buffer.empty()) {
        if (PASSWORD == input_buffer) {
          Serial.printf("Password entered, authenticated.\n");
          clear_screen();
          access_state = true;
        } else {
          Serial.printf("Adding user: \"%s\"\n", input_buffer.c_str());
          clear_screen();
          queue.push_back(input_buffer);
          update_queue();
        }
      }
      input_buffer.clear();
      display_string.clear();
    } else if (KEY_BACKSPACE == key && !input_buffer.empty()) {
      input_buffer.pop_back();
      display_string.pop_back();
      clear_screen();
    } else if (KEY_TOGGLE_HIDE == key) {
      hide_mode = !hide_mode;
      input_buffer.clear();
      display_string.clear();
      clear_screen();
    }
  }

  // Check if pop has been pressed.
  if (0 == digitalRead(POP_BUTTON_PIN)) {
    if (false == pop_pressed) {
      if (!access_state) {
        tone(BUZZER_PIN, 1000, 5000);
        noTone(BUZZER_PIN);
        return;
      }
      if (!queue.empty()) {
        Serial.printf("Popping user: \"%s\"\n", queue.front().c_str());
        queue.pop_front();
        update_queue();
        clear_screen();
      }
      access_state = false;
      pop_pressed = true;
    }
  } else {
    pop_pressed = false;
  }

  delay(10);
}
