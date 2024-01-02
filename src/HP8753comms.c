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
#include "hp8753.h"
#include "GPIBcomms.h"
#include "hp8753comms.h"
#include "HPGLplot.h"

#include "messageEvent.h"

#define QUERY_SIZE	100
#define ANSWER_SIZE	100

// Index to HP8753 learn string for items that we cannot
// get with conventional queries.
// This will no doubt be different for every firmware version, so if in doubt
// we don't access markers
tLearnStringIndexes learnStringIndexes[] = {
		{
		  .version        = 413,          // version valid for the data below
		  .iActiveChannel = 1859,         // active channel (0x01 or 0x02)
		  .iMarkersOn     = {2323, 2325}, // markers on     (bit or of 0x02 (marker 1) to 0x10 (marker 4) with 0x20 as all off)
		  .iMarkerActive  = {1285, 1378}, // current marker (0x02 (marker 1) to 0x10 (marker 4))
		  .iMarkerDelta   = {1286, 1379}, // (bit or of 0x02 (marker 1) to 0x10 (marker 4) with 0x20 (fixed) 0x40 as all off)
		  .iStartStop     = {2383, 2385}, // stimulus start/stop or center/span (0x01 is start/stop)
		  .iSmithMkrType  = {1289, 1382}, // Smith marker type 0x00 - Lin / 0x01 - Log / 0x02 - Re-Im / 0x04 - R+jX / 0x08 - G+jB
		  .iPolarMkrType  = {1288, 1381}, // Polar marker type 0x10 - Lin / 0x20 - log / 0x40 - Re-Im
          .iNumSegments   = {2465, 2467}  // Number of segments defined (needed with 'all segments' sweep)
		}
};

/*!     \brief  Enable for SRQ for OPC
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 *
 * \param  descGPIB_HP8753  GPIB descriptor for HP8753 device
 * \param  pGPIBstatus      pointer to GPIB status
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
enableSRQonOPC( gint descGPIB_HP8753, gint *pGPIBstatus ) {
    if (GPIBfailed(*pGPIBstatus))
        return eRDWT_PREVIOUS_ERROR;

    return GPIBasyncWrite(descGPIB_HP8753, "ESE1;SRE32;", pGPIBstatus,  10 * TIMEOUT_RW_1SEC);
}
/*!     \brief  Write string preceeding with OPC or binary adding OPC;NOOP;, then wait for SRQ
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 * After a command that sets the OPC, wait for the event without tying up the GPIB.
 *
 * \param descGPIB_HP8753  GPIB descriptor for HP8753 device
 * \param pData             ointer to command to send (OPC permitted) or binary data
 * \param nBytes			number of bytes or -1 for NULL terminated string
 * \param pGPIBstatus       pointer to GPIB status
 * \param timeoutSecs		timout period to wait
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
GPIBasyncSRQwrite( gint descGPIB_HP8753, void *pData,
		gint nBytes, gint *pGPIBstatus, gdouble timeoutSecs ) {

#define SRQ_EVENT       1
#define TIMEOUT_EVENT   0
#define THIRTY_MS 0.030

	gchar *pPayload = NULL;

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint currentTimeoutDevice, currentTimeoutController;
    gdouble waitTime = 0.0;
    gint GPIBcontrollerIndex = 0;
    gint nTotalBytes = 0;

#define SIZE_OPC_NOOP	9	// # bytes in OPC;NOOP;

    if (GPIBfailed(*pGPIBstatus)) {
        return eRDWT_PREVIOUS_ERROR;
    }

    if( nBytes < 0 ) {
    	pPayload = g_strdup_printf( "OPC;%s", (gchar *)pData );
    	nTotalBytes = strlen( pPayload );
    } else {
    	pPayload = g_malloc( nBytes + SIZE_OPC_NOOP );
    	memcpy( pPayload, (guchar *)pData, nBytes );
    	memcpy( pPayload + nBytes, "OPC;NOOP;", SIZE_OPC_NOOP );
    	nTotalBytes = nBytes + SIZE_OPC_NOOP;
    }

	if( GPIBasyncWriteBinary( descGPIB_HP8753, pPayload, nTotalBytes,
							pGPIBstatus, timeoutSecs ) != eRDWT_OK ) {
	    g_free( pPayload );
	    return eRDWT_ERROR;
	} else {
		g_free( pPayload );
	}

	// get the controller index
    ibask( descGPIB_HP8753, IbaBNA, &GPIBcontrollerIndex);
    ibask( descGPIB_HP8753, IbaTMO, &currentTimeoutDevice);
    ibtmo( descGPIB_HP8753, T1s);
    ibask( GPIBcontrollerIndex, IbaTMO, &currentTimeoutController);
    ibtmo( GPIBcontrollerIndex, T30ms);	// just to check if we've been ordered to abandon ship
    DBG( eDEBUG_EXTENSIVE, "Waiting for SRQ" );
    do {
        short waitResult = 0;
        char status = 0;
        // This will timeout every 30ms (the timeout we set for the controller)
        WaitSRQ( GPIBcontrollerIndex, &waitResult);

        if ( waitResult == SRQ_EVENT ) {
            // This actually is an SRQ ..  is it from the HP8753 ?
            // Serial poll for status to reset SRQ and find out if it was the HP8753
            if( (*pGPIBstatus = ibrsp( descGPIB_HP8753, &status)) & ERR ) {
                LOG(G_LOG_LEVEL_CRITICAL, "HPIB serial poll fail %04X/%d", *pGPIBstatus, AsyncIberr());
                rtn = eRDWT_ERROR;
            } else if( status & ST_SRQ ) {
            	// there is but one condition that asserts the SRQ ... the OPC
            	// so it's probably not necessary in our setup to read the ESR.
            	// It also tends to re-enable the SRQ ... which is odd
#ifndef CLEAR_ESR
                // We've cleared the SRQ bit in the Status Register by the serial poll
                // now clear the ESR flag by reading it
#define ESR_RESPONSE_MAXSIZE	5		// more than enough
            	gchar sESR[ ESR_RESPONSE_MAXSIZE ] = {0};
            	// For some bazaar reason the HP8753C can raise SRQ again when the ESR?; is written and cleared when is read
            	// so mask it out before that.

            	if( GPIBasyncWrite(descGPIB_HP8753, "ESR?;", pGPIBstatus,
            								10 * TIMEOUT_RW_1SEC) == eRDWT_OK
            		&& GPIBasyncRead( descGPIB_HP8753, sESR, ESR_RESPONSE_MAXSIZE, pGPIBstatus,
            								10 * TIMEOUT_RW_1SEC ) == eRDWT_OK ) {
                	gint ESR = atoi( sESR );
                	if( ESR & ESE_OPC ) {
                		rtn = eRDWT_OK;
                	} else {
                		DBG(eDEBUG_ALWAYS, "SRQ but ESR did not show OPC.. ESR = $s", sESR);
						rtn = eRDWT_ERROR;
                	}
                } else {
                	rtn = eRDWT_ERROR;
                }
                // thats it .. we are good to go
#else
            	rtn = eRDWT_OK;
#endif
            }
            // its not the HP8753 ... some other GPIB device is requesting service
        } else { // it''s a 30ms timeout
            // If we get a message on the queue, it is assumed to be an abort
            if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
                // This will stop future GPIB commands for this sequence
                *pGPIBstatus |= ERR;
                rtn = eRDWT_ABORT;
            }
        }
        waitTime += THIRTY_MS;
        if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
        	gchar *sMessage;
        	if( nBytes == WAIT_STR && timeoutSecs > 15 ) {	// this means we have a "WAIT;" message .. so show the estimated time
        		sMessage = g_strdup_printf("✳️ Waiting for HP8753 : %ds / %.0lfs", (gint) (waitTime), (double)timeoutSecs / TIMEOUT_SAFETY_FACTOR );
        	} else {
        		sMessage = g_strdup_printf("✳️ Waiting for HP8753 : %ds", (gint) (waitTime));
        	}
            postInfo(sMessage);
            g_free(sMessage);
        }
    } while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if( rtn == eRDWT_OK ) {
        DBG( eDEBUG_EXTENSIVE, "SRQ asserted and acknowledged" );
    } else {
        DBG( eDEBUG_ALWAYS, "SRQ error waiting: %04X/%d", ibsta, iberr );
    }

    // Return timeouts
    ibtmo( descGPIB_HP8753, currentTimeoutDevice);
    ibtmo( GPIBcontrollerIndex, currentTimeoutDevice);

    if( rtn == eRDWT_CONTINUE ) {
        *pGPIBstatus |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}



/*!     \brief  Get option setting from list of possible
 *
 * Several options are set as 1 of n possibilities (radio buttons).
 * Each of these must be interrogated in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  optList			list of options
 * \[aram  maxOptions		length of list
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
findHP8753option(gint descGPIB_HP8753, const HP8753C_option *optList,
		gint maxOptions, gint *pGPIBstatus) {
	gboolean result;
	int i;

	for (i = 0; i < maxOptions; i++) {
		result = askOption( descGPIB_HP8753, optList[i].code, pGPIBstatus );
		if (result == TRUE)
			break;
	}
	if ( GPIBfailed(*pGPIBstatus) || i == maxOptions)
		return ERROR;
	else
		return i;
}

const HP8753C_option optFormat[] = {
		{ "LOGM?;", "Log Magnitude" },
		{ "PHAS?;", "Phase" },
		{ "DELA?;", "Delay" },
		{ "SMIC?;", "Smith Chart" },
		{ "POLA?;", "Polar" },
		{ "LINM?;", "Linear Magnitude" },
		{ "SWR?;", "SWR" },
		{ "REAL?;", "Real" },
		{ "IMAG?;", "Imaginary" } };
/*!     \brief  Find the display/readout format for the current channel
 *
 * Find the format for the current channel.
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753format( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optFormat, sizeof(optFormat) / sizeof(HP8753C_option), pGPIBstatus);
}

const tGrid gridType[] = { eGridCartesian, eGridCartesian, eGridCartesian,
		eGridSmith, eGridPolar, eGridCartesian, eGridCartesian, eGridCartesian,
		eGridCartesian };

const HP8753C_option optSweepType[] = {
		{ "LINFREQ?;", "Linear Frequency" },
		{ "LOGFREQ?;", "Log Frequency" },
		{ "LISFREQ?;", "List Frequency" }, // dont look for SEG[1-30]
		{ "CWTIME?;", "CW Time" },
		{ "POWS?;", "Power" }};
/*!     \brief  Find the sweep format for the current channel
 *
 * Find the sweep format for the current channel.
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753sweepType( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optSweepType, sizeof(optSweepType) / sizeof(HP8753C_option), pGPIBstatus);
}

// only one input port or S-paramaters are active at one time
// S11 is A/R, S12 is B/R for example
const HP8753C_option optMeasurementType[] = {
		{ "S11?;", "S11" },
		{ "S12?;", "S12" },
		{ "S21?;", "S21" },
		{ "S22?;", "S22" },
		{ "AR?;", "A/R" },
		{ "BR?;", "B/R" },
		{ "AB?;", "A/B" },
		{ "MEASA?;", "A" },
		{ "MEASB?;", "B" },
		{ "MEASR?;", "R" } };
/*!     \brief  Find the measurement type for the current channel
 *
 * Find the measurement type for the current channel.
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753measurementType( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optMeasurementType, sizeof(optMeasurementType) / sizeof(HP8753C_option), pGPIBstatus);
}

const HP8753C_option optSmithMkrType[] = {
		{ "SMIMLIN?;", "Linear" },
		{ "SMIMLOG?;", "Log" },
		{ "SMIMRI?;", "Real/Imaginary" },
		{ "SMIMRX?;", "R+jX" },
		{ "SMIMGB?;", "G+jB" } };
/*!     \brief  Find the type of Smith marker for the current channel
 *
 * Find the type of Smith marker for the current channel.
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753smithMkrType( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optSmithMkrType, sizeof(optSmithMkrType) / sizeof(HP8753C_option), pGPIBstatus);
}

const HP8753C_option optPolarMkrType[] = {
		{ "POLMLIN?;", "Linear" },
		{ "POLMLOG?;", "Log" },
		{ "POLMRI?;", "Real/Imaginary" } };
/*!     \brief  Find the type of Polar marker for the current channel
 *
 * Find the type of Smith marker for the current channel.
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753polarMkrType( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optPolarMkrType, sizeof(optPolarMkrType) / sizeof(HP8753C_option), pGPIBstatus);
}

const HP8753C_option optCalType[] =
	{ { "CALN?;", 		"None" },
	  { "CALIRESP?;",	"Response" },
	  { "CALIRAI?;", 	"Response & Isolation" },
	  { "CALIS111?;", 	"S11 1-port" },
	  { "CALIS221?;", 	"S22 1-port" },
	  { "CALIFUL2?;", 	"Full 2-port" },
	  { "CALIONE?;", 	"One path 2-port" },
	  { "CALITRL2?;", 	"TRL*/LRM* 2-port" } };
/*!     \brief  Find the type of calibration enabled
 *
 * Find the type of calibration enabled
 * Each of the possible options are tested in order to determine the one set,
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return option number or ERROR
 */
gint
getHP8753calType( gint descGPIB_HP8753, gint *pGPIBstatus ) {
	return findHP8753option( descGPIB_HP8753, optCalType, sizeof(optCalType) / sizeof(HP8753C_option), pGPIBstatus);
}

/*!     \brief  Set the channel
 *
 * Set the channel (1 or 2) to which the subsequent commands operate
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  channel			which channel (eCH_ONE or eCH_TWO)
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 or ERROR
 */
gint
setHP8753channel( gint descGPIB_HP8753, eChannel channel, gint *pGPIBstatus ) {
	gchar qString[ QUERY_SIZE ];

	g_snprintf( qString, QUERY_SIZE, "CHAN%d;", channel+1);
	GPIBasyncSRQwrite( descGPIB_HP8753, qString, NULL_STR, pGPIBstatus, 15 * TIMEOUT_RW_1SEC );
	return ( GPIBfailed(*pGPIBstatus) ? ERROR : 0);
}

/*!     \brief  Query an on/off option (1 or 0)
 *
 * Send a query ? to the HP8753 and test if the response is 0 or 1
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  option			the query string (like "DUAC?;")
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 or 1
 */
gboolean
askOption( gint descGPIB_HP8753, gchar *option, gint *pGPIBstatus ) {
	gchar result = '0';
	GPIBasyncWrite(descGPIB_HP8753, option, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	GPIBasyncRead(descGPIB_HP8753, &result, 1, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

	return (result == '1');
}

/*!     \brief  Send a query to the HP8753 returning a floating point number
 *
 * Send a query to the HP8753 and return the floating point number response
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  mnemonic			the query string (without the ?)
 * \param  pDblRresult		pointer to the location to save the answer
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 or 1
 */
gint
askHP8753C_dbl(gint descGPIB_HP8753, gchar *mnemonic, gdouble *pDblRresult, gint *pGPIBstatus) {
#define DBL_ASCII_SIZE	25
	gchar *queryString = g_strdup_printf("%s?;", mnemonic);
	gchar ASCIIanswer[DBL_ASCII_SIZE + 1] = { 0 };
	gint sRtn = 0;

	GPIBasyncWrite(descGPIB_HP8753, queryString, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	GPIBasyncRead(descGPIB_HP8753, &ASCIIanswer, DBL_ASCII_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if( GPIBsucceeded(*pGPIBstatus) )
		sRtn = sscanf(ASCIIanswer, "%le", pDblRresult);

	g_free(queryString);

	if (GPIBfailed(*pGPIBstatus))
		return ERROR;
	else
		return sRtn;
}

/*!     \brief  Send a query to the HP8753 returning an integer number
 *
 * Send a query to the HP8753 and return the integer number response
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  mnemonic			the query string (without the ?)
 * \param  pIntResult		pointer to the location to save the answer
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 or 1
 */
gint
askHP8753C_int(gint descGPIB_HP8753, gchar *mnemonic, gint *pIntResult, gint *pGPIBstatus) {
#define DBL_ASCII_SIZE	25
	gchar *queryString = g_strdup_printf("%s?;", mnemonic);
	gchar ASCIIanswer[DBL_ASCII_SIZE + 1] = { 0 };
	gint sRtn = 0;

	GPIBasyncWrite(descGPIB_HP8753, queryString, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	GPIBasyncRead(descGPIB_HP8753, &ASCIIanswer, DBL_ASCII_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if( GPIBsucceeded(*pGPIBstatus) )
		sRtn = sscanf(ASCIIanswer, "%d", pIntResult);

	g_free(queryString);

	if (GPIBfailed(*pGPIBstatus))
		return ERROR;
	else
		return sRtn;
}

/*!     \brief  Get the firmware version of the HP8753
 *
 * Get the firmware version (like 4.13) and product (like HP8753C) of the HP8753
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  psProduct		address of the location to place the alloced product string
 * \param  pGPIBstatus		pointer to GPIB status
 * \return firmware version as integer (like 413 for 4.13)
 */
gint
get8753firmwareVersion(gint descGPIB_HP8753, gchar **psProduct, gint *pGPIBstatus) {
#define MAX_IDN_SIZE	50
	gint nConv = 0;
	gint ver=0, rev=0;
	gchar ASCIIanswer[MAX_IDN_SIZE + 1] = { 0 };
	gchar sManufacturer[MAX_IDN_SIZE + 1] = {0};
	gchar sProduct[MAX_IDN_SIZE + 1] = {0};

	GPIBasyncWrite(descGPIB_HP8753, "IDN?;", pGPIBstatus, 20 * TIMEOUT_RW_1SEC);
	GPIBasyncRead(descGPIB_HP8753, &ASCIIanswer, MAX_IDN_SIZE, pGPIBstatus, 20 * TIMEOUT_RW_1SEC);

	if( GPIBsucceeded( *pGPIBstatus ) ) {
		LOG( G_LOG_LEVEL_INFO, "IDN returns \"%s\"", ASCIIanswer );
		nConv = sscanf( ASCIIanswer, "%50[^,],%50[^,],%*d,%d.%d", sManufacturer, sProduct, &ver, &rev );
	} else {
		LOG( G_LOG_LEVEL_CRITICAL, "GPIB communication prevented reading of IDN" );
	}

	// should return 413 for 4.13 or INVALID on error
	if ( GPIBfailed( *pGPIBstatus )  || nConv != 4 ) {
		return INVALID;
	} else {
		if( psProduct ) {
			g_free(*psProduct);
			*psProduct = g_strdup( sProduct );
		}
		return ( ver * 100 + rev );
	}
}

/*!     \brief  Get the markers and segments from the HP8753
 *
 * Get the markers and segments (if relevent) from the HP8753
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGlobal			pointer to global data structure
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 (OK) or 1 (error)
 */
gint
getHP8753markersAndSegments( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus ) {
	gchar sQuery[ QUERY_SIZE ];
	gchar sAnswer[ ANSWER_SIZE ];
	gchar complete;
	gboolean bMarkerChanged = FALSE;
	gdouble re, im, sourceValue;
	gint n;
	int activeChannelNow;
	tChannel *pChannel;
	eChannel channel;
	gint nChannelsExamined;

	enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );

	activeChannelNow = pGlobal->HP8753.activeChannel;

	for ( channel = activeChannelNow, nChannelsExamined = 0;
			nChannelsExamined < eNUM_CH;
			channel = (channel+1) % eNUM_CH, nChannelsExamined++ ) {
		gint mkrNo, flagBit;

		bMarkerChanged = FALSE;
		// if we are using just one channel .. the data goes to channel 1 and
		// we don't examine the other channel
		if( !pGlobal->HP8753.flags.bDualChannel ) {
			channel = eCH_ONE;
		} else {
		// We should not need to wait for these because
		// we only switch from active channel if dual channel is enabled
			if( activeChannelNow != channel ) {
				setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
				activeChannelNow = channel;
			}
		}

		pChannel = &pGlobal->HP8753.channels[ channel ];

		// now get the marker source values and response values
		for( mkrNo = 0, flagBit = 0x01; mkrNo < MAX_NUMBERED_MKRS; mkrNo++, flagBit <<= 1 ) {
			if( pChannel->chFlags.bMkrs & flagBit ) {
				// Select marker (i.e. MARK1;) and then read values
				g_snprintf( sQuery, QUERY_SIZE, "MARK%d;OUTPMARK;", mkrNo+1);
				do {
					GPIBasyncWrite(descGPIB_HP8753, sQuery, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
					GPIBasyncRead(descGPIB_HP8753, sAnswer, ANSWER_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				} while (FALSE);
				n = sscanf( sAnswer, "%le, %le, %le", &re, &im, &sourceValue );
				if( n == 3 ) {
					pChannel->numberedMarkers[ mkrNo ].point.r = re;
					pChannel->numberedMarkers[ mkrNo ].point.i = im;
					pChannel->numberedMarkers[ mkrNo ].sourceValue = sourceValue;
				}

				bMarkerChanged = (pChannel->activeMarker != mkrNo );
			}
		}

		// we need to find the frequency and level of the marker that
		// is used for delta (so we can determine where they all are)
		if( pChannel->chFlags.bMkrs && pChannel->chFlags.bMkrsDelta ) {
			postInfo( "Get delta marker data");
			// we need to find the frequency and level of the marker that
			// is used for delta (so we can determine where they all are)

			gint deltaMarker = pChannel->deltaMarker;
			if( deltaMarker == FIXED_MARKER ) {
				// FDelta Fixed marker
				if( askHP8753C_dbl(descGPIB_HP8753, "MARKFSTI", &sourceValue, pGPIBstatus) == ERROR )
					break;
				if( askHP8753C_dbl(descGPIB_HP8753, "MARKFVAL", &re, pGPIBstatus) == ERROR )
					break;
				if( askHP8753C_dbl(descGPIB_HP8753, "MARKFAUV", &im, pGPIBstatus) == ERROR )
					break;
				pChannel->numberedMarkers[ deltaMarker ].point.r = re;
				pChannel->numberedMarkers[ deltaMarker ].point.i = im;
				pChannel->numberedMarkers[ deltaMarker ].sourceValue = sourceValue;
			} else {
				// Delta numbered marker
				GPIBasyncWrite(descGPIB_HP8753, "DELO;",pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				// Select marker (i.e. MARK1;) and then read values
				g_snprintf( sQuery, QUERY_SIZE, "MARK%d;", deltaMarker+1);
				GPIBasyncWrite(descGPIB_HP8753, sQuery,pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				GPIBasyncWrite(descGPIB_HP8753, "OUTPMARK;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				GPIBasyncRead(descGPIB_HP8753, sAnswer, ANSWER_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				n = sscanf( sAnswer, "%le, %le, %le", &re, &im, &sourceValue );
				if( n == 3 ) {
					pChannel->numberedMarkers[ deltaMarker ].point.r = re;
					pChannel->numberedMarkers[ deltaMarker ].point.i = im;
					pChannel->numberedMarkers[ deltaMarker ].sourceValue = sourceValue;
				}
				g_snprintf( sQuery, QUERY_SIZE, "DELR%d;", deltaMarker+1);
				GPIBasyncWrite(descGPIB_HP8753, sQuery, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				if( pChannel->activeMarker != deltaMarker )
					bMarkerChanged = TRUE;
			}

		}

		if( bMarkerChanged ) {
			g_snprintf( sQuery, QUERY_SIZE, "MARK%d;ENTO;", pChannel->activeMarker+1);
			GPIBasyncWrite(descGPIB_HP8753, sQuery, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
		}

		// find out the style of marker to display (smith and polar)
		pChannel->chFlags.bAdmitanceSmith = FALSE;
		if( pChannel->chFlags.bMkrs ) {
			pChannel->mkrType = eMkrDefault;
			if( pChannel->format == eFMT_SMITH ) {
				pChannel->mkrType = getHP8753smithMkrType( descGPIB_HP8753, pGPIBstatus);
				pChannel->chFlags.bAdmitanceSmith = (pChannel->mkrType == eMkrGjB);
			} else if( pChannel->format == eFMT_POLAR ) {
				pChannel->mkrType = getHP8753polarMkrType( descGPIB_HP8753, pGPIBstatus);
			} else {
				pChannel->mkrType = eMkrDefault;
			}
		}

		// if we have markers, see if we have enabled bandwidth
		if( pChannel->chFlags.bMkrs ) {
			GPIBasyncWrite(descGPIB_HP8753, "WIDT?;",pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
			GPIBasyncRead(descGPIB_HP8753, &complete, 1, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
			pChannel->chFlags.bBandwidth = (complete == '1');

			if( pChannel->chFlags.bBandwidth ) {
				GPIBasyncWrite(descGPIB_HP8753, "OUTPMWID;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				GPIBasyncRead(descGPIB_HP8753, sAnswer, ANSWER_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				n = sscanf( sAnswer, "%le, %le, %le",
						&pChannel->bandwidth[BW_WIDTH],
						&pChannel->bandwidth[BW_CENTER],
						&pChannel->bandwidth[BW_Q] );
			}
		}

		// Get segments if we are in list frequency sweep with all segments
		// We need to switch to each segment to read the start/stop freq and number of points.
		// unfortunately this destroys the trace
		eChannel otherChannel = (channel == eCH_ONE ? eCH_TWO : eCH_ONE);
		if (  pGlobal->HP8753.flags.bSourceCoupled
				&& pGlobal->HP8753.channels[ otherChannel ].chFlags.bValidSegments ) {
			// if we have coupled sources, then just copy the data from the
			// other channel if we have already harvested it.
			for( gint seg=0; seg < pGlobal->HP8753.channels[ otherChannel ].nSegments; seg++ ) {
				pChannel->segments[ seg ].nPoints =
						pGlobal->HP8753.channels[ otherChannel ].segments[ seg ].nPoints;
				pChannel->segments[ seg ].startFreq =
						pGlobal->HP8753.channels[ otherChannel ].segments[ seg ].startFreq;
				pChannel->segments[ seg ].stopFreq =
						pGlobal->HP8753.channels[ otherChannel ].segments[ seg ].stopFreq;
			}
//			g_free( pChannel->stimulusPoints );
//			pChannel->stimulusPoints = g_memdup2( pGlobal->HP8753.channels[ otherChannel ].stimulusPoints,
//					pGlobal->HP8753.channels[ otherChannel ].nPoints * sizeof( gdouble ) );
		} else {
			getHP8753channelListFreqSegments( descGPIB_HP8753, pGlobal, channel, pGPIBstatus );
		}

		// only do this for the active channel if we do not have both channels on
		if( !pGlobal->HP8753.flags.bDualChannel )
			break;
	}

	GPIBasyncWrite(descGPIB_HP8753, "ENTO", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

	// return to active channel if we switched
	if( activeChannelNow != pGlobal->HP8753.activeChannel ) {
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753.activeChannel, pGPIBstatus );
	}

	return (GPIBfailed(*pGPIBstatus));
}

/*!     \brief  Get an on/off setting from the HP8753
 *
 * Get an on/off setting from the HP8753
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  sRequest			mnomnic of the request
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 or 1 or -1 (error)
 */
gint
getHP8753switchOnOrOff( gint descGPIB_HP8753, gchar *sRequest, gint *pGPIBstatus ) {
	gchar *sInterrogationRequest = g_strdup_printf( "%s?;", sRequest );
	gchar response;
	gboolean bSuccess = TRUE;

	if( GPIBasyncWrite( descGPIB_HP8753, (guchar *)sInterrogationRequest, pGPIBstatus, 10 * TIMEOUT_RW_1SEC ) == ERROR ) {
		bSuccess = FALSE;
	} else {
		if( GPIBasyncRead( descGPIB_HP8753, (guchar *)&response, 1, pGPIBstatus, 10 * TIMEOUT_RW_1SEC ) == ERROR )
			bSuccess = FALSE;
	}

	g_free( sInterrogationRequest );

	if( bSuccess ) {
		return (response == '1');
	} else {
		return ERROR;
	}
}


/*!     \brief  assign learning string indexes based on firmware version
 *
 * Assign learning string indexes (to pointer in gloabl data structure) based on firmware version
 *
 * \param  pGlobal  pointer to global data
 * \return true if assigned
 */
gboolean
selectLearningStringIndexes( tGlobal *pGlobal ) {
	gint i;
	gboolean bFound = FALSE;

	pGlobal->HP8753.pLSindexes = (tLearnStringIndexes *)INVALID;

	for( i = 0; i < sizeof(learnStringIndexes) / sizeof(tLearnStringIndexes); i++ ) {
		if ( learnStringIndexes[ i ].version == pGlobal->HP8753.firmwareVersion ) {
			pGlobal->HP8753.pLSindexes = &learnStringIndexes[i];
			bFound = TRUE;
			break;
		}
	}

	if( !bFound && pGlobal->HP8753.firmwareVersion == pGlobal->HP8753.analyzedLSindexes.version ) {
		pGlobal->HP8753.pLSindexes = &pGlobal->HP8753.analyzedLSindexes;
		bFound = TRUE;
	}

	return (bFound);
}

/*!     \brief  Get learning string from HP8753
 *
 * Get Learning String from HP8753
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  ppLearnString	address of pointer to learning string
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 if no error or 1 if error
 */
gint
get8753learnString( gint descGPIB_HP8753, guchar **ppLearnString, gint *pGPIBstatus ) {

	gint LSsize = 0;
	guint16 LSheaderAndSize[2];

	// GPIBasyncWrite(descGPIB_HP8753, "FORM1;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);// Form 1 internal binary format (this is not necessary)
	GPIBasyncWrite(descGPIB_HP8753, "OUTPLEAS;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// first read header and size of data

	if( GPIBasyncRead(descGPIB_HP8753, &LSheaderAndSize, HEADER_SIZE, pGPIBstatus,
			10 * TIMEOUT_RW_1SEC) == ERROR )
		goto err;
	// Convert from big endian to local (Intel LE)
	LSsize = GUINT16_FROM_BE(LSheaderAndSize[1]);

	g_free( *ppLearnString );   // if we had previously allocated memory .. free it now

	*ppLearnString = g_malloc( LSsize + HEADER_SIZE);
	// copy header to save as one chunk
	memmove(*ppLearnString, LSheaderAndSize, HEADER_SIZE);
	// read learn string into malloced space (not clobbering header and size)
	GPIBasyncRead(descGPIB_HP8753, *ppLearnString + HEADER_SIZE, LSsize, pGPIBstatus,
			10 * TIMEOUT_RW_1SEC);

err:
	return (GPIBfailed(*pGPIBstatus));
}

/*!     \brief  Get  the configuration and trace data for a channel
 *
 * Get the configuration and trace data for the channel
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGlobal	   pointer global data
 * \param  channel     which channel
 * \param  pGPIBstatus pointer of GPIB status
 * \return 0 (OK) or 1 (error)
 */
gint
getHP8753channelTrace(gint descGPIB_HP8753, tGlobal *pGlobal, eChannel channel, gint *pGPIBstatus ) {
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	guint16 sizeF2 = 0, headerAndSize[2];
	guint8 *pFORM2 = 0;
	gint i;

	union {
		float IEEE754;
		guint32 bytes;
	} rBits, iBits;

	pChannel->chFlags.bValidData = FALSE;

	pChannel->format = getHP8753format(descGPIB_HP8753, pGPIBstatus);
	if (pChannel->format == ERROR)
		return TRUE;

	askHP8753C_dbl(descGPIB_HP8753, "SCAL", &pChannel->scaleVal, pGPIBstatus);
	askHP8753C_dbl(descGPIB_HP8753, "REFP", &pChannel->scaleRefPos, pGPIBstatus);
	askHP8753C_dbl(descGPIB_HP8753, "REFV", &pChannel->scaleRefVal, pGPIBstatus);

	if ( pChannel->chFlags.bCenterSpan ) {
		gdouble cent = 1500.150e6, span = 2999.7e6;
		askHP8753C_dbl( descGPIB_HP8753, "CENT", &cent, pGPIBstatus );
		askHP8753C_dbl( descGPIB_HP8753, "SPAN", &span, pGPIBstatus );
		pChannel->sweepStart = cent - span/2.0;
		pChannel->sweepStop  = cent + span/2.0;
	} else {
		askHP8753C_dbl( descGPIB_HP8753, "STAR", &pChannel->sweepStart, pGPIBstatus );
		askHP8753C_dbl( descGPIB_HP8753, "STOP", &pChannel->sweepStop, pGPIBstatus );
	}
	pChannel->sweepType = getHP8753sweepType( descGPIB_HP8753, pGPIBstatus );
	askHP8753C_dbl( descGPIB_HP8753, "IFBW", &pChannel->IFbandwidth, pGPIBstatus );

	if( pChannel->sweepType == eSWP_CWTIME || pChannel->sweepType == eSWP_PWR )
		askHP8753C_dbl( descGPIB_HP8753, "CWFREQ", &pChannel->CWfrequency, pGPIBstatus );

	// if we are sweeping in list frequency mode
	// find out if is just one segment or all segments
	if( pChannel->sweepType == eSWP_LSTFREQ )
		pChannel->chFlags.bAllSegments = askOption( descGPIB_HP8753, "ASEG?;", pGPIBstatus );
	pChannel->chFlags.bAveraging = askOption( descGPIB_HP8753, "AVERO?;", pGPIBstatus );
	pChannel->measurementType = getHP8753measurementType( descGPIB_HP8753, pGPIBstatus);

	GPIBasyncWrite(descGPIB_HP8753, "FORM2;OUTPFORM;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// first read header and size of data
	GPIBasyncRead(descGPIB_HP8753, headerAndSize, HEADER_SIZE, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	sizeF2 = GUINT16_FROM_BE(headerAndSize[1]);
	pFORM2 = g_malloc(sizeF2);
	GPIBasyncRead(descGPIB_HP8753, pFORM2, sizeF2, pGPIBstatus, 30 * TIMEOUT_RW_1SEC);

	pChannel->nPoints = sizeF2 / (sizeof(gint32) * 2);
	pChannel->responsePoints = g_realloc( pChannel->responsePoints, sizeof(tComplex) * sizeF2 );
	pChannel->stimulusPoints = g_realloc( pChannel->stimulusPoints, sizeof(gdouble) * sizeF2 );

	gdouble logSweepStart = log10( pChannel->sweepStart );
	gdouble logStimulusStop = log10( pChannel->sweepStop );
	for ( i = 0; i < pChannel->nPoints; i++) {
		gdouble stimulusSample, stimulusFraction;

		rBits.bytes = GUINT32_FROM_BE( *(guint32* )(pFORM2 + i * sizeof(gint32) * 2));
		iBits.bytes = GUINT32_FROM_BE( *(guint32* )(pFORM2 + i * sizeof(gint32) * 2 + sizeof(gint32)));
		pChannel->responsePoints[i].r = rBits.IEEE754;
		pChannel->responsePoints[i].i = iBits.IEEE754;
		// g_print( "%3d : %15e + j %15e\n", i, trace->points[i].r, trace->points[i].i);

		stimulusFraction = (gdouble) i / (pChannel->nPoints-1);

		switch( pChannel->sweepType ) {
		case eSWP_LSTFREQ:
		case eSWP_LINFREQ:
		case eSWP_CWTIME:
		case eSWP_PWR:
		default:
			// n.b: 3 is the minimum number of points so this will not blow up
			// This calculation will be wrong for list sweep if we sweep all segments; therefore, we redo this in
			//      getHP8753markersAndSegments in that case.
			//      The logial place to do it is here but to do so we must change the sweep to each of the segments
			//      and this destroys the traces (both channels). We get the trace data for both channels first
			//      and then get the segments in order to calculate the stimulus value for each point;
			stimulusSample = pChannel->sweepStart + (pChannel->sweepStop - pChannel->sweepStart) * stimulusFraction;
			break;
		case eSWP_LOGFREQ:
			stimulusSample = pow( 10.0,  logSweepStart + ( logStimulusStop - logSweepStart) * logStimulusStop );
			break;
		}
		pChannel->stimulusPoints[i] = stimulusSample;
	}

	if (pChannel->nPoints != 0 && !GPIBfailed(*pGPIBstatus))
		pChannel->chFlags.bValidData = TRUE;
	g_free(pFORM2);

	return (GPIBfailed(*pGPIBstatus));
}


const HP8753C_option optPlotQuadrant[] = {
        { "LEFL?;", "Lower Left" },
        { "LEFU?;", "Upper Left" },
        { "RIGL?;", "Lower Right" },
        { "RIGU?;", "Upper Right" }};

/*!     \brief  Get  the configuration and trace data for a channel
 *
 * Get the configuration and trace data for the channel
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGlobal	   pointer global data
 * \param  pGPIBstatus pointer of GPIB status
 * \return 0 (OK) or 1 (error)
 */
#define MAX_HPGL_PLOT_CHUNK	1000
gint
acquireHPGLplot( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus ) {
	gchar sHPGL[ MAX_HPGL_PLOT_CHUNK + 1 ];
	gint nTokens = 0;
	gboolean bFullPagePlot = TRUE;
	gint plotQuadrant = 0;
    gboolean bPresumedEnd = FALSE;

	pGlobal->HP8753.flags.bHPGLdataValid = FALSE;

	// See if 8753 is set to ploat the full page..
	// If it isn't find out what quadrant its set to, set it to full page and
	// reset it after plot acquired
	bFullPagePlot = askOption( descGPIB_HP8753, "FULP?;", pGPIBstatus );

	if( !bFullPagePlot ) {
	    plotQuadrant = findHP8753option( descGPIB_HP8753, optPlotQuadrant, sizeof(optPlotQuadrant) / sizeof(HP8753C_option), pGPIBstatus);
	}
	GPIBasyncWrite(descGPIB_HP8753, "SCAPFULL;FULP;PTEXT ON;OUTPPLOT;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// The number of characters is dependent on the number of points and number of traces (including memory traces)
	// that are enabled. The GPIB END is asserted at the end of a line and ibcnt will indicate the actual count.
	sHPGL[0] = 0;	// we start with no remainder from a previous call
	parseHPGL( NULL, pGlobal );
	// We do a number of reads to obtain the HPGL...
	// The number of reads is different on the c and the d models so we cannot
	// assume to know what this is. We could just read until a timeout but then we always have
	// an delay on the last read. The HPGL selects pen 0 (white) as the anti-penultimate command.
	// we can use this to indicate that no more reads are needed. When parsedd, this give us our 'presumed end'
	do {
		gint offset = strlen(sHPGL);
		int n;
		if( GPIBasyncRead(descGPIB_HP8753, sHPGL+offset, MAX_HPGL_PLOT_CHUNK-offset,
				pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
			break;
		sHPGL[ AsyncIbcnt()+offset ] = 0;
		if( GPIBsucceeded(*pGPIBstatus) ) {
	        if( pGlobal->flags.bbDebug == 6 )
	            g_printerr( "%.*s", AsyncIbcnt(), sHPGL+offset );

			gchar **tokens =  g_strsplit ( sHPGL, ";", -1 );
			gint max=g_strv_length(tokens);
			// the last string may be partial, so stuff it into
			// the beginning of the next buffer
			for( n=0 ; n < max-1; n++ ) {
			    bPresumedEnd = parseHPGL( tokens[n], pGlobal );
				nTokens++;
			}
			// copy remainder (possibly only a partial) to the buffer to be
			// added to on the next read
			strcpy( sHPGL, tokens[max-1] );
			g_strfreev(tokens);
		}
        postInfoWithCount( "Received %d HPGL instructions", nTokens, 0 );
	} while ( ((*pGPIBstatus & END) != END || !bPresumedEnd)  && GPIBsucceeded(*pGPIBstatus)  );
	// the last command must be parsed
	if( GPIBsucceeded(*pGPIBstatus) ) {
		parseHPGL( sHPGL, pGlobal );
		pGlobal->HP8753.flags.bHPGLdataValid = TRUE;
	} else {
		// Abandon partial HPGL
		parseHPGL( NULL, pGlobal );
		// make sure we do not attempt to show the HPGL plot
		pGlobal->HP8753.flags.bHPGLdataValid = FALSE;
	}

	// Restore plot quadrant .. if it was previously set
	if( !bFullPagePlot && plotQuadrant < sizeof(optPlotQuadrant) / sizeof(HP8753C_option) ) {
		gchar *plotQuadrantCmd = g_strdup_printf( "%.4s;", optPlotQuadrant[ plotQuadrant ].code );
	    GPIBasyncWrite(descGPIB_HP8753, plotQuadrantCmd, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	    g_free( plotQuadrantCmd );
	}

	return ( GPIBfailed(*pGPIBstatus) );
}

/*!     \brief  Use the learn string to find the active channel
 *
 * There is no GPIB command to find the active channel; however there
 * is a byte in the learn string that indicates this. We use this byte to
 * tell us what channel is active
 *
 * \param  pLearn	pointer to learn string
 * \param  pGlobal	pointer global data
 * \return active channel (channel 1 if no learn string found)
 */
eChannel
getActiveChannelFrom8753learnString( guchar *pLearn, tGlobal *pGlobal ) {
	if( pGlobal->HP8753.pLSindexes == (void *)INVALID )
		return eCH_ONE;
	return pLearn[ pGlobal->HP8753.pLSindexes->iActiveChannel ] == 0x01 ? eCH_ONE : eCH_TWO;
}

/*!     \brief  Use the learn string to find if we display start/stop or center/span sweep
 *
 * There is no GPIB command to find out if the display shows start/stop or center/span; however,
 * there are bytes in the learn string that indicates this. We use these bytes to
 * tell us what style is used
 *
 * \param  pLearn	pointer to learn string
 * \param  pGlobal	pointer global data
 * \return active channel (channel 1 if no learn string found)
 */
gboolean
getStartStopOrCenterSpanFrom8753learnString( guchar *pLearn, tGlobal *pGlobal, eChannel channel ) {
	if( pGlobal->HP8753.pLSindexes == (void *)INVALID )
		return TRUE;
	return pLearn[ pGlobal->HP8753.pLSindexes->iStartStop[ channel] ] == 0x01;
}

/*!     \brief  Process the learn string to extract certain states nbot available through GPIB
 *
 * There are no GPIB commands to find some required data; however there
 * are bytes in the learn string that we can use to determine these data.
 * Based on the firmware version we use different offsets in the learn string to
 * find this.
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGlobal	pointer global data
 * \param  pGPIBstatus		pointer to GPIB status
 * \return OK on success or ERROR
 */
gint
process8753learnString( gint descGPIB_HP8753, guchar *pHP8753C_learn, tGlobal *pGlobal, gint *pGPIBstatus ) {
	tLearnStringIndexes *pLSindexes = pGlobal->HP8753.pLSindexes;

	// we should have the firmware version & learn string by this point
	if( pGlobal->HP8753.firmwareVersion == INVALID
			|| pHP8753C_learn == NULL )
		return ERROR;

	if( pLSindexes == (tLearnStringIndexes *)INVALID ) {
		// No valid offsets found for this firmware
		pGlobal->HP8753.flags.bLearnStringParsed = FALSE;
		return ERROR;
	} else {
		pGlobal->HP8753.flags.bLearnStringParsed = TRUE;
	}

	pGlobal->HP8753.activeChannel
		= pHP8753C_learn[ pLSindexes->iActiveChannel ] == 0x01 ? 0 : 1;

	for ( eChannel channel = eCH_ONE; channel < eNUM_CH; channel++) {
		unsigned short mkrs = 0;
		gint mkrNo, testBit, flagBit;

		pGlobal->HP8753.channels[ channel ].chFlags.bMkrsDelta = FALSE;
		// from the learning string 0x02 is marker1, 0x04 marker 2 etc to 0x10
		// delta can be from 0x02 (delta marker 1) to 0x20 (delta fixed)
		for( mkrNo=0, testBit = 0x02, flagBit = 0x01; mkrNo < MAX_MKRS; mkrNo++, testBit <<= 1, flagBit <<= 1 ) {
			// this doesn't apply to fixed marker
			if( mkrNo < MAX_NUMBERED_MKRS
					&& ( pHP8753C_learn[ pLSindexes->iMarkersOn[ channel ] ] & testBit) )
				mkrs |= flagBit;
			// max of one deta/delta fixed marker
			if( pHP8753C_learn[ pLSindexes->iMarkerDelta[ channel ] ] & testBit ) {
				pGlobal->HP8753.channels[ channel ].deltaMarker = mkrNo;
				pGlobal->HP8753.channels[ channel ].chFlags.bMkrsDelta = TRUE;
			}
			// max of one active marker
			if( pHP8753C_learn[ pLSindexes->iMarkerActive[ channel ] ] & testBit )
				pGlobal->HP8753.channels[ channel ].activeMarker = mkrNo;
		}
		pGlobal->HP8753.channels[ channel ].chFlags.bMkrs = mkrs;

		if( pHP8753C_learn[ pLSindexes->iStartStop[ channel ] ] & 0x01 )
			pGlobal->HP8753.channels[ channel ].chFlags.bCenterSpan = FALSE;
		else
			pGlobal->HP8753.channels[ channel ].chFlags.bCenterSpan = TRUE;

		pGlobal->HP8753.channels[ channel ].nSegments = pHP8753C_learn[ pLSindexes->iNumSegments[ channel ] ];
	}
	return OK;
}

/*!     \brief  Determine the offsets in the learn string that correspond to needed data
 *
 * There are no GPIB commands to find some required data; however there
 * are bytes in the learn string that we can use to determine these data.
 * We change settings with GPIB commands and then read back the learn string to
 * find which bytes have changed in the expected manner
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pLSindex	pointer to the learn string index structure that will be updated
 * \param  pGPIBstatus		pointer to GPIB status
 * \return TRUE on success or FALSE
 */
gboolean
analyze8753learnString( gint descGPIB_HP8753, tLearnStringIndexes *pLSindexes, gint *pGPIBstatus ) {
	guchar *currentStateLS = NULL, *baselineLS = NULL, *modifiedLS = NULL;
	gint LSsize, i, channel, channelFn2;
	gboolean bCompleteWithoutError = FALSE;

#define START_OF_LS_PAYLOAD		4
#define LS_PAYLOAD_SIZE_INDEX	2
	// We can restore the current state after examining changes
    enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );

	// If we have communication problems exit
	if( GPIBfailed(*pGPIBstatus) )
		goto err;

	postInfo("Process Learn String for ...");
	if ( get8753learnString( descGPIB_HP8753, &currentStateLS, pGPIBstatus ) )
		goto err;
	// Preset state
	GPIBasyncWrite(descGPIB_HP8753, "PRES;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &baselineLS, pGPIBstatus ) )
		goto err;

	postInfo("active channel");
	LSsize = GUINT16_FROM_BE(*(guint16 *)(baselineLS+LS_PAYLOAD_SIZE_INDEX));
// Active channel
	GPIBasyncWrite(descGPIB_HP8753, "PRES;S21;CHAN2;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
			goto err;
#define LS_ACTIVE_CHAN1	0x01
#define	LS_ACTIVE_CHAN2	0x02
	for( i=START_OF_LS_PAYLOAD; i < LSsize; i++ ) {
		if( baselineLS[i] == LS_ACTIVE_CHAN1 && modifiedLS[i] == LS_ACTIVE_CHAN2 ) {
			pLSindexes->iActiveChannel = i;
		}
	}

	postInfo("enabled markers");
	// Markers and active marker
	GPIBasyncWrite(descGPIB_HP8753, "PRES;MARK1;MARK4;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
		goto err;
#define LS_NO_MARKERS		0x00
#define	LS_MARKERS_1AND4	0x12
#define LS_NO_ACTIVE_MKRS	0x00
#define LS_ACTIVE_MKR_4		0x10
	for( i=START_OF_LS_PAYLOAD, channel=eCH_ONE, channelFn2=eCH_ONE;
			i < LSsize && (channel <= eCH_TWO || channelFn2 <= eCH_TWO); i++ ) {
		if( baselineLS[i] == LS_NO_MARKERS && modifiedLS[i] == LS_MARKERS_1AND4 ) {
			pLSindexes->iMarkersOn[ channel++ ] = i;
		}
		if( baselineLS[i] == LS_NO_ACTIVE_MKRS && modifiedLS[i] == LS_ACTIVE_MKR_4 ) {
			pLSindexes->iMarkerActive[ channelFn2++ ] = i;
		}
	}

	postInfo("enabled delta marker");
	// Delta Marker
	GPIBasyncWrite(descGPIB_HP8753, "PRES;DELR4;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
		goto err;
#define LS_NO_DELTA_MKR	0x40
#define	LS_DELTA_MKR4	0x10
	for( i=START_OF_LS_PAYLOAD, channel=eCH_ONE; i < LSsize && channel <= eCH_TWO; i++ ) {
		if( baselineLS[i] == LS_NO_DELTA_MKR && modifiedLS[i] == LS_DELTA_MKR4 ) {
			pLSindexes->iMarkerDelta[ channel++ ] = i;
		}
	}

	postInfo("start/stop or center/span");
#define LS_START_STOP	0x01
#define	LS_CENTER_SPAN	0x00
	// Strt/Stop or Center
	GPIBasyncWrite(descGPIB_HP8753, "PRES;CENT1500.15E6;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	GPIBasyncWrite(descGPIB_HP8753, "CHAN2;CENT1500.15E6;CHAN1;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
			goto err;
	for( i=START_OF_LS_PAYLOAD, channel=eCH_ONE; i < LSsize && channel <= eCH_TWO; i++ ) {
		if( baselineLS[i] == LS_START_STOP && modifiedLS[i] == LS_CENTER_SPAN ) {
			pLSindexes->iStartStop[ channel++ ] = i;
		}
	}

	postInfo("polar/smith marker");
#define LS_POLMKR_AngAmp	0x10
#define LS_POLMKR_RI		0x40
#define LS_SMIMKR_RI		0x04
#define LS_SMIMKR_GB		0x08
	GPIBasyncWrite(descGPIB_HP8753, "PRES;POLMRI;SMIMGB;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
			goto err;
	for( i=START_OF_LS_PAYLOAD, channel=eCH_ONE, channelFn2=eCH_ONE;
			i < LSsize && (channel <= eCH_TWO || channelFn2 <= eCH_TWO); i++ ) {
		if( baselineLS[i] == LS_POLMKR_AngAmp && modifiedLS[i] == LS_POLMKR_RI ) {
			pLSindexes->iPolarMkrType[ channel++ ] = i;
		}
		if( baselineLS[i] == LS_SMIMKR_RI && modifiedLS[i] == LS_SMIMKR_GB ) {
			pLSindexes->iSmithMkrType[ channelFn2++ ] = i;
		}
	}

	postInfo("enabled segments");
#define LS_NO_SEGMENTS	0x00
#define	LS_ONE_SEGMENT	0x03
	// Number of list segments
	GPIBasyncWrite(descGPIB_HP8753, "PRES;EDITLIST;SADD;SADD;SADD;EDITDONE;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &modifiedLS, pGPIBstatus ) )
			goto err;
	for( i=START_OF_LS_PAYLOAD, channel=eCH_ONE; i < LSsize && channel <= eCH_TWO; i++ ) {
		if( baselineLS[i] == LS_NO_SEGMENTS && modifiedLS[i] == LS_ONE_SEGMENT ) {
			pLSindexes->iNumSegments[ channel++ ] = i;
		}
	}

	// The PRES commands above will have wiped out the SRQ enable ...
	enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );
    GPIBasyncSRQwrite( descGPIB_HP8753, "NOOP;", NULL_STR, pGPIBstatus, 2.0 * TIMEOUT_RW_1SEC );

	// If we have communication problems exit
	if( GPIBfailed(*pGPIBstatus) )
		goto err;

	// tie this data to the firmware version
	pLSindexes->version = get8753firmwareVersion(descGPIB_HP8753, NULL, pGPIBstatus);

	postInfo("Returning state of HP8753");
	GPIBasyncWrite( descGPIB_HP8753, "FORM1;INPULEAS;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// Includes the 4 byte header with size in bytes (big endian)
	GPIBasyncSRQwrite( descGPIB_HP8753, currentStateLS,
			lengthFORM1data( currentStateLS ),
			pGPIBstatus, 10 * TIMEOUT_RW_1MIN );

	// If the calibration needs to be interpolated, the processing of the learn string can be over a minute
	// A long sweep (narrow IFBW) can take 5 min for both channels
	// Re-applying learn string wipes out SRQ enable
    enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );

	postInfo("");
	if( GPIBsucceeded(*pGPIBstatus) )
		bCompleteWithoutError = TRUE;

err:
	if( !bCompleteWithoutError )
		LOG( G_LOG_LEVEL_CRITICAL, "analyze8753learnString failed", ibsta, iberr );
	g_free( currentStateLS );
	g_free( baselineLS );
	g_free( modifiedLS );
	return( GPIBfailed( *pGPIBstatus ) );
}

/*!     \brief  Get list segments for a channel
 *
 * Get list segments for a channel
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGlobal          pointer to global data
 * \param  channel			channel to get list segments
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 if no error or 1 if error
 */
gint
getHP8753channelListFreqSegments(gint descGPIB_HP8753, tGlobal *pGlobal, eChannel channel, gint *pGPIBstatus ) {
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];

	// This is complicated.. we need to determine the actual frequencies for all points
	// there can be overlapping segments
	if( pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments && pChannel->nSegments > 0 ) {
		gdouble nPoints, startFreq, stopFreq;
		gdouble totalPoints = 0;

		for( int seg = 1; seg <= pChannel->nSegments; seg++ ) {
			// Select the segment and determine the points
			GPIBasyncWriteOneOfN( descGPIB_HP8753, "SSEG%d;", seg, pGPIBstatus, 10 * TIMEOUT_RW_1SEC );
			askHP8753C_dbl(descGPIB_HP8753, "POIN", &nPoints, pGPIBstatus);
			askHP8753C_dbl(descGPIB_HP8753, "STAR", &startFreq, pGPIBstatus);
			askHP8753C_dbl(descGPIB_HP8753, "STOP", &stopFreq, pGPIBstatus);

			pChannel->segments[ seg-1 ].nPoints = (gint)nPoints;
			pChannel->segments[ seg-1 ].startFreq = startFreq;
			pChannel->segments[ seg-1 ].stopFreq = stopFreq;
			pChannel->stimulusPoints = g_realloc( pChannel->stimulusPoints,
										(totalPoints + nPoints) * sizeof( gdouble ));
			if ( nPoints == 1 )
				pChannel->stimulusPoints[ (gint)totalPoints ] = startFreq;
			else
				for( int i=0; i < nPoints; i++ ) {
					gdouble freq = startFreq + i * (stopFreq - startFreq)/(nPoints-1);
					pChannel->stimulusPoints[ (gint)totalPoints + i ] = freq;
				}
			totalPoints += nPoints;
		}
		pChannel->chFlags.bValidSegments = TRUE;
		GPIBasyncWrite(descGPIB_HP8753, "ASEG;MENUON;MENUSTIM;MENUOFF;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	} else {
		pChannel->chFlags.bValidSegments = FALSE;
	}
	return (GPIBfailed(*pGPIBstatus));
}

/*!     \brief  Send calibration kit to HP8753
 *
 * Send the calibration kit to the HP8753
 *
 * \param  descGPIB_HP8753	     GPIB descriptor for HP8753 device
 * \param  tHP8753calibrationKit pointer to calibration kit structure
 * \param  pGPIBstatus		pointer to GPIB status
 * \return 0 if no error or 1 if error
 */
gint
sendHP8753calibrationKit(gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus ) {
	tHP8753calibrationKit *pHP8753calibrationKit = &pGlobal->HP8753calibrationKit;

	GString *stCalKit= g_string_new("CALKN50;MODI1; "); // Get 50 ohm N kit and modify

	for( int i=0; i < MAX_CAL_STANDARDS; i++ ) {
		tHP8753calibrationStandard *pStandard = &pHP8753calibrationKit->calibrationStandards[ i ];
		if( pStandard->bSpecified ) {
			g_string_append_printf( stCalKit, "DEFS%d; FIXE; LABS \"%s\"; ", i+1, pStandard->label );
			switch( pStandard->calibrationType ) {
			case eStdTypeOpen:
				g_string_append_printf( stCalKit, "STDTOPEN; C0 %.17le; C1 %.17e; C2 %.17e; C3 %.17e; ",
						pStandard->C[0], pStandard->C[1], pStandard->C[2], pStandard->C[3]);
				break;
			case eStdTypeShort:
				g_string_append_printf( stCalKit, "STDTSHOR; " );
				break;
			case eStdTypeFixedLoad:
				g_string_append_printf( stCalKit, "STDTLOAD; " );
				break;
			case eStdTypeThru:
				g_string_append_printf( stCalKit, "STDTDELA; " );
				break;
			case eStdTypeSlidingLoad:
				g_string_append_printf( stCalKit, "STDTLOAD; SLIL; " );
				break;
			case eStdTypeArbitraryImpedanceLoad:
				g_string_append_printf( stCalKit, "STDTARBI; TERI%lg; ",
						pStandard->arbitraryZ0 );
				break;
			default:
				break;
			}
			g_string_append_printf( stCalKit, "OFSD %.17le; OFSL %.17e; OFSZ %lg; MINF %lu; MAXF %lu; %s; STDD; ",
					pStandard->offsetDelay, pStandard->offsetLoss, pStandard->offsetZ0,
					pStandard->minFreqHz, pStandard->maxFreqHz,
					pStandard->connectorType == eConnectorTypeCoaxial ? "COAX" : "WAVE");
		}

	}

	const gchar *classMnemonics[] = { "RESP", "RESI", "S11A", "S11B",
							        "S11C", "S22A", "S22B", "S22C",
							        "FWDT", "FWDM", "REVT", "REVM",
									// HP8753D and above
							        "TRFM", "TRRM", "TLFM", "TLFT",
							        "TLRM", "TLRT", "TTFM", "TTFT",
									"TTRM", "TTRT" };

	for( gint classIndex=0; classIndex < MAX_CAL_CLASSES; classIndex++ ) {
		tHP8753calibrationClass *pClass = &pHP8753calibrationKit->calibrationClasses[classIndex];
		if( classIndex >= eHP8753calClassTRLreflectFwdMatch &&
				pGlobal->HP8753.firmwareVersion < 500 )
			break;
		if( !pClass->bSpecified ) {
			g_string_append_printf( stCalKit, "SPEC%s; LABE%s \"N/A\"; ",
					classMnemonics[ classIndex ], classMnemonics[ classIndex ] );
		} else {
			g_string_append_printf( stCalKit, "SPEC%s %s; LABE%s \"%s\"; ",
					classMnemonics[ classIndex ], pClass->standards,
					classMnemonics[ classIndex ], pClass->label);
		}
	}

	g_string_append_printf( stCalKit, "LABK \"%s\"; ", pHP8753calibrationKit->label );
	g_string_append_printf( stCalKit, "KITD; " );
	if( pGlobal->flags.bSaveUserKit )
		g_string_append_printf( stCalKit, "SAVEUSEK; " );
	g_string_append_printf( stCalKit, "MENUCAL;" );
	// LOG( G_LOG_LEVEL_DEBUG, stCalKit->str );
	GPIBasyncWrite( descGPIB_HP8753, stCalKit->str, pGPIBstatus, 10.0 * TIMEOUT_RW_1SEC );
	g_string_free( stCalKit, TRUE );
	return (GPIBfailed(*pGPIBstatus));
}
