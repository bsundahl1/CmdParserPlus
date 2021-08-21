/* Copyright 2016 Pascal Vizeli <pvizeli@syshack.ch>
 * BSD License
 *
 * https://github.com/pvizeli/CmdParser
 */

#include "CmdBuffer.h"

/**
 * Clear buffer and set defaults.
 */
CmdBufferObject::CmdBufferObject()
      : m_endChar(CMDBUFFER_CHAR_LF),
        m_bsChar(CMDBUFFER_CHAR_BS),
        m_strtChar(0),
        m_numStrtChars(0),
        m_ID(CMDBUFFER_NO_ID),
        m_foundStartChar(0),
        m_dataOffset(0),
        m_echo(false)
{
}


bool CmdBufferObject::readFromSerial(Stream *serial, uint32_t timeOut)
{
    uint32_t isTimeOut;
    uint32_t startTime;
    bool     over = false;

    // UART initialize?
    if (serial == NULL) {
        return false;
    }

    ////
    // Calc Timeout
    if (timeOut != 0) {
        startTime = millis();
        isTimeOut = startTime + timeOut;

        // overloaded
        if (isTimeOut < startTime) {
            over = true;
        } else {
            over = false;
        }
    }

    ////
    // process serial reading
    do {

        // if data in serial input buffer
        while (serial->available()) {

            if (this->readSerialChar(serial)) {
                return true;
            }
        }

        // Timeout is active?
        if (timeOut != 0) {
            // calc diff timeout
            if (over) {
                if (startTime > millis()) {
                    over = false;
                }
            }

            // timeout is receive
            if (isTimeOut <= millis() && !over) {
                return false;
            }
        }

    } while (true); // timeout

    return false;
}


// Checks for start and end characters
// Saves printable characters in buffer
// @return  true if line terminator found
bool CmdBufferObject::readSerialChar(Stream *serial)
{
    uint8_t  readChar;
    uint8_t *buffer = this->getBuffer();

    // UART initialize?
    if (serial == NULL) {
        return false;
    }

    if (serial->available()) {
        // is buffer full?
        if (m_dataOffset >= this->getBufferSize()) {
            m_dataOffset = 0;
            m_foundStartChar = 0;
            return false;
        }

        // read into buffer
        readChar = serial->read();

        if (m_echo) {
            serial->write(readChar);
        }

        // is that the start character?
        if ( (m_strtChar != 0) && (m_foundStartChar < m_numStrtChars) ) {
            if (readChar == m_strtChar ) {
                // multiple start characters can be configured. This counts
                // how many we have so far
                m_foundStartChar++;       // if found, increment count
                                          // but dont save anything in the buffer
            }
            else {
                m_foundStartChar = 0;     // start charactes must be  consecutive
            }
            return false;    // if not, try again next time
        }
        // if we get here, then the start character(s) were found

        // is that the device ID?
        if ( (m_ID != CMDBUFFER_NO_ID) && (m_foundStartChar == m_numStrtChars) ) {
            if (readChar == m_ID ) {
                m_foundStartChar++;        // if found, increment count
                                           // but dont save anything in the buffer
            }
            else {
                m_foundStartChar = 0;       // ID MUST immediatly follow the start
                                            // character, otherwise the message is not
                                            // for us. Clear the flag and start over.
            }
            return false;    // if not, try again next time
        }
        // if we get this far, we have a valid ID

        // is that the end of command?
        if (readChar == m_endChar) {
            buffer[m_dataOffset] = '\0';
            m_dataOffset         = 0;
            m_foundStartChar     = 0;
            return true;
        }

        // is that a backspace char?
        if ((readChar == m_bsChar) && (m_dataOffset > 0)) {
            // buffer[--m_dataOffset] = 0;
            --m_dataOffset;
            if (m_echo) {
                serial->write(' ');
                serial->write(readChar);
            }
            return false;
        }

        // if is a printable character, finally save it in the buffer
        if (readChar > CMDBUFFER_CHAR_PRINTABLE) {
            buffer[m_dataOffset++] = readChar;
        }
    }
    return false;
}
