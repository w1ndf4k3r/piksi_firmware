/*
 * Copyright (C) 2012 Fergus Noble <fergusnoble@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SWIFTNAV_SETTINGS_H
#define SWIFTNAV_SETTINGS_H

#include "settings.h"
#include "hw/usart.h"

settings_t settings __attribute__ ((section (".settings_area"))) =
/* Default settings: */
{
  .settings_valid = VALID,

  .ftdi_usart = {
    .mode = PIKSI_BINARY,
    .baud_rate = USART_DEFAULT_BAUD,
    .message_mask = 0xFF,
  },
  .uarta_usart = {
    .mode = PIKSI_BINARY,
    .baud_rate = USART_DEFAULT_BAUD,
    .message_mask = 0xFF,
  },
  .uartb_usart = {
    .mode = NMEA,
    .baud_rate = 115200,
  },
};

#endif /* SWIFTNAV_SETTINGS_H */
