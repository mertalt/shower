#include <Arduino.h>
#include <math.h>

// Pin tanımlamaları
const int motorIleriPWM = 3; // PWM destekleyen motor ileri dönüş pini
const int motorGeriPWM = 4;  // PWM destekleyen motor geri dönüş pini
const int potansPin = A5;    // Potansiyometre pin
const int ntcPin = A3;       // NTC sensör pin
int hedefPotansDegeri = 0;   // Hedef potansiyometre değeri
const int motorEnable = 2;   // Motor enable pini
const int actionPin = 37;
bool hedefModu = false;      // Hedef modunu kontrol et

// PID Kontrol Değişkenleri
float Kp = 18; // Oransal katsayı
int maksimumPWM = 255; // Motorun alabileceği maksimum PWM değeri
bool actionPinState = false; // actionPin'in mevcut durumu
void setup() {
  Serial.begin(9600); // Seri haberleşmeyi başlat
  pinMode(motorIleriPWM, OUTPUT);
  pinMode(motorGeriPWM, OUTPUT);
  pinMode(motorEnable, OUTPUT);
  digitalWrite(motorEnable, HIGH); // Motoru etkinleştir
  Serial.println("Komutlar: 'f' ileri, 'r' geri, 's' durdur, 'h' hedef belirle.");
}

void loop() {
  static unsigned long sonOkumaZamani = 0;
  static unsigned long sonSerialZamani = 0;
  unsigned long suankiZaman = millis();
  
  if (suankiZaman - sonOkumaZamani >= 10) {
    int toplamPotansDegeri = 0;
    float toplamNTC = 0;
    for (int i = 0; i < 5; i++) {
      toplamPotansDegeri += analogRead(potansPin);
      toplamNTC += NTC_Oku(analogRead(ntcPin));
      delay(2); // 10ms toplamda 10ms beklet
    }
    int ortalamaPotansDegeri = toplamPotansDegeri / 5;
    float ortalamaNTC = toplamNTC / 5;

    sonOkumaZamani = suankiZaman;

    if (suankiZaman - sonSerialZamani >= 500) {
      Serial.print("Filtrelenmis Potansiyometre Degeri: ");
      Serial.println(ortalamaPotansDegeri);
      Serial.print("Filtrelenmis NTC Sicaklik: ");
      Serial.println(ortalamaNTC);
      sonSerialZamani = suankiZaman;
    }
  }

  // Motor kontrol ve seri porttan komut okuma
  if (Serial.available() > 0) {
    String komut = Serial.readStringUntil('\n');

    if (komut == "f") {
      hareketEt(255, 0);
      hedefModu = false;
    } else if (komut == "r") {
      hareketEt(0, 255);
      hedefModu = false;
    } else if (komut == "s") {
      hareketEt(0, 0);
      hedefModu = false;
    } else if (komut.startsWith("h")) {
      int yeniHedef = komut.substring(1).toInt();
      if (yeniHedef >= 20 && yeniHedef <= 1018) {
        hedefPotansDegeri = yeniHedef;
        Serial.print("Hedef potansiyometre degeri belirlendi: ");
        Serial.println(hedefPotansDegeri);
        hedefModu = true;
      } else {
        Serial.println("Hata: Hedef deger 20 ile 1018 arasinda olmalidir.");
      }

    }
     if (komut == "a") {
      actionPinState = !actionPinState; // Durumu değiştir
      digitalWrite(actionPin, actionPinState ? HIGH : LOW); // Pin durumunu güncelle
      Serial.println(actionPinState ? "Action pin açık" : "Action pin kapalı"); // Durumu seri porta yaz
    }
  }

  // Hedef modu kontrolü ve motor hareketi
  if (hedefModu) {
    int hata = hedefPotansDegeri - analogRead(potansPin);
    int motorPWM = Kp * abs(hata);
    motorPWM = constrain(motorPWM, 0, maksimumPWM);

    if (hata > 5) {
      hareketEt(motorPWM, 0); // İleri
    } else if (hata < -5) {
      hareketEt(0, motorPWM); // Geri
    } else {
      hareketEt(0, 0); // Durdur
      Serial.println("Hedefe ulasildi. Motor durduruldu.");
      hedefModu = false;
    }
  }
}

void hareketEt(int ileriPWM, int geriPWM) {
  analogWrite(motorIleriPWM, ileriPWM);
  analogWrite(motorGeriPWM, geriPWM);
}

double NTC_Oku(int ADC_NTC) {
  if (ADC_NTC == 0) return -273.15; // ADC değeri 0 ise hata önleme

  double Rseri = 10000.0; // Seri direnç değeri (Ohm)
  double Vcc = 4.9; // Besleme voltajı (Volt) [Not: Bu değer burada tanımlanmış ancak kullanılmamış]
  double ADCmax = 1023.0; // ADC'nin maksimum değeri
  double Rntc = Rseri * ((ADCmax / ADC_NTC) - 1); // NTC direncini hesapla
  
  double T0 = 298.15; // Referans sıcaklık (Kelvin olarak 25°C)
  double B = 3977.0; // B katsayısı
  double R0 = 8450.0; // 25°C'de NTC direnci (Ohm) [6.90K Ohm olarak güncellendi]

  // Sıcaklığı hesapla ve Celsius cinsinden dön
  double temperature = 1.0 / (1.0/T0 + (1.0/B) * log(Rntc/R0));
  temperature = temperature - 273.15; // Kelvinden Celsius'a çevir

  return temperature;
}
float MotorKonumuHesapla(float sicaklik) {
  float MaMin = 115;    // Analog değer minimum (A5'te okunan minimum değer)
  float MaMax = 980; // Analog değer maksimum (A5'te okunan maksimum değer)
  float tmin = 20;    // Sıcaklık aralığının minimum değeri (örneğin, 20°C)
  float tmax = 60;    // Sıcaklık aralığının maksimum değeri (örneğin, 30°C)
  
  // Sıcaklık set değerine göre motor konumunu ayarlayacak formül
  float motorKonumu = ((MaMax - MaMin) / (tmax - tmin)) * (tmin - tmin) + MaMin;
  motorKonumu = constrain(motorKonumu, MaMin, MaMax); // Konumu minimum ve maksimum değerlerle sınırla
  
  return motorKonumu;
}