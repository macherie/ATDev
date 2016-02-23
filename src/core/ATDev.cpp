
#include "ATDev.h"


ATDev::ATDev()
{
    m_hwSerial      = NULL;

    memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE + 1);
    memset(m_cmdBuffer, 0x00, ATDEV_BUFF_CMD_SIZE + 1);
    memset(m_msgBuffer, 0x00, ATDEV_BUFF_MSG_SIZE + 1);

    m_readPtr       ^= m_readPtr;
    m_timeOut       ^= m_timeOut;
    m_onModulePin   ^= m_onModulePin;
}

void ATDev::flushInput()
{
    // UART initialize?
    if (m_hwSerial == NULL) {
        return;
    }

    // Clear input Serial buffer
    while (m_hwSerial->available() > 0) {
        while (m_hwSerial->read() >= 0);
        delay(ATDEV_FLUSH);
    }
}

uint8_t ATDev::sendATCmd(bool abruptEnd, bool streamBuf, char* readBuf, uint16_t readBufSize)
{
    uint32_t    isTimeOut;
    uint32_t    startTime;
    uint8_t     endSize;
    bool        over;

    // UART initialize?
    if (m_hwSerial == NULL) {
        return ATDEV_ERR_INITIALIZE;
    }

    ////
    // init buffers
    memset(readBuf, 0x00, readBufSize);

    // init Counter
    m_readPtr ^= m_readPtr;
    
    // is it the default AT end or had his own end?
    // set default end of AT communication
    if (!abruptEnd) {
        strncpy_P(m_endBuffer, ATDEV_END_OK, ATDEV_BUFF_END_SIZE);    
    }
    endSize = strlen(m_endBuffer);

    ////
    // Send AT command
    if (m_cmdBuffer[0] != 0x00) {

        // Clear input Serial buffer
        this->flushInput();

        // send command
        m_hwSerial->println(m_cmdBuffer);

        // clean comand buffer for next use
        memset(m_cmdBuffer, 0x00, ATDEV_BUFF_CMD_SIZE);

        ////
        // wait until all data are send
        m_hwSerial->flush();
    }

    ////
    // Calc Timeout
    startTime = millis();
    isTimeOut = startTime + m_timeOut;

    // overloaded
    if (isTimeOut < startTime) {
        over = true;
    }
    else {
        over = false;
    }

    // reset timeout for next function
    m_timeOut = ATDEV_DEFAULT_TIMEOUT;
    

    ////
    // process answer
    do {

        // if data in serial input buffer
        while (m_hwSerial->available()) {

            // buffer is full (not circle)
            if (!streamBuf && m_readPtr >= readBufSize) {
                memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1);
                return ATDEV_ERR_BUFFER_FULL;
            }

            ////
            // Circle buffer / Line end
            if (streamBuf) {
                // Buffer full or Line end detect
                if (m_readPtr >= readBufSize || (m_readPtr > 0 && readBuf[m_readPtr -1] == ATDEV_CH_LF)) {
                    memset(readBuf, 0x00, readBufSize);
                    m_readPtr ^= m_readPtr;
                }
            }

            // read into buffer
            readBuf[m_readPtr++] = m_hwSerial->read();

            ////
            // if abrupt end of communication is set
            if (abruptEnd && m_readPtr >= endSize && 
                    strstr(&readBuf[m_readPtr - endSize], m_endBuffer) != 0) {
                memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1);
                return ATDEV_OK; 
            }
        }

        ////
        // check is it the end of AT Command in answer buffer
        if (m_readPtr >= endSize && strstr(readBuf, m_endBuffer) != 0) {
            memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1);
            return ATDEV_OK; 
        }
        // Error
        else if (m_readPtr >= ATDEV_END_ERROR_SIZE &&
                strstr_P(readBuf, ATDEV_END_ERROR) != 0) {
            memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1);
            return ATDEV_ERR_ERROR_RECEIVED;
        }

        // calc diff timeout
        if (over) {
            if (startTime > millis()) {
                over = false;
            }
        }

    } while ((isTimeOut > millis() && !over) || over); // timeout

    memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1);
    return ATDEV_ERR_TIMEOUT;
}

uint8_t ATDev::readLine(char* readBuf, uint16_t readBufSize)
{
    // CR LF to end buffer
    strncpy_P(m_endBuffer, ATDEV_END_LINE, ATDEV_BUFF_END_SIZE);    

    return this->sendATCmdAbrupt(readBuf, readBufSize);
}

uint8_t ATDev::parseInternalData(char* readBuf, uint16_t readBufSize)
{
    bool    isString    = false;
    uint8_t params      = 0;

    // search hole string
    for (uint16_t i = 0; i < readBufSize; i++) {

        // end
        if (readBuf[i] == 0x00) {
            return params;
        }
        // is string "xy zyx"
        else if (readBuf[i] == ATDEV_CH_IC) {
            readBuf[i] = 0x00;
            isString = !isString;
        }
        // ' ' or ',' or ':' replace with '\0'
        else if (!isString && (readBuf[i] == ATDEV_CH_SP || 
                readBuf[i] == ATDEV_CH_CO ||
                readBuf[i] == ATDEV_CH_DD)) {
            readBuf[i] = 0x00;
        }

        // count
        if (i > 0 && readBuf[i] == 0x00 && readBuf[i-1] != 0x00) {
            params++;
        }
    }

    return params;
}

char* ATDev::getParseElement(uint8_t indx, char* readBuf, uint16_t readBufSize)
{
    uint8_t count = 0;

    // search hole string
    for (uint16_t i = 0; i < readBufSize; i++) {

        // find next position
        if (readBuf[i] == 0x00 && i > 0 && readBuf[i-1] != 0x00) {
            count++;
        }

        // found indx with next character
        if (count == indx && readBuf[i] != 0x00) {
            return &readBuf[i];
        }
    }

    return NULL;
}

uint8_t ATDev::waitDevice(uint8_t ret)
{
    if (ret == ATDEV_OK) {
        delay(ATDEV_WAIT);
    }

    return ret;
}

void ATDev::initialize(HardwareSerial *UART, long baudrate, uint8_t onPinMod)
{
    m_hwSerial      = UART;
    m_onModulePin   = onPinMod;

    // init serial interface
    pinMode(m_onModulePin, OUTPUT);
    m_hwSerial->begin(baudrate);
}

uint8_t ATDev::onPower()
{
    // First AT CMD timeout
    m_timeOut = ATDEV_FIRST_ATCMD_TIMEOUT;

    // Wait for starting up modul
    delay(m_timeOut);

    if (this->isReady() != ATDEV_OK) {

        digitalWrite(m_onModulePin, HIGH);
        delay(3000);
        digitalWrite(m_onModulePin, LOW);
        delay(1000);

        // check is modem response
        for (uint8_t i = 0; i < ATDEV_POWER_RETRY; i++) {
            if (this->isReady() == ATDEV_OK) {
                return this->waitDevice(ATDEV_OK);
            }
        }
    }
    // power is allready on
    else {
        return ATDEV_OK;
    }

    // timeout
    return ATDEV_ERR_TIMEOUT;
}

uint8_t ATDev::isReady()
{
    strncpy_P(m_cmdBuffer, ATDEV_CMD_AT, ATDEV_BUFF_CMD_SIZE);
   
    return this->sendATCmd();
}

uint8_t ATDev::setSIMPin(uint16_t pin)
{
    snprintf_P(m_cmdBuffer, ATDEV_BUFF_CMD_SIZE, ATDEV_CMD_CPIN, pin);

    return this->waitDevice(this->sendATCmd());
}

uint8_t ATDev::getNetworkStatus()
{
    strncpy_P(m_cmdBuffer, ATDEV_CMD_CREG, ATDEV_BUFF_CMD_SIZE);
   
    if (this->sendATCmd() == ATDEV_OK) {
        // parse answer
        this->parseInternalData();

        return atoi(this->getParseElement(2));
    }

    return ATDEV_NETSTAT_UNKNOWN;
}

bool ATDev::isCMSError()
{
    // found "+CMS ERROR: %d"
    if (strstr_P(m_msgBuffer, ATDEV_STR_CMS) != NULL) {
        return true;
    }

    return false;
}

// vim: set sts=4 sw=4 ts=4 et:
