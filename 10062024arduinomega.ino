#include <Arduino.h>

// Pin tanımlamaları
const int pinRed = 13;
const int pinGreen = 12;
const int pinBlue = 11;
const int motorIleriPWM = 3;
const int motorGeriPWM = 4;
const int potansPin = A5;
const int motorEnable = 2;
const int actionPin = 37;
const int thermoPin = A3;  // AD8495 bağlı analog pin
const int pinB2 = 33;
const int pinB3 = 35;
const int pinB4 = 37;
const int pinB5 = 39;
const int pinB6 = 41;
const int pinB7 = 43;
const int pinB8 = 45;
const int pinB9 = 47;

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
double istenenSicaklik = 33.0;  // G modu başlangıç değeri
unsigned long sonButonZamani = 0; // Son buton basma zamanını takip etmek için
bool gModuBaslamakUzere = false;
bool hedefeUlasildi = false;
bool gModuAktif = false;  
bool stateB2 = false;
bool stateB3 = false;
bool stateB4 = false;
bool stateB5 = false;
bool stateB6 = false;
bool stateB7 = false;
bool stateB8 = false;
bool stateB9 = false;

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

void sendToNextion(double temperature) {
    String command = "n0.val=" + String((int)temperature);
    command += "\xFF\xFF\xFF";  // Nextion komut sonlandırma karakterleri
    Serial3.print(command);
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

void togglePin(int pin, bool *state) {
    *state = !*state;
    digitalWrite(pin, *state ? HIGH : LOW);
}

void setColor(int red, int green, int blue) {
    analogWrite(pinRed, red);
    analogWrite(pinGreen, green);
    analogWrite(pinBlue, blue);
}

void startFastEffect() {
    // RGB için hızlı renk geçişi efekti
    for (int i = 0; i < 5; i++) {
        setColor(255, 0, 0);  // Kırmızı
        delay(100);
        setColor(0, 255, 0);  // Yeşil
        delay(100);
        setColor(0, 0, 255);  // Mavi
        delay(100);
    }
}

void startSmoothEffect() {
    // RGB için yavaş renk geçişi efekti
    for (int red = 0; red < 255; red += 5) {
        setColor(red, 255 - red, 0);
        delay(10);
    }
    for (int green = 0; green < 255; green += 5) {
        setColor(255 - green, green, 0);
        delay(10);
    }
    for (int blue = 0; blue < 255; blue += 5) {
        setColor(0, 255 - blue, blue);
        delay(10);
    }
}

}
void setup() {
    Serial.begin(9600);
    Serial3.begin(9600);

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
   
    sendToNextion(temperature);

    if (Serial3.available()) {
        delay(10);  // Gelen verinin tamamlanması için kısa bir gecikme
        while (Serial3.available()) {
            String komut = Serial3.readStringUntil('\n');
            komut.trim();

            // Ekrandan gelen veriyi Serial monitöre yazdır
            Serial.print("Nextion'dan gelen veri: ");
            Serial.println(komut);

            // Geçerli komut olup olmadığını kontrol et
            int index = komut.lastIndexOf("b");
            if (index == -1) {
                index = komut.lastIndexOf("c");
            }

            if (index != -1) {
                String temizKomut = komut.substring(index);

                // Sıcaklık ayarı ve RGB LED kontrolü için butonlar
                if (temizKomut == "b0") {
                    istenenSicaklik += 0.25;
                    sonButonZamani = suankiZaman;
                    gModuBaslamakUzere = true;
                } else if (temizKomut == "b1") {
                    istenenSicaklik -= 0.25;
                    sonButonZamani = suankiZaman;
                    gModuBaslamakUzere = true;
                }

                // Sıcaklık değerini ekranda güncelle
                String command = "n1.val=" + String((int)(istenenSicaklik * 10));
                Serial3.print(command);
                Serial3.write("\xFF\xFF\xFF");

                // Pin kontrolü ve LED efektleri
                if (temizKomut == "b2") {
                    togglePin(pinB2, &stateB2);
                } else if (temizKomut == "b3") {
                    togglePin(pinB3, &stateB3);
                } else if (temizKomut == "b4") {
                    togglePin(pinB4, &stateB4);
                } else if (temizKomut == "b5") {
                    togglePin(pinB5, &stateB5);
                } else if (temizKomut == "b6") {
                    togglePin(pinB6, &stateB6);
                } else if (temizKomut == "b7") {
                    togglePin(pinB7, &stateB7);
                } else if (temizKomut == "b8") {
                    togglePin(pinB8, &stateB8);
                } else if (temizKomut == "b9") {
                    togglePin(pinB9, &stateB9);
                } else if (temizKomut == "b12") {
                    setColor(255, 255, 0);  // Sarı
                } else if (temizKomut == "b13") {
                    setColor(255, 0, 0);  // Kırmızı
                } else if (temizKomut == "b14") {
                    setColor(0, 0, 255);  // Mavi
                } else if (temizKomut == "b15") {
                    setColor(0, 255, 0);  // Yeşil
                } else if (temizKomut == "b16") {
                    setColor(255, 105, 180);  // Pembe
                } else if (temizKomut == "b17") {
                    setColor(128, 0, 128);  // Mor
                } else if (temizKomut == "b18") {
                    setColor(255, 165, 0);  // Turuncu
                } else if (temizKomut == "b19") {
                    startFastEffect();  // Hızlı renk geçişi
                } else if (temizKomut == "b20") {
                    startSmoothEffect();  // Yavaş renk geçişi
                } else if (temizKomut == "b21") {
                    setColor(255, 255, 255);  // Beyaz
                } else if (temizKomut == "b22") {
                    setColor(0, 0, 0);  // LED'ler kapalı
                }
            }
        }
    }

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
    if (gModuBaslamakUzere && suankiZaman - sonButonZamani > 5000) {
        gModuAktif = true;
        gModuHedefeVarisZamani = 0; // Bekleme süresini sıfırla
        hesaplananVanaKonumu = HesaplaVanaKonumu(istenenSicaklik);
        Serial.print("G modu başlatıldı, hedef konum: ");
        Serial.println(hesaplananVanaKonumu);
        gModuBaslamakUzere = false;
    }
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
