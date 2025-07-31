#ifndef ERROR_H
#define ERROR_H

#define LED_PIN 2

inline void blinkError(int code, int repeat = 1) {
  pinMode(LED_PIN, OUTPUT);
  for (int r = 0; r < repeat; r++) {
    for (int i = 0; i < code; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(300);
      digitalWrite(LED_PIN, LOW);
      delay(300);
    }
    delay(1000); // Pauză între cicluri
  }
}

#endif
