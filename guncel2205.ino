#include <Arduino.h>

// Pin tanımlamaları
const int motorIleriPWM = 3;  
const int motorGeriPWM = 4;  
const int potansPin = A5;  
const int motorEnable = 2;  
const int actionPin = 37;  
const int thermoPin = A3;  // AD8495 bağlı analog pin

// PID Kontrol Değişkenleri
float Kp = 14;  
int maksimumPWM = 255;  
int minimumPWM = 50;  
bool actionPinState = false;  
bool hedefModu = false;  
int hedefPotansDegeri = 0;  
int hesaplananVanaKonumu = 0;
int ortalamaPotansDegeri = 0;
unsigned long gModuBaslamaZamani = 0;
double spoofedDeger = -1000.0;  // Başlangıçta geçersiz bir değer

bool hedefeUlasildi = false;
bool gModuAktif = false;  
double istenenSicaklik = 0;  
unsigned long gModuHedefeVarisZamani = 0; 
bool gModuAyarYapiliyor = false;

const int HATA_PAYI = 10; 
const float TOLERANS = 1.0; 
const int VANA_AYAR_MIKTARI = 10; 
const unsigned long BEKLEME_SURESI = 10000; // 10 saniye bekleme süresi

// Filtreleme için değişkenler
const int FILTRE_UZUNLUGU = 3;
double termoOkumalari[FILTRE_UZUNLUGU];
int okumaIndeksi = 0;

double readFilteredCelsius() {
    int raw = analogRead(thermoPin);
    double voltage = raw * (5.0 / 1023.0);
    double temperature = (voltage - 1.25) / 0.005;
    termoOkumalari[okumaIndeksi] = temperature;
    okumaIndeksi = (okumaIndeksi + 1) % FILTRE_UZUNLUGU;
    double total = 0.0;
    for (int i = 0; i < FILTRE_UZUNLUGU; i++) {                       
        total += termoOkumalari[i];
    }
    return total / FILTRE_UZUNLUGU;
}

double yeniDeger() {
    return spoofedDeger != -1000.0 ? spoofedDeger : readFilteredCelsius();
}

int HesaplaVanaKonumu(double istenenSicaklik) {
    return (int)(-38.89 * istenenSicaklik + 1763.56);
}

void hareketEt(int ileriPWM, int geriPWM) {
    analogWrite(motorIleriPWM, ileriPWM);
    analogWrite(motorGeriPWM, geriPWM);
}
void setup() {
    Serial.begin(9600);


  // Nextion ekranını başlat

    pinMode(motorIleriPWM, OUTPUT);
    pinMode(motorGeriPWM, OUTPUT);
    pinMode(motorEnable, OUTPUT);
    digitalWrite(motorEnable, HIGH);
    for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
        termoOkumalari[i] = 0.0;
    }
    Serial.println("Komutlar: 'f' ileri, 'r' geri, 's' durdur, 'h' hedef belirle, 'Gxx' G modu aktifleştir.");
}

void loop() {
  static unsigned long sonOkumaZamani = 0;
  static unsigned long sonSerialZamani = 0;
  unsigned long suankiZaman = millis();
  int averageValue = ortalamaPotansDegeri; // Veya herhangi bir hesaplama metodu
  double temperature = yeniDeger();


  if (suankiZaman - sonOkumaZamani >= 10) {
    int toplamPotansDegeri = 0;
    for (int i = 0; i < 5; i++) {
      toplamPotansDegeri += analogRead(potansPin);
      delay(2);
    }
    ortalamaPotansDegeri = toplamPotansDegeri / 5;
    sonOkumaZamani = suankiZaman;

    if (suankiZaman - sonSerialZamani >= 500) {

      Serial.print("Ortalama Potans Degeri: ");
      Serial.println(ortalamaPotansDegeri);

      sonSerialZamani = suankiZaman;
    }
  }

  if (Serial.available() > 0) {
    String komut = Serial.readStringUntil('\n');
    komut.trim();
     if (komut.startsWith("d")) {
      double gelenDeger = komut.substring(1).toDouble();
      if (gelenDeger >= 0 && gelenDeger <= 1023) {
        spoofedDeger = gelenDeger;
        Serial.print("Yeni değer: ");
        Serial.println(spoofedDeger);
      } else {
        Serial.println("Hata: Geçersiz değer.");
      }
    } else if (komut == "f") {
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
    } else if (komut.startsWith("g")) {
      istenenSicaklik = komut.substring(1).toDouble();
      gModuAktif = true;
      gModuHedefeVarisZamani = 0; // Bekleme süresini sıfırla
      hesaplananVanaKonumu = HesaplaVanaKonumu(istenenSicaklik);
      Serial.print("Hedef konum: ");
      Serial.println(hesaplananVanaKonumu);
    }
  }

  // G Modu hareket kontrolü
  // G Modu hareket kontrolü
// G Modu hareket kontrolü
  if (gModuAktif) {
    int mevcutPotansDegeri = analogRead(potansPin);
    int hata = hesaplananVanaKonumu - mevcutPotansDegeri;
    int motorPWM = constrain(Kp * abs(hata), minimumPWM, maksimumPWM);

    if (abs(hata) > HATA_PAYI) {
      hareketEt(hata > 0 ? motorPWM : 0, hata > 0 ? 0 : motorPWM);
    } else {
      hareketEt(0, 0);
          double mevcutSicaklik = yeniDeger();
              Serial.print(" mevcut sıcaklık: ");
    Serial.println(mevcutSicaklik);
      Serial.println("Hedefe ulaşıldı. Motor durduruldu.");


      if (!gModuAyarYapiliyor) {
        gModuBaslamaZamani = millis();
        gModuAyarYapiliyor = true;
      }
    }
  }

  // Bekleme süresi kontrolü ve sıcaklık ayarı
  if (gModuAyarYapiliyor && (millis() - gModuBaslamaZamani >= BEKLEME_SURESI)) {
    double mevcutSicaklik = yeniDeger();
    Serial.print("Bekleme sonrası mevcut sıcaklık: ");
    Serial.println(mevcutSicaklik);

    if (abs(mevcutSicaklik - istenenSicaklik) > TOLERANS) {
      double sicaklikFarki = istenenSicaklik - mevcutSicaklik;
      hesaplananVanaKonumu += (sicaklikFarki > 0) ? VANA_AYAR_MIKTARI : -VANA_AYAR_MIKTARI;
      hesaplananVanaKonumu = constrain(hesaplananVanaKonumu, 0, 1023);
      Serial.print("Ayarlanmış vana konumu: ");
      Serial.println(hesaplananVanaKonumu);
      gModuBaslamaZamani = millis();
    } else {
      Serial.println("İstenen sıcaklık aralığına ulaşıldı. Motor durduruldu ve G modu kapatıldı.");
      gModuAktif = false;
      gModuAyarYapiliyor = false;
    }
  }

  // Hedef Modu hareket kontrolü
  if (hedefModu) {
    int potansDegeri = analogRead(potansPin);
    int hata = hedefPotansDegeri - potansDegeri;
    int motorPWM = constrain(Kp * abs(hata), minimumPWM, maksimumPWM); // Motor hızını hata ile ölçeklendirin

    if (abs(hata) > 10) {
      if (hata > 0) {
        hareketEt(motorPWM, 0);
        Serial.println("Hedefe ulaşmak için ileri hareket ediliyor.");
      } else {
        hareketEt(0, motorPWM);
        Serial.println("Hedefe ulaşmak için geri hareket ediliyor.");
      }
    } else {
      hareketEt(0, 0); // Hedefe ulaşıldığında motoru durdurd
      Serial.println("Hedefe ulaşıldı. Motor durduruldu.");
      hedefModu = false;
    }
  }
}

    