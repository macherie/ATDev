// Option
#define SIM5218_USE_ARDUINO
#define SIM5218_USE_SMS

#include <ATDev_HW.h>

SIM5218 modem;

char numberBuf[ATDEV_DEFAULT_NUM_SIZE];
char messageBuf[ATDEV_DEFAULT_SMSTXT_SIZE];

// PIN code
uint16_t simPin = 0;

void setup() {
  uint8_t networkStatus = 0;
  uint16_t smsIdx       = 0;
  ATData_SMS smsData(numberBuf, ATDEV_DEFAULT_NUM_SIZE, 
                     messageBuf, ATDEV_DEFAULT_SMSTXT_SIZE);

  // Device data initialize
  modem.initialize(&Serial, 115200);

  // Power on the Shield && init
  if (modem.onPower(2) == ATDEV_OK) {

    // set PIN is avilable
    if (simPin > 0) {
      if (modem.setSIMPin(simPin) != ATDEV_OK) {
        Serial.println(F("SIMPin fails!"));
      }
    }

    // Check Network Status
    networkStatus = modem.getNetworkStatus();

    // Connected with Carrier
    if (networkStatus == ATDEV_NETSTAT_REGISTERED || networkStatus == ATDEV_NETSTAT_ROAMING) {
      Serial.println(F("Modem connected with carrier"));

      if (modem.initializeSMS() == ATDEV_OK) {

        // read all SMS and Delete it
        do {
          smsIdx = modem.readNextIdxSMS();

          // if SMS store index number
          if (smsIdx != ATDEV_SMS_NO_MSG) {

            // read sms into SRAM
            if (modem.receiveSMS(&smsData, smsIdx) == ATDEV_OK) {
              Serial.print(F("Number: "));
              Serial.println(smsData.m_number);
              Serial.println(smsData.m_message);
            }
            else {
              Serial.println(F("readSMS fail!"));
            }

            // Delete SMS
            if (modem.deleteSMS(smsIdx) != ATDEV_OK) {
              Serial.println(F("deleteSMS fail!"));
              smsIdx = ATDEV_SMS_NO_MSG;
            }
          }
         
        } while (smsIdx != ATDEV_SMS_NO_MSG);

        // flush all SIM storage
        if (modem.deleteAllSMS(ATDEV_SMS_DEL_ALL) != ATDEV_OK) {
          Serial.println(F("deleteAllSMS fail!"));
        }
      }
      else {
        Serial.println(F("initializeSMS fail!"));
      }
    }
    else {
      Serial.print(F("Modem not connected! State: "));
      Serial.println(networkStatus, HEX);
    }
  }
  else {
    Serial.println(F("PowerOn fails!"));
  }

}

void loop() {
  

}
