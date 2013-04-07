#include <Arduino.h>
#include <OneButton.h>

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

// Button pin, pushbutton between digital 12 and GND
OneButton button(12, true);

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
  
  sendPanasonicCKPRaw(sendBuffer);
}

void sendPanasonicCKPRaw(byte sendBuffer[])
{
  // 40 kHz PWM frequency
  enableIROut(40);

  // Header, two first bytes repeated
  mark(PANASONIC_AIRCON1_HDR_MARK);
  space(PANASONIC_AIRCON1_HDR_SPACE);

  for (int i=0; i<2; i++) {
    sendPanasonicCKPRawByte(sendBuffer[0]);
    sendPanasonicCKPRawByte(sendBuffer[0]);
    sendPanasonicCKPRawByte(sendBuffer[1]);
    sendPanasonicCKPRawByte(sendBuffer[1]);

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
    sendPanasonicCKPRawByte(sendBuffer[2]);
    sendPanasonicCKPRawByte(sendBuffer[2]);
    sendPanasonicCKPRawByte(sendBuffer[3]);
    sendPanasonicCKPRawByte(sendBuffer[3]);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);
  }

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(0);
}

void sendPanasonicCKPRawByte(byte sendByte)
{
  for (int i=0; i<8 ; i++)
  {
    if (sendByte & 0x01)
    {
      mark(PANASONIC_AIRCON1_BIT_MARK);
      space(PANASONIC_AIRCON1_ONE_SPACE);
    }
    else
    {
      mark(PANASONIC_AIRCON1_BIT_MARK);
      space(PANASONIC_AIRCON1_ZERO_SPACE);
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


// Initialize the serial port to 9600 bps and the pushbutton to run 'sendIR'
void setup()
{
  Serial.begin(9600);
  button.attachClick(sendIR);
}

// Just poll the pushbutton in the main loop
void loop()
{
  button.tick();
}

// Send the IR code when the button is clicked
void sendIR()
{
  Serial.println("Sending Panasonic CKP IR code...");
  
  // See also the Netduino application to record codes:
  // https://github.com/ToniA/netduino-ir-receiver

  sendPanasonicCKP(PANASONIC_AIRCON1_MODE_HEAT | PANASONIC_AIRCON1_MODE_ONOFF, PANASONIC_AIRCON1_FAN_AUTO, 23, PANASONIC_AIRCON1_VS_UP, PANASONIC_AIRCON1_HS_AUTO);
}
