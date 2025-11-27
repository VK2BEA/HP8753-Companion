/*
 * Copyright (c) 2022-2026 Michael G. Katzmann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*
 * HP_FORM1toDouble
 *
 *From document "HP 8530A Automated Measurements" (pp 39-40)
 *  Created on: Mar 9, 2022
 *      Author: Michael Katzmann
 *
 *      Form 1 	This is the native internal data format of the receiver.
 *      		Each point of data contains a header byte, followed by
 *      		three, 16-bit words. Form 1 offers very fast transfer
 *      		speeds, and Form 1 data can be converted to floating point
 *      		data in the computer.
 *
 *		16 bit words from the instrument are 'big endian' and must be
 *		converted to native byte order.
 *
 *		This uses glib-2 for convenience.
 *
 *		Algorith from page 13-48 8510C Network Analyzer System Operating and Programming Manual 08510-90281 May 2001
 *		... but the exponent doesn't give sensible answers
 */

#include <glib-2.0/glib.h>
#include <math.h>

#define TOPBITofBYTE 0x80
#define LOWER_8BITS 0xFF
void
FORM1toDouble( gint16 *form1, gdouble *real, gdouble *imag, gboolean bDBnotLinear )
{
    gdouble dExp;

    // big endien .. so the lower 8 bits of the 16 bit
      gint16 iExp = GINT16_FROM_BE( form1[2] ) & LOWER_8BITS;

    // big endien .. so the lower 8 bits of the 16 bit
    if( (iExp & TOPBITofBYTE) == 0 )
        dExp = pow( 2.0, (double)( iExp - 15 ));
    else
        dExp = pow( 2.0, (double)(~(iExp ^ LOWER_8BITS) + 1) - 15.0 );

    *real = GINT16_FROM_BE( form1[1] ) * dExp;
    *imag = GINT16_FROM_BE( form1[0] ) * dExp;

    if( bDBnotLinear ) {
        *real = 20.0 * log10( *real );
        *real = 20.0 * log10( *imag );
    }
}


