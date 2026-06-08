#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

const char* ssid = "Sera";
const char* password = "12345678";

#define DHTPIN 14
#define DHTTYPE DHT22
#define FANPIN 26

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);
Preferences ayarlar;

float nem = 0;
float sicaklik = 0;
float fanEsik = 30.0;
bool fanDurumu = false;
bool sensorCalisiyor = false;
bool otomatikMod = true;
bool manuelFan = false;
unsigned long sonOkuma = 0;

const char sayfaHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sera</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 16px;
    }

    .kart {
      border: 1px solid #ccc;
      padding: 12px;
      margin-bottom: 10px;
    }

    input, button, select {
      padding: 8px;
      margin-top: 6px;
    }
  </style>
</head>
<body>
  <div class="kart">Sıcaklık: <span id="sicaklik">--</span>°C</div>
  <div class="kart">Nem: %<span id="nem">--</span></div>
  <div class="kart">Fan durumu: <span id="fan">--</span></div>

  <div class="kart">
    <div>Fan eşik değeri</div>
    <input id="esik" type="number" step="0.1">
    <button onclick="esikKaydet()">Kaydet</button>
  </div>

  <div class="kart">
    <div>Mod</div>
    <select id="mod" onchange="modKaydet()">
      <option value="oto">Otomatik</option>
      <option value="manuel">Manuel</option>
    </select>
  </div>

  <div class="kart" id="manuelAlan">
    <div>Manuel fan kontrolü</div>
    <button onclick="fanAyarla(1)">Aç</button>
    <button onclick="fanAyarla(0)">Kapat</button>
  </div>

  <script>
    async function veriAl() {
      const cevap = await fetch("/veri");
      const veri = await cevap.json();
      document.getElementById("sicaklik").textContent = veri.sicaklik;
      document.getElementById("nem").textContent = veri.nem;
      document.getElementById("fan").textContent = veri.fan;
      document.getElementById("esik").value = veri.esik;
      document.getElementById("mod").value = veri.mod;
      document.getElementById("manuelAlan").style.display = veri.mod === "manuel" ? "block" : "none";
    }

    async function esikKaydet() {
      const esik = document.getElementById("esik").value;
      await fetch("/esik?deger=" + encodeURIComponent(esik));
      veriAl();
    }

    async function modKaydet() {
      const mod = document.getElementById("mod").value;
      await fetch("/mod?deger=" + encodeURIComponent(mod));
      veriAl();
    }

    async function fanAyarla(durum) {
      await fetch("/fan?durum=" + durum);
      veriAl();
    }

    veriAl();
    setInterval(veriAl, 2000);
  </script>
</body>
</html>
)rawliteral";

void fanGuncelle() {
  if (otomatikMod) {
    fanDurumu = sensorCalisiyor && sicaklik >= fanEsik;
  } else {
    fanDurumu = manuelFan;
  }

  digitalWrite(FANPIN, fanDurumu ? HIGH : LOW);
}

void anaSayfa() {
  server.send_P(200, "text/html", sayfaHtml);
}

void veriGonder() {
  String json = "{";
  json += "\"sicaklik\":\"";
  json += (sensorCalisiyor ? String(sicaklik, 1) : "--");
  json += "\",\"nem\":\"";
  json += (sensorCalisiyor ? String(nem, 1) : "--");
  json += "\",\"fan\":\"";
  json += (fanDurumu ? "Açık" : "Kapalı");
  json += "\",\"esik\":\"";
  json += String(fanEsik, 1);
  json += "\",\"mod\":\"";
  json += (otomatikMod ? "oto" : "manuel");
  json += "\"}";
  server.send(200, "application/json", json);
}

void esikKaydet() {
  if (server.hasArg("deger")) {
    float yeniEsik = server.arg("deger").toFloat();

    if (yeniEsik > -20 && yeniEsik < 80) {
      fanEsik = yeniEsik;
      ayarlar.putFloat("esik", fanEsik);
    }
  }

  fanGuncelle();
  server.send(200, "text/plain", "OK");
}

void modKaydet() {
  if (server.hasArg("deger")) {
    String mod = server.arg("deger");

    if (mod == "oto") {
      otomatikMod = true;
    }

    if (mod == "manuel") {
      otomatikMod = false;
    }

    ayarlar.putBool("otomatik", otomatikMod);
  }

  fanGuncelle();
  server.send(200, "text/plain", "OK");
}

void fanKaydet() {
  if (server.hasArg("durum")) {
    manuelFan = server.arg("durum") == "1";
    ayarlar.putBool("manuelFan", manuelFan);
  }

  fanGuncelle();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  pinMode(DHTPIN, INPUT_PULLUP);
  pinMode(FANPIN, OUTPUT);
  digitalWrite(FANPIN, LOW);

  ayarlar.begin("sera", false);
  fanEsik = ayarlar.getFloat("esik", 30.0);
  otomatikMod = ayarlar.getBool("otomatik", true);
  manuelFan = ayarlar.getBool("manuelFan", false);

  dht.begin();
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont12_tf);
  u8g2.drawStr(10, 35, "Sistem Baslatiliyor...");
  u8g2.sendBuffer();

  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  server.on("/", anaSayfa);
  server.on("/veri", veriGonder);
  server.on("/esik", esikKaydet);
  server.on("/mod", modKaydet);
  server.on("/fan", fanKaydet);
  server.begin();

  fanGuncelle();
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
    fanGuncelle();
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
  fanGuncelle();

  Serial.print("Sicaklik: ");
  Serial.print(sicaklik);
  Serial.print("C | Nem: ");
  Serial.print(nem);
  Serial.print("% | Fan: ");
  Serial.println(fanDurumu ? "Acik" : "Kapali");

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
