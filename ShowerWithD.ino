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
bool gModuAktif = false;        // G Modunun aktif olup olmadığını kontrol eder
double istenenSicaklik = 0;     // G Modu için istenen sıcaklık
unsigned long gModuBaslamaZamani; // G Modunun başlama zamanını saklar
unsigned long hareketBitisZamani; // Hareketin bitiş zamanını saklar

int hesaplananVanaKonumu = 0;

int FILTRE_UZUNLUGU = 3;
double termokuplOkumalari[3];
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
  Serial.begin(9600);
  pinMode(motorIleriPWM, OUTPUT);
  pinMode(motorGeriPWM, OUTPUT);
  pinMode(motorEnable, OUTPUT);
  digitalWrite(motorEnable, HIGH);
  SPI.begin();
  pinMode(thermoCS, OUTPUT);
  digitalWrite(thermoCS, HIGH);
  for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
    termokuplOkumalari[i] = 0.0;
  }
  Serial.println("Komutlar: 'f' ileri, 'r' geri, 's' durdur, 'h' hedef belirle, 'Gxx' G modu aktifleştir.");
}

void loop() {
  static unsigned long sonOkumaZamani = 0;
  static unsigned long sonSerialZamani = 0;
  unsigned long suankiZaman = millis();

  if (suankiZaman - sonOkumaZamani >= 10) {
    int toplamPotansDegeri = 0;
    for (int i = 0; i < 5; i++) {
      toplamPotansDegeri += analogRead(potansPin);
      delay(2);
    }
    int ortalamaPotansDegeri = toplamPotansDegeri / 5;
    sonOkumaZamani = suankiZaman;

    if (suankiZaman - sonSerialZamani >= 500) {
      double filteredTemp = readFilteredCelsius();
      Serial.print("Filtrelenmis Termokupl Sicaklik: ");
      Serial.println(filteredTemp);
      Serial.print("Ortalama Potans Degeri: ");
      Serial.println(ortalamaPotansDegeri);

      sonSerialZamani = suankiZaman;
    }
  }

  // G modu aktifse ve hareket tamamlandıysa
  if (gModuAktif && millis() - hareketBitisZamani > 60000) {
    double mevcutSicaklik = readFilteredCelsius();
    if (abs(mevcutSicaklik - istenenSicaklik) > 1) {
      // İstenen sıcaklık aralığının dışında ise
      for (int i = 0; i < 3; i++) {
        // İstenen sıcaklık ile mevcut sıcaklık arasındaki farkı hesapla
        double sicaklikFarki = istenenSicaklik - mevcutSicaklik;

        // Sicaklik farkina gore konum degisikligi yapalim
        if (sicaklikFarki > 0) {
          // İstenen sıcaklık yükseldiyse, vana konumunu arttırmak gerekiyor
          hesaplananVanaKonumu += 10; // +-10 konum hareketi
        } else if (sicaklikFarki < 0) {
          // İstenen sıcaklık düştüyse, vana konumunu azaltmak gerekiyor
          hesaplananVanaKonumu -= 10; // +-10 konum hareketi
        }
        
        // Motoru ayarla
        hareketEt((sicaklikFarki > 0) ? 190 : 0, (sicaklikFarki < 0) ? 190 : 0);
        delay(15000); // 15 saniye bekle
        Serial.println("Hareket ediyor");
        mevcutSicaklik = readFilteredCelsius(); // Sıcaklığı tekrar kontrol et
        if (abs(mevcutSicaklik - istenenSicaklik) <= 1) {
          // İstenen sıcaklık aralığına ulaşıldıysa döngüden çık
          break;
        }
      }
    }
    gModuAktif = false; // G modunu kapat
    Serial.println("Gmodu kapandı.");
  }

  // Hedef modu kontrolü
  if (hedefModu) {
    int potansDegeri = analogRead(potansPin);
    int hata = hedefPotansDegeri - potansDegeri;
    int motorPWM = Kp * abs(hata);
    motorPWM = constrain(motorPWM, 0, maksimumPWM);

    if (hata > 10) {
      hareketEt(motorPWM, 0);
      Serial.println("Hedefe ulaşmak için ileri hareket ediliyor.");
    } else if (hata < -10) {
      hareketEt(0, motorPWM);
      Serial.println("Hedefe ulaşmak için geri hareket ediliyor.");
    } else {
      hareketEt(0, 0); // Hedefe ulaşıldı
      Serial.println("Hedefe ulaşıldı. Motor durduruldu.");
      hedefModu = false;
    }
  }

  // G modu aktif değilse ve bir komut alındıysa
  else if (!gModuAktif && Serial.available() > 0) {
    String komut = Serial.readStringUntil('\n');
    komut.trim();
    if (komut.startsWith("G")) {
      istenenSicaklik = komut.substring(1).toDouble();
      gModuAktif = true; // G modunu aktifleştir
      hareketBitisZamani = millis(); // Hareketin başlangıç zamanını güncelle
      hesaplananVanaKonumu = HesaplaVanaKonumu(istenenSicaklik); // Hedef konumu hesapla
      // Hedef konumu ayarla
      // hareketEt(motorHizi, 0); // İleri hareket
      Serial.println("G modu aktifleştirildi. Hedef konuma hareket ediliyor.");
    }
  }
}
