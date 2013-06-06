/*
 * Copyright (C) 2011 Fergus Noble <fergusnoble@gmail.com>
 * Copyright (C) 2013 Colin Beighley <colinbeighley@gmail.com>
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

#include "track_channel.h"
#include "nap_common.h"
#include "conf.h"

#include "libswiftnav/prns.h"

/*
 * Actual number of track channels NAP configuration was built with - read from
 * configuration flash at runtime with nap_setup().
 */
u8 TRACK_N_CHANNELS;

/* Pack data for writing to a NAP track channel's INIT register.
 *
 * \param pack          Array of u8 to pack data into.
 * \param prn           CA code PRN number (0-31) to track. (deprecated)
 * \param carrier_phase Initial carrier phase.
 * \param code_phase    Initial code phase.
 */
void track_pack_init(u8 pack[], u8 prn, s32 carrier_phase, u16 code_phase)
{
  pack[0] = ((code_phase << 5) >> 16) & 0x07;
  pack[1] = (code_phase << 5) >> 8;
  pack[2] = (((carrier_phase << 5) >> 24) & 0x1F) | (code_phase << 5);
  pack[3] = (carrier_phase << 5) >> 16;
  pack[4] = (carrier_phase << 5) >> 8;
  pack[5] = (prn & 0x1F) | (carrier_phase << 5 & 0xE0);
}

/** Write to a NAP track channel's INIT register.
 * Sets PRN (deprecated), initial carrier phase, and initial code phase of a
 * NAP track channel. The tracking channel will start correlating with these
 * parameters at the falling edge of the next NAP internal timing strobe.
 * NOTE: The track channel's UPDATE register, which sets the carrier and
 * code phase rates, must also be written to before the internal timing strobe
 * goes low.
 *
 * \param channel       NAP track channel whose INIT register to write.
 * \param prn           CA code PRN (0-31) to track. (deprecated)
 * \param carrier_phase Initial code phase.
 * \param code_phase    Initial carrier phase.
 */
void track_write_init_blocking(u8 channel, u8 prn, s32 carrier_phase, u16 code_phase)
{
  u8 temp[6] = {0, 0, 0, 0, 0, 0};
  track_pack_init(temp, prn, carrier_phase, code_phase);
  nap_xfer_blocking(NAP_REG_TRACK_BASE + channel*TRACK_SIZE + TRACK_INIT_OFFSET, 6, 0, temp);
}

/** Pack data for writing to a NAP track channel's UPDATE register.
 *
 * \param pack            Array of u8 to pack data into.
 * \param carrier_freq    Next correlation period's carrier frequency.
 * \param code_phase_rate Next correlation period's code phase rate.
 */
void track_pack_update(u8 pack[], s32 carrier_freq, u32 code_phase_rate)
{
  pack[0] = (code_phase_rate >> 24) & 0x1F;
  pack[1] = (code_phase_rate >> 16);
  pack[2] = (code_phase_rate >> 8);
  pack[3] = code_phase_rate;
  pack[4] = (carrier_freq >> 8);
  pack[5] = carrier_freq;
}

/** Write to a NAP track channel's UPDATE register.
 * Write new carrier frequency and code phase rate to a NAP track channel's
 * UPDATE register, which will be used to accumulate the channel's carrier and
 * code phases during the next correlation period.
 * NOTE: This must be called in addition to track_write_init_blocking when a
 * new track channel is being set up, before the NAP's internal timing
 * strobe goes low.
 * NOTE: If two track channel IRQ's occur without a write to the tracking
 * channel's UPDATE register between them, the error bit for the track
 * channel in the NAP error register will go high.
 *
 * \param channel         NAP track channel whose UPDATE register to write.
 * \param carrier_freq    Next correlation period's carrier frequency.
 * \param code_phase_rate Next correlation period's code phase rate.
 */
void track_write_update_blocking(u8 channel, s32 carrier_freq, u32 code_phase_rate)
{
  u8 temp[6] = {0, 0, 0, 0, 0, 0};
  track_pack_update(temp, carrier_freq, code_phase_rate);
  nap_xfer_blocking(NAP_REG_TRACK_BASE + channel*TRACK_SIZE + TRACK_UPDATE_OFFSET, 6, 0, temp);
}

/** Unpack data read from a NAP track channel's CORR register.
 *
 * \param packed       Array of u8 data read from channnel's CORR register.
 * \param sample_count Number of sample clock cycles in correlation period.
 * \param corrs        Array of E,P,L correlations from correlation period.
 */
void track_unpack_corr(u8 packed[], u16* sample_count, corr_t corrs[])
{
  /* graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend */
  struct {s32 xtend:24;} sign;

  *sample_count = (packed[0]<<8) | packed[1];

  for (u8 i=0; i<3; i++) {

    sign.xtend  = (packed[6*(3-i-1)+2] << 16)    /* MSB */
                | (packed[6*(3-i-1)+3] << 8)     /* Middle byte */
                | (packed[6*(3-i-1)+4]);         /* LSB */

    corrs[i].Q = sign.xtend; /* Sign extend! */

    sign.xtend  = (packed[6*(3-i-1)+5] << 16)    /* MSB */
                | (packed[6*(3-i-1)+6] << 8)     /* Middle byte */
                | (packed[6*(3-i-1)+7]);         /* LSB */

    corrs[i].I = sign.xtend; /* Sign extend! */
  }
}

/** Read data from a NAP track channel's CORR register.
 *
 * \param channel      NAP track channel whose CORR register to read.
 * \param sample_count Number of sample clock cycles in correlation period.
 * \param corrs        Array of E,P,L correlations from correlation period.
 */
void track_read_corr_blocking(u8 channel, u16* sample_count, corr_t corrs[])
{
  /* 2 (I or Q) * 3 (E, P or L) * 3 (24 bits / 8) + 16 bits sample count. */
  u8 temp[2*3*3+2];
  nap_xfer_blocking(NAP_REG_TRACK_BASE + channel*TRACK_SIZE + TRACK_CORR_OFFSET, 2*3*3+2, temp, temp);
  track_unpack_corr(temp, sample_count, corrs);
}

/** Unpack data read from a NAP track channel's PHASE register.
 *
 * \param packed        Array of u8 data read from channnel's PHASE register.
 * \param carrier_phase Carrier phase at end of last correlation period.
 * \param code_phase    Prompt code phase at end of last correlation period.
 *                      (deprecated, is always zero, as prompt code phase
 *                       rollovers are defined to be edges of correlation
 *                       period)
 */
/* TODO : take code phase out of phase register, it's always zero */
void track_unpack_phase(u8 packed[], u32* carrier_phase, u64* code_phase)
{
  *carrier_phase = packed[8] |
                   (packed[7] << 8) |
                   (packed[6] << 16);
  *code_phase = (u64)packed[5] |
                ((u64)packed[4] << 8) |
                ((u64)packed[3] << 16) |
                ((u64)packed[2] << 24) |
                ((u64)packed[1] << 32) |
                ((u64)packed[0] << 40);
}

/** Read data from a NAP track channel's PHASE register.
 *
 * \param channel       NAP track channel whose PHASE register to read.
 * \param carrier_phase Carrier phase at end of last correlation period.
 * \param code_phase    Prompt code phase at end of last correlation period.
 *                      (deprecated, is always zero, as prompt code phase
 *                       rollovers are defined to be edges of correlation
 *                       period)
 */
void track_read_phase_blocking(u8 channel, u32* carrier_phase, u64* code_phase)
{
  u8 temp[9] = {0, 0, 0x22, 0, 0, 0, 0, 0, 0};
  nap_xfer_blocking(NAP_REG_TRACK_BASE + channel*TRACK_SIZE + TRACK_PHASE_OFFSET, 9, temp, temp);
  track_unpack_phase(temp, carrier_phase, code_phase);
}

/** Write CA code to track channel's code ram.
 * CA Code for SV to be searched for must be written into channel's code ram
 * before acquisitions are started.
 *
 * \param prn PRN number (0-31) of CA code to be written.
 */
void track_write_code_blocking(u8 channel, u8 prn)
{
  nap_xfer_blocking(NAP_REG_TRACK_BASE + channel*TRACK_SIZE + TRACK_CODE_OFFSET, 128, 0, ca_code(prn));
}
