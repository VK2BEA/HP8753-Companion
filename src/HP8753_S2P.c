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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <errno.h>
#include <hp8753.h>
#include <GPIBcomms.h>
#include <hp8753comms.h>

#include "messageEvent.h"

gint
getSparam( gint descGPIB_HP8753, tGlobal *pGlobal, tComplex *Sparam[], gint *nPoints, gint *pGPIBstatus )
{

	guint16 sizeF2 = 0, headerAndSize[2];
	guint8 *pFORM2 = 0;
	union {
		float IEEE754;
		guint32 bytes;
	} rBits, iBits;

	GPIBasyncWrite(descGPIB_HP8753, "FORM2;OUTPFORM;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// first read header and size of data
	GPIBasyncRead(descGPIB_HP8753, headerAndSize, HEADER_SIZE, pGPIBstatus, 20 * TIMEOUT_RW_1SEC);
	sizeF2 = GUINT16_FROM_BE(headerAndSize[1]);
	pFORM2 = g_malloc(sizeF2);
	GPIBasyncRead(descGPIB_HP8753, pFORM2, sizeF2, pGPIBstatus, 30 * TIMEOUT_RW_1SEC);

	*nPoints = sizeF2 / (sizeof(gint32) * 2);
	*Sparam = g_realloc( *Sparam, sizeof(tComplex) * sizeF2 );

	for ( int i = 0; i < *nPoints; i++) {
		rBits.bytes = GUINT32_FROM_BE( *(guint32* )(pFORM2 + i * sizeof(gint32) * 2));
		iBits.bytes = GUINT32_FROM_BE( *(guint32* )(pFORM2 + i * sizeof(gint32) * 2 + sizeof(gint32)));
		(*Sparam)[i].r = rBits.IEEE754;
		(*Sparam)[i].i = iBits.IEEE754;
	}
	g_free(pFORM2);

	return (GPIBfailed(*pGPIBstatus));
}

gint
getHP3753_S2P( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus )
{
	guchar *learnString = NULL;
	gdouble sweepStart = 300.0e3, sweepStop=3.0e9;
	int i;

    enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );

	postInfo("Determine current configuration");
	if( !getHP8753switchOnOrOff( descGPIB_HP8753, "COUC", pGPIBstatus ) ) {
		postError("Source must be coupled for S2P");
		return ERROR;
	}
	// Request Learn string
	GPIBasyncWrite(descGPIB_HP8753, "FORM1;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &learnString, pGPIBstatus ))
		goto err;

	GPIBasyncWrite(descGPIB_HP8753, "HOLD;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	setHP8753channel( descGPIB_HP8753, eCH_ONE, pGPIBstatus );

	if( getStartStopOrCenterSpanFrom8753learnString( learnString, pGlobal, eCH_ONE ) ) {
		askHP8753C_dbl(descGPIB_HP8753, "STAR", &sweepStart, pGPIBstatus);
		askHP8753C_dbl(descGPIB_HP8753, "STOP", &sweepStop, pGPIBstatus);
	} else {
		gdouble sweepCenter=1500.15e6, sweepSpan=2999.70e6;
		askHP8753C_dbl(descGPIB_HP8753, "CENT", &sweepCenter, pGPIBstatus);
		askHP8753C_dbl(descGPIB_HP8753, "SPAN", &sweepSpan, pGPIBstatus);
		sweepStart = sweepCenter - sweepSpan/2.0;
		sweepStop = sweepCenter + sweepSpan/2.0;
	}

	postInfo("Set for S11 + S21");
	GPIBasyncWrite(descGPIB_HP8753, "S11;SMIC;LINFREQ;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// Sweep
	setHP8753channel( descGPIB_HP8753, eCH_TWO, pGPIBstatus );
	// Depending upon the settings, a sweep may take a long time
	if( waitFor8753_OPC_SRQ( descGPIB_HP8753, "WAIT;", pGPIBstatus, 10 * TIMEOUT_RW_1MIN ) != eRDWT_OK ) {
		*pGPIBstatus = ERR;
		goto err;
	}
	// Read real / imag S11
	postInfo("Read S21 data");
	getSparam( descGPIB_HP8753, pGlobal, &pGlobal->HP8753.S2P.S21, &pGlobal->HP8753.S2P.nPoints, pGPIBstatus );

	// Derive the frequency points
	pGlobal->HP8753.S2P.freq = g_realloc(pGlobal->HP8753.S2P.freq, pGlobal->HP8753.S2P.nPoints * sizeof(gdouble) );
	for ( i = 0; i < pGlobal->HP8753.S2P.nPoints; i++) {
		pGlobal->HP8753.S2P.freq[i] = sweepStart + (sweepStop - sweepStart) * ((gdouble) i / (gdouble)pGlobal->HP8753.S2P.nPoints);
	}
	// Next sweep on channel 2 will be S12
	GPIBasyncWrite(descGPIB_HP8753, "S12;SMIC;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

	// ... but first get S11 from channel 1
	setHP8753channel( descGPIB_HP8753, eCH_ONE, pGPIBstatus );
	postInfo("Read S11 data");
	getSparam( descGPIB_HP8753, pGlobal, &pGlobal->HP8753.S2P.S11, &pGlobal->HP8753.S2P.nPoints, pGPIBstatus );

	// Set channel 1 to measure S22 and sweep
	postInfo("Set for S12 + S22");
	// Depending upon the settings, a sweep may take a long time
	GPIBasyncWrite( descGPIB_HP8753, "S22;SMIC;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if( waitFor8753_OPC_SRQ( descGPIB_HP8753, "SING;", pGPIBstatus, 10 * TIMEOUT_RW_1MIN ) != eRDWT_OK ) {
        *pGPIBstatus = ERR;
        goto err;
    }

	// collect S22 data
	postInfo("Read S22 data");
	getSparam( descGPIB_HP8753, pGlobal, &pGlobal->HP8753.S2P.S22, &pGlobal->HP8753.S2P.nPoints, pGPIBstatus );

	// Switch to channel two and get the S12 data
	setHP8753channel( descGPIB_HP8753, eCH_TWO, pGPIBstatus );
	postInfo("Read S12 data");
	getSparam( descGPIB_HP8753, pGlobal, &pGlobal->HP8753.S2P.S12, &pGlobal->HP8753.S2P.nPoints, pGPIBstatus );

	postInfo("Restore setup");
	// Return the analyzer to the previous configuration by sending back the learn string
	GPIBasyncWrite( descGPIB_HP8753, "FORM1;INPULEAS;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC );
	// Includes the 4 byte header with size in bytes (big endian)
	GPIBasyncWriteBinary( descGPIB_HP8753,
			learnString,
			GUINT16_FROM_BE(*(guint16 *)(learnString+2)) + 4, pGPIBstatus, 10 * TIMEOUT_RW_1SEC );
    enableSRQonOPC( descGPIB_HP8753, pGPIBstatus ); // learn string wipes out ESR and SRQ enables
	waitFor8753_OPC_SRQ( descGPIB_HP8753, "WAIT;", pGPIBstatus, 6 * TIMEOUT_RW_1MIN );

	g_free( learnString );

	return( GPIBfailed( *pGPIBstatus )  );
err:
	return ERROR;
}
