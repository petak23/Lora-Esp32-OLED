#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <AsyncMqttClient.h>
#include <SPI.h>
#include <LoRa.h>
#include "pv_crypt.h"
#include "definitions.h"
#include "pvMeasure.h"

/**
 * Program pre LoRa komunikáciu pomocou ESP32 - prijímač + zobrazenie na OLED
 *
 * Posledná zmena(last change): 16.04.2023
 * @author Ing. Peter VOJTECH ml. <petak23@gmail.com>
 * @copyright  Copyright (c) 2022 - 2023 Ing. Peter VOJTECH ml.
 * @license GNU-GPL
 * @link       http://petak23.echo-msz.eu
 * @version 1.0.1
 */

OLED_CLASS_OBJ display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// display size for better string placement
int width;
int height;
String meteo[10];
float temp; // Teplota
float hum;	// Vlhkosť
float relp; // Relatívny tlak

pvMeasure data[100];
int measureCount = 0;

unsigned long lastMqttReconectTime = 0; // Uloží posledný čas pokusu o pripojenie k MQTT
int mqtt_state = 0;											// Stav MQTT pripojenia podľa https://pubsubclient.knolleary.net/api#state

pvCrypt *pv_crypr = new pvCrypt();

String LoRaData = ""; // Dáta prijaté cez LoRa prenos
int rssi = -120;			// Sila signálu

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

/* Konverzia výstupov do JSON-u  V prípade chyby dekódovania vráti "#Err" */
String getOutputStates()
{
	JSONVar myArray;
	String dectext = "";
	String jsonString = "";
	if (LoRaData.length() > 0)
	{
		int delimiter_pos = LoRaData.indexOf(':');
		String enc_message = LoRaData.substring(delimiter_pos + 1, LoRaData.length());
		char *itemName = (char *)"BMP280";
		char enc_text[256];
		enc_message.toCharArray(enc_text, 256);

		dectext = pv_crypr->decrypt(itemName, enc_text);

		myArray["mqtt"] = String(mqtt_state);
		myArray["time"] = millis();

		if (!dectext.startsWith("#Err"))
		{
#if SERIAL_PORT_ENABLED
			Serial.println("");
			Serial.println("Dekódovaná správa: '" + dectext + "'");
#endif
			myArray["lora"] = dectext;
			myArray["rssi"] = rssi;
			split(dectext, ";");
			jsonString = JSON.stringify(myArray);
		}
		else
		{
#if SERIAL_PORT_ENABLED
			Serial.println("Pri dekódovaní došlo k chybe");
#endif
			jsonString = "#Err";
		}
	}

	return jsonString;
}

/* Ak je chyba dekódovania vráti false inak true */
bool onLoRaData()
{
	String out = getOutputStates();
	if (!out.startsWith("#Err"))
	{
		int l_m = out.length() + 1;
		char tmp_m[l_m];
		out.toCharArray(tmp_m, l_m);
		mqttClient.publish(topic_meteo_status, 0, true, tmp_m);
		// notifyClients(out);
		return true;
	}
	else
	{
		return false;
	}
}

/* Funkcia pre pripojenie na wifi */
void connectToWifi()
{
	Serial.print("Connecting to ");
	Serial.println(WIFI_SSID);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	// Print local IP address and start web server
	Serial.println("");
	Serial.println("WiFi connected.");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

/* Pripojenie k MQTT brokeru */
void connectToMqtt()
{
	mqttClient.connect();
}

/* Spustená pri prerušení spojenia s MQTT brokerom */
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	if (WiFi.isConnected())
	{
		xTimerStart(mqttReconnectTimer, 0);
	}
	mqtt_state = 0; // Nastav príznak chýbajúceho MQTT spojenia
									// notifyClients(getOutputStates());
}

/* Spustená pri pripojení k MQTT brokeru */
void onMqttConnect(bool sessionPresent)
{
	// Prihlásenie sa na odber:
	mqttClient.subscribe(topic_meteo_last, 1);

	mqtt_state = 1; // Nastav príznak MQTT spojenia
									// notifyClients(getOutputStates()); // Aktualizuj stavy webu
}

/* Správa WIFI eventov */
void WiFiEvent(WiFiEvent_t event)
{
	switch (event)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		connectToMqtt();
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		xTimerStop(mqttReconnectTimer, 0);
		xTimerStart(wifiReconnectTimer, 0);
		break;
	default:
		break;
	}
}

void onMqttPublish(uint16_t packetId)
{
	Serial.println("Publish acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

/* This functions is executed when some device publishes a message to a topic that your ESP32 is subscribed to
 * Change the function below to add logic to your program, so when a device publishes a message to a topic that
 * your ESP32 is subscribed you can actually do something */
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
	// Zmeň message na string
	String messageTemp;
	for (int i = 0; i < len; i++)
	{
		messageTemp += (char)payload[i];
	}
	String topicTmp;
	for (int i = 0; i < strlen(topic); i++)
	{
		topicTmp += (char)topic[i];
	}

	// Ak príde správa s topic začínajúcim na main_topic_switch, tak zistím obsah správy a spracujem
	if (topicTmp.startsWith(topic_meteo_last))
	{
		getOutputStates();
		// notifyClients(getOutputStates());
	}
}

void setup()
{
#if SERIAL_PORT_ENABLED
	delay(2500);
	// initialize Serial Monitor
	Serial.begin(115200);
	while (!Serial)
		;
	Serial.println("LoRa Receiver");
#endif

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
	width = display.getWidth() / 2;		// Polovica šírky (64)
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
	while (!LoRa.begin(LORA_FREQ))
	{
#if SERIAL_PORT_ENABLED
		Serial.print(".");
#endif
		delay(500);
	}

	LoRa.setSyncWord(LORA_SYNC_WORD);
#if SERIAL_PORT_ENABLED
	Serial.println("");
	Serial.println("LoRa Initializing OK!");
#endif

	if (!LORA_SENDER)
	{
		display.clear();
		display.drawString(width - 50, height - 8, "LoraRecv++ Ready");
		display.drawString(width - 50, height + 8, "Waiting for data");
		display.display();
#if SERIAL_PORT_ENABLED
		Serial.println("LoraRecv++ Ready");
#endif
	}

	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
	wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

	WiFi.onEvent(WiFiEvent);

	mqttClient.setKeepAlive(5); // Set the keep alive. Defaults to 15 seconds.
	mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onMessage(onMqttMessage);
	mqttClient.onPublish(onMqttPublish);
	mqttClient.setServer(MQTT_HOST, MQTT_PORT);
	mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);

	connectToWifi();

	pv_crypr->setInfo((char *)DEVICE_ID, (char *)PASS_PHRASE);
}

int count = 0;
void loop()
{
	// try to parse packet
	int packetSize = LoRa.parsePacket();
	if (packetSize)
	{
		LoRaData = "";
		rssi = -120;
		// read packet
		while (LoRa.available())
		{
			LoRaData = LoRa.readString();
		}
		rssi = LoRa.packetRssi();
		bool state = onLoRaData();
#if SERIAL_PORT_ENABLED
		// print RSSI of packet
		Serial.print("' with RSSI: ");
		Serial.print(rssi);
		Serial.print("dBm ");
		Serial.print((10 / 9) * (rssi + 120));
		Serial.println("%");
#endif
		int rssi = LoRa.packetRssi();
		int perc = (int)((10 / 9) * (rssi + 120));
		int barS = (int)(3 * perc / 10);
		int snr = (LoRa.packetSnr() * 10);
		int cas = millis() / 1000;
		String m = ((cas / 60) < 10 ? "0" : "") + String((int)(cas / 60));
		String s = ((cas % 60) < 10 ? "0" : "") + String((int)(cas % 60));

		display.clear();
		if (state)
		{
			display.drawString(width - 60, height + 4, "T: " + String(temp));
			display.drawString(width - 60, height + 13, "H: " + String(hum));
			display.drawString(width - 60, height + 22, "P: " + String(relp));
		}
		else
		{
#if SERIAL_PORT_ENABLED
			Serial.println("Pri dekódovaní došlo k chybe");
#endif
			display.drawString(width - 60, height + 4, "Decode Error!");
		}

		display.drawString(width - 60, height - 22, "RSSI: " + String(rssi) + " (" + String(perc) + "%)");
		display.drawString(width - 60, height - 32, "SNR: " + String(snr / 10));
		display.drawString(width - 60, height - 12, "TIME: " + m + ":" + s);
		display.drawLine(width - 60, height + 2, width, height + 2);
		display.drawRect(width + 55, height - 32, 8, 34);
		display.fillRect(width + 57, height - barS, 4, barS);

		display.drawString(width - 10, height + 13, WiFi.localIP().toString());
		display.drawString(width + 10, height + 22, mqtt_state == 1 ? "MQTT On" : "MQTT Off");
		display.display();
	}
}