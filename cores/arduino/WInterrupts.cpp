/**
 * Copyright (c) 2011-2012 Arduino.  All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <LPC17xx.h>
#include <bit_manipulation.h>

#include <Arduino.h>

#define GNUM 31

typedef void (*interruptCB)(void);

static interruptCB callbacksP0[GNUM] = {};
static interruptCB callbacksP2[GNUM] = {};

extern "C" void GpioEnableInt(const uint32_t port, const uint32_t pin, const uint32_t mode);
extern "C" void GpioDisableInt(const uint32_t port, const uint32_t pin);


static void __initialize() {
  NVIC_SetPriority(EINT3_IRQn, NVIC_EncodePriority(0, 1, 0));
  NVIC_EnableIRQ(EINT3_IRQn);
}

void attachInterrupt(const pin_t pin, void (*callback)(void), uint32_t mode) {
  static int enabled = 0;

  if (!LPC176x::gpio_interrupt_capable(pin)) return;

  if (!enabled) {
    __initialize();
    ++enabled;
  }

  uint8_t myport = LPC176x::pin_port(pin),
          mypin = LPC176x::pin_bit(pin);

  if (myport == 0)
    callbacksP0[mypin] = callback;
  else
    callbacksP2[mypin] = callback;

  // Enable interrupt
  GpioEnableInt(myport,mypin,mode);
}

void detachInterrupt(const pin_t pin) {
  if (!LPC176x::gpio_interrupt_capable(pin)) return;

  const uint8_t myport = LPC176x::pin_port(pin),
                mypin = LPC176x::pin_bit(pin);

  // Disable interrupt
  GpioDisableInt(myport, mypin);

  // unset callback
  if (myport == 0)
    callbacksP0[mypin] = 0;
  else //if (myport == 2 )
    callbacksP2[mypin] = 0;
}


extern "C" void GpioEnableInt(uint32_t port, uint32_t pin, uint32_t mode) {
  //pin here is the processor pin, not logical pin
  if (port == 0) {
    LPC_GPIOINT->IO0IntClr = LPC176x::util::bit_value(pin);
    if (mode == RISING) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO0IntEnR, pin);
      LPC176x::util::bit_clear(LPC_GPIOINT->IO0IntEnF, pin);
    }
    else if (mode == FALLING) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO0IntEnF, pin);
      LPC176x::util::bit_clear(LPC_GPIOINT->IO0IntEnR, pin);
    }
    else if (mode == CHANGE) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO0IntEnR, pin);
      LPC176x::util::bit_set(LPC_GPIOINT->IO0IntEnF, pin);
    }
  }
  else {
    LPC_GPIOINT->IO2IntClr = LPC176x::util::bit_value(pin);
    if (mode == RISING) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO2IntEnR, pin);
      LPC176x::util::bit_clear(LPC_GPIOINT->IO2IntEnF, pin);
    }
    else if (mode == FALLING) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO2IntEnF, pin);
      LPC176x::util::bit_clear(LPC_GPIOINT->IO2IntEnR, pin);
    }
    else if (mode == CHANGE) {
      LPC176x::util::bit_set(LPC_GPIOINT->IO2IntEnR, pin);
      LPC176x::util::bit_set(LPC_GPIOINT->IO2IntEnF, pin);
    }
  }
}

extern "C" void GpioDisableInt(const uint32_t port, const uint32_t pin) {
  if (port == 0) {
    LPC176x::util::bit_clear(LPC_GPIOINT->IO0IntEnR, pin);
    LPC176x::util::bit_clear(LPC_GPIOINT->IO0IntEnF, pin);
    LPC_GPIOINT->IO0IntClr = LPC176x::util::bit_value(pin);
  }
  else {
    LPC176x::util::bit_clear(LPC_GPIOINT->IO2IntEnR, pin);
    LPC176x::util::bit_clear(LPC_GPIOINT->IO2IntEnF, pin);
    LPC_GPIOINT->IO2IntClr = LPC176x::util::bit_value(pin);
  }
}

extern "C" __attribute__ ((weak)) void EINT3_IRQHandler(void) {
  // Read in all current interrupt registers. We do this once as the
  // GPIO interrupt registers are on the APB bus, and this is slow.
  uint32_t rise0 = LPC_GPIOINT->IO0IntStatR,
           fall0 = LPC_GPIOINT->IO0IntStatF,
           rise2 = LPC_GPIOINT->IO2IntStatR,
           fall2 = LPC_GPIOINT->IO2IntStatF;

  // Clear the interrupts ASAP
  LPC_GPIOINT->IO0IntClr = LPC_GPIOINT->IO2IntClr = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(EINT3_IRQn);

  while (rise0 > 0) {                                       // If multiple pins changes happened continue as long as there are interrupts pending
    const uint8_t bitloc = 31 - __CLZ(rise0);               // CLZ returns number of leading zeros, 31 minus that is location of first pending interrupt
    if (callbacksP0[bitloc] != NULL) callbacksP0[bitloc]();
    rise0 -= LPC176x::util::bit_value(bitloc);
  }

  while (fall0 > 0) {
    const uint8_t bitloc = 31 - __CLZ(fall0);
    if (callbacksP0[bitloc] != NULL) callbacksP0[bitloc]();
    fall0 -= LPC176x::util::bit_value(bitloc);
  }

  while(rise2 > 0) {
    const uint8_t bitloc = 31 - __CLZ(rise2);
    if (callbacksP2[bitloc] != NULL) callbacksP2[bitloc]();
    rise2 -= LPC176x::util::bit_value(bitloc);
  }

  while (fall2 > 0) {
    const uint8_t bitloc = 31 - __CLZ(fall2);
    if (callbacksP2[bitloc] != NULL) callbacksP2[bitloc]();
    fall2 -= LPC176x::util::bit_value(bitloc);
  }
}
