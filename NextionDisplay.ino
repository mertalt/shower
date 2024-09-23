


#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Pin tanımlamaları
#define LED_PIN 11
#define NUM_LEDS 100

// NeoPixel objesini tanımla
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_BRG + NEO_KHZ800);

// Motor ve sıcaklık kontrolü için pinler
const int motorIleriPWM = 3;
const int motorGeriPWM = 4;
const int potansPin = A5;
const int motorEnable = 2;
const int thermoPin = A3;  // AD8495 bağlı analog pin

// Diğer pinler
const int pinB2 = 37;
const int pinB3 = 35;
const int pinB4 = 39;
const int pinB5 = 41;
const int pinB6 = 43;
const int pinB7 = 45;
const int pinB8 = 47;
const int pinB9 = 49;

// PID Kontrol Değişkenleri
float Kp = 14;
int maksimumPWM = 255;
int minimumPWM = 50;
bool hedefModu = false;
int hedefPotansDegeri = 0;
uint8_t slidingColors[3][3];  // Üç renk için RGB değerlerini tutan dizi
int renkIndex = 0;  // Hangi rengin işlendiğini takip etmek için
// 0x3A komutuyla aktif olacak durumlar
bool renkKaydetmeAktif = false;  // 0x3A komutuyla renk kaydetme aktif olur
bool slidingEffectActive = false;
int hesaplananVanaKonumu = 0;
int ortalamaPotansDegeri = 0;
unsigned long gModuBaslamaZamani = 0;
double spoofedDeger = -1000.0;  // Başlangıçta geçersiz bir değer
double istenenSicaklik = 33.0;  // G modu başlangıç değeri
unsigned long sonButonZamani = 0; // Son buton basma zamanını takip etmek için
bool gModuBaslamakUzere = false;
bool gModuAktif = false;

// Filtreleme için değişkenler
const int FILTRE_UZUNLUGU = 3;
double termoOkumalari[FILTRE_UZUNLUGU];
int okumaIndeksi = 0;

// Diğer değişkenler
bool stateB2 = false;
bool stateB3 = false;
bool stateB4 = false;
bool stateB5 = false;
bool stateB6 = false;
bool stateB7 = false;
bool stateB8 = false;
bool stateB9 = false;

unsigned long lastDataTime = 0;  // Son veri alım zamanı
const unsigned long timeout = 3000;  // 3 saniye

byte buffer[15];  // 15 baytlık buffer
int bufferIndex = 0;

bool breathingMode = false;  // Breathing mode aktif mi?
bool rainbowCycleMode = false;  // Rainbow Cycle Mode aktif mi?
bool bouncingMode = false;  // Bouncing mode aktif mi?
unsigned long previousMillis = 0;  // Son LED güncelleme zamanı
int brightness = 0;  // LED parlaklık değeri
int fadeAmount = 5;  // Parlaklık değişim hızı
int rainbowSpeed = 100;  // Rainbow Cycle efekti hızı (ms)
int bounceSpeed = 200;  // Bouncing hızı (ms)
int slideIndex = 0;  // Bouncing sırasında aktif LED indeksi
bool bounceDirection = true;  // Bouncing yönü

uint8_t currentR = 255;  // Mevcut kırmızı renk değeri
uint8_t currentG = 255;  // Mevcut yeşil renk değeri
uint8_t currentB = 255;  // Mevcut mavi renk değeri
    static bool motorStopped = false; // Motorun durdurulup durdurulmadığını takip etmek için

void setup() {
    Serial.begin(9600);
    Serial3.begin(115200);

    strip.begin();
    strip.show(); // LED'leri sıfırla (tümünü kapat)

    pinMode(motorIleriPWM, OUTPUT);
    pinMode(motorGeriPWM, OUTPUT);
    pinMode(motorEnable, OUTPUT);
    digitalWrite(motorEnable, HIGH);

    pinMode(pinB2, OUTPUT);
    pinMode(pinB3, OUTPUT);
    pinMode(pinB4, OUTPUT);
    pinMode(pinB5, OUTPUT);
    pinMode(pinB6, OUTPUT);
    pinMode(pinB7, OUTPUT);
    pinMode(pinB8, OUTPUT);
    pinMode(pinB9, OUTPUT);

    digitalWrite(pinB2, HIGH);
    digitalWrite(pinB3, HIGH);
    digitalWrite(pinB4, HIGH);
    digitalWrite(pinB5, HIGH);
    digitalWrite(pinB6, HIGH);
    digitalWrite(pinB7, HIGH);
    digitalWrite(pinB8, HIGH);
    digitalWrite(pinB9, HIGH);
    sendButtonState("bt0", stateB2);  // Send initial state of button bt0
    sendButtonState("bt1", stateB3);  // Send initial state of button bt1
    sendButtonState("bt2", stateB4);  // Send initial state of button bt2
    sendButtonState("bt3", stateB5);  // Send initial state of button bt3
    sendButtonState("bt4", stateB6);  // Send initial state of button bt4
    sendButtonState("bt5", stateB7);  // Send initial state of button bt5
    sendButtonState("bt6", stateB8);  // Send initial state of button bt6
    sendButtonState("bt7", stateB9);  // Send initial state of button bt7
    sendTemperatureToNextion(istenenSicaklik);


    for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
        termoOkumalari[i] = 0.0;
    }
}
void loop() {
       unsigned long suankiZaman = millis();
    if (Serial.available() > 0) {
        String komut = Serial.readStringUntil('\n');
        komut.trim();  // Komutu düzeltme

        // Derece değerini kandırmak için komut
        if (komut.startsWith("d")) {
            double gelenDeger = komut.substring(1).toDouble();
            if (gelenDeger >= 0 && gelenDeger <= 1023) {
                spoofedDeger = gelenDeger;  // Spoofed değeri güncelle
                Serial.print("Yeni değer: ");
                Serial.println(spoofedDeger);
            } else {
                Serial.println("Hata: Geçersiz değer.");
            }
        }
    } 
    if (Serial3.available() > 0) {
        double temperature = yeniDeger(); //bunu burdan al 
        byte incomingByte = Serial3.read();
          lastDataTime = millis();  // Son veri alım zamanını güncelle
          String command = "n1.val=" + String((int)(istenenSicaklik));
  

        if (bufferIndex < 15) {
            buffer[bufferIndex++] = incomingByte;
        }

        // Uyku ve uyanma komutlarını kontrol et
        if (bufferIndex >= 4 && (buffer[bufferIndex-4] == 0x86 || buffer[bufferIndex-4] == 0x87) &&
            buffer[bufferIndex-3] == 0xFF && buffer[bufferIndex-2] == 0xFF && buffer[bufferIndex-1] == 0xFF) {
            if (buffer[bufferIndex-4] == 0x86) {
                Serial.println("Uyku komutu alındı.");
                bufferIndex = 0;
            } else {
                Serial.println("Uyanma komutu alındı.");
               sendToNextion(readFilteredCelsius());//Uyanınca dereceyi yenile.
                bufferIndex = 0;
            }
            bufferIndex = 0;
            return;
        }

        if (bufferIndex == 2) {
            if (buffer[0] == 0x7A) {
                switch (buffer[1]) {
                  case 0x30: // component id: 65 01 01 00 FF FF FF b0
                  istenenSicaklik -= 0.25;
                  sonButonZamani = millis();
                  gModuBaslamakUzere = true;     
                  Serial3.print(command);
                  Serial3.write("\xFF\xFF\xFF");
                    Serial.println("Z0 komutu alındı: İstenen sıcaklık azaltıldı.");
                    sonButonZamani = suankiZaman;
                    gModuAktif = false;
                    break;
                    case 0x31: //component id: 65 01 02 00 FF FF FF b1
                    istenenSicaklik += 0.25;
                    sonButonZamani = millis();
                    gModuBaslamakUzere = true;
                    Serial3.print(command);
                    Serial3.write("\xFF\xFF\xFF");
                    Serial.println("Z1 komutu alındı: İstenen sıcaklık artırıldı.");
                    sendToNextion(readFilteredCelsius());
                    sonButonZamani = suankiZaman;
                    gModuAktif = false;
                    break;
                    case 0x32: //component id: 65 01 06 00 FF FF FF  //bt1
                        togglePin(pinB2, &stateB2);
                        Serial.println("Z2 komutu alındı: PinB2 toggle edildi.");
                        sendButtonState("bt1", stateB2);  // Update Nextion button bt0 state
                        sendToNextion(readFilteredCelsius());
                        break;
                    case 0x33: // component id: 65 01 07 00 FF FF FF //bt2
                        togglePin(pinB3, &stateB3);
                        Serial.println("Z3 komutu alındı: PinB3 toggle edildi.");
                        sendToNextion(readFilteredCelsius());
                        sendButtonState("bt2", stateB3);
                        break;
                    case 0x34: // component id: 65 01 08 00 FF FF FF //bt3
                        togglePin(pinB4, &stateB4);
                        Serial.println("Z4 komutu alındı: PinB4 toggle edildi.");
                         sendButtonState("bt3", stateB4);
                        sendToNextion(readFilteredCelsius());
                        break;
                    case 0x35: //component id: 65 01 09 00 FF FF FF //bt4
                        togglePin(pinB5, &stateB5);
                        Serial.println("Z5 komutu alındı: PinB5 toggle edildi.");
                        sendButtonState("bt4", stateB5);
                        sendToNextion(readFilteredCelsius());
                        break;
                    case 0x36: // component id: 65 01 0A 00 FF FF FF //bt5
                        togglePin(pinB6, &stateB6);
                        Serial.println("Z6 komutu alındı: PinB6 toggle edildi.");
                        sendButtonState("bt5", stateB6);
                        sendToNextion(readFilteredCelsius());
                        break;
                    case 0x37: // component id: 65 01 0C 00 FF FF FF //bt6
                        togglePin(pinB7, &stateB7);
                        Serial.println("Z7 komutu alındı: PinB7 toggle edildi.");
                        sendButtonState("bt7", stateB7);
                        sendToNextion(readFilteredCelsius());
                        break;
                    case 0x38: // component id: 65 01 0B 00 FF FF FF //bt7
                        togglePin(pinB8, &stateB8);
                        Serial.println("Z8 komutu alındı: PinB8 toggle edildi.");
                        sendButtonState("bt6", stateB8);
                       sendToNextion(readFilteredCelsius());
                        break;
  case 0x3A:  // Z10 komutu: 3 renk kaydet ve kayar efekt başlat // component id: 65 02 0E 00 FF FF FF 
    Serial.println("Z10 komutu alındı: 3 renk kaydedilecek.");
    renkIndex = 0;  // Renk indexini sıfırla
    renkKaydetmeAktif = true;  // Renk kaydetme işlemini aktif et
    break;
                    case 0x39: // component id: 65 02 0E 00 FF FF FF 
                        handleRainbowMode();
                        Serial.println("Z10 komutu alındı: Rainbow Cycle mode toggle edildi.");
                        
                        break;
                    case 0x3B: //component id: 65 02 11 00 FF FF FF 
    // Z11 komutu: Tüm LED modlarını kapat ve LED'leri söndür
    // Tüm LED modlarını kapat
    breathingMode = false;
    rainbowCycleMode = false;
    bouncingMode = false;
    slidingEffectActive = false;

    // Tüm LED'leri kapat
    strip.clear();
    strip.show();

    Serial.println("Z11 komutu alındı: Tüm LED modları kapatıldı ve LED'ler söndürüldü.");
    break;
                    case 0x3C: //iptal
                        rainbowSpeed = min(rainbowSpeed + 10, 255 );
                        Serial.print("Z12 komutu alındı: Rainbow Cycle hızı azaltıldı. Yeni hız: ");
                        Serial.println(rainbowSpeed);
                        break;
                    case 0x3D: //component id: 65 02 0F 00 FF FF FF 
                        handleBouncingMode();
                        Serial.println("Z13 komutu alındı: Bouncing mode toggle edildi.");
                        break;
                    case 0x3E: //component id: 65 02 0B 00 FF FF FF 
     bounceSpeed = max(bounceSpeed - 10, 10);  // Bounce speed minimum 10 olabilir, 5 birim azalır
    rainbowSpeed = max(rainbowSpeed - 10, 10);  // Rainbow speed 10 birim azalır
    Serial.print("Z15 komutu alındı: LED hızları azaltıldı. Yeni bounce speed: ");
    Serial.println(bounceSpeed);
    Serial.print("Yeni rainbow speed: ");
    Serial.println(rainbowSpeed);
    break;
                    case 0x3F: //component id: 65 02 0A 00 FF FF FF 
      bounceSpeed = min(bounceSpeed + 10, 100);  // Bounce speed maksimum 100 olabilir, 5 birim artar
    rainbowSpeed = min(rainbowSpeed + 10, 100);  // Rainbow speed 10 birim artar
    Serial.print("Z14 komutu alındı: LED hızları artırıldı. Yeni bounce speed: ");
    Serial.println(bounceSpeed);
    Serial.print("Yeni rainbow speed: ");
    Serial.println(rainbowSpeed);
    break;
                    case 0x29: // compentent id: 65 01 05 00 FF FF FF bt0
                        togglePin(pinB9, &stateB9);
                        Serial.println("B9 komutu alındı: PinB9 toggle edildi.");
                        sendButtonState("bt0", stateB9);
                       sendToNextion(readFilteredCelsius());
                        break;
                    default:
                        Serial.println("Bilinmeyen komut alındı.");
                        break;
                }
                bufferIndex = 0;
            }
        } else if (bufferIndex == 15) {
            if (isValidLEDData(buffer)) {
                processLEDData();
                Serial.println("Geçerli LED verisi alındı ve işlendi.");
            }
            bufferIndex = 0;
        }
    }

    // Motor kontrolü ve G modu işlemleri burada yer alacak
    handleMotorControl();

    // LED modlarını çalıştır
    if (breathingMode) {
        runBreathingMode();
    }
    if (rainbowCycleMode) {
        runRainbowCycleEffect();
    }
    if (bouncingMode) {
        runBouncingMode();
    }
    if (slidingEffectActive) {
        runSlidingEffect();
    }
    if (gModuBaslamakUzere && suankiZaman - sonButonZamani > 5000) {
    gModuAktif = true;
    gModuBaslamaZamani = millis();
    hesaplananVanaKonumu = HesaplaVanaKonumu(istenenSicaklik);
    Serial.print("G modu başlatıldı, hedef konum: ");
    Serial.println(hesaplananVanaKonumu);
    gModuBaslamakUzere = false;
    motorStopped = false;
    bufferIndex = 0;
}
}

// Motor kontrolü fonksiyonu
void handleMotorControl() {


    if (gModuAktif && !motorStopped) {
        int mevcutPotansDegeri = analogRead(potansPin);
        int hata = hesaplananVanaKonumu - mevcutPotansDegeri;
        int motorPWM = constrain(Kp * abs(hata), minimumPWM, maksimumPWM);

        if (abs(hata) > 10) {
            hareketEt(hata > 0 ? motorPWM : 0, hata > 0 ? 0 : motorPWM);
        } else {
            hareketEt(0, 0);
            double mevcutSicaklik = yeniDeger();
            Serial.print("Mevcut sıcaklık: ");
            Serial.println(mevcutSicaklik);
            Serial.println("Hedefe ulaşıldı. Motor durduruldu.");
            Serial.print("Motor konumu: ");
            Serial.println(mevcutPotansDegeri);
            bufferIndex = 0;

            motorStopped = true;  // Motor durdu, bir daha hata kontrolü yapma
        }
    }

    // G Modu hareket kontrolü
    if (gModuAktif && (millis() - gModuBaslamaZamani >= 10000)) {
        double mevcutSicaklik = yeniDeger();
        Serial.print("Bekleme sonrası mevcut sıcaklık: ");
        Serial.println(mevcutSicaklik);
        Serial.print("Motor konumu: ");
        Serial.println(ortalamaPotansDegeri);
        bufferIndex = 0;

        if (abs(mevcutSicaklik - istenenSicaklik) > 1.0) {
            double sicaklikFarki = istenenSicaklik - mevcutSicaklik;
            hesaplananVanaKonumu += (sicaklikFarki > 0) ? 10 : -10;
            hesaplananVanaKonumu = constrain(hesaplananVanaKonumu, 0, 1023);
            Serial.print("Ayarlanmış vana konumu: ");
            Serial.println(hesaplananVanaKonumu);
            bufferIndex = 0;

            gModuBaslamaZamani = millis();
            motorStopped = false;  // Motor tekrar çalışabilir, hata kontrolü yapılabilir
        } else {
            Serial.println("İstenen sıcaklık aralığına ulaşıldı. Motor durduruldu ve G modu kapatıldı.");
            gModuAktif = false;
            motorStopped = false;
            bufferIndex = 0;
        }
    
}


    // Hedef Modu hareket kontrolü
    if (hedefModu) {
        int potansDegeri = analogRead(potansPin);
        int hata = hedefPotansDegeri - potansDegeri;
        int motorPWM = constrain(Kp * abs(hata), minimumPWM, maksimumPWM);

        if (abs(hata) > 10) {
            if (hata > 0) {
                hareketEt(motorPWM, 0);
                Serial.println("Hedefe ulaşmak için ileri hareket ediliyor.");         
            } else {
                hareketEt(0, motorPWM);
                Serial.println("Hedefe ulaşmak için geri hareket ediliyor.");
            }
        } else {
            hareketEt(0, 0);  // Hedefe ulaşıldığında motoru durdur
            Serial.println("Hedefe ulaşıldı. Motor durduruldu.");
            hedefModu = false;
        }
    }
}

// LED verisinin geçerliliğini kontrol eden fonksiyon
bool isValidLEDData(byte* data) {
    return (data[0] == 0x72 && data[5] == 0x67 && data[10] == 0x62);
}

// LED verisini işleyen fonksiyon
void processLEDData() {
    currentR = buffer[1];
    currentG = buffer[6];
    currentB = buffer[11];

    // Eğer 0x3A komutu geldiyse ve renk kaydetme aktifse renkleri kaydet
    if (renkKaydetmeAktif) {
        slidingColors[renkIndex][0] = currentR;  // R
        slidingColors[renkIndex][1] = currentG;  // G
        slidingColors[renkIndex][2] = currentB;  // B

        Serial.print("Renk "); Serial.print(renkIndex + 1); Serial.print(" kaydedildi: ");
        Serial.print("R: "); Serial.print(currentR);
        Serial.print(" G: "); Serial.print(currentG);
        Serial.print(" B: "); Serial.println(currentB);

        renkIndex++;

        // 3 renk kaydedildiyse kayar efekti başlat
        if (renkIndex >= 3) {
            renkIndex = 0;  // Renk indexini sıfırla
            renkKaydetmeAktif = false;  // Renk kaydetmeyi kapat
            slidingEffectActive = true;  // Kayar efekti aktif et
            Serial.println("3 renk kaydedildi, kayar efekt başlatılıyor.");
     
        }
    } else {
        // Normal LED işlemesi
        Serial.println("Normal LED işleme aktif.");
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, currentR, currentG, currentB);
        }
        strip.show();
    }

    Serial.print("Veri: ");
    for (int i = 0; i < 15; i++) {
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.print(" -> R: ");
    Serial.print(currentR, HEX);
    Serial.print(" G: ");
    Serial.print(currentG, HEX);
    Serial.print(" B: ");
    Serial.println(currentB, HEX);
}

// Belirtilen pini toggle eden fonksiyon
void togglePin(int pin, bool *state) {
    *state = !*state;
    digitalWrite(pin, *state ? LOW : HIGH);
}

// Breathing mode'u yönetir
void handleBreathingMode() {
    if (breathingMode) {
        breathingMode = false;  // Breathing mode'u iptal et
        Serial.println("Breathing mode kapatıldı.");
    } else {
        breathingMode = true;  // Breathing mode'u aktif et
        rainbowCycleMode = false;  // Rainbow Cycle modunu devre dışı bırak
        slidingEffectActive = false;
        bouncingMode = false;  // Bouncing mode'u devre dışı bırak
        Serial.println("Breathing mode aktif hale getirildi.");

    }
}

// Rainbow Cycle mode'u yönetir
void handleRainbowMode() {
    if (rainbowCycleMode) {
        rainbowCycleMode = false;  // Rainbow Cycle modunu iptal et
        Serial.println("Rainbow Cycle mode kapatıldı.");
    } else {
        rainbowCycleMode = true;  // Rainbow Cycle modunu aktif et
        breathingMode = false;  // Breathing mode'u devre dışı bırak
        bouncingMode = false;  // Bouncing mode'u devre dışı bırak
        slidingEffectActive = false;

        Serial.println("Rainbow Cycle mode aktif hale getirildi.");
    }
}

// Bouncing mode'u yönetir
void handleBouncingMode() {
    if (bouncingMode) {
        bouncingMode = false;  // Bouncing mode'u iptal et
        Serial.println("Bouncing mode kapatıldı.");
    } else {
        bouncingMode = true;  // Bouncing mode'u aktif et
        breathingMode = false;  // Breathing mode'u devre dışı bırak
        rainbowCycleMode = false;  // Rainbow Cycle modunu devre dışı bırak
        slidingEffectActive = false;
        Serial.println("Bouncing mode aktif hale getirildi.");
    }
}

// Breathing mode'u çalıştıran fonksiyon
void runBreathingMode() {
    unsigned long currentMillis = millis();

    // Belirli aralıklarla LED'leri güncelle
    if (currentMillis - previousMillis >= 30) {  // 30 ms güncelleme aralığı
        previousMillis = currentMillis;

        brightness += fadeAmount;

        // Parlaklığı alt ve üst sınırlarda tut
        if (brightness <= 1 || brightness >= 255) {
            fadeAmount = -fadeAmount;
        }

        // Tüm LED'lerin parlaklığını ayarla (mevcut rengi koruyarak)
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, 
                (currentR * brightness) / 255, 
                (currentG * brightness) / 255, 
                (currentB * brightness) / 255);
        }
        strip.show();
    }
}

// Rainbow Cycle efektini çalıştıran fonksiyon
void runRainbowCycleEffect() {
    unsigned long currentMillis = millis();

    // Belirli aralıklarla LED'leri güncelle
    if (currentMillis - previousMillis >= rainbowSpeed) {
        previousMillis = currentMillis;

        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, Wheel(((i * 256 / NUM_LEDS) + slideIndex) & 255));
        }

        strip.show();
        slideIndex++;
        if (slideIndex >= 256) slideIndex = 0;
    }
}

// Renk paleti oluşturmak için kullanılan fonksiyon
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    } else {
        WheelPos -= 170;
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }
}

// Bouncing mode'u çalıştıran fonksiyon
void runBouncingMode() {
    unsigned long currentMillis = millis();

    // Belirli aralıklarla LED'leri güncelle
    if (currentMillis - previousMillis >= bounceSpeed) {
        previousMillis = currentMillis;

        // LED'leri temizle
        strip.clear();

        // Mevcut indeksteki LED'i yak
        strip.setPixelColor(slideIndex, currentR, currentG, currentB);

        // Sonraki LED'e geç
        if (bounceDirection) {
            slideIndex++;
            if (slideIndex >= NUM_LEDS) {
                bounceDirection = false;
                slideIndex--;
            }
        } else {
            slideIndex--;
            if (slideIndex < 0) {
                bounceDirection = true;
                slideIndex++;
            }
        }

        // LED'leri göster
        strip.show();
    }
}

// Nextion'a sıcaklık verisini gönderir
void sendToNextion(double temperature) {
    String command = "n0.val=" + String((int)temperature);
    command += "\xFF\xFF\xFF";  // Nextion komut sonlandırma karakterleri
    Serial3.print(command);
}
void sendTemperatureToNextion(double temperature) {
    String command = "n1.val=" + String((int)(temperature * 10)); // Format temperature value
    command += "\xFF\xFF\xFF";  // End command for Nextion
    Serial3.print(command);
}
void sendButtonState(String buttonID, bool state) {
    String command = buttonID + ".val=" + String(state ? 1 : 0);  // Button state: 1 for ON, 0 for OFF
    command += "\xFF\xFF\xFF";  // Nextion command terminator
    Serial3.print(command);  // Send command to Nextion
}

void runSlidingEffect() {
    static int effectStep = 0;           // Şu anki efekt adımını takip eder
    static int fillIndex = 0;            // Şerit boyunca ilerleyen indeks
    static unsigned long lastUpdate = 0; // Son güncelleme zamanı
    unsigned long currentTime = millis();

    if (currentTime - lastUpdate >= rainbowSpeed) {
        lastUpdate = currentTime;

        // Şeridi temizlemiyoruz, böylece önceki renkler kalır

        switch (effectStep) {
            case 0:
                // Birinci renk baştan sona doğru ilerler
                strip.setPixelColor(fillIndex, slidingColors[0][0], slidingColors[0][1], slidingColors[0][2]);
                fillIndex++;
                if (fillIndex >= NUM_LEDS) {
                    fillIndex = 0;
                    effectStep = 1;
                }
                break;
            case 1:
                // İkinci renk sondan başa doğru ilerler
                strip.setPixelColor(NUM_LEDS - 1 - fillIndex, slidingColors[1][0], slidingColors[1][1], slidingColors[1][2]);
                fillIndex++;
                if (fillIndex >= NUM_LEDS) {
                    fillIndex = 0;
                    effectStep = 2;
                }
                break;
            case 2:
                // Üçüncü renk baştan sona doğru ilerler
                strip.setPixelColor(fillIndex, slidingColors[2][0], slidingColors[2][1], slidingColors[2][2]);
                fillIndex++;
                if (fillIndex >= NUM_LEDS) {
                    fillIndex = 0;
                    effectStep = 3;
                }
                break;
            case 3:
                // Birinci renk sondan başa doğru ilerler
                strip.setPixelColor(NUM_LEDS - 1 - fillIndex, slidingColors[0][0], slidingColors[0][1], slidingColors[0][2]);
                fillIndex++;
                if (fillIndex >= NUM_LEDS) {
                    fillIndex = 0;
                    effectStep = 0; // Efekt döngüsünü tekrar başlat
                }
                break;
        }

        strip.show();

        // Debug için adım ve indeks bilgisini yazdır
        Serial.print("Effect Step: ");
        Serial.print(effectStep);
        Serial.print(" Fill Index: ");
        Serial.println(fillIndex);
    }
}



// Termistörden sıcaklık okumasını filtreler
double readFilteredCelsius() {
    int raw = analogRead(thermoPin);
    double voltage = raw * (5.01 / 1023.0);
    double temperature = (voltage - 1.25) / 0.005;
    termoOkumalari[okumaIndeksi] = temperature;
    okumaIndeksi = (okumaIndeksi + 1) % FILTRE_UZUNLUGU;
    double total = 0.0;
    for (int i = 0; i < FILTRE_UZUNLUGU; i++) {
        total += termoOkumalari[i];
    }
    return total / FILTRE_UZUNLUGU;
}

// Yeni değer için kullanılacak olan fonksiyon
double yeniDeger() {
    return spoofedDeger != -1000.0 ? spoofedDeger : readFilteredCelsius();
}

// Hareketi kontrol eden fonksiyon
void hareketEt(int ileriPWM, int geriPWM) {
    analogWrite(motorIleriPWM, ileriPWM);
    analogWrite(motorGeriPWM, geriPWM);
}
int HesaplaVanaKonumu(double istenenSicaklik) {
    return (int)(-38.89 * istenenSicaklik + 1763.56);
}
