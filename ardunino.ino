#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Sera";
const char* password = "12345678";

#define DHTPIN 14
#define DHTTYPE DHT22

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

float nem = 0;
float sicaklik = 0;
bool sensorCalisiyor = false;
unsigned long sonOkuma = 0;

String sayfa() {
  String html = "<!DOCTYPE html><html lang='tr'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Sera</title></head><body>";
  html += "<span id='sicaklik'>--</span> C";
  html += "<script>";
  html += "async function guncelle(){try{const cevap=await fetch('/sicaklik');const sicaklik=await cevap.text();document.getElementById('sicaklik').textContent=sicaklik}catch(hata){document.getElementById('sicaklik').textContent='--'}}";
  html += "guncelle();setInterval(guncelle,2000);";
  html += "</script></body></html>";
  return html;
}

void anaSayfa() {
  server.send(200, "text/html", sayfa());
}

void sicaklikGonder() {
  if (sensorCalisiyor) {
    server.send(200, "text/plain", String(sicaklik, 1));
  } else {
    server.send(200, "text/plain", "--");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(DHTPIN, INPUT_PULLUP);
  dht.begin();
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(10, 35, "Sistem Baslatiliyor...");
  u8g2.sendBuffer();

  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  server.on("/", anaSayfa);
  server.on("/sicaklik", sicaklikGonder);
  server.begin();

  delay(2000);
}

void loop() {
  server.handleClient();

  if (millis() - sonOkuma < 2000) {
    return;
  }

  sonOkuma = millis();

  float yeniNem = dht.readHumidity();
  float yeniSicaklik = dht.readTemperature();

  if (isnan(yeniNem) || isnan(yeniSicaklik)) {
    sensorCalisiyor = false;
    Serial.println("DHT22'den veri okunamadi!");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.drawStr(15, 30, "Sensor Hatasi!");
    u8g2.drawStr(15, 45, "Baglantiyi Kontrol Et");
    u8g2.sendBuffer();
    return;
  }

  nem = yeniNem;
  sicaklik = yeniSicaklik;
  sensorCalisiyor = true;

  Serial.print("Sicaklik: ");
  Serial.print(sicaklik);
  Serial.print("C | Nem: ");
  Serial.print(nem);
  Serial.println("%");

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(32, 10, "ODA DURUMU");
  u8g2.drawHLine(0, 13, 128);

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(0, 32, "Sicaklik:");

  u8g2.setFont(u8g2_font_profont29_tf);
  u8g2.drawStr(70, 35, String(sicaklik, 1).c_str());

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(115, 32, "C");

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(0, 58, "Nem     :");

  u8g2.setFont(u8g2_font_profont29_tf);
  u8g2.drawStr(70, 60, String(nem, 1).c_str());

  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(115, 58, "%");

  u8g2.sendBuffer();
}
