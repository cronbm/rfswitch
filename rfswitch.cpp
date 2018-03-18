/*
  RFSwitch - Arduino libary for remote control different shutters
  author: Benedikt Michl
  Copyright (c) 2018 Benedikt Michl.  All right reserved.

  Project home: https://github.com/cronbm/rfswitch/

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "RFSwitch.hpp"

#if defined(ESP8266) || defined(ESP32)
    // interrupt handler and related code must be in RAM on ESP8266,
    // according to issue #46.
    #define RECEIVE_ATTR ICACHE_RAM_ATTR
#else
    #define RECEIVE_ATTR
#endif


/* Format for protocol definitions:
 * {pulselength, Sync bit, "0" bit, "1" bit}
 *
 * pulselength: pulse length in microseconds, e.g. 350
 * Sync bit: {1, 31} means 1 high pulse and 31 low pulses
 *     (perceived as a 31*pulselength long pulse, total length of sync bit is
 *     32*pulselength microseconds), i.e:
 *      _
 *     | |_______________________________ (don't count the vertical bars)
 * "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
 *     and 3 low pulses, total length (1+3)*pulselength, i.e:
 *      _
 *     | |___
 * "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
 *      ___
 *     |   |_
 *
 * These are combined to form Tri-State bits when sending or receiving codes.
 */
#if defined(ESP8266) || defined(ESP32)
static const RFSwitch::Protocol proto[] = {
#else
static const RFSwitch::Protocol PROGMEM proto[] = {
#endif
  { 360, { 12, 3 }, { 1, 2 }, { 2, 1 }, false },    // protocol 1, not inverted, rohrmotor24
  { 360, { 12, 3 }, { 1, 2 }, { 2, 1 }, true },    // protocol 2, inverted
};

enum {
   numProto = sizeof(proto) / sizeof(proto[0])
};


RFSwitch::RFSwitch() {
  this->nTransmitterPin = -1;
  this->setProtocol(1);
}

/**
  * Sets the protocol to send.
  */
void RFSwitch::setProtocol(Protocol protocol) {
  this->protocol = protocol;
}

/**
  * Sets the protocol to send, from a list of predefined protocols
  */
void RFSwitch::setProtocol(int nProtocol) {
  if (nProtocol < 1 || nProtocol > numProto) {
    nProtocol = 1;  // TODO: trigger an error, e.g. "bad protocol" ???
  }
#if defined(ESP8266) || defined(ESP32)
  this->protocol = proto[nProtocol-1];
#else
  memcpy_P(&this->protocol, &proto[nProtocol-1], sizeof(Protocol));
#endif
}

/**
  * Sets the protocol to send with pulse length in microseconds.
  */
void RFSwitch::setProtocol(int nProtocol, int nPulseLength) {
  setProtocol(nProtocol);
  this->setPulseLength(nPulseLength);
}


/**
  * Sets pulse length in microseconds
  */
void RFSwitch::setPulseLength(int nPulseLength) {
  this->protocol.pulseLength = nPulseLength;
}

/**
 * Enable transmissions
 *
 * @param nTransmitterPin    Arduino Pin to which the sender is connected to
 */
void RFSwitch::enableTransmit(int nTransmitterPin) {
  this->nTransmitterPin = nTransmitterPin;
  pinMode(this->nTransmitterPin, OUTPUT);
}

/**
  * Disable transmissions
  */
void RFSwitch::disableTransmit() {
  this->nTransmitterPin = -1;
}

/**
 * Transmit the first 'length' bits of the integer 'code'. The
 * bits are sent from MSB to LSB, i.e., first the bit at position length-1,
 * then the bit at position length-2, and so on, till finally the bit at position 0.
 */
void RFSwitch::send(unsigned int repeatTransmit, unsigned long code, unsigned int channel, unsigned int command) {
  if (this->nTransmitterPin == -1)
    return;

  for (int nRepeat = 0; nRepeat < repeatTransmit; nRepeat++) {
    this->transmit(protocol.syncFactor);
    for (int i = 27; i >= 0; i--) {
      if (code & (1L << i))
        this->transmit(protocol.one);
      else
        this->transmit(protocol.zero);
    }
    for (int i = 3; i >= 0; i--) {
      if (channel & (1L << i))
        this->transmit(protocol.one);
      else
        this->transmit(protocol.zero);
    }
    for (int i = 7; i >= 0; i--) {
      if (command & (1L << i))
        this->transmit(protocol.one);
      else
        this->transmit(protocol.zero);
    }
  }
  // Disable transmit after sending (i.e., for inverted protocols)
  digitalWrite(this->nTransmitterPin, LOW);
}

/**
 * Transmit a single high-low pulse.
 */
void RFSwitch::transmit(HighLow pulses) {
  uint8_t firstLogicLevel = (this->protocol.invertedSignal) ? LOW : HIGH;
  uint8_t secondLogicLevel = (this->protocol.invertedSignal) ? HIGH : LOW;

  digitalWrite(this->nTransmitterPin, firstLogicLevel);
  delayMicroseconds( this->protocol.pulseLength * pulses.high);
  digitalWrite(this->nTransmitterPin, secondLogicLevel);
  delayMicroseconds( this->protocol.pulseLength * pulses.low);
}
