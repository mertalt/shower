  #include <Arduino.h>
#include <SPI.h>
#include <max6675.h>

// Pin tanımlamaları
const int motorIleriPWM = 3;    // PWM destekleyen motor ileri dönüş pini
const int motorGeriPWM = 4;     // PWM destekleyen motor geri dönüş pini
const int potansPin = A5;       // Potansiyometre pin
const int motorEnable = 2;      // Motor enable pini
const int actionPin = 37;       // Eylem pin
int thermoDO = 50;              // Termokupl DO pini
int thermoCS = 53;              // Termokupl CS pini
int thermoCLK = 52;             // Termokupl CLK pini

// PID Kontrol Değişkenleri
float Kp = 14;                  // Oransal katsayı
int maksimumPWM = 255;          // Motorun alabileceği maksimum PWM değeri
bool actionPinState = false;    // actionPin'in mevcut durumu
bool hedefModu = false;         // Hedef modunu kontrol et
int hedefPotansDegeri = 0;      // Hedef potansiyometre değeri

// Filtreleme için değişkenler
const int FILTRE_UZUNLUGU = 3;
double termokuplOkumalari[FILTRE_UZUNLUGU];
int okumaIndeksi = 0;

double readFilteredCelsius() {
  uint16_t v = 0;
  digitalWrite(thermoCS, LOW);
  delay(1); // SPI'nin hazır olmasını sağla
  v = SPI.transfer(0x00) << 8;
  v |= SPI.transfer(0x00);
  digitalWrite(thermoCS, HIGH);
  if (v & 0x4) {
    // Okuma hatası
    return -1;
  }
  v >>= 3;
  double temperature = v * 0.25;

  // Okumayı diziye ekle ve indeksi güncelle
  termokuplOkumalari[okumaIndeksi] = temperature;
  okumaIndeksi = (okumaIndeksi + 1) % FILTRE_UZUNLUGU;

  // Hareketli ortalama hesapla
  double toplam = 0;
  for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
    toplam += termokuplOkumalari[i];
  }
  return toplam / FILTRE_UZUNLUGU;
}

void hareketEt(int ileriPWM, int geriPWM) {
  analogWrite(motorIleriPWM, ileriPWM);
  analogWrite(motorGeriPWM, geriPWM);
}

void setup() {
  Serial.begin(9600);         // Seri haberleşmeyi başlat
  pinMode(motorIleriPWM, OUTPUT);
  pinMode(motorGeriPWM, OUTPUT);
  pinMode(motorEnable, OUTPUT);
  digitalWrite(motorEnable, HIGH); // Motoru etkinleştir
  SPI.begin();
  pinMode(thermoCS, OUTPUT);
  digitalWrite(thermoCS, HIGH);
  // Filtre dizisini sıfırla
  for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
    termokuplOkumalari[i] = 0.0;
  }
  Serial.println("Komutlar: 'f' ileri, 'r' geri, 's' durdur, 'h' hedef belirle.");
}

void loop() {
  static unsigned long sonOkumaZamani = 0;
  static unsigned long sonSerialZamani = 0;
  unsigned long suankiZaman = millis();

  if (suankiZaman - sonOkumaZamani >= 10) {
    // Potansiyometre okuması
    int toplamPotansDegeri = 0;
    for (int i = 0; i < 5; i++) {
      toplamPotansDegeri += analogRead(potansPin);
      delay(2); // Her okuma arasında kısa bir gecikme
    }
    int ortalamaPotansDegeri = toplamPotansDegeri / 5;
    sonOkumaZamani = suankiZaman;

    // Her 500 ms'de bir ortalama sıcaklık ve potansiyometre değerini seri porta yaz
    if (suankiZaman - sonSerialZamani >= 500) {
      double filteredTemp = readFilteredCelsius();
      Serial.print("Filtrelenmis Termokupl Sicaklik: ");
      Serial.println(filteredTemp);
      Serial.print("Ortalama Potans Degeri: ");
      Serial.println(ortalamaPotansDegeri);
      sonSerialZamani = suankiZaman;
    }
  }

  if (Serial.available() > 0) {
    String komut = Serial.readStringUntil('\n');
    komut.trim(); // Komuttan başındaki ve sonundaki boşlukları temizle

    if (komut == "f") {
      // İleri hareket komutu
      hareketEt(maksimumPWM, 0); // Motoru maksimum hızda ileri doğru hareket ettir
      hedefModu = false; // Hedef modunu kapat
      Serial.println("Motor ileri doğru hareket ediyor.");
    } else if (komut == "r") {
      // Geri hareket komutu
      hareketEt(0, maksimumPWM); // Motoru maksimum hızda geri doğru hareket ettir
      hedefModu = false; // Hedef modunu kapat
      Serial.println("Motor geri doğru hareket ediyor.");
    } else if (komut == "s") {
      // Durma komutu
      hareketEt(0, 0); // Motoru durdur
      hedefModu = false; // Hedef modunu kapat
      Serial.println("Motor durduruldu.");
    } else if (komut.startsWith("h")) {
      // Hedef modu aktifleştirme komutu. Örnek: h500
      int yeniHedef = komut.substring(1).toInt(); // İlk karakteri atla ve sayısal değeri al
      if (yeniHedef >= 0 && yeniHedef <= 1023) {
        hedefPotansDegeri = yeniHedef;
        hedefModu = true; // Hedef modunu aktifleştir
        Serial.print("Hedef potansiyometre değeri belirlendi: ");
        Serial.println(hedefPotansDegeri);
      } else {
        Serial.println("Hata: Hedef değer 0 ile 1023 arasında olmalıdır.");
      }
    } else if (komut == "a") {
      // Action pin durumunu değiştir
      actionPinState = !actionPinState;
      digitalWrite(actionPin, actionPinState ? HIGH : LOW);
      Serial.println(actionPinState ? "Action pin açık" : "Action pin kapalı");
    }
  }

  // Hedef modu kontrolü
  if (hedefModu) {
    int potansDegeri = analogRead(potansPin);
    int hata = hedefPotansDegeri - potansDegeri;
    int motorPWM = Kp * abs(hata);
    motorPWM = constrain(motorPWM, 0, maksimumPWM);

    if (hata > 10) { // Hedef değerden büyük bir sapma varsa ileri
      hareketEt(motorPWM, 0);
      Serial.println("Hedefe ulaşmak için ileri hareket ediliyor.");
    } else if (hata < -10) { // Hedef değerden küçük bir sapma varsa geri
      hareketEt(0, motorPWM);
      Serial.println("Hedefe ulaşmak için geri hareket ediliyor.");
    } else {
      hareketEt(0, 0); // Hedefe ulaşıldı
      Serial.println("Hedefe ulaşıldı. Motor durduruldu.");
      hedefModu = false;
    } 
    }
    }
    

