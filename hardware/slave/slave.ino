#include <SoftwareSerial.h>

SoftwareSerial HC12(3, 2);  // RX, TX pins

int timer[4] = {};
int number_of_faucets = 4;

void setup() {
  Serial.begin(115200);
  HC12.begin(115200);
  for (int i = 0; i < number_of_faucets; i++)
    pinMode(i + 4, OUTPUT);
}

void loop() {
  char data[5] = "0000";
  if (HC12.available()) {
    HC12.readBytes(data, 4); // Read 4 characters into the data array
    data[4] = '\0';
  }
  for (int i = 0; i < number_of_faucets; i++) {
    if (data[i] == '1') {
      timer[i] = 0;
      digitalWrite(i + 4, HIGH);
    } else {
      timer[i]++;
      if (timer[i] > 50) {
        timer[i] = 0;
        digitalWrite(i + 4, LOW);
      }
    }
  }
  delay(100);
}
