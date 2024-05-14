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
  Serial.println("Komutlar: 'f' ileri, 'r' geri, 's' durdur, 'h' hedef belirle, 'Gxx' G modu.");
}

void loop() {
  static unsigned long sonOkumaZamani = 0;
  static unsigned long sonSerialZamani = 0;
  unsigned long suankiZaman = millis();

  if (suankiZaman - sonOkumaZamani >= 10) {
    int toplamPotansDegeri = 0;
    for (int i = 0; i < 5; i++) {
      toplamPotansDegeri += analogRead(potansPin);
      delay(2); // Her okuma arasında kısa bir gecikme
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

  if (Serial.available() > 0) {
    String komut = Serial.readStringUntil('\n');
    komut.trim(); // Komuttan başındaki ve sonundaki boşlukları temizle

    if (komut == "f") {
      hareketEt(maksimumPWM, 0);
      hedefModu = false;
      Serial.println("Motor ileri doğru hareket ediyor.");
    } else if (komut == "r") {
      hareketEt(0, maksimumPWM);
      hedefModu = false;
      Serial.println("Motor geri doğru hareket ediyor.");
    } else if (komut == "s") {
      hareketEt(0, 0);
      hedefModu = false;
      Serial.println("Motor durduruldu.");
    } else if (komut.startsWith("h")) {
      int yeniHedef = komut.substring(1).toInt();
      if (yeniHedef >= 0 && yeniHedef <= 1023) {
        hedefPotansDegeri = yeniHedef;
        hedefModu = true;
        Serial.print("Hedef potansiyometre değeri belirlendi: ");
        Serial.println(hedefPotansDegeri);
      } else {
        Serial.println("Hata: Hedef değer 0 ile 1023 arasında olmalıdır.");
      }
    } else if (komut == "a") {
      actionPinState = !actionPinState;
      digitalWrite(actionPin, actionPinState ? HIGH : LOW);
      Serial.println(actionPinState ? "Action pin açık" : "Action pin kapalı");
    } else if (komut.startsWith("G")) {
      double istenilenSicaklik = komut.substring(1).toDouble();
      GModu(istenilenSicaklik);
    }
  }
}

void GModu(double istenilenSicaklik) {
  Serial.print("G modu aktif. İstenilen sıcaklık: ");
  Serial.println(istenilenSicaklik);

  double vanaKonumu = HesaplaVanaKonumu(istenilenSicaklik);
  Serial.print("Hesaplanan vana konumu: ");
  Serial.println(vanaKonumu);

  // Vana konumuna göre motorun hareket ettirilmesi
  // Bu kısımda potansiyometre okumasına göre motor hareketi kontrol edilir
  MotorKonumunaGit(vanaKonumu);

  unsigned long baslangicZamani = millis();
  while (millis() - baslangicZamani < 60000) { // 1 dakika boyunca
    double okunanSicaklik = readFilteredCelsius();
    Serial.print("Okunan Sıcaklık: ");
    Serial.println(okunanSicaklik);

    if (abs(okunanSicaklik - istenilenSicaklik) > 1) { // 1 derece tolerans kontrolü
      InceAyar(okunanSicaklik, istenilenSicaklik);
      break; // İnce ayarı yaptıktan sonra döngüden çık
    }
    delay(1000); // Okumalar arası bekleme süresi
  }
}

double HesaplaVanaKonumu(double istenilenSicaklik) {
  // İstenilen sıcaklık değerine göre vana konumunun hesaplanması
  return 2241.99 - 66.999 * istenilenSicaklik + 0.420 * pow(istenilenSicaklik, 2);
}

void MotorKonumunaGit(double hedefKonum) {
  // Motorun hedef konuma hareket ettirilmesi
  int potansOkuma = analogRead(potansPin);
  int hedefPWM = map(hedefKonum, 20, 30, 0, maksimumPWM); // Örnek olarak 20°C - 30°C aralığında bir haritalama
  hareketEt(hedefPWM, 0); // Motoru hareket ettir
  delay(500); // Motorun hareket etmesi için zaman tanı
  hareketEt(0, 0); // Motoru durdur
}

void InceAyar(double okunanSicaklik, double istenilenSicaklik) {
  Serial.println("Ince ayar yapılıyor...");
  double fark = okunanSicaklik - istenilenSicaklik;

  if (fark > 1) {
    // Sıcaklık fazla, motoru geri çevir
    Serial.println("Sıcaklık fazla, motor geri çevriliyor...");
    hareketEt(0, maksimumPWM); // Geri hareket
    delay(1000); // Belirli bir süre geri hareket
    hareketEt(0, 0); // Motoru durdur
  } else if (fark < -1) {
    // Sıcaklık az, motoru ileri çevir
    Serial.println("Sıcaklık az, motor ileri çevriliyor...");
    hareketEt(maksimumPWM, 0); // İleri hareket
    delay(1000); // Belirli bir süre ileri hareket
    hareketEt(0, 0); // Motoru durdur
  } else {
    // Sıcaklık istenilen aralıkta, ince ayara gerek yok
    Serial.println("Sıcaklık istenilen aralıkta, ince ayara gerek yok.");
  }

  // İnce ayar yapıldıktan sonra döngüden çık
  Serial.println("İnce ayar tamamlandı, döngüden çıkılıyor...");
  return; // İnce ayar sonrası döngüden çıkış
}

double readFilteredCelsius() {
  // Termokuplden sıcaklık okuma ve filtreleme
  uint16_t v = 0;
  digitalWrite(thermoCS, LOW);
  delay(1);
  v = SPI.transfer(0x00) << 8;
  v |= SPI.transfer(0x00);
  digitalWrite(thermoCS, HIGH);
  if (v & 0x4) {
    return -1; // Okuma hatası
  }
  v >>= 3;
  double temperature = v * 0.25;

  termokuplOkumalari[okumaIndeksi] = temperature;
  okumaIndeksi = (okumaIndeksi + 1) % FILTRE_UZUNLUGU;
  double toplam = 0;
  for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
    toplam += termokuplOkumalari[i];
  }
  return toplam / FILTRE_UZUNLUGU;
}
