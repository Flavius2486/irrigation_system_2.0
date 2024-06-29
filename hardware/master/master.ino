  #include <DS3231.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include "WiFi.h"
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <HTTPClient.h>

//change this in userSetup.h every update/*
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define TOUCH_CS 16
*/

TaskHandle_t background_task;
SPIClass* hspi = new SPIClass(HSPI);
SoftwareSerial HC12(33, 32);  //rx tx

//tft
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
//rtc
DS3231 rtc;
// tft btns
TFT_eSPI_Button key[20];
TFT_eSPI_Button back_btn;
TFT_eSPI_Button toggle_wifi_btn;
TFT_eSPI_Button keyboard_keys[35];


#define GRAY 0x8410
#define WHITE 0xFFFF
#define BLACK 0x0000
#define DARKGREY 0x7BEF
//------------------------------------------------------------------------------------------
#define tft_backlight 17
#define systemErrorLedPin 25

//menu variables
int page_index = 0;
int faucet_number = 0;
int nav_history[5] = {};

//networks variables
int found_networks_number = 0;
int view_networks_page_number = 0;
int show_wifi_options = -1;
int selected_network_index = -1;
int prev_selected_network_index = -1;
int number_of_networks_per_page = 4;
int prev_shown_wifi_icon_status = -1;
int wifi_on = 0;
int wifiReconnectionTimer = 0;

//kayboard
int keyboard_page = 0;
char keyboard_data[50] = "";

//timer variables
int prev_second = -1;
int inactivity_seconds = 0;
int incativity_max_seconds = 60;

//system settings
int tft_brightness = 99;
int systemComponentsCheckCounter = 0;

//keyboard
int show_keyboard = 0;

const int number_of_faucets = 4;
int systemHardwareError = false;

bool h12 = false;
bool AM_PM = false;

struct faucet {
  int start_hour = 0;
  int start_minute = 0;
  int start_second = 0;
  unsigned int start_total_seconds = 0;
  int stop_hour = 0;
  int stop_minute = 0;
  int stop_second = 0;
  unsigned int stop_total_seconds = 0;
  int timer_hour = 0;
  int timer_minute = 0;
  int timer_second = 0;
  int8_t day_interval = 0;
  int8_t day_counter = 1;
  bool timer_on = false;
  unsigned int last_modified = 0;
};

faucet faucets[number_of_faucets];

struct button {
  int target_page;
  uint16_t x;
  uint16_t y;
  int height;
  int width;
  int text_size;
  u_int16_t bg_color;
  u_int16_t outline_color;
  u_int16_t text_color;
  char text[20];
  void (*callback)(int);
};

struct page {
  int number;
  int nested_position;
  int num_of_btns;
  struct button* buttons;  // Use a pointer for flexible array member
};

/*--------------------------------------------------------------*\
                    Set system time functions
\*--------------------------------------------------------------*/

int new_hour;
int new_minute;
int new_second;

void increment_new_hour(int faucet_number) {
  new_hour++;
  if (new_hour > 23)
    new_hour = 0;
};

void decrement_new_hour(int faucet_number) {
  new_hour--;
  if (new_hour < 0)
    new_hour = 23;
};

void increment_new_minute(int faucet_number) {
  new_minute++;
  if (new_minute > 59)
    new_minute = 0;
};

void decrement_new_minute(int faucet_number) {
  new_minute--;
  if (new_minute < 0)
    new_minute = 59;
};

void increment_new_second(int faucet_number) {
  new_second++;
  if (new_second > 59)
    new_second = 0;
};

void decrement_new_second(int faucet_number) {
  new_second--;
  if (new_second < 0)
    new_second = 59;
};

void save_new_time(int a) {
  rtc.setHour(new_hour);
  rtc.setMinute(new_minute);
  rtc.setSecond(new_second);
}


/*--------------------------------------------------------------*\
                    Start time functions
\*--------------------------------------------------------------*/


void start_time_to_seconds(int faucet_number) {
  faucets[faucet_number].start_total_seconds = (3600 * faucets[faucet_number].start_hour) + (60 * faucets[faucet_number].start_minute) + faucets[faucet_number].start_second;
}

void increment_start_hour(int faucet_number) {
  faucets[faucet_number].start_hour++;
  if (faucets[faucet_number].start_hour > 23)
    faucets[faucet_number].start_hour = 0;
};

void decrement_start_hour(int faucet_number) {
  faucets[faucet_number].start_hour--;
  if (faucets[faucet_number].start_hour < 0)
    faucets[faucet_number].start_hour = 23;
};

void increment_start_minute(int faucet_number) {
  faucets[faucet_number].start_minute++;
  if (faucets[faucet_number].start_minute > 59)
    faucets[faucet_number].start_minute = 0;
};

void decrement_start_minute(int faucet_number) {
  faucets[faucet_number].start_minute--;
  if (faucets[faucet_number].start_minute < 0)
    faucets[faucet_number].start_minute = 59;
};

void increment_start_second(int faucet_number) {
  faucets[faucet_number].start_second++;
  if (faucets[faucet_number].start_second > 59)
    faucets[faucet_number].start_second = 0;
};

void decrement_start_second(int faucet_number) {
  faucets[faucet_number].start_second--;
  if (faucets[faucet_number].start_second < 0)
    faucets[faucet_number].start_second = 59;
};


/*--------------------------------------------------------------*\
                    Stop time functions
\*--------------------------------------------------------------*/

void stop_time_to_seconds(int faucet_number) {
  faucets[faucet_number].stop_total_seconds = (3600 * faucets[faucet_number].stop_hour) + (60 * faucets[faucet_number].stop_minute) + faucets[faucet_number].stop_second;
}

void increment_stop_hour(int faucet_number) {
  faucets[faucet_number].stop_hour++;
  if (faucets[faucet_number].stop_hour > 23)
    faucets[faucet_number].stop_hour = 0;
};

void decrement_stop_hour(int faucet_number) {
  faucets[faucet_number].stop_hour--;
  if (faucets[faucet_number].stop_hour < 0)
    faucets[faucet_number].stop_hour = 23;
};

void increment_stop_minute(int faucet_number) {
  faucets[faucet_number].stop_minute++;
  if (faucets[faucet_number].stop_minute > 59)
    faucets[faucet_number].stop_minute = 0;
};

void decrement_stop_minute(int faucet_number) {
  faucets[faucet_number].stop_minute--;
  if (faucets[faucet_number].stop_minute < 0)
    faucets[faucet_number].stop_minute = 59;
};

void increment_stop_second(int faucet_number) {
  faucets[faucet_number].stop_second++;
  if (faucets[faucet_number].stop_second > 59)
    faucets[faucet_number].stop_second = 0;
};

void decrement_stop_second(int faucet_number) {
  faucets[faucet_number].stop_second--;
  if (faucets[faucet_number].stop_second < 0)
    faucets[faucet_number].stop_second = 59;
};

void save_start_stop_time(int faucet_number) {
  stop_time_to_seconds(faucet_number);
  start_time_to_seconds(faucet_number);
  update_sd_faucets_data(faucet_number, "start_stop_time");
};


/*--------------------------------------------------------------*\
                    Timer functions
\*--------------------------------------------------------------*/

void increment_timer_hour(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_hour++;
    if (faucets[faucet_number].timer_hour > 23)
      faucets[faucet_number].timer_hour = 0;
  }
};

void decrement_timer_hour(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_hour--;
    if (faucets[faucet_number].timer_hour < 0)
      faucets[faucet_number].timer_hour = 24;
  }
};

void increment_timer_minute(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_minute++;
    if (faucets[faucet_number].timer_minute > 59)
      faucets[faucet_number].timer_minute = 0;
  }
};

void decrement_timer_minute(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_minute--;
    if (faucets[faucet_number].timer_minute < 0)
      faucets[faucet_number].timer_minute = 59;
  }
};

void increment_timer_second(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_second++;
    if (faucets[faucet_number].timer_second > 59)
      faucets[faucet_number].timer_second = 0;
  }
};

void decrement_timer_second(int faucet_number) {
  if (!faucets[faucet_number].timer_on) {
    faucets[faucet_number].timer_second--;
    if (faucets[faucet_number].timer_second < 0)
      faucets[faucet_number].timer_second = 59;
  }
};

void toggle_timer(int faucet_number) {
  faucets[faucet_number].timer_on = !faucets[faucet_number].timer_on;
}

void reset_timer(int faucet_number) {
  faucets[faucet_number].timer_hour = 0;
  faucets[faucet_number].timer_minute = 0;
  faucets[faucet_number].timer_second = 0;
}


/*--------------------------------------------------------------*\
                    Day interval functions
\*--------------------------------------------------------------*/

void increment_day_interval(int faucet_number) {
  faucets[faucet_number].day_interval++;
  if (faucets[faucet_number].day_interval > 14)
    faucets[faucet_number].day_interval = 0;
};

void decrement_day_interval(int faucet_number) {
  faucets[faucet_number].day_interval--;
  if (faucets[faucet_number].day_interval < 0)
    faucets[faucet_number].day_interval = 14;
};

void save_day_interval(int faucet_number) {
  update_sd_faucets_data(faucet_number, "day_interval");
};

/*--------------------------------------------------------------*\
                    Setting functions
\*--------------------------------------------------------------*/

int convert_brightness_value() {
  float outputValueFloat = map(tft_brightness, 8, 99, 0, 255);
  // Convert the output value to the appropriate voltage level
  float outputVoltage = (outputValueFloat / 255.0) * 3.3;
  // Convert the voltage to the PWM duty cycle (0-255)
  return (outputVoltage / 3.3) * 255;
}

void store_brightness_to_sd() {
  File file = SD.open("/settings/brightness.txt", FILE_WRITE);
  if (file) {
    file.seek(0);
    file.println(String(tft_brightness));
  } else {
    Serial.println("Falied to update wifi toggle");
  }
  file.close();
}

void increase_brightness(int a) {
  if (tft_brightness < 99) {
    tft_brightness += 3;
    analogWrite(tft_backlight, convert_brightness_value());
    store_brightness_to_sd();
  }
}

void decrease_brightness(int a) {
  if (tft_brightness > 13) {
    tft_brightness -= 3;
    analogWrite(tft_backlight, convert_brightness_value());
    store_brightness_to_sd();
  }
}

/*--------------------------------------------------------------*\
                    System menu structure
\*--------------------------------------------------------------*/

void next_networks_page(int a) {
  int max_network_pages = found_networks_number / number_of_networks_per_page == 0 ? found_networks_number / number_of_networks_per_page : ceil(found_networks_number / number_of_networks_per_page);
  if (view_networks_page_number < max_network_pages) {
    view_networks_page_number++;
    tft.fillRect(0, 40, 320, 165, BLACK);
    show_networks();
  }
};

void prev_networks_page(int a) {
  if (view_networks_page_number > 0) {
    view_networks_page_number--;
    tft.fillRect(0, 40, 320, 165, BLACK);
    show_networks();
  }
};

void refresh_networks(int a);

/*--------------------------------------------------------------*\
                    System menu structure
\*--------------------------------------------------------------*/

struct page pages[] = {
  { 1, 0, 4, (struct button[]){ { 4, 160, 70, 40, 300, 3, GRAY, GRAY, WHITE, "Faucets", nullptr }, { 2, 160, 115, 40, 300, 3, GRAY, GRAY, WHITE, "Clock", nullptr }, { 3, 160, 160, 40, 300, 3, GRAY, GRAY, WHITE, "WI-FI", nullptr }, { 9, 160, 205, 40, 300, 3, GRAY, GRAY, WHITE, "Settings", nullptr } } },
  { 2, 1, 7, (struct button[]){ { 0, 160, 215, 30, 310, 2, GRAY, GRAY, WHITE, "Save", save_new_time }, { 0, 105, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_new_hour }, { 0, 105, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_new_hour }, { 0, 160, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_new_minute }, { 0, 160, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_new_minute }, { 0, 215, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_new_second }, { 0, 215, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_new_second } } },
  { 3, 1, 3, (struct button[]){ { 0, 15, 225, 40, 40, 3, BLACK, BLACK, WHITE, "<", prev_networks_page }, { 0, 305, 225, 40, 40, 3, BLACK, BLACK, WHITE, ">", next_networks_page }, { 0, 270, 225, 40, 40, 2, BLACK, BLACK, WHITE, "Re", refresh_networks } } },
  { 4, 1, 4, (struct button[]){ { 5, 160, 70, 40, 300, 3, GRAY, GRAY, WHITE, "Faucet 1", nullptr }, { 5, 160, 115, 40, 300, 3, GRAY, GRAY, WHITE, "Faucet 2", nullptr }, { 5, 160, 160, 40, 300, 3, GRAY, GRAY, WHITE, "Faucet 3", nullptr }, { 5, 160, 205, 40, 300, 3, GRAY, GRAY, WHITE, "Faucet 4", nullptr } } },
  { 5, 2, 3, (struct button[]){ { 6, 160, 70, 40, 300, 3, GRAY, GRAY, WHITE, "Set Timer", nullptr }, { 7, 160, 115, 40, 300, 3, GRAY, GRAY, WHITE, "Start/Stop hours", nullptr }, { 8, 160, 160, 40, 300, 3, GRAY, GRAY, WHITE, "Day interval", nullptr } } },
  { 6, 3, 8, (struct button[]){ { 0, 80, 215, 30, 150, 2, GRAY, GRAY, WHITE, "Start/Stop", toggle_timer }, { 0, 240, 215, 30, 150, 2, GRAY, GRAY, WHITE, "Reset", reset_timer }, { 0, 105, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_timer_hour }, { 0, 105, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_timer_hour }, { 0, 160, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_timer_minute }, { 0, 160, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_timer_minute }, { 0, 215, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_timer_second }, { 0, 215, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_timer_second } } },
  { 7, 3, 13, (struct button[]){ { 0, 160, 215, 30, 310, 2, GRAY, GRAY, WHITE, "Save", save_start_stop_time }, { 0, 105, 62, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_start_hour }, { 0, 105, 103, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_start_hour }, { 0, 160, 62, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_start_minute }, { 0, 160, 103, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_start_minute }, { 0, 215, 62, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_start_second }, { 0, 215, 103, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_start_second }, { 0, 105, 138, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_stop_hour }, { 0, 105, 180, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_stop_hour }, { 0, 160, 138, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_stop_minute }, { 0, 160, 180, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_stop_minute }, { 0, 215, 138, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_stop_second }, { 0, 215, 180, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_stop_second } } },
  { 8, 3, 3, (struct button[]){ { 0, 160, 215, 30, 310, 2, GRAY, GRAY, WHITE, "Save", save_day_interval }, { 0, 160, 105, 30, 30, 2, BLACK, BLACK, WHITE, ">", increment_day_interval }, { 0, 160, 147, 30, 30, 2, BLACK, BLACK, WHITE, "<", decrement_day_interval } } },
  { 9, 1, 2, (struct button[]){ { 0, 215, 70, 40, 30, 3, BLACK, BLACK, WHITE, "<", decrease_brightness }, { 0, 305, 70, 40, 30, 3, BLACK, BLACK, WHITE, ">", increase_brightness } } },
};


/*--------------------------------------------------------------*\
                    Virtual keyboard
\*--------------------------------------------------------------*/

struct keyboard_struct {
  const char (*keys)[13];  // Pointer to array of characters
};

const char lower_letters_keys[3][13] PROGMEM = {
  { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '\0' },
  { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '\0' },
  { 'z', 'x', 'c', 'v', 'b', 'n', 'm', '\0' },
};

const char capital_letters_keys[3][13] PROGMEM = {
  { 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '\0' },
  { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '\0' },
  { 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '\0' },
};

const char numbers_keys[3][13] PROGMEM = {
  { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\0' },
  { '-', '/', ':', ';', '(', ')', '€', '&', '@', '"', '\0' },
  { '.', ',', '?', '!', '\'', '\0' }
};

const char symbols_keys[3][13] PROGMEM = {
  { '[', ']', '{', '}', '#', '%', '^', '*', '+', '=', '\0' },
  { '_', '\\', '|', '~', '<', '>', '$', '£', '¥', '•', '\0' },
  { '.', ',', '?', '!', '\'', '\0' }
};

// Define the keyboard instances
keyboard_struct keyboard[] = {
  { lower_letters_keys },
  { capital_letters_keys },
  { numbers_keys },
  { symbols_keys }
};

struct other_keyboard_buttons {
  int x, y, width, height;
  char text[10];
};

other_keyboard_buttons switch_keyboard_buttons[6] = {
  { 32, 64 + 110, 64, 32, "CAPS" },
  { 32, 96 + 110, 64, 32, "#+=" },
  { 120, 96 + 110, 112, 32, "SPACE" },
  { 216, 96 + 110, 80, 32, "Cancel" },
  { 288, 96 + 110, 64, 32, "Save" },
  { 285, 68, 45, 25, "<-" }
};

void draw_keyboard() {
  tft.fillRect(0, 110, 320, 240, BLACK);
  //start with six because of the fixed keys
  int characters_counter = 6;
  // 3 - number of lines
  for (int key_row = 0; key_row < 3; key_row++) {
    //get the line characters length
    int numberOfCharactersPerLine = strlen(keyboard[keyboard_page].keys[key_row]);
    for (int key_column = 0; key_column < numberOfCharactersPerLine; key_column++) {
      //determine the start x coord for the first key
      int buttonXPos = ((10 - numberOfCharactersPerLine) / 2 + key_column) * 32;
      if (numberOfCharactersPerLine % 2 != 0)
        buttonXPos += 16;
      if (key_row == 2) buttonXPos = 64 + key_column * 32;
      //extract the character
      char temp_character[2] = { keyboard[keyboard_page].keys[key_row][key_column], '\0' };
      //create the key
      keyboard_keys[characters_counter].initButton(&tft,
                                                   buttonXPos + 15,     // X Cord
                                                   key_row * 32 + 110,  // Y CORD
                                                   32,                  // WIDTH
                                                   32,                  // HEIGHT
                                                   GRAY,                //OUTLINE
                                                   BLACK,               // FILL
                                                   WHITE,               // TEXT COLOR
                                                   temp_character,      // TEXT TO PRINT
                                                   2);                  // TEXT SIZE: SEE ABOVE Line 23
      keyboard_keys[characters_counter].drawButton();
      characters_counter++;
    }
  }
  //show the fixed buttons of the keyword
  for (int key_index = 0; key_index < 6; key_index++) {
    keyboard_keys[key_index].initButton(&tft,
                                        switch_keyboard_buttons[key_index].x,       // X Cord
                                        switch_keyboard_buttons[key_index].y,       // Y CORD
                                        switch_keyboard_buttons[key_index].width,   // WIDTH
                                        switch_keyboard_buttons[key_index].height,  // HEIGHT
                                        GRAY,                                       //OUTLINE
                                        BLACK,                                      // FILL
                                        WHITE,                                      // TEXT COLOR
                                        switch_keyboard_buttons[key_index].text,    // TEXT TO PRINT
                                        2);                                         // TEXT SIZE
    keyboard_keys[key_index].drawButton();
  }
}

void keyboard_system(uint16_t t_x, uint16_t t_y) {
  //keyboard
  if (show_keyboard) {
    //we run the for loop 6 times, because the kayboard has 6 stable button(remain the same even if the caps or symbols keyboard was accessed)
    for (int key_index = 0; key_index < 6; key_index++) {
      keyboard_keys[key_index].press(keyboard_keys[key_index].contains(t_x, t_y));
      if (keyboard_keys[key_index].justPressed()) {
        switch (key_index) {
          case 0:
            //change keyboard page
            keyboard_page = keyboard_page == 1 ? 0 : 1;
            draw_keyboard();
            break;
          case 1:
            //change keyboard page
            keyboard_page = keyboard_page == 0 || keyboard_page == 1 || keyboard_page == 3 ? 2 : 3;
            draw_keyboard();
            break;
          case 2:
            //add space on button press
            if (strlen(keyboard_data) < 20) {
              strcat(keyboard_data, " ");
              draw_text(keyboard_data, 10, 65, 2, BLACK);
            }
            break;
          case 3:
            //cancel button pressed
            strcpy(keyboard_data, "");
            show_keyboard = 0;
            tft.fillRect(0, 40, 320, 240, BLACK);
            show_networks();
            draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
            show_wifi_options = selected_network_index = -1;
            // Go to the Wi-Fi pages and delete string
            break;
          case 4:
            //disconnect from previous wifi
            // WiFi.disconnect();
            //connect with the typed password to the selected network
            WiFi.begin(WiFi.SSID(selected_network_index), keyboard_data);
            //hide keyboard
            show_keyboard = 0;
            tft.fillRect(0, 40, 320, 240, BLACK);
            //if known encryption wait to connect
            if (true) {
              draw_text("Connecting...", 45, 100, 3, BLACK);
              //wait 10 seconds to connect
              int connection_timer = 0;
              while (WiFi.status() != WL_CONNECTED && connection_timer < 10) {
                connection_timer++;
                delay(1000);
              }
              //connection falied
              if (connection_timer == 10) {
                tft.fillRect(0, 40, 320, 240, BLACK);
                draw_text("Falied to", 70, 90, 3, BLACK);
                draw_text("connect!", 80, 120, 3, BLACK);
              } else {
                //connected
                tft.fillRect(0, 40, 320, 240, BLACK);
                draw_text("Connection", 60, 90, 3, BLACK);
                draw_text("succesfull!", 60, 120, 3, BLACK);
                //create the network file
                File file = SD.open("/wifi/" + String(WiFi.SSID(selected_network_index)) + ".txt", FILE_WRITE);
                if (!file) {
                  Serial.println("Failed to create file (remember network)");
                } else {
                  //add the network ssid and password in the sd
                  // file.println(WiFi.SSID(selected_network_index));
                  file.println(keyboard_data);
                }
                file.close();
              }
            }
            //  else {
            //   //if the encryption type is not recognize show message
            //   draw_text("Can't connect to ", 15, 90, 3, BLACK);
            //   draw_text("this network!", 45, 120, 3, BLACK);
            // }
            delay(4000);
            //show the networks
            tft.fillRect(0, 40, 320, 240, BLACK);
            show_networks();
            draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
            show_wifi_options = selected_network_index = -1;
            break;
          case 5:
            //each time the delete character button, delete the last character
            if (strlen(keyboard_data) > 0) {
              keyboard_data[strlen(keyboard_data) - 1] = '\0';
              strcat(keyboard_data, " ");
              draw_text(keyboard_data, 10, 65, 2, BLACK);
              keyboard_data[strlen(keyboard_data) - 1] = '\0';
            }
            break;
        }
        keyboard_keys[key_index].press(false);
        break;
      }
    }
    int characters_counter = 6;
    bool exit_loop = false;
    for (int key_row = 0; key_row < 3 && !exit_loop; key_row++) {
      for (int key_column = 0; key_column < strlen(keyboard[keyboard_page].keys[key_row]); key_column++) {
        keyboard_keys[characters_counter].press(keyboard_keys[characters_counter].contains(t_x, t_y));
        if (keyboard_keys[characters_counter].justPressed()) {
          // Serial.println(keyboard[keyboard_page].keys[key_row][key_column]);
          char temp_string[2] = { keyboard[keyboard_page].keys[key_row][key_column], '\0' };
          //add the character only if the length if lower than 20
          if (strlen(keyboard_data) < 20) {
            strcat(keyboard_data, temp_string);
            draw_text(keyboard_data, 10, 65, 2, BLACK);
          }
          keyboard_keys[characters_counter].press(false);
          exit_loop = true;
          break;
        }
        characters_counter++;
      }
    }
  }
}


/*--------------------------------------------------------------*\
                    SD writing functions
\*--------------------------------------------------------------*/

time_t convertToUnixTime(int year, int month, int day, int hour, int minute, int second) {
  tmElements_t tm;
  tm.Year = year - 1970;  // Years since 1970
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  // Convert to Unix time
  return makeTime(tm);
}

void update_sd_faucets_data(int faucet_number, String variable) {
  int faucetTotalStartSeconds = 0, faucetTotalStopSeconds = 0, faucetDayInterval = 0, faucetDayCounter = 0, faucetLastModified = 0;
  bool update = false;
  //get faucet file
  String filePath = "/faucets/" + String(faucet_number) + ".txt";
  File file = SD.open(filePath.c_str(), FILE_READ);
  if (file) {
    //get faucet data and store it
    faucetTotalStartSeconds = file.readStringUntil('\n').toInt();
    faucetTotalStopSeconds = file.readStringUntil('\n').toInt();
    faucetDayInterval = file.readStringUntil('\n').toInt();
    faucetDayCounter = file.readStringUntil('\n').toInt();
    update = true;
    file.close();
    //delete the file
    SD.remove(filePath.c_str());
  }
  //create a ne file and add the new faucet data in it
  file = SD.open(filePath.c_str(), FILE_WRITE);
  if (file && update) {
    if (variable == "day_counter") {
      file.println(faucetTotalStartSeconds);
      file.println(faucetTotalStopSeconds);
      file.println(faucetDayInterval);
      file.println(faucets[faucet_number].day_counter);
    } else {
      if (variable == "start_stop_time") {
        file.println(faucets[faucet_number].start_total_seconds);
        file.println(faucets[faucet_number].stop_total_seconds);
        file.println(faucetDayInterval);
        file.println(faucetDayCounter);
      } else if (variable == "day_interval") {
        file.println(faucetTotalStartSeconds);
        file.println(faucetTotalStopSeconds);
        file.println(faucets[faucet_number].day_interval);
        file.println(faucetDayCounter);
      }
      bool mo = false;
      file.println(convertToUnixTime(rtc.getYear(), rtc.getMonth(mo), rtc.getDate(), rtc.getHour(h12, AM_PM), rtc.getMinute(), rtc.getSecond()));
    }
    file.close();
  }
}


/*--------------------------------------------------------------*\
                        Wifi functions
\*--------------------------------------------------------------*/

String get_wifi_encryption_type(int network_index) {
  switch (WiFi.encryptionType(network_index)) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2 PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2 PSK";
    default:
      return "Unknown";
  }
}

// String get_network_file_name(int network_index) {
//   //if true selected the bssid of the selected network if not conected to wifi
//   //otherwise selected the current wifi bssid
//   String bssid = network_index >= 0 ? WiFi.BSSIDstr(network_index) : WiFi.BSSIDstr();
//   //replace : with - to be able to store the file
//   for (int i = 0; i < bssid.length(); i++)
//     if (bssid[i] == ':')
//       bssid[i] = '-';
//   return "/wifi/" + bssid + ".txt";
// }

int get_network_signal_strenght(int rssi) {
  if (rssi >= -30) {
    return 4;
  } else if (rssi >= -60) {
    return 3;
  } else if (rssi >= -85) {
    return 2;
  } else if (rssi < -85) {
    return 1;
  }
}

void wifi_signal_icon(int x, int y, int size, int signal_strenght) {
  int height = 5, width = 8;
  if (size == 1) {
    height = 4;
    width = 6;
  }
  //start with 3(the smallest line)
  //increase x with half of the next line position to keep it in the middle
  //adjust y based on the height and leave 1px gap between
  //adjust width
  //set fill color based on the signal strenght
  for (int i = 3; i >= 0; i--)
    drawBorderedRect(x + (width / 2 * i), y + ((height + 1) * i), (4 - i) * width - 2, height, WHITE, 4 - i <= signal_strenght ? WHITE : GRAY);
  if (size > 1) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED, GRAY);
    tft.setCursor(x + (width * 3) - 4, y + (height * 4) - 2);
    signal_strenght == 0 ? tft.print("x") : tft.print(" ");
  }
}

void show_networks() {
  //network starting pos
  int y_pos = 50;
  tft.fillRect(0, 40, 320, 165, BLACK);
  //based on the page start with the coresponding network index
  //ex: if second page was accessed there it will be shown network from index 4 to 8 (4*page number)
  for (int network_index = number_of_networks_per_page * view_networks_page_number; network_index < number_of_networks_per_page * view_networks_page_number + number_of_networks_per_page && network_index < found_networks_number; network_index++) {
    //add 3 dots if the ssid is too long
    String wifi_ssid = WiFi.SSID(network_index);
    if (wifi_ssid.length() > 15) {
      wifi_ssid = wifi_ssid.substring(0, 15);
      wifi_ssid += "...";
    }
    //add a grey background to the selected network
    if (network_index == selected_network_index) {
      tft.fillRect(5, y_pos - 5, 310, 35, DARKGREY);
      draw_text(wifi_ssid, 10, y_pos, 2, DARKGREY);
      draw_text(get_wifi_encryption_type(network_index), 10, y_pos + 17, 1, DARKGREY);
    } else {
      draw_text(wifi_ssid, 10, y_pos, 2, BLACK);
      draw_text(get_wifi_encryption_type(network_index), 10, y_pos + 17, 1, BLACK);
    }
    wifi_signal_icon(278, y_pos + 4, 1, get_network_signal_strenght(WiFi.RSSI(network_index)));
    y_pos += 40;
  }
}

void scan_networks() {
  //show message
  draw_text("Scanning for", 45, 80, 3, BLACK);
  draw_text("networks...", 60, 110, 3, BLACK);
  //scan for networks and store the number of them
  found_networks_number = WiFi.scanNetworks();
  tft.fillRect(0, 40, 320, 165, BLACK);
  //show messsage if no networks were found
  if (found_networks_number == 0) {
    draw_text("No networks", 50, 80, 3, BLACK);
    draw_text("found!", 90, 110, 3, BLACK);
  } else {
    //otherwise list the networks
    show_networks();
  }
}

void refresh_networks(int a) {
  if (wifi_on) {
    found_networks_number = view_networks_page_number = 0;
    show_wifi_options = prev_selected_network_index = selected_network_index = -1;
    show_keyboard = 0;
    tft.fillRect(0, 40, 320, 200, BLACK);
    draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
    WiFi.scanDelete();
    if (WiFi.status() != WL_CONNECTED)
      WiFi.disconnect();
    scan_networks();
  }
}

void selected_network_options(uint16_t t_x, uint16_t t_y) {
  //networks page open; found more than one network and the kayboard it is not shown
  if (pages[page_index].number == 3 && found_networks_number > 0 && !show_keyboard) {
    int listed_network_base_y = 50;
    for (int network_index = 0; network_index < number_of_networks_per_page; network_index++) {
      if (t_y >= listed_network_base_y - 5 && t_y <= listed_network_base_y + 35) {
        selected_network_index = (view_networks_page_number * number_of_networks_per_page) + network_index;
        if (selected_network_index != prev_selected_network_index) {
          //update the previos network selected
          prev_selected_network_index = selected_network_index;
          //list again the networks and mark the selected one
          show_networks();
          // get the selected network data
          String selected_network_ssid = WiFi.SSID((view_networks_page_number * number_of_networks_per_page) + network_index);
          // String selected_network_bssid = WiFi.BSSIDstr((view_networks_page_number * number_of_networks_per_page) + network_index);
          //selected the network data from sd
          File file = SD.open("/wifi/" + String(WiFi.SSID(selected_network_index)) + ".txt", FILE_READ);
          if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == selected_network_ssid && /*WiFi.BSSIDstr() == selected_network_bssid &&*/ show_wifi_options != 0) {
            //if already connected to it shoew coresponding options
            tft.fillRect(32, 210, 220, 240, BLACK);
            draw_text("Disconnect|Forget", 31, 212, 2, BLACK);
            show_wifi_options = 0;
          } else if (file && show_wifi_options != 1) {
            //if it is not connected to the seleted network, but it is in the sd show coresponding options
            tft.fillRect(32, 210, 220, 240, BLACK);
            draw_text("Connect...|Forget", 31, 212, 2, BLACK);
            show_wifi_options = 1;
          } else {
            //if the network it is not known
            // Show button to connect
            tft.fillRect(32, 210, 220, 240, BLACK);
            draw_text("Connect", 31, 212, 2, BLACK);
            show_wifi_options = 2;
          }
          file.close();
        }
        break;
      }
      //set index for the next network from the page
      listed_network_base_y += 40;
    }
  }
  //if the system was previously connected to the selected network o
  //or it is curently connected
  if (show_wifi_options == 0 || show_wifi_options == 1) {
    //first button pressed
    if (t_x > 32 && t_x < 160 && t_y > 205 && t_y < 220) {
      // disconnect button pressed
      // show a pop up while disconnecting
      if (show_wifi_options == 0) {
        WiFi.disconnect();
        tft.fillRect(0, 40, 320, 240, BLACK);
        draw_text("Disconnecting...", 40, 100, 3, BLACK);
        //wait for the esp to disconnect
        while (WiFi.status() != WL_DISCONNECTED) {
          delay(100);
        }
      } else {
        //reconnect button was pressed
        //get the file with the network data
        File file = SD.open("/wifi/" + String(WiFi.SSID(selected_network_index)) + ".txt", FILE_READ);
        if (!file) {
          Serial.println("Failed to open file (reconnect)");
        } else {
          //disconnect from previous network and connect to the selected one
          WiFi.disconnect();
          //select the network data and delete white spaces
          // String ssid = file.readStringUntil('\n');
          // ssid.trim();
          String password = file.readStringUntil('\n');
          password.trim();
          //connect
          WiFi.begin(String(WiFi.SSID(selected_network_index)), password.c_str());
          tft.fillRect(0, 40, 320, 240, BLACK);
          draw_text("Connecting...", 45, 100, 3, BLACK);
          //wait to connect
          int connection_timer = 0;
          while (WiFi.status() != WL_CONNECTED && connection_timer < 10) {
            connection_timer++;
            delay(1000);
          }
          //could not connect
          if (connection_timer == 10) {
            tft.fillRect(0, 40, 320, 240, BLACK);
            draw_text("Falied to", 70, 90, 3, BLACK);
            draw_text("connect!", 80, 120, 3, BLACK);
          } else {
            //connected
            tft.fillRect(0, 40, 320, 240, BLACK);
            draw_text("Connection", 60, 90, 3, BLACK);
            draw_text("succesfull!", 60, 120, 3, BLACK);
          }
          delay(4000);
        }
        file.close();
      }
      //show the available networks
      tft.fillRect(0, 40, 320, 240, BLACK);
      show_networks();
      draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
    } else if (t_x > 160 && t_x < 228 && t_y > 205 && t_y < 230) {
      //forget network button pressed -> deisconnect from the network
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        tft.fillRect(0, 40, 320, 240, BLACK);
        draw_text("Disconnecting...", 20, 100, 3, BLACK);
        //wait to disconnect
        int disconnecting_counter = 0;
        while (WiFi.status() != WL_DISCONNECTED || disconnecting_counter == 5) {
          disconnecting_counter++;
          delay(1000);
        }
      }
      //show the available networks
      tft.fillRect(0, 40, 320, 240, BLACK);
      show_networks();
      draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
      //delete the network data from the sd
      // SD.remove(get_network_file_name(selected_network_index));
      SD.remove("/wifi/" + String(WiFi.SSID(selected_network_index)) + ".txt");
    }
  } else if (show_wifi_options == 2 && t_x > 32 && t_x < 130 && t_y > 205 && t_y < 230) {
    //new network selected
    //connect button pressed
    //show virtual keyboard
    tft.fillRect(0, 40, 320, 240, BLACK);
    init_keyboard();
    show_keyboard = 1;
  }
}

/*--------------------------------------------------------------*\
                      TFT drawing functions
\*--------------------------------------------------------------*/

void draw_buttons(int num_of_btns, struct button btn_data[]) {
  //draw the buttons from the selected page
  for (int btn_index = 0; btn_index < num_of_btns; btn_index++) {
    key[btn_index].initButton(&tft,                               // REF
                              btn_data[btn_index].x,              // X
                              btn_data[btn_index].y,              // Y
                              btn_data[btn_index].width,          // WIDTH
                              btn_data[btn_index].height,         // HEIGHT
                              btn_data[btn_index].outline_color,  //bg color
                              btn_data[btn_index].bg_color,       // OUTLINE
                              btn_data[btn_index].text_color,     // TEXT COLOR
                              btn_data[btn_index].text,           // TEXT TO PRINT
                              btn_data[btn_index].text_size);     // TEXT SIZE
    key[btn_index].drawButton();
  }
}

void draw_navbar() {
  tft.fillRect(0, 0, 320, 40, GRAY);
  back_btn.initButton(&tft,   // REF
                      20,     // X
                      25,     // Y
                      30,     // WIDTH
                      30,     // HEIGHT
                      GRAY,   //bg color
                      GRAY,   // OUTLINE
                      WHITE,  // TEXT COLOR
                      "<",    // TEXT TO PRINT
                      3);     // TEXT SIZE
  back_btn.drawButton();
}

void draw_wifi_toggle_button() {
  uint16_t btn_bg_color = TFT_GREEN;
  char text[3] = "ON";
  if (!wifi_on) {
    btn_bg_color = TFT_RED;
    strcpy(text, "OFF");
  }
  toggle_wifi_btn.initButton(&tft,          // REF
                             260,           // X
                             110,           // Y
                             60,            // WIDTH
                             30,            // HEIGHT
                             btn_bg_color,  //bg color
                             btn_bg_color,  // OUTLINE
                             WHITE,         // TEXT COLOR
                             text,          // TEXT TO PRINT
                             3);            // TEXT SIZE
  toggle_wifi_btn.drawButton();
}

void show_time_format(int hour, int minute, int second, int x, int y, int size, u_int16_t bg_color) {
  char time_string[9];
  //if the hour/minute/second value is lower than 10 put a 0 before it
  if (hour < 10)
    sprintf(time_string, "0%d", hour);
  else
    sprintf(time_string, "%d", hour);
  strcat(time_string, ":");
  if (minute < 10)
    sprintf(time_string + strlen(time_string), "0%d", minute);
  else
    sprintf(time_string + strlen(time_string), "%d", minute);
  strcat(time_string, ":");
  if (second < 10)
    sprintf(time_string + strlen(time_string), "0%d", second);
  else
    sprintf(time_string + strlen(time_string), "%d", second);
  //print it on the screen
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.setTextColor(WHITE, bg_color);
  tft.print(time_string);
}

void show_day_interval(int day_interval, int x, int y, int size) {
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.setTextColor(WHITE, BLACK);
  if (day_interval < 10) tft.print("0" + String(day_interval));
  else tft.print(String(day_interval));
}

void draw_text(String text, int x, int y, int size, u_int16_t bg_color) {
  tft.setTextSize(size);
  tft.setTextColor(WHITE, bg_color);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawBorderedRect(int x, int y, int width, int height, uint16_t borderColor, uint16_t innerColor) {
  tft.drawRect(x, y, width, height, borderColor);
  tft.fillRect(x + 1, y + 1, width - 2, height - 2, innerColor);
}

/*--------------------------------------------------------------*\
                         Background tasks
\*--------------------------------------------------------------*/

void timer_countdown() {
  //check for every faucet if th timer has to start
  //decrese hour/minute/second until all of them are 0
  for (int faucet_index = 0; faucet_index < number_of_faucets; faucet_index++) {
    if (faucets[faucet_index].timer_on && (faucets[faucet_index].timer_second || faucets[faucet_index].timer_minute || faucets[faucet_index].timer_hour)) {
      faucets[faucet_index].timer_second--;
      if (faucets[faucet_index].timer_second < 0) {
        faucets[faucet_index].timer_second = 59;
        faucets[faucet_index].timer_minute--;
        if (faucets[faucet_index].timer_minute < 0) {
          faucets[faucet_index].timer_minute = 59;
          faucets[faucet_index].timer_hour--;
        }
      }
    } else if (faucets[faucet_index].timer_on) {
      faucets[faucet_index].timer_on = false;
      faucets[faucet_index].timer_second = 0;
      faucets[faucet_index].timer_minute = 0;
      faucets[faucet_index].timer_hour = 0;
    }
  }
}

void update_day_counter() {
  //check every faucet and update the day counter when the day changes
  for (int faucet_index = 0; faucet_index < number_of_faucets; faucet_index++) {
    if (rtc.getDate() != faucets[faucet_index].day_counter && faucets[faucet_index].day_interval > 0) {
      faucets[faucet_index].day_counter++;
      if (faucets[faucet_index].day_counter >= faucets[faucet_index].day_interval + 1)
        faucets[faucet_index].day_counter = 1;
      //set the data to sd
      update_sd_faucets_data(faucet_index, "day_counter");
    }
  }
}

void open_close_faucet() {
  //create the array to send the data to the slave
  String data = "0000";
  int dataToSend[number_of_faucets] = {};
  int current_time_seconds = (rtc.getHour(h12, AM_PM) * 3600) + (rtc.getMinute() * 60) + rtc.getSecond();
  for (int faucet_index = 0; faucet_index < number_of_faucets; faucet_index++) {
    //extract data from the sd for every faucet
    String filePath = "/faucets/" + String(faucet_index) + ".txt";
    File file = SD.open(filePath.c_str(), FILE_READ);
    if (file) {
      int faucet_open_seconds = file.readStringUntil('\n').toInt();
      int faucet_close_seconds = file.readStringUntil('\n').toInt();
      int day_interval = file.readStringUntil('\n').toInt();
      int day_counter = file.readStringUntil('\n').toInt();
      //verify if the current hour is between the faucet start and stop hour and see if day interval is the same as day counter
      //or turn on if timer is on
      if ((current_time_seconds >= faucet_open_seconds && current_time_seconds < faucet_close_seconds && faucet_open_seconds < faucet_close_seconds && day_interval && day_interval == day_counter) || faucets[faucet_index].timer_on) {
        data[faucet_index] = '1';
      }
    } else {
      Serial.println("Failed to open " + String(filePath) + " (data to send)");
    }
  }
  HC12.write(data.c_str());
}

void send_data_to_server() {
  // if (WiFi.status() == WL_CONNECTED) {
  //   HTTPClient http;
  //   WiFiClient client;
  //   http.begin("http://172.18.144.1:3000/esp32/faucets-data");
  //   http.addHeader("Content-Type", "application/json");
  //   String data = "{\"sensor\":\"temperature\",\"value\":25.5}";
  //   int httpCode = http.POST(data);
  //   if (httpCode > 0) {
  //     String payload = http.getString();
  //     Serial.println(payload);
  //   } else {
  //     Serial.println("Error on HTTP request");
  //   }

  //   http.end();
  // }
}

void run_in_background(void* pvParameters) {
  for (;;) {
    //timer
    timer_countdown();
    //day counter
    update_day_counter();
    //faucets signal
    open_close_faucet();
    send_data_to_server();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void run_every_sec() {
  if (rtc.getSecond() != prev_second) {
    prev_second = rtc.getSecond();
    //count the incativity seconds
    inactivity_seconds++;
    //if the time is up put tft to sleep
    if (inactivity_seconds == incativity_max_seconds) {
      tft.writecommand(0x10);
      //reset keyboard variables
      show_keyboard = 0;
      strcpy(keyboard_data, "");
      //reset network
      analogWrite(tft_backlight, 0);
      WiFi.scanDelete();
      found_networks_number = view_networks_page_number = 0;
      show_wifi_options = prev_selected_network_index = prev_shown_wifi_icon_status = selected_network_index = -1;
    } else if (inactivity_seconds > incativity_max_seconds) {
      inactivity_seconds = incativity_max_seconds + 1;
    } else {
      //-----run only if display is not on sleep mode-----//
      //show time on navbar
      show_time_format(rtc.getHour(h12, AM_PM), rtc.getMinute(), rtc.getSecond(), 90, 10, 3, GRAY);
      //show timer when the page is opened
      if (faucet_number != -1 && pages[page_index].number == 6 && faucets[faucet_number].timer_on) {
        show_time_format(faucets[faucet_number].timer_hour, faucets[faucet_number].timer_minute, faucets[faucet_number].timer_second, 90, 110, 3, BLACK);
      }
      //update time variables while the page is not accessed
      if (pages[page_index].number != 2) {
        new_hour = rtc.getHour(h12, AM_PM);
        new_minute = rtc.getMinute();
        new_second = rtc.getSecond();
      }


      // show wifi strenghr based on rssi value on the naw bar
      // draw again only of a new icon must be shown
      if (WiFi.status() != WL_CONNECTED && prev_shown_wifi_icon_status != 0) {
        wifi_signal_icon(278, 8, 2, 0);
        prev_shown_wifi_icon_status = 0;
      } else if (WiFi.status() == WL_CONNECTED) {
        int wifiStrength = get_network_signal_strenght(WiFi.RSSI());
        if (wifiStrength == 4 && prev_shown_wifi_icon_status != 1) {
          wifi_signal_icon(278, 8, 2, 4);
          prev_shown_wifi_icon_status = 1;
        } else if (wifiStrength == 3 && prev_shown_wifi_icon_status != 2) {
          wifi_signal_icon(278, 8, 2, 3);
          prev_shown_wifi_icon_status = 2;
        } else if (wifiStrength == 2 && prev_shown_wifi_icon_status != 3) {
          wifi_signal_icon(278, 8, 2, 2);
          prev_shown_wifi_icon_status = 3;
        } else if (wifiStrength == 1 && prev_shown_wifi_icon_status != 4) {
          wifi_signal_icon(278, 8, 2, 1);
          prev_shown_wifi_icon_status = 4;
        }
      }
    }
    //try and reconnect after lost connection
    wifiReconnectionTimer++;
    if (wifiReconnectionTimer > 10) {
      if (WiFi.status() != WL_CONNECTED && page_index != 3 && wifi_on)
        WiFi.reconnect();
      wifiReconnectionTimer = 0;
    }

    systemComponentsCheckCounter++;
    if (systemComponentsCheckCounter > 5) {
      if (!SD.exists("/"))
        systemHardwareError = true;
      systemComponentsCheckCounter = 0;
    }

    if (systemHardwareError)
      digitalWrite(systemErrorLedPin, HIGH);
  }
}
/*--------------------------------------------------------------*\
                    Setup functions
\*--------------------------------------------------------------*/

void init_tft() {
  tft.begin();
  tft.setRotation(3);
  tft.fillRect(0, 0, 320, 240, BLACK);
  //show navbar
  draw_navbar();
  //show the time on the navbar
  show_time_format(rtc.getHour(h12, AM_PM), rtc.getMinute(), rtc.getSecond(), 90, 10, 3, GRAY);
  //access first page
  draw_buttons(pages[0].num_of_btns, pages[0].buttons);
  //turn on tft light
  analogWrite(tft_backlight, convert_brightness_value());
}

void init_keyboard() {
  keyboard_page = 0;
  //set kayboard data to empty string
  strcpy(keyboard_data, "");
  //draw keyboard
  draw_keyboard();
  //draw the input box and the text
  drawBorderedRect(5, 50, 250, 35, GRAY, BLACK);
  draw_text(keyboard_data, 10, 65, 2, BLACK);
}

void update_faucets_data() {
  //on restart restore the faucets data from the sd
  File file;
  for (int faucet_index = 0; faucet_index < number_of_faucets; faucet_index++) {
    //open faucet file
    String filePath = "/faucets/" + String(faucet_index) + ".txt";
    file = SD.open(filePath.c_str(), FILE_READ);
    if (file) {
      //extract data
      faucets[faucet_index].start_total_seconds = file.readStringUntil('\n').toInt();
      faucets[faucet_index].stop_total_seconds = file.readStringUntil('\n').toInt();
      faucets[faucet_index].day_interval = file.readStringUntil('\n').toInt();
      faucets[faucet_index].day_counter = file.readStringUntil('\n').toInt();
      faucets[faucet_index].last_modified = file.readStringUntil('\n').toInt();

      //concert start hour in time format
      int seconds = faucets[faucet_index].start_total_seconds;
      faucets[faucet_index].start_hour = seconds / 3600;
      seconds %= 3600;
      faucets[faucet_index].start_minute = seconds / 60;
      seconds %= 60;
      faucets[faucet_index].start_second = seconds;
      //convert stop hour in time format
      seconds = faucets[faucet_index].stop_total_seconds;
      faucets[faucet_index].stop_hour = seconds / 3600;
      seconds %= 3600;
      faucets[faucet_index].stop_minute = seconds / 60;
      seconds %= 60;
      faucets[faucet_index].stop_second = seconds;
    } else {
      Serial.println("Failed to open " + String(filePath) + " (update on restart)");
    }
  }
  file.close();
}

void update_settings_on_restart() {
  File file;
  file = SD.open("/settings/brightness.txt", FILE_READ);
  if (file) {
    tft_brightness = file.readStringUntil('\n').toInt();
    analogWrite(tft_backlight, convert_brightness_value());
  } else {
    Serial.println("Failed to open brigtness file (update on restart)");
  }
  file.close();
  //set wifi status
  file = SD.open("/settings/wifi.txt", FILE_READ);
  if (file) {
    if (file.readStringUntil('\n').toInt()) {
      WiFi.begin();
      WiFi.mode(WIFI_STA);
      wifi_on = 1;
    } else {
      wifi_on = 0;
      WiFi.mode(WIFI_OFF);
    }
  } else {
    Serial.println("Failed to open wifi settings file (update on restart)");
  }
  file.close();
}

void setup() {
  Serial.begin(115200);
  HC12.begin(115200);
  Wire.begin();
  hspi->begin();
  //pins mode
  pinMode(tft_backlight, OUTPUT);
  pinMode(systemErrorLedPin, OUTPUT);
  digitalWrite(systemErrorLedPin, LOW);
  delay(1000);
  //rtc
  // rtc.begin();
  rtc.setClockMode(false);
  //card sd
  if (!SD.begin(5, *hspi)) {
    Serial.println("SD NOT found on HSPI");
    systemHardwareError = true;
  }

  // //wifi
  // WiFi.begin();
  // WiFi.mode(WIFI_STA);
  //tft
  init_tft();
  wifi_signal_icon(278, 8, 2, 0);
  //show error
  //retreive data from sd on restart
  update_faucets_data();
  update_settings_on_restart();
  //create task
  xTaskCreatePinnedToCore(
    run_in_background,  /* Task function. */
    "Background tasks", /* name of task. */
    10000,              /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    &background_task,   /* Task handle to keep track of created task */
    0);                 /* pin task to core 0 */
}

int start_reconnecting = 1;

void loop(void) {
  uint16_t t_x = 0, t_y = 0;
  boolean pressed = tft.getTouch(&t_x, &t_y);
  t_x = (t_x - 320) * -1;  //fix touch coordonates issue
  run_every_sec();
  if (pressed) {
    //show first page after wake up
    if (inactivity_seconds >= incativity_max_seconds) {
      init_tft();
      page_index = 0;
    } else {
      for (int button_index = 0; button_index < pages[page_index].num_of_btns; button_index++) {
        key[button_index].press(key[button_index].contains(t_x, t_y));
        if (key[button_index].justPressed()) {
          if (pages[page_index].buttons[button_index].target_page == 0) {
            //call the button function
            pages[page_index]
              .buttons[button_index]
              .callback(faucet_number);
          } else {
            //update page number
            page_index = pages[page_index].buttons[button_index].target_page - 1;
            //store the faucet index for further modifications
            if (pages[page_index].number == 5)
              faucet_number = button_index;
            //store the page number on it's nested position
            nav_history[pages[page_index].nested_position] = page_index;
            //show the buttons frpm the new menu
            tft.fillRect(0, 40, 320, 240, BLACK);
            draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
            if (pages[page_index].number == 3) {
              if (wifi_on) {
                if (WiFi.status() != WL_CONNECTED)
                  WiFi.disconnect();
                scan_networks();
              } else {
                draw_text("  WiFi is", 60, 90, 3, BLACK);
                draw_text("not enabled!", 60, 120, 3, BLACK);
              }
            }
          }
          //---------update the screen when incresing/decreasing a number--------//
          // if (pages[page_index].number == 3) createScrollableContainer(t_x, t_y);
          //page 6 is for setting a timer
          if (pages[page_index].number == 6 && faucet_number != -1) {
            draw_text("Set timer", 30, 55, 2, BLACK);
            show_time_format(faucets[faucet_number].timer_hour, faucets[faucet_number].timer_minute, faucets[faucet_number].timer_second, 90, 110, 3, BLACK);
          }
          //page 2 is for editing the system time
          if (pages[page_index].number == 2) {
            draw_text("Set time", 30, 55, 2, BLACK);
            show_time_format(new_hour, new_minute, new_second, 90, 110, 3, BLACK);
          }
          //page 7 is to set open/close hours
          if (pages[page_index].number == 7) {
            draw_text("Start:", 15, 70, 2, BLACK);
            show_time_format(faucets[faucet_number].start_hour, faucets[faucet_number].start_minute, faucets[faucet_number].start_second, 90, 67, 3, BLACK);
            draw_text("Stop:", 15, 145, 2, BLACK);
            show_time_format(faucets[faucet_number].stop_hour, faucets[faucet_number].stop_minute, faucets[faucet_number].stop_second, 90, 143, 3, BLACK);
          }
          //page 8 is to set day interval
          if (pages[page_index].number == 8) {
            draw_text("Set day interval", 30, 55, 2, BLACK);
            show_day_interval(faucets[faucet_number].day_interval, 144, 110, 3);
          }
          //settingd page
          if (pages[page_index].number == 9) {
            draw_text("Brightness:", 10, 55, 3, BLACK);
            draw_text(String(tft_brightness), 245, 55, 3, BLACK);
            draw_text("WiFi:", 10, 100, 3, BLACK);
            draw_wifi_toggle_button();
          }
          key[button_index].press(false);
          break;
        }
      }

      //check if toggle wifi button pressed
      if (pages[page_index].number == 9) {
        toggle_wifi_btn.press(toggle_wifi_btn.contains(t_x, t_y));
        if (toggle_wifi_btn.justPressed()) {
          wifi_on = !wifi_on;
          File file = SD.open("/settings/wifi.txt", FILE_WRITE);
          if (file) {
            file.seek(0);
            file.println(String(wifi_on));
          } else {
            Serial.println("Falied to update wifi toggle");
          }
          file.close();
          if (wifi_on) {
            WiFi.begin();
            WiFi.mode(WIFI_STA);
          } else {
            WiFi.mode(WIFI_OFF);
          }
          draw_wifi_toggle_button();
          toggle_wifi_btn.press(false);
        }
      }

      //select network to connect
      selected_network_options(t_x, t_y);
      //keyboard
      keyboard_system(t_x, t_y);
      //navigate back button
      back_btn.press(back_btn.contains(t_x, t_y));
      if (back_btn.justPressed() && pages[page_index].nested_position > 0) {
        if (pages[page_index].number == 3) {
          WiFi.scanDelete();
          found_networks_number = view_networks_page_number = 0;
          show_wifi_options = prev_selected_network_index = selected_network_index = -1;
          show_keyboard = 0;
        }
        //if not saved make sure to have the sd data in the system variables
        if (pages[page_index].number == 7 || pages[page_index].number == 8) {
          update_faucets_data();
        }
        //update page number from the array
        page_index = nav_history[pages[page_index].nested_position - 1];
        if (pages[page_index].number != 5) faucet_number = -1;
        //show previous page
        tft.fillRect(0, 40, 320, 240, BLACK);
        draw_buttons(pages[page_index].num_of_btns, pages[page_index].buttons);
        back_btn.press(false);
      }
    }
    //reset incativity time on touch
    inactivity_seconds = 0;
  }
  delay(50);
}
