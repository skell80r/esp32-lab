#include <Arduino.h>

int x = 0;

void setup() 
{
  Serial.begin(9600);
}

void loop() 
{
  Serial.println(x);
  delay(2000);
  x = x + 1;
}