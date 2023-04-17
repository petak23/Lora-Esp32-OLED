#include "pv_crypt.h"

#define CBC 1
#define ECB 0
#define CTR 0
extern "C"
{
#include "aes.h"
}

pvCrypt::pvCrypt()
{
}

pvCrypt::~pvCrypt()
{
}

void pvCrypt::setInfo(char *deviceId, char *appPassPhrase)
{
  strncpy(this->deviceId, deviceId, 32);
  this->deviceId[32] = 0;
  strncpy(this->appPassPhrase, appPassPhrase, 32);
  this->appPassPhrase[32] = 0;
}

String pvCrypt::encrypt(char *itemName, char *value)
{
  char aes_key[AES_KEYLEN];
  char buffer[256];
  char enctext[256];

  this->createEncryptionKey(this->deviceId, this->appPassPhrase, (char *)aes_key);

  this->encryptItem((char *)aes_key, itemName, value, enctext, 256);

  return enctext;
}

String pvCrypt::decrypt(char *itemName, char *enctext)
{
  char aes_key[AES_KEYLEN];
  char buffer[256];
  char dectext[256];

  this->createEncryptionKey(this->deviceId, this->appPassPhrase, (char *)aes_key);

  if (!this->dectryptItem((char *)aes_key, itemName, enctext, dectext, 256))
  {
    return String("#Err [pv_crypt.cpp]");
  }
  else
  {
    return String(dectext);
  }
}

/**
 * Device ID se pouziva pro sifrovani konfiguracniho souboru
 * a pro nastaveni jmena konfiguracniho AP.
 * Pro jedno zarizeni musi vracet pokazde stejnou hodnotu.
 *
 * @doc Prevzaté z originálneho súboru ra.ino
 */
void pvCrypt::getDeviceId(char *buf)
{
#ifdef ESP8266
  sprintf(buf, "%d", ESP.getChipId());
#endif

#ifdef ESP32
  uint64_t macAddressTrunc = ((uint64_t)ESP.getEfuseMac()) << 40;
  long chipId = macAddressTrunc >> 40;
  sprintf(buf, "%d", chipId);
#endif
}

void pvCrypt::createEncryptionKey(char *deviceId, char *appPassphrase, char *aes_key)
{
  char fullPwd[256];
  snprintf(fullPwd, 255, "%s%s", deviceId, appPassphrase);
  fullPwd[255] = 0;

  Sha256 sha256;
  sha256.update((const BYTE *)fullPwd, strlen((const char *)fullPwd));
  sha256.final((unsigned char *)aes_key);
}

void createItemIv(char *item, char *aes_iv)
{
  BYTE hash[AES_KEYLEN];

  Sha256 sha256;
  sha256.update((const BYTE *)item, strlen((const char *)item));
  sha256.final(hash);
  memcpy(aes_iv, hash, AES_BLOCKLEN);
}

void pvCrypt::encryptItem(char *aes_key, char *itemName, char *value, char *target, int targetSize)
{
  BYTE aes_iv[AES_BLOCKLEN];
  createItemIv(itemName, (char *)aes_iv);

  BYTE payload[257];
  int sourceLen = strlen(value);
  payload[4] = (sourceLen >> 8) & 0xff;
  payload[5] = sourceLen & 0xff;
  memcpy(payload + 6, value, sourceLen);

  uint32_t crc1 = CRC32::calculate((unsigned char *)value, sourceLen);
  payload[0] = (BYTE)((crc1 >> 24) & 0xff);
  payload[1] = (BYTE)((crc1 >> 16) & 0xff);
  payload[2] = (BYTE)((crc1 >> 8) & 0xff);
  payload[3] = (BYTE)(crc1 & 0xff);

  int celkovaDelka = sourceLen + 2 + 4;
  int zbyva = AES_BLOCKLEN - (celkovaDelka % AES_BLOCKLEN);
  if (zbyva > 0)
  {
    random_block(payload + celkovaDelka, zbyva);
  }

  celkovaDelka = celkovaDelka + zbyva;

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, (unsigned char const *)aes_key, (unsigned char const *)aes_iv);
  AES_CBC_encrypt_buffer(&ctx, (unsigned char *)payload, celkovaDelka);

  btohexa(payload, celkovaDelka, target, targetSize);
}

bool pvCrypt::dectryptItem(char *aes_key, char *itemName, char *ciphertext, char *target, int targetSize)
{
  BYTE aes_iv[AES_BLOCKLEN];
  createItemIv(itemName, (char *)aes_iv);

  BYTE plainText[257];
  hexatob(ciphertext, (BYTE *)plainText, 256);

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, (unsigned char *)aes_key, aes_iv);
  AES_CBC_decrypt_buffer(&ctx, (unsigned char *)plainText, strlen(ciphertext) / 2);

  uint32_t crcReceived = (plainText[0] << 24) | (plainText[1] << 16) | (plainText[2] << 8) | plainText[3];
  int len = (plainText[4] << 8) | plainText[5];

  plainText[len + 6] = 0;

  uint32_t crcComputed = CRC32::calculate((unsigned char *)plainText + 6, len);

  if (crcReceived != crcComputed)
  {
    return false;
  }

  strncpy(target, (char *)(plainText + 6), targetSize);
  target[targetSize - 1] = 0;

  return true;
}

unsigned char btohexa_high(unsigned char b)
{
  b >>= 4;
  return (b > 0x9u) ? b + 87 : b + '0'; // 87 = 'a'-10
}

unsigned char btohexa_low(unsigned char b)
{
  b &= 0x0F;
  return (b > 9u) ? b + 87 : b + '0'; // 87 = 'a'-10
}

void btohexa(unsigned char *bytes, int inLength, char *output, int outLength)
{
  char *out = output;
  for (int i = 0; i < inLength && i < outLength; i++)
  {
    *out = btohexa_high(bytes[i]);
    out++;
    *out = btohexa_low(bytes[i]);
    out++;
  }
  *out = 0;
}

int hexatob(char *input, unsigned char *output, int outLength)
{
  unsigned char *out = output;
  unsigned char *outEnd = output + outLength;
  int inLength = strlen(input);
  while ((*input != 0) && (out < outEnd))
  {
    char ch = *input;
    input++;
    char cl = *input;
    if (cl == 0)
      break;
    input++;
    unsigned char b = ch >= 'a' ? (ch - 'a' + 10) << 4 : (ch >= 'A' ? (ch - 'A' + 10) << 4 : (ch - '0') << 4);
    b |= cl >= 'a' ? (cl - 'a' + 10) : (cl >= 'A' ? (cl - 'A' + 10) : (cl - '0'));
    *out = b;
    out++;
  }
  return (out - output);
}

#if defined(ESP32)
// https://rweather.github.io/arduinolibs/RNG_8cpp_source.html
int trng()
{
  return esp_random();
}
#endif

#if defined(ESP8266)
/**
    https://github.com/CSSHL/ESP8266-Arduino-cryptolibs

    Simple access to ESP8266's TRNG as documented on
    http://esp8266-re.foogod.com/wiki/Random_Number_Generator

    To use, just call trng() from your code: you'll get 4 completely random bytes.
*/
int trng()
{
  return *((volatile int *)0x3FF20E44);
}

#endif

/**
 * Vygeneruje blok nahodnych bytes; delka musi byt delitelna 4!
 */
void random_block(BYTE *target, int size)
{
  for (int i = 0; i < size;)
  {

    // definovano v platform/esp8266-trng.h platform/esp32-trng.h
    long l = trng();

    target[i++] = (unsigned char)(l & 0xff);
    target[i++] = (unsigned char)((l >> 8) & 0xff);
    target[i++] = (unsigned char)((l >> 16) & 0xff);
    target[i++] = (unsigned char)((l >> 24) & 0xff);
  }
}