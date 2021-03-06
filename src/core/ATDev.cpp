
#include "ATDev.h"

// return value for function sendATCmd
#define SENDATCMD_END(X) \
    memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE +1); \
    return X;


ATDev::ATDev()
{
    m_hwSerial      = NULL;

    memset(m_endBuffer, 0x00, ATDEV_BUFF_END_SIZE + 1);
    memset(m_cmdBuffer, 0x00, ATDEV_BUFF_CMD_SIZE + 1);
    memset(m_msgBuffer, 0x00, ATDEV_BUFF_MSG_SIZE + 1);

    m_readPtr       ^= m_readPtr;
    m_timeOut       ^= m_timeOut;
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
        SENDATCMD_END(ATDEV_ERR_INITIALIZE);
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
                SENDATCMD_END(ATDEV_ERR_BUFFER_FULL);
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
                SENDATCMD_END(ATDEV_OK);
            }
        }

        ////
        // check is it the end of AT Command in answer buffer
        if (m_readPtr >= endSize && strstr(readBuf, m_endBuffer) != 0) {
            SENDATCMD_END(ATDEV_OK);
        }
        // Error
        else if (m_readPtr >= ATDEV_END_ERROR_SIZE &&
                strstr_P(readBuf, ATDEV_END_ERROR) != 0) {
            SENDATCMD_END(ATDEV_ERR_ERROR_RECEIVED);
        }

        // calc diff timeout
        if (over) {
            if (startTime > millis()) {
                over = false;
            }
        }

    } while ((isTimeOut > millis() && !over) || over); // timeout

    SENDATCMD_END(ATDEV_ERR_TIMEOUT);
}

uint8_t ATDev::readLine(char* readBuf, uint16_t readBufSize)
{
    // CR LF to end buffer
    strncpy_P(m_endBuffer, ATDEV_END_LINE, ATDEV_BUFF_END_SIZE);    

    return this->sendATCmdAbrupt(readBuf, readBufSize);
}

uint8_t ATDev::parseATCmdData(char* readBuf, uint16_t readBufSize)
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

void ATDev::trimATEnd(char* readBuf, uint16_t readBufSize, uint16_t dataSize)
{
    bool        foundAT = false;
    uint16_t    pos     = dataSize -2;

    // secure check
    if (pos < 0) {
        return;
    }

    // Search OK AT Answer String
    for ( ; pos > 0; --pos) {
        if (strstr_P(readBuf + pos, ATDEV_END_OK) != NULL) {
            --pos;
            foundAT = true;
            break;
        }
    }

    // NO AT CMD found!
    if (!foundAT) {
        return;
    }

    // find next character they not for AT communication
    for ( ; pos > 0; --pos) {
        if (readBuf[pos] != ATDEV_CH_CR && readBuf[pos] != ATDEV_CH_LF) {
            break;
        }
    }

    // clean AT communication controll characters
    memset(readBuf + pos +1, 0x00, readBufSize - pos -1);
}

uint8_t ATDev::isReady()
{
    strncpy_P(m_cmdBuffer, ATDEV_CMD_AT, ATDEV_BUFF_CMD_SIZE);
   
    return this->sendATCmd();
}

// vim: set sts=4 sw=4 ts=4 et:
