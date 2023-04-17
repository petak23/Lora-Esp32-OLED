#include <Arduino.h>

#ifndef PV_CRYPT__H
#define PV_CRYPT__H

/**
 * Prevzaté z: https://github.com/petrbrouzda/RatatoskrIoT časť ra_config
 * a upravené pre PlatformIO a vlastné potreby.
 *
 * Posledná zmena(Last change): 09.09.2022
 *
 * @authors Petr Brouzda <petr.brouzda@gmail.com>; Ing. Peter VOJTECH ml. <petak23@gmail.com>
 * @copyright  Copyright (c) 2016 - 2022 Petr Brouzda, Ing. Peter VOJTECH ml.
 * @license Apache License 2.0
 * @link       http://petak23.echo-msz.eu
 * @version 1.0.2
 */

#include "sha256.h"
#include <CRC32.h>

class pvCrypt
{
public:
  pvCrypt();
  ~pvCrypt();
  void setInfo(char *deviceId, char *appPassPhrase);

  void getDeviceId(char *buf);
  String encrypt(char *itemName, char *value);
  String decrypt(char *itemName, char *enctext);

  /** je public jen kvuli testovani, nepouzivat */
  bool dectryptItem(char *aes_key, char *itemName, char *ciphertext, char *target, int targetSize);
  /** je public jen kvuli testovani, nepouzivat */
  void encryptItem(char *aes_key, char *itemName, char *value, char *target, int targetSize);
  /** je public jen kvuli testovani, nepouzivat */
  void createEncryptionKey(char *deviceId, char *appPassphrase, char *aes_key);

private:
  char deviceId[33];
  char appPassPhrase[33];
};

void random_block(BYTE *target, int size);
void btohexa(unsigned char *bytes, int inLength, char *output, int outLength);
int hexatob(char *input, unsigned char *output, int outLength);
int trng();

#endif
