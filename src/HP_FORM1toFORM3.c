/*
 * Copyright (c) 2022 Michael G. Katzmann
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
 */

#include <glib-2.0/glib.h>
#include <math.h>

static gdouble exponents[ 0x100 ];	// third byte is index to exponent

void
initializeFORM1exponentTable( void )
{
	gint i;
	for (i=0x00, exponents[0x00] = 2.0e-15; i < 0x7F; i++ )
		exponents[i+1] =  2 * exponents[i];

	for (i=0x7F, exponents[0x80] = 2.0e-143; i <0xFF; i++ )
		exponents[i+1] =  2 * exponents[i];
}

#define TOPBITofBYTE 0x80
void
FORM1toDouble( guint8 *form1, gdouble *real, gdouble *imag, gboolean bDBnotLinear )
{
	double exp;

	if( (form1[5] & TOPBITofBYTE) == 0 )
		exp = pow( 2.0, (double)((int)(form1[5]))-15.0 );
	else
		exp = pow( 2.0, (double)((int)(form1[5]))-271.0 );

	*real = GINT16_FROM_BE( form1[2] ) * exp;
	*imag = GINT16_FROM_BE( form1[0] ) * exp;

	if( bDBnotLinear ) {
		*real = 20.0 * log10( *real );
		*real = 20.0 * log10( *imag );
	}
	// *real = GINT16_FROM_BE( form1[2] ) * exponents[ form1[5] ];
	// *imag = GINT16_FROM_BE( form1[0] ) * exponents[ form1[5] ];
}

