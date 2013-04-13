// Simple web form based application to control a 
// Panasonic CKP-series heat pump
//
// Connect an IR led (with 1k resistor in series)
// between GND and digital pin 3
//
// Browse to http://192.168.0.210 (or whatever you set as the address)

// Requires the Ethernet shield and Webduino:
// https://github.com/sirleech/Webduino


#include <Arduino.h>
#include "SPI.h"
#include "avr/pgmspace.h"
#include "Ethernet.h"
#include "WebServer.h"


// Infrared LED on PIN digital 3
// Connect with 1 kOhm resistor in series to GND
#define IR_LED_PIN        3


// Panasonic CKP timing constants
#define PANASONIC_AIRCON1_HDR_MARK   3400
#define PANASONIC_AIRCON1_HDR_SPACE  3500
#define PANASONIC_AIRCON1_BIT_MARK   800
#define PANASONIC_AIRCON1_ONE_SPACE  2700
#define PANASONIC_AIRCON1_ZERO_SPACE 1000
#define PANASONIC_AIRCON1_MSG_SPACE  14000

// Panasonic CKP codes
#define PANASONIC_AIRCON1_MODE_AUTO  0x06 // Operating mode
#define PANASONIC_AIRCON1_MODE_HEAT  0x04
#define PANASONIC_AIRCON1_MODE_COOL  0x02
#define PANASONIC_AIRCON1_MODE_DRY   0x03
#define PANASONIC_AIRCON1_MODE_FAN   0x01
#define PANASONIC_AIRCON1_MODE_ONOFF 0x00 // Toggle ON/OFF
#define PANASONIC_AIRCON1_MODE_KEEP  0x08 // Do not toggle ON/OFF
#define PANASONIC_AIRCON1_FAN_AUTO   0xF0 // Fan speed
#define PANASONIC_AIRCON1_FAN1       0x20
#define PANASONIC_AIRCON1_FAN2       0x30
#define PANASONIC_AIRCON1_FAN3       0x40
#define PANASONIC_AIRCON1_FAN4       0x50
#define PANASONIC_AIRCON1_FAN5       0x60
#define PANASONIC_AIRCON1_VS_AUTO    0xF0 // Vertical swing
#define PANASONIC_AIRCON1_VS_UP      0x90
#define PANASONIC_AIRCON1_VS_MUP     0xA0
#define PANASONIC_AIRCON1_VS_MIDDLE  0xB0
#define PANASONIC_AIRCON1_VS_MDOWN   0xC0
#define PANASONIC_AIRCON1_VS_DOWN    0xD0
#define PANASONIC_AIRCON1_HS_AUTO    0x08 // Horizontal swing
#define PANASONIC_AIRCON1_HS_MANUAL  0x00

// flash-based web pages to save precious RAM

P(indexpage) = "<!DOCTYPE html>"
"<html>"
"<body>"
""
"<form name=\"input\" action=\"send_ir.html\" method=\"get\">"
""
"<table>"
"<tr><td>"
"<input type=\"checkbox\" name=\"power\" value=\"power\">"
"</td><td>"
"Switch power state"
"</td></tr>"
""
"<tr><td>"
"<select name=\"mode\">"
"  <option value=\"1\">AUTO</option>"
"  <option value=\"2\" selected=\"selected\">HEAT</option>"
"  <option value=\"3\">COOL</option>"
"  <option value=\"4\">DRY</option>"
"  <option value=\"5\">FAN</option>"
"</select> "
"</td><td>"
"Mode"
"</td></tr>"
""
"<tr><td>"
"<select name=\"fan\">"
"  <option value=\"1\" selected=\"selected\">AUTO</option>"
"  <option value=\"2\">1</option>"
"  <option value=\"3\">2</option>"
"  <option value=\"4\">3</option>"
"  <option value=\"5\">4</option>"
"  <option value=\"6\">5</option>"
"</select> "
"</td><td>"
"Fan speed"
"</td></tr>"
""
"<tr><td>"
"<select name=\"temperature\">"
"  <option value=\"16\">16</option>"
"  <option value=\"17\">17</option>"
"  <option value=\"18\">18</option>"
"  <option value=\"19\">19</option>"
"  <option value=\"20\">20</option>"
"  <option value=\"21\">21</option>"
"  <option value=\"22\">22</option>"
"  <option value=\"23\" selected=\"selected\">23</option>"
"  <option value=\"24\">24</option>"
"  <option value=\"25\">25</option>"
"  <option value=\"26\">26</option>"
"  <option value=\"27\">27</option>"
"  <option value=\"28\">28</option>"
"  <option value=\"29\">29</option>"
"  <option value=\"30\">30</option>"
"</select> "
"</td><td>"
"Temperature"
"</td></tr>"
""
"<tr><td>"
"<select name=\"vswing\">"
"  <option value=\"1\" >AUTO</option>"
"  <option value=\"2\" selected=\"selected\">UP</option>"
"  <option value=\"3\" >MIDDLE UP</option>"
"  <option value=\"4\" >MIDDLE</option>"
"  <option value=\"5\" >MIDDLE DOWN</option>"
"  <option value=\"6\" >DOWN</option>"
"</select>"
"</td><td>"
"Vertical swing"
"</td></tr>"
""
"<tr><td>"
"<select name=\"hswing\">"
"  <option value=\"1\" >AUTO</option>"
"  <option value=\"2\" selected=\"selected\">MANUAL</option>"
"</select>"
"</td><td>"
"Horizontal swing"
"</td></tr>"
""
"</table>"
""
"<br>"
"<input type=\"submit\" value=\"Submit\">"
"</form> "
""
"</body>"
"</html>";

P(formactionpage_header) = "<!DOCTYPE html>"
"<html>"
"<body>"
"<h1>Parameters</h1>";

P(formactionpage_footer) = ""
"</body>"
"</html>";

// Lenght parameters
// * name of parameter
// * value of parameter
// * overall lenght of the 'GET' type URL

#define NAMELEN 32
#define VALUELEN 32
#define URLLEN 128

// Webserver instance initialization on port 80

WebServer webserver("",80);

// Send the Panasonic CKP code

void sendPanasonicCKP(byte operatingMode, byte fanSpeed, byte temperature, byte swingV, byte swingH)
{
  byte sendBuffer[4];
  
  // Fan speed & temperature, temperature needs to be 27 in FAN mode
  if (operatingMode == PANASONIC_AIRCON1_MODE_FAN || operatingMode == (PANASONIC_AIRCON1_MODE_FAN | PANASONIC_AIRCON1_MODE_KEEP ))
  {
    temperature = 27;
  }

  sendBuffer[0] = fanSpeed | (temperature - 15);

  // Power toggle & operation mode
  sendBuffer[1] = operatingMode;

  // Swings
  sendBuffer[2] = swingV | swingH;

  // Always 0x36
  sendBuffer[3]  = 0x36;

  // Send the code
  sendPanasonicCKP(sendBuffer);
}

// Send the Panasonic raw code

void sendPanasonicCKP(byte sendBuffer[])
{
  // 40 kHz PWM frequency
  enableIROut(40);

  // Header, two first bytes repeated
  mark(PANASONIC_AIRCON1_HDR_MARK);
  space(PANASONIC_AIRCON1_HDR_SPACE);

  for (int i=0; i<2; i++) {
    sendIRByte(sendBuffer[0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);
  }

  // Pause

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(PANASONIC_AIRCON1_MSG_SPACE);

  // Header, two last bytes repeated

  mark(PANASONIC_AIRCON1_HDR_MARK);
  space(PANASONIC_AIRCON1_HDR_SPACE);

  for (int i=0; i<2; i++) {
    sendIRByte(sendBuffer[2], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[2], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[3], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[3], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);
  }

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(0);
}

// Send a byte over IR

void sendIRByte(byte sendByte, int bitMarkLength, int zeroSpaceLength, int oneSpaceLength)
{
  for (int i=0; i<8 ; i++)
  {
    if (sendByte & 0x01)
    {
      mark(bitMarkLength);
      space(oneSpaceLength);
    }
    else
    {
      mark(bitMarkLength);
      space(zeroSpaceLength);
    }
    
    sendByte >>= 1;
  }
}

// 'mark', 'space' and 'enableIROut' have been taken
// from Ken Shirriff's IRRemote library:
// https://github.com/shirriff/Arduino-IRremote

void mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  (TCCR2A |= _BV(COM2B1)); // Enable pin 3 PWM output
  delayMicroseconds(time);
}

void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  (TCCR2A &= ~(_BV(COM2B1))); // Disable pin 3 PWM output
  delayMicroseconds(time);
}

void enableIROut(int khz) {
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW); // When not sending PWM, we want it low

  const uint8_t pwmval = F_CPU / 2000 / (khz);
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
  OCR2A = pwmval;
  OCR2B = pwmval / 3;
}

// The index.html web page. This presents the HTML form

void indexCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  server.httpSuccess();
  server.printP(indexpage);
}


// The send_ir.html. This handles the parameters submitted by the form

void formCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];
  int param = 0;

  // Sensible defaults for the heat pump mode

  byte operatingMode = PANASONIC_AIRCON1_MODE_HEAT | PANASONIC_AIRCON1_MODE_KEEP;
  byte fanSpeed      = PANASONIC_AIRCON1_FAN_AUTO;
  byte temperature   = 23;
  byte swingV        = PANASONIC_AIRCON1_VS_UP;
  byte swingH        = PANASONIC_AIRCON1_HS_AUTO;

  server.httpSuccess();
  server.printP(formactionpage_header);

  if (strlen(url_tail))
    {
    while (strlen(url_tail))
      {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc != URLPARAM_EOS)
        {
          if (strcmp(name, "power") == 0 )
          {
            operatingMode ^= PANASONIC_AIRCON1_MODE_KEEP;
          }
          else if (strcmp(name, "mode") == 0 )
          {
            operatingMode &= PANASONIC_AIRCON1_MODE_KEEP;
            param = atoi(value);
            
            switch (param)
            {
              case 1:
                operatingMode |= PANASONIC_AIRCON1_MODE_AUTO;
                break;
              case 2:
                operatingMode |= PANASONIC_AIRCON1_MODE_HEAT;
                break;
              case 3:
                operatingMode |= PANASONIC_AIRCON1_MODE_COOL;
                break;
              case 4:
                operatingMode |= PANASONIC_AIRCON1_MODE_DRY;
                break;
              case 5:
                operatingMode |= PANASONIC_AIRCON1_MODE_FAN;
                break;
            }
          }
          else if (strcmp(name, "fan") == 0 )
          {
            param = atoi(value);
            
            switch (param)
            {
              case 1:
                fanSpeed = PANASONIC_AIRCON1_FAN_AUTO;
                break;
              case 2:
                fanSpeed = PANASONIC_AIRCON1_FAN1;
                break;
              case 3:
                fanSpeed = PANASONIC_AIRCON1_FAN2;
                break;
              case 4:
                fanSpeed = PANASONIC_AIRCON1_FAN3;
                break;
              case 5:
                fanSpeed = PANASONIC_AIRCON1_FAN4;
                break;
              case 6:
                fanSpeed = PANASONIC_AIRCON1_FAN5;
                break;
            }
          }
          else if (strcmp(name, "temperature") == 0 )
          {
            param = atoi(value);
            if ( param >= 15 && param <= 31)
            {
              temperature = param;
            }
          }
          else if (strcmp(name, "vswing") == 0 )
          {
            param = atoi(value);
        
            switch (param)
            {
              case 1:
                swingV = PANASONIC_AIRCON1_VS_AUTO;
                break;
              case 2:
                swingV = PANASONIC_AIRCON1_VS_UP;
                break;
              case 3:
                swingV = PANASONIC_AIRCON1_VS_MUP;
                break;
              case 4:
                swingV = PANASONIC_AIRCON1_VS_MIDDLE;
                break;
              case 5:
                swingV = PANASONIC_AIRCON1_VS_MDOWN;
                break;
              case 6:
                swingV = PANASONIC_AIRCON1_VS_DOWN;
                break;
            }
          }
          else if (strcmp(name, "hswing") == 0 )
          {
            param = atoi(value);

            switch (param)
            {
              case 1:
                swingH = PANASONIC_AIRCON1_HS_AUTO;
                break;
              case 2:
                swingH = PANASONIC_AIRCON1_HS_MANUAL;
                break;
            }
          }          
        server.print(name);
        server.print(" = ");
        server.print(param);
        server.print("<br>");
        }
      }
    }

  server.printP(formactionpage_footer);
  
  sendPanasonicCKP(operatingMode, fanSpeed, temperature, swingV, swingH);
}

// The setup

void setup()
{
  // MAC address of the Arduino, this needs to be unique within 
  // your home network
  static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

  // IP address of the Arduino. Make sure this is outside of the DHCP pool
  static uint8_t ip[] = { 192, 168, 0, 210 };

  // initialize the Ethernet adapter
  Ethernet.begin(mac, ip);

  // Setup the default and the index page
  webserver.setDefaultCommand(&indexCmd);
  webserver.addCommand("index.html", &indexCmd);

  // The form handler
  webserver.addCommand("send_ir.html", &formCmd);

  // start the webserver
  webserver.begin();
}

// The loop

void loop()
{
  // URL buffer for GET parameters
  char buff[URLLEN];
  int len = URLLEN;

  // process incoming connections one at a time forever
  webserver.processConnection(buff, &len);
}
