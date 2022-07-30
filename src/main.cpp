#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "definitions.h"
#include "pvMeasure.h"

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
float temp; // Teplota
float hum;  // Vlhkosť
float relp; // Relatívny tlak
pvMeasure data[100];
int measureCount = 0;

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
  temp = meteo[0].substring(3).toFloat();
  hum = meteo[1].substring(3).toFloat();
  relp = meteo[3].substring(3).toFloat();
  data[measureCount].temperature = temp;
  data[measureCount].humidity = hum;
  data[measureCount].rel_pressure = relp;
  measureCount = measureCount < 99 ? measureCount + 1 : 0;
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
    int rssi = LoRa.packetRssi();
    int perc = (int)((10 / 9) * (rssi + 120));
    int barS = (int)(3 * perc / 10);
    int snr = (LoRa.packetSnr() * 10);
    int cas = millis() / 1000;
    String m = ((cas / 60) < 10 ? "0" : "") + String((int)(cas / 60));
    String s = ((cas % 60) < 10 ? "0" : "") + String((int)(cas % 60));

    display.clear();
    display.drawString(width - 60, height + 4, "T: " + String(temp));
    display.drawString(width - 60, height + 13, "H: " + String(hum));
    display.drawString(width - 60, height + 22, "P: " + String(relp));

    display.drawString(width - 60, height - 22, "RSSI: " + String(rssi) + " (" + String(perc) + "%)");
    display.drawString(width - 60, height - 32, "SNR: " + String(snr / 10));
    display.drawString(width - 60, height - 12, "TIME: " + m + ":" + s);
    display.drawLine(width - 60, height + 2, width, height + 2);
    display.drawRect(width + 55, height - 32, 8, 34);
    display.fillRect(width + 57, height - barS, 4, barS);
    display.display();
  }
}