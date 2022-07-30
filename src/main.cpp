#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "definitions.h"

/**
 * Program pre LoRa komunikáciu pomocou ESP32 - prijímač + zobrazenie na OLED
 *
 * Posledná zmena(last change): 30.07.2022
 * @author Ing. Peter VOJTECH ml. <petak23@gmail.com>
 * @copyright  Copyright (c) 2022 - 2022 Ing. Peter VOJTECH ml.
 * @license GNU-GPL
 * @link       http://petak23.echo-msz.eu
 * @version 1.0.0
 */

OLED_CLASS_OBJ display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

// display size for better string placement
int width;
int height;
String meteo[10];

/* Rozdelenie textu podľa rozdelovača do poľa meteo
 * zdroj: https://forum.arduino.cc/t/how-to-split-a-string-with-space-and-store-the-items-in-array/888813/8 */
void split(String s, String delimiter)
{
  int StringCount = 0;

  while (s.length() > 0)
  {
    int index = s.indexOf(delimiter);
    if (index == -1) // No space found
    {
      meteo[StringCount++] = s;
      break;
    }
    else
    {
      meteo[StringCount++] = s.substring(0, index);
      s = s.substring(index + 1);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  if (OLED_RST > 0)
  {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, HIGH);
    delay(100);
    digitalWrite(OLED_RST, LOW);
    delay(100);
    digitalWrite(OLED_RST, HIGH);
  }

  display.init();
  width = display.getWidth() / 2;   // Polovica šírky (64)
  height = display.getHeight() / 2; // Polovica výšky (32)
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(width - 50, height - 16, "LoRa++ Receiver");
  display.drawString(width - 50, height, "Width: " + String(width));
  display.drawString(width - 50, height + 16, "Height: " + String(height));
  display.display();
  delay(2000);

  SPI.begin(CONFIG_CLK, CONFIG_MISO, CONFIG_MOSI, CONFIG_NSS);
  LoRa.setPins(CONFIG_NSS, CONFIG_RST, CONFIG_DIO0);
  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  if (!LORA_SENDER)
  {
    display.clear();
    display.drawString(width - 50, height, "LoraRecv++ Ready");
    display.display();
  }
}

int count = 0;
void loop()
{
  if (LoRa.parsePacket())
  {
    String recv = "";
    while (LoRa.available())
    {
      recv = LoRa.readString();
    }
    split(recv, ";");
    display.clear();
    display.drawString(width - 60, height, String(meteo[0]));
    display.drawString(width - 60, height + 10, String(meteo[1]));
    display.drawString(width - 60, height + 20, String(meteo[2]));
    display.drawString(width, height, String(meteo[3]));
    display.drawString(width, height + 10, String(meteo[4]));
    display.drawString(width, height + 20, String(meteo[5]));
    int rssi = LoRa.packetRssi();
    String info = "RSSI " + String(rssi) + " -> " + String((int)((10 / 9) * (rssi + 120))) + "%";
    String snr = "SNR: " + String(LoRa.packetSnr());
    display.drawString(width - 60, height - 22, info);
    display.drawString(width - 60, height - 32, snr);
    display.drawLine(width - 60, height - 5, width + 60, height - 5);
    display.display();
  }
}