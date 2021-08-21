/* Copyright 2016 Pascal Vizeli <pvizeli@syshack.ch>
 * BSD License
 *
 * https://github.com/pvizeli/CmdParser
 */


#include "CmdParser.h"

CmdParser::CmdParser()
    : m_ignoreQuote(false),
      m_useKeyValue(false),
      m_seperator(CMDPARSER_CHAR_SP),
      m_checkParens(false),
      m_open_paren(  '(' ),
      m_close_paren( ')' ),
    //  m_ID(CMDPARSER_NO_ID),
      m_buffer(NULL),
      m_bufferSize(0),
      m_paramCount(0),
      m_errorStr(NULL),
      m_warningStr(NULL)
{
}

uint16_t CmdParser::parseCmd(uint8_t *buffer, size_t bufferSize)
{
    bool isString = false;
    bool isInsideParen  = false;
    m_paramCount = 0;   // init param count
    m_errorStr   = NULL;   // clear errors at start of parsing
    m_warningStr = NULL;

    // buffer is not okay
    if (buffer == NULL || bufferSize == 0 || buffer[0] == 0x00) {
        //if(m_errorStr == NULL)
        //    m_errorStr = (char *)"Error: parse buffer is NULL or empty";
        return CMDPARSER_ERROR;
    }

    // init buffer
    m_buffer     = buffer;
    m_bufferSize = bufferSize;

    ////
    // Run Parser
    for (size_t i = 0; i < bufferSize; i++) {

        // this should never happen
        if (buffer[i] == 0x00 || m_paramCount == 0xFFFE) {
            if (i > 0 && buffer[i-1] != 0x00) {
                m_paramCount++;
            }
            if( isString == true ) {
                if(m_errorStr == NULL)
                    m_errorStr = (char *)"Error: something strange happened";
            }
            return m_paramCount;
        }
        // remove quotes, but do not remove seperator inside quotes
        // example string: "Hello world!"
        else if (!m_ignoreQuote && buffer[i] == CMDPARSER_CHAR_DQ) {
            buffer[i] = 0x00;
            isString  = !isString;
        }
        // replace seperator with '\0'
        else if (!isString && !isInsideParen && buffer[i] == m_seperator) {
            buffer[i] = 0x00;
        }
        // check for parentheses
        else if (m_checkParens && buffer[i] == m_open_paren) {
            if( isInsideParen ==  true ) {
                if(m_warningStr == NULL)
                    m_warningStr = (char *)"Warning: expected closing parentheses";
            }
            else
                isInsideParen = true;
            buffer[i] = 0x00;
        }
        else if (m_checkParens && buffer[i] == m_close_paren) {
            if( isInsideParen ==  false ) {
                if(m_warningStr == NULL)
                    m_warningStr = (char *)"Warning: expected opening parentheses";
            }
            else
                isInsideParen = false;
            buffer[i] = 0x00;
        }
        // replace = with '\0' if KEY=Value is set
        //else if (!isString && m_useKeyValue && buffer[i] == CMDPARSER_CHAR_EQ) {
        //    buffer[i] = 0x00;
        //}

        // count
        if (i > 0 && buffer[i] != 0x00 && buffer[i-1] == 0x00) {
            m_paramCount++;  // found start of word
        }
        if (i == 0 && buffer[i] != 0x00) {
            m_paramCount++;  // found word at beginning of buffer
            // Note: need to count the command word here to prevent a misscount
            // if there is a leading seperator at the beginning of the buffer
        }
    }

    // check for missing quotes
    if( isString == true ) {
        if(m_warningStr == NULL)
            m_warningStr = (char *)"Warning: Missmatched quotes";
    }
    // check for missing parentheses
    if( isInsideParen == true ) {
        if(m_warningStr == NULL)
            m_warningStr = (char *)"Warning: Missmatched parentheses";
    }

    if( m_paramCount > 0 )
      m_paramCount--;  // do not count command word
    return m_paramCount;
}


// Get parameter string
// @param  parameter number starting from 1; 0=command
// @return  char pointer, pointing to parameter text
char *CmdParser::getCmdParam(uint16_t idx)
{
    uint16_t count = 0;

    // idx out of range
    if (idx > m_paramCount) {
        if(m_errorStr == NULL)
            m_errorStr = (char *)"Error: missing parameter";
        return NULL;
    }

    // search hole cmd buffer
    for (size_t i = 0; i < m_bufferSize; i++) {

        // increment count at the end of each string
        // parameters are separated by NULLs
        if (i > 0 && m_buffer[i] == 0x00 && m_buffer[i - 1] != 0x00) {
            count++;
        }

        // find start of string with requested indx
        if (count == idx && m_buffer[i] != 0x00) {
            return reinterpret_cast<char *>(&m_buffer[i]);
        }
    }

    // something went wrong
    if(m_errorStr == NULL)
        m_errorStr = (char *)"Error: parsing error";
    return NULL;
}


// return parameter idx as a float or double
double CmdParser::getCmdParamAsFloat(uint16_t idx)
{
   //char *err_msg = (char *)"Error: Expecting floating point value";

   // Check for NULL string (this should never happen)
   char *str = this->getCmdParam(idx);
   if( str == NULL ) {
      if(m_errorStr == NULL)
          m_errorStr = (char *)"Error: missing parameter";
      return( 0.0 );
   }

   // Check for a valid floating point value in the string
   if( this->floatInStr( str ) ) {  // if str contains a float
      return( strtod(str, NULL) );          // convert to a float
   }
   // If this is a valid integer value, give a warning, but
   // return the value
   if( this->intInStr( str ) ) {    // if str contains an integer
      if(m_warningStr == NULL)
          m_warningStr = (char *)"Warning: expecting float";
      return( strtod(str, NULL) );          // convert to a float
   }

   // if we get here, a valid number was not found in the string
   if(m_errorStr == NULL)
       m_errorStr = (char *)"Error: not a valid floating point number";
   return( 0.0 );
}


// return parameter idx as a float or double with range checking
// @param idx    cmd parameter index (from 1)
// @param min    minimum allowed value
// @param max    maximum allowed value
// @param treatAsError   0=range check failure is a warning(default),
//                       1=range check failure is an error
double CmdParser::getCmdParamAsFloat(uint16_t idx, double min, double max, uint8_t treatAsError)
{
   double value = this->getCmdParamAsFloat(idx);

   if( value < min ) {
      if( treatAsError ) {
         if(m_errorStr == NULL)
             m_errorStr = (char *)"Error: value out of range";
         return( 0.0 );
      }
      else { // treat as warning
         if(m_warningStr == NULL)
             m_warningStr = (char *)"Warning: using min value";
         return( min );
      }
   }
   else if( value > max ) {
      if( treatAsError ) {
         if(m_errorStr == NULL)
             m_errorStr = (char *)"Error: value out of range";
         return( 0.0 );
      }
      else { // treat as warning
         if(m_warningStr == NULL)
             m_warningStr = (char *)"Warning: using max value";
         return( max );
      }
   }
   // else value is in range
   return( value );
}


// return parameter idx as an int or long
long CmdParser::getCmdParamAsInt(uint16_t idx)
{
   // Check for NULL string (this should never happen)
   char *str = this->getCmdParam(idx);
   if( str == NULL ) {
      if(m_errorStr == NULL)
          m_errorStr = (char *)"Error: missing parameter";
      return( 0.0 );
   }

   // Check for a valid integer value in the string
   if( this->intInStr( str ) ) {  // if str contains an integer value
      return( strtol(str, NULL, 10) );        // convert to a long
   }
   if( this->hexInStr( str ) ) {  // if str contains an integer value
      return( strtol(str, NULL, 16) );        // convert to a long
   }
   // If this is a valid float value, give a warning, but
   // return the value
   if( this->floatInStr( str ) ) {    // if str contains a float
      if(m_warningStr == NULL)
          m_warningStr = (char *)"Warning: truncated to integer";
      return( strtol(str, NULL, 10) );          // convert to a long
   }

   // if we get here, a valid number was not found in the string
   if(m_errorStr == NULL)
       m_errorStr = (char *)"Error: not a valid integer number";
   return( 0.0 );
}


// return parameter idx as an int or long with range checking
// @param idx    cmd parameter index (from 1)
// @param min    minimum allowed value
// @param max    maximum allowed value
// @param treatAsError   0=range check failure is a warning(default),
//                       1=range check failure is an error
long CmdParser::getCmdParamAsInt(uint16_t idx, long min, long max, uint8_t treatAsError)
{
   long value = this->getCmdParamAsInt(idx);

   if( value < min ) {
      if( treatAsError ) {
         if(m_errorStr == NULL)
             m_errorStr = (char *)"Error: value out of range";
         return( 0 );
      }
      else { // treat as warning
         if(m_warningStr == NULL)
             m_warningStr = (char *)"Warning: using min value";
         return( min );
      }
   }
   else if( value > max ) {
      if( treatAsError ) {
         if(m_errorStr == NULL)
             m_errorStr = (char *)"Error: value out of range";
         return( 0 );
      }
      else { // treat as warning
         if(m_warningStr == NULL)
             m_warningStr = (char *)"Warning: using max value";
         return( max );
      }
   }
   // else value is in range
   return( value );
}


char *CmdParser::getValueFromKey(const char *key, bool progmem)
{
    bool foundKey = false;
    size_t  i;

    for(i=1; i<m_bufferSize; i++) {

        // find the start of an element
        if (m_buffer[i] != 0 && m_buffer[i-1] == 0x00 && !foundKey) {
            // if key in SRAM
            if (!progmem
                // the key must match AND be immediately followed by the EQ character
                && strncasecmp(reinterpret_cast<char *>(&m_buffer[i]), key, strlen(key)) == 0
                && m_buffer[i+=strlen(key)] == CMDPARSER_CHAR_EQ) {
                   foundKey = true;
            }
            // if key in PROGMEM
            else if (progmem &&
                strncasecmp_P(reinterpret_cast<char *>(&m_buffer[i]), key, strlen(key)) == 0
                && m_buffer[i+=strlen(key)] == CMDPARSER_CHAR_EQ) {
                   foundKey = true;
            }
        }
        // Next element after found key is the value...
        else if( foundKey && m_buffer[i] != 0 ) {
            return reinterpret_cast<char *>(&m_buffer[i]);
        }
    }

    return NULL;
}


// checks string for a leading negative sign
bool CmdParser::negInStr( char *s )
{
   if( s == NULL )
      return false;
   if( s[0] == '-' )  // we are looking for a negative sign as the first charcter
      return true;
   else
      return false;   // negative sign not found
}


// checks for a string containing any valid integer or long value
bool CmdParser::intInStr( char *s )
{
   unsigned int i;
   int digits = 0;

   if( s == NULL )
      return false;

   for( i=0; i<strlen(s); i++) {
      // check for valid charcters
      if( i == 0 ) { // only valid as first character
         if( s[i] == '+' ) continue;
         if( s[i] == '-' ) continue;
      }
      if( s[i] >= '0' && s[i] <= '9' ) {
         digits++;
         continue;
      }
      return false; // exit if any invalid char found
   }
   //Serial.print( "size:" );
   //Serial.print( strlen(s) );
   //Serial.print( " i:" );
   //Serial.print( i );
   //Serial.print( " dig:" );
   //Serial.println( digits );

   if( digits > 0 )
      return true;
   else
      return false;
}


// checks for a string containing any valid floating point value
bool CmdParser::floatInStr( char *s )
{
   unsigned int i = 0;
   int digits = 0;
   int dec_pt = 0;

   if( s == NULL )
      return false;

   // + or - are allowed as first character only
   if( s[i] == '+' || s[i] == '-' )
      i++;

   // check for valid charcters
   for( ; i<strlen(s); i++) {
      //if( i == 0 ) { // only valid as first character
      //   if( s[i] == '+' ) continue;
      //   if( s[i] == '-' ) continue;
      //}
      if( s[i] >= '0' && s[i] <= '9' ) {
         digits++;
         continue;
      }
      if( s[i] == '.' ) {
         dec_pt++;
         continue;
      }
      return false; // exit if any invalid char found
   }
   //Serial.print( "size:" );
   //Serial.print( strlen(s) );
   //Serial.print( " i:" );
   //Serial.print( i );
   //Serial.print( " dig:" );
   //Serial.print( digits );
   //Serial.print( " dec:" );
   //Serial.println( dec_pt );

   if( digits > 0 && dec_pt == 1 )
      return true;
   else
      return false;
}



// checks for a string containing any valid hexidecimal value
bool CmdParser::hexInStr( char *s )
{
   unsigned int i = 0;
   int digits = 0;

   if( s == NULL )
      return false;

   // + or - are allowed as first character only
   if( s[i] == '+' || s[i] == '-' )
      i++;

   // hexidecimal must start with "0x"
   if( s[i++] != '0' )
      return false;
   if( s[i++] != 'x' )
      return false;

   // check for valid hex charcters
   for( ; i<strlen(s); i++) {
      if( s[i] >= '0' && s[i] <= '9' ) {
         digits++;
         continue;
      }
      if( s[i] >= 'a' && s[i] <= 'f' ) {
         digits++;
         continue;
      }
      if( s[i] >= 'A' && s[i] <= 'F' ) {
         digits++;
         continue;
      }
      return false; // exit if any invalid char found
   }
   //Serial.print( "size:" );
   //Serial.print( strlen(s) );
   //Serial.print( " i:" );
   //Serial.print( i );
   //Serial.print( " dig:" );
   //Serial.println( digits );

   if( digits > 0 )
      return true;
   else
      return false;
}

