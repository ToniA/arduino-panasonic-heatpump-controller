// Simple web form based application to control a
// Panasonic DKE-series heat pump
//
// Connect an IR led (with 1k resistor in series)
// between GND and digital pin 3 (Mega 2560 pin 9)
//
// Browse to http://192.168.1.210 (or whatever you set as the address)

// Requires the Ethernet shield and Webduino:
// https://github.com/sirleech/Webduino


#include <Arduino.h>
#include "SPI.h"
#include "avr/pgmspace.h"
#include "Ethernet.h"
#include "WebServer.h"



// Infrared LED on PIN digital 9
// Connect with 1 kOhm resistor in series to GND
#define IR_LED_PIN        9


// Panasonic DKE timing constants
#define PANASONIC_AIRCON2_HDR_MARK   3500
#define PANASONIC_AIRCON2_HDR_SPACE  1800
#define PANASONIC_AIRCON2_BIT_MARK   420
#define PANASONIC_AIRCON2_ONE_SPACE  1350
#define PANASONIC_AIRCON2_ZERO_SPACE 470
#define PANASONIC_AIRCON2_MSG_SPACE  10000

// Panasonic DKE codes
#define PANASONIC_AIRCON2_MODE_AUTO  0x00 // Operating mode
#define PANASONIC_AIRCON2_MODE_HEAT  0x40
#define PANASONIC_AIRCON2_MODE_COOL  0x30
#define PANASONIC_AIRCON2_MODE_DRY   0x20
#define PANASONIC_AIRCON2_MODE_FAN   0x60
#define PANASONIC_AIRCON2_MODE_MASK  0x70
#define PANASONIC_AIRCON2_MODE_OFF   0x00 // Power OFF
#define PANASONIC_AIRCON2_MODE_ON    0x01
#define PANASONIC_AIRCON2_TIMER_CNL  0x08
#define PANASONIC_AIRCON2_FAN_AUTO   0xA0 // Fan speed
#define PANASONIC_AIRCON2_FAN1       0x30
#define PANASONIC_AIRCON2_FAN2       0x40
#define PANASONIC_AIRCON2_FAN3       0x50
#define PANASONIC_AIRCON2_FAN4       0x60
#define PANASONIC_AIRCON2_FAN5       0x70
#define PANASONIC_AIRCON2_VS_AUTO    0x0F // Vertical swing
#define PANASONIC_AIRCON2_VS_UP      0x01
#define PANASONIC_AIRCON2_VS_MUP     0x02
#define PANASONIC_AIRCON2_VS_MIDDLE  0x03
#define PANASONIC_AIRCON2_VS_MDOWN   0x04
#define PANASONIC_AIRCON2_VS_DOWN    0x05
#define PANASONIC_AIRCON2_HS_AUTO    0x0D // Horizontal swing
#define PANASONIC_AIRCON2_HS_MIDDLE  0x06
#define PANASONIC_AIRCON2_HS_LEFT    0x09
#define PANASONIC_AIRCON2_HS_MLEFT   0x0A
#define PANASONIC_AIRCON2_HS_MRIGHT  0x0B
#define PANASONIC_AIRCON2_HS_RIGHT   0x0C

// flash-based web pages to save precious RAM

P(indexpage) = "<!DOCTYPE html>\n"
"<html>\n"
"<body>\n"
"\n"
"\n"
"<table border=\"1\">\n"
"<tr><td>\n"
"PANASONIC DKE\n"
"<form name=\"input\" action=\"send_ir_dke.html\" method=\"get\">\n"
"<table border=\"1\">\n"
"<tr><td>\n"
"<input type=\"radio\" name=\"power\" value=\"1\" checked>ON\n"
"<input type=\"radio\" name=\"power\" value=\"0\">OFF\n"
"</td><td>\n"
"Power state\n"
"</td></tr>\n"
"\n"
"<tr><td>\n"
"<select name=\"mode\">\n"
"  <option value=\"1\">AUTO</option>\n"
"  <option value=\"2\" selected=\"selected\">HEAT</option>\n"
"  <option value=\"3\">COOL</option>\n"
"  <option value=\"4\">DRY</option>\n"
"  <option value=\"5\">FAN</option>\n"
"</select> \n"
"</td><td>\n"
"Mode\n"
"</td></tr>\n"
"\n"
"<tr><td>\n"
"<select name=\"fan\">\n"
" <option value=\"1\" selected=\"selected\">AUTO</option>\n"
" <option value=\"2\">1</option>\n"
" <option value=\"3\">2</option>\n"
" <option value=\"4\">3</option>\n"
" <option value=\"5\">4</option>\n"
" <option value=\"6\">5</option>\n"
"</select> \n"
"</td><td>\n"
"Fan speed\n"
"</td></tr>\n"
"\n"
"<tr><td>\n"
"<select name=\"temperature\">\n"
"  <option value=\"8\">8 (HEAT only)</option>\n"
"  <option value=\"10\">10 (HEAT only)</option>\n"
"  <option value=\"16\">16</option>\n"
"  <option value=\"17\">17</option>\n"
"  <option value=\"18\">18</option>\n"
"  <option value=\"19\">19</option>\n"
"  <option value=\"20\">20</option>\n"
"  <option value=\"21\" selected=\"selected\">21</option>\n"
"  <option value=\"22\">22</option>\n"
"  <option value=\"23\">23</option>\n"
"  <option value=\"24\">24</option>\n"
"  <option value=\"25\">25</option>\n"
"  <option value=\"26\">26</option>\n"
"  <option value=\"27\">27</option>\n"
"  <option value=\"28\">28</option>\n"
"  <option value=\"29\">29</option>\n"
"  <option value=\"30\">30</option>\n"
"</select> \n"
"</td><td>\n"
"Temperature\n"
"</td></tr>\n"
"\n"
"<tr><td>\n"
"<select name=\"vswing\">\n"
"  <option value=\"1\" selected=\"selected\">AUTO</option>\n"
"  <option value=\"2\" >UP</option>\n"
"  <option value=\"3\" >MIDDLE UP</option>\n"
"  <option value=\"4\" >MIDDLE</option>\n"
"  <option value=\"5\" >MIDDLE DOWN</option>\n"
"  <option value=\"6\" >DOWN</option>\n"
"</select>\n"
"</td><td>\n"
"Vertical swing\n"
"</td></tr>\n"
"\n"
"<tr><td>\n"
"<select name=\"hswing\">\n"
"  <option value=\"1\" selected=\"selected\">AUTO</option>\n"
"  <option value=\"2\" >MIDDLE</option>\n"
"  <option value=\"3\" >LEFT</option>\n"
"  <option value=\"4\" >LEFT MIDDLE</option>\n"
"  <option value=\"5\" >RIGHT</option>\n"
"  <option value=\"6\" >RIGHT MIDDLE</option>\n"
"</select>\n"
"</td><td>\n"
"Horizontal swing\n"
"</td></tr>\n"
"\n"
"</td></tr>\n"
"</table>\n"
"<input type=\"submit\" value=\"Submit DKE\">\n"
"</form>\n"
"</td></tr>\n"
"\n"
"</table>\n"
"\n"
"</body>\n"
"</html>\n";

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


// Send the Panasonic DKE raw code

void sendPanasonicDKEraw(byte operatingMode, byte fanSpeed, byte temperature, byte swingV, byte swingH)
{
  byte DKE_template[] = { 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x48, 0x2E, 0x80, 0xA3, 0x0D, 0x00, 0x0E, 0xE0, 0x00, 0x00, 0x89, 0x00, 0x00, 0xA2 };
  byte checksum = 0xF4;

  DKE_template[13] = operatingMode;
  DKE_template[14] = temperature << 1;
  DKE_template[16] = fanSpeed | swingV;
  DKE_template[17] = swingH;

  // Checksum

  for (int i=0; i<26; i++) {
    checksum += DKE_template[i];
  }

  DKE_template[26] = checksum;

  // 36 kHz PWM frequency
  enableIROut(36);

  // Header
  mark(PANASONIC_AIRCON2_HDR_MARK);
  space(PANASONIC_AIRCON2_HDR_SPACE);

  // First 8 bytes
  for (int i=0; i<8; i++) {
    sendIRByte(DKE_template[i], PANASONIC_AIRCON2_BIT_MARK, PANASONIC_AIRCON2_ZERO_SPACE, PANASONIC_AIRCON2_ONE_SPACE);
  }

  // Pause
  mark(PANASONIC_AIRCON2_BIT_MARK);
  space(PANASONIC_AIRCON2_MSG_SPACE);

  // Header
  mark(PANASONIC_AIRCON2_HDR_MARK);
  space(PANASONIC_AIRCON2_HDR_SPACE);

  // Last 19 bytes
  for (int i=8; i<27; i++) {
    sendIRByte(DKE_template[i], PANASONIC_AIRCON2_BIT_MARK, PANASONIC_AIRCON2_ZERO_SPACE, PANASONIC_AIRCON2_ONE_SPACE);
  }

  mark(PANASONIC_AIRCON2_BIT_MARK);
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
  (TCCR2A |= _BV(COM2B1)); // Enable pin 9 PWM output
  delayMicroseconds(time);
}

void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  (TCCR2A &= ~(_BV(COM2B1))); // Disable pin 9 PWM output
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



// The send_ir_dke.html. This handles the parameters submitted by the form

void dkeFormCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];
  int param = 0;

  // Sensible defaults for the heat pump mode

  byte operatingMode = PANASONIC_AIRCON2_TIMER_CNL;
  byte fanSpeed      = PANASONIC_AIRCON2_FAN_AUTO;
  byte temperature   = 21;
  byte swingV        = PANASONIC_AIRCON2_VS_AUTO;
  byte swingH        = PANASONIC_AIRCON2_HS_AUTO;

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
            param = atoi(value);

            switch (param)
            {
              case 1:
                operatingMode |= PANASONIC_AIRCON2_MODE_ON;
            }
          }
          else if (strcmp(name, "mode") == 0 )
          {
            param = atoi(value);

            switch (param)
            {
              case 1:
                operatingMode |= PANASONIC_AIRCON2_MODE_AUTO;
                break;
              case 2:
                operatingMode |= PANASONIC_AIRCON2_MODE_HEAT;
                break;
              case 3:
                operatingMode |= PANASONIC_AIRCON2_MODE_COOL;
                break;
              case 4:
                operatingMode |= PANASONIC_AIRCON2_MODE_DRY;
                break;
              case 5:
                operatingMode |= PANASONIC_AIRCON2_MODE_FAN;
                temperature = 27; // Temperature is always 27 in FAN mode
                break;
            }
          }
          else if (strcmp(name, "fan") == 0 )
          {
            param = atoi(value);

            switch (param)
            {
              case 1:
                fanSpeed = PANASONIC_AIRCON2_FAN_AUTO;
                break;
              case 2:
                fanSpeed = PANASONIC_AIRCON2_FAN1;
                break;
              case 3:
                fanSpeed = PANASONIC_AIRCON2_FAN2;
                break;
              case 4:
                fanSpeed = PANASONIC_AIRCON2_FAN3;
                break;
              case 5:
                fanSpeed = PANASONIC_AIRCON2_FAN4;
                break;
              case 6:
                fanSpeed = PANASONIC_AIRCON2_FAN5;
                break;
            }
          }

          else if (strcmp(name, "temperature") == 0 )
          {
            param = atoi(value);
            if ((param >= 15 && param <= 31 || param == 8 || param == 10) && temperature == 21)
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
                swingV = PANASONIC_AIRCON2_VS_AUTO;
                break;
              case 2:
                swingV = PANASONIC_AIRCON2_VS_UP;
                break;
              case 3:
                swingV = PANASONIC_AIRCON2_VS_MUP;
                break;
              case 4:
                swingV = PANASONIC_AIRCON2_VS_MIDDLE;
                break;
              case 5:
                swingV = PANASONIC_AIRCON2_VS_MDOWN;
                break;
              case 6:
                swingV = PANASONIC_AIRCON2_VS_DOWN;
                break;
            }
          }
          else if (strcmp(name, "hswing") == 0 )
          {
            param = atoi(value);

            switch (param)
            {
              case 1:
                swingH = PANASONIC_AIRCON2_HS_AUTO;
                break;
              case 2:
                swingH = PANASONIC_AIRCON2_HS_MIDDLE;
                break;
              case 3:
                swingH = PANASONIC_AIRCON2_HS_LEFT;
                break;
              case 4:
                swingH = PANASONIC_AIRCON2_HS_MLEFT;
                break;
              case 5:
                swingH = PANASONIC_AIRCON2_HS_RIGHT;
                break;
              case 6:
                swingH = PANASONIC_AIRCON2_HS_MRIGHT;
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

  if (temperature == 8 || temperature == 10)
  {
    if ((operatingMode & PANASONIC_AIRCON2_MODE_MASK) == PANASONIC_AIRCON2_MODE_HEAT)
    {
      fanSpeed = PANASONIC_AIRCON2_FAN5;
    }
    else
    {
      temperature = 21;
    }
  }

  sendPanasonicDKEraw(operatingMode, fanSpeed, temperature, swingV, swingH);
}

// The setup

void setup()
{
  // MAC address of the Arduino, this needs to be unique within
  // your home network
  static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

  // IP address of the Arduino. Make sure this is outside of the DHCP pool
  static uint8_t ip[] = { 192, 168, 1, 210 };

  // initialize the Ethernet adapter
  Ethernet.begin(mac, ip);

  // Setup the default and the index page
  webserver.setDefaultCommand(&indexCmd);
  webserver.addCommand("index.html", &indexCmd);

  // The form handlers
  webserver.addCommand("send_ir_dke.html", &dkeFormCmd);

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
