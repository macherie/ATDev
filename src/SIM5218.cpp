
#include<SIM5218.h>


SIM5218_sms::cleanUp()
{
    memset(m_number, 0, SIM5218_NUM_SIZE + 1);
    memset(m_message, 0, SIM5218_SMS_SIZE + 1);
}


SIM5218_gps::cleanUp()
{
    memset(m_latitude, 0, SIM5218_LATITUDE_SIZE + 1);
    memset(m_longitude, 0, SIM5218_LONGITUDE_SIZE + 1);
    memset(m_altitude, 0, SIM5218_ALTITUDE_SIZE + 1);
    memset(m_speed, 0, SIM5218_SPEED_SIZE + 1);

    m_latPos    = 0x00;
    m_longPos   = 0x00;
}

SIM5218::SIM5218()
{
    memset(m_endBuffer, 0, SIM5218_BUFF_END_SIZE + 1);
    memset(m_cmdBuffer, 0, SIM5218_BUFF_CMD_SIZE + 1);
    memset(m_msgBuffer, 0, SIM5218_BUFF_MSG_SIZE + 1);

    m_msgCount = 0;
}

SIM5218::~SIM5218()
{

}

uint8_t SIM5218::sendATCmd(bool defaultEnd, uint16_t timeout)
{
    // init Buffer
    memset(m_cmdBuffer, 0, SIM5218_BUFF_CMD_SIZE + 1);
    memset(m_msgBuffer, 0, SIM5218_BUFF_MSG_SIZE + 1);

    // init Counter
    m_msgCount = 0;
    
    // is it the default AT end or had his own end?
    if (defaultEnd) {
        // set default end of AT communication
        memset(m_endBuffer, 0, SIM5218_BUFF_END_SIZE + 1);
        strncpy_P(m_endBuffer, SIM5218_END_OK, SIM5218_BUFF_END_SIZE);    
    }

    // Clear input Serial buffer
    while (Serial.read() >= 0);

    // Send AT command
    Serial.println(m_cmdBuffer);

    // wait until all data are send
    Serial.flush();

    // process answer
    do {

        // if data in serial input buffer
        while (Serial.available()) {

            // buffer is full
            if (m_msgCount + 1 >= SIM5218_BUFF_MSG_SIZE) {
                return SIM5218_ERR_BUFFER_FULL;
            }

            // read into buffer
            m_msgBuffer[++m_msgCount] = Serial.read();
        }

    } while (); // timeout

    return SIM5218_ERR_TIMEOUT;
}

# vim: set sts=4 sw=4 ts=4 et: