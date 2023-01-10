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

/*!     \brief  Retrieve Setup (learn string) and Calibration data from HP8753
 *
 * Extract the HP8753 saved setup condition (including calibration).
 * If, the interpolative correction is on, this is disabled prior to retrieving
 * the learn string but is noted in the saved configuration.
 * Disabling interpolative correction in the saved learn string speeds up restoration.
 * The interpolative correction is explicitly enabled after setup and
 * calibration is restored (rather than by the learn string).
 *
 * If the source is not coupled, the calibration for both channels are retrieved.
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return TRUE on success or ERROR on problem
 */
gint
get8753setupAndCal( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus ) {

	guint16 CALheaderAndSize[2], CALsize;
	gchar sCommand[ MAX_OUTPCAL_LEN ];
	eChannel channel = eCH_ONE;
	int i;

	postInfo("Retrieve learn string");
	// Request Learn string (probably don't need to set the format as the LS is always in 'form1')
	GPIBwrite(descGPIB_HP8753, "FORM1;", pGPIBstatus);
	if ( get8753learnString( descGPIB_HP8753, &pGlobal->HP8753cal.pHP8753C_learn, pGPIBstatus ))
		goto err;
	// find active channel from learn string
	pGlobal->HP8753cal.settings.bActiveChannel =
			getActiveChannelFrom8753learnString( pGlobal->HP8753cal.pHP8753C_learn, pGlobal );

	postInfo("Determine channel configuration");
	// See of we have a coupled source. If uncoupled, we will need to get both sets of calibration correction arrays.
	pGlobal->HP8753cal.settings.bSourceCoupled  = getHP8753switchOnOrOff( descGPIB_HP8753, "COUC", pGPIBstatus );

	// Initialize the calibration structure before we set them from the current states
	for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		for (i = 0; i < MAX_CAL_ARRAYS; i++) {
			g_free( pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i] );
			pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i] = NULL;
		}
		pGlobal->HP8753cal.perChannelCal[ channel ].iCalType = eCALtypeNONE;
		pGlobal->HP8753cal.perChannelCal[ channel ].nPoints = 0;
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eNoInterplativeCalibration;
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bValid = FALSE;
	}

	// maximum of two sets of calibration correction arrays.
	// if we have a coupled source we break out after retrieving one set
	for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		// The calibration can only be different if the two channels have different
		// source parameters
		if( !pGlobal->HP8753cal.settings.bSourceCoupled )
			setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );

		// we separately save this state so we can selectively restart scanning on restoration
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
							= getHP8753switchOnOrOff( descGPIB_HP8753, "HOLD", pGPIBstatus );
		GPIBwrite(descGPIB_HP8753, "HOLD;", pGPIBstatus);
		// Calibration data
		// Depending on the calibration mode, no, 1, 2, 3 or 8 calibration error arrays are retrieved
		postInfo("Determine the type of calibration");
		pGlobal->HP8753cal.perChannelCal[ channel ].iCalType
			= getHP8753calType( descGPIB_HP8753, pGPIBstatus);
		// If we ask for start/stop this actually changes the display (from start/stop to center/span say)
		// so we ask for the appropriate settings based on the learn string
		if( getStartStopOrCenterSpanFrom8753learnString( pGlobal->HP8753cal.pHP8753C_learn, pGlobal, channel ) ) {
			askHP8753C_dbl(descGPIB_HP8753, "STAR", &pGlobal->HP8753cal.perChannelCal[ channel ].sweepStart, pGPIBstatus);
			askHP8753C_dbl(descGPIB_HP8753, "STOP", &pGlobal->HP8753cal.perChannelCal[ channel ].sweepStop, pGPIBstatus);
		} else {
			gdouble sweepCenter=1500.15e6, sweepSpan=2999.70e6;
			askHP8753C_dbl(descGPIB_HP8753, "CENT", &sweepCenter, pGPIBstatus);
			askHP8753C_dbl(descGPIB_HP8753, "SPAN", &sweepSpan, pGPIBstatus);
			pGlobal->HP8753cal.perChannelCal[ channel ].sweepStart = sweepCenter - sweepSpan/2.0;
			pGlobal->HP8753cal.perChannelCal[ channel ].sweepStop = sweepCenter + sweepSpan/2.0;
		}
		// Ask for the IF resolution BW
		askHP8753C_dbl(descGPIB_HP8753, "IFBW", &pGlobal->HP8753cal.perChannelCal[ channel ].IFbandwidth, pGPIBstatus);
		pGlobal->HP8753cal.perChannelCal[ channel ].sweepType
			= getHP8753sweepType( descGPIB_HP8753, pGPIBstatus );
		// Get CW frequency
		askHP8753C_dbl(descGPIB_HP8753, "CWFREQ", &pGlobal->HP8753cal.perChannelCal[ channel ].CWfrequency, pGPIBstatus);
		// Are we averaging ?
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bAveraging = askOption( descGPIB_HP8753, "AVERO?;", pGPIBstatus );

		postInfo("Retrieve the calibration arrays");
		// Get each error coefficient array (up to 12) based on the calibration type
		for (i = 0; i < numOfCalArrays[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ]; i++) {
			// First see if we are using interpolated calibration coefficients
			// If so, take those rather than the regular coefficients
			if( i == 0 && pGlobal->HP8753.firmwareVersion >= 411 ) {	    // OUTPICALnn only available in FW 4.11 and above
				GPIBwrite(descGPIB_HP8753, "OUTPICAL01;", pGPIBstatus);	    // https://na.support.keysight.com/8753/firmware/history.htm#53c413
				GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_READ_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);

				// Flag as being interpolated if there is data in first interpolated array
				if (CALsize > 0) {
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibration;
					postInfo( "Retrieve the interpolated calibration arrays");
				} else {
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eNoInterplativeCalibration;
					// Get measured calibration arrays if there are no interpolated arrays
					GPIBwrite(descGPIB_HP8753, "OUTPCALC01;", pGPIBstatus);
					GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_READ_1MIN);
					CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
				}
			} else {
				// Get the other arrays 02 .. 02 (where applicable)
				g_snprintf(sCommand, MAX_OUTPCAL_LEN,
						(pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration)
							? "OUTPICAL%02d;" :"OUTPCALC%02d;", i + 1);
				GPIBwrite(descGPIB_HP8753, sCommand, pGPIBstatus);
				// first read header and size of data
				GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_READ_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
			}
			// allocate and read calibration error coefficient array
			if (CALsize > 0) {
				pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i] = g_malloc( CALsize + HEADER_SIZE);
				memmove(pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i], CALheaderAndSize, HEADER_SIZE);
				GPIBasyncRead(descGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ] + HEADER_SIZE,
						CALsize, pGPIBstatus, TIMEOUT_READ_1MIN);

				if( pGlobal->HP8753cal.settings.bSourceCoupled )
					postInfoWithCount( "Retrieve calibration array %d", i+1, 0 );
				else
					postInfoWithCount( "Retrieve channel %d calibration array %d", channel+1, i+1 );

				if( i==0 ) {
					pGlobal->HP8753cal.perChannelCal[ channel ].nPoints = CALsize / BYTES_PER_CALPOINT;
				}
			}
		}

		// We don't want to have the interpolation on when we send back the learn string
		// because it can cause a long  delay. We will re-enable them after the cal is restored.
		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			if( getHP8753switchOnOrOff( descGPIB_HP8753, "CORI", pGPIBstatus ) == FALSE ) {
				pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibrationButNotEnabled;
			} else {
				GPIBwrite(descGPIB_HP8753, "CORIOFF;", pGPIBstatus);
			}
		}

		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bValid = TRUE;
		// Only get the calibration parameters for the first channel
		// if they are duplicated in the second
		if( pGlobal->HP8753cal.settings.bSourceCoupled )
			break;
	}
	// Request modified learn string (without interpolative correction and with hold)
	// so that when we restore we do so efficiently.
	// We will re-enable cal after loading learn string.
	if ( get8753learnString( descGPIB_HP8753, &pGlobal->HP8753cal.pHP8753C_learn, pGPIBstatus ) != 0)
		goto err;

	// Re-enable interpolative correction and sweeping if it was on previously
	if( pGlobal->HP8753.calSettings.bSourceCoupled ) {
		// We didn't change channels so just check channel one
		if( pGlobal->HP8753cal.perChannelCal[ eCH_SINGLE ].settings.bbInterplativeCalibration == eInterplativeCalibration )
			GPIBwrite(descGPIB_HP8753, "CORION;", pGPIBstatus);

		if( !pGlobal->HP8753cal.perChannelCal[ eCH_SINGLE ].settings.bSweepHold )
			GPIBwrite(descGPIB_HP8753, "CONT;", pGPIBstatus);
	} else {
		// channel is eCH_TWO from the for loop above when the source is uncoupled
		// interpolative correction is possibly on in one or both of uncoupled channels
		if( pGlobal->HP8753cal.perChannelCal[ eCH_TWO ].settings.bbInterplativeCalibration == eInterplativeCalibration )
			GPIBwrite(descGPIB_HP8753, "CORION;", pGPIBstatus);

		if( !pGlobal->HP8753cal.perChannelCal[ eCH_TWO ].settings.bSweepHold )
			GPIBwrite(descGPIB_HP8753, "CONT;", pGPIBstatus);

		if( pGlobal->HP8753cal.perChannelCal[ eCH_ONE ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			setHP8753channel( descGPIB_HP8753, channel = eCH_ONE, pGPIBstatus );
			GPIBwrite(descGPIB_HP8753, "CORION;", pGPIBstatus);
		}

		if ( !pGlobal->HP8753cal.perChannelCal[ eCH_TWO ].settings.bSweepHold ) {
			if( channel != eCH_ONE )
				setHP8753channel( descGPIB_HP8753, channel = eCH_ONE, pGPIBstatus );
			GPIBwrite(descGPIB_HP8753, "CONT;", pGPIBstatus);
		}
	}

	// We may have changed channels - return to regularly scheduled programming
	if( !pGlobal->HP8753.calSettings.bSourceCoupled && pGlobal->HP8753cal.settings.bActiveChannel != channel )
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel, pGPIBstatus );

#if 0
	// Continuous Sweep
	GPIBwrite( descGPIB_HP8753, "CONT;", &GPIBstatus );
	GPIBwrite( descGPIB_HP8753, "OPC?;WAIT;", &GPIBstatus);
	// read "1" for complete
	if( GPIBasyncRead( descGPIB_HP8753, &complete, 1, &GPIBstatus, 100.0, pGlobal->messageQueueToGPIB ) != eRD_OK ) {
		GPIBstatus = ERR;
		break;
	}
#endif
	// beep
	GPIBwrite( descGPIB_HP8753, "EMIB;", pGPIBstatus );
	return OK;

err:
	return ERROR;
}


/*!     \brief  Send Setup (learn string) and Calibration data to HP8753
 *
 * Restore the HP8753 to the saved setup condition (including calibration).
 * If, prior to saving, the interpolative correction was on, this was disabled
 * prior to saving the learn string. This speeds up restoration. The interpolative
 * correction is explicitly enabled after calibration is restored.
 * If the source is not coupled, the calibration for both channels are restored.
 *
 * \param  descGPIB_HP8753	GPIB descriptor for HP8753 device
 * \param  pGPIBstatus		pointer to GPIB status
 * \return TRUE on success or ERROR on problem
 */
gint
send8753setupAndCal( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus )  {
	guint8 complete;
	gchar sCommand[ MAX_OUTPCAL_LEN ];
	eChannel channel = eCH_ONE;
	int i;

	GPIBwrite( descGPIB_HP8753, "FORM1;INPULEAS;", pGPIBstatus);
	// Includes the 4 byte header with size in bytes (big endian)
	GPIBwriteBinary( descGPIB_HP8753,
			pGlobal->HP8753cal.pHP8753C_learn,
			GUINT16_FROM_BE(*(guint16 *)(pGlobal->HP8753cal.pHP8753C_learn+2)) + 4, pGPIBstatus );
	GPIBwrite( descGPIB_HP8753, "OPC?;WAIT;", pGPIBstatus);
	GPIBasyncRead( descGPIB_HP8753, &complete, 1, pGPIBstatus, 8 * TIMEOUT_READ_1MIN);
	// on restoration the trigger is in hold and the interpolative cal is disabled
	//  these are restored after cal arrays are sent to the 8753

	// the restored learn string will set the active channel but if the source is uncoupled
	// we need to restore cal arrays for both channels
	for(channel = eCH_ONE; channel < eNUM_CH; channel++ ) {

		// we only to apply to multiple channels if the source is uncoupled
		if( !pGlobal->HP8753cal.settings.bSourceCoupled )
			setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
		if( pGlobal->HP8753cal.settings.bSourceCoupled )
			postInfo( "Set channel calibration type" );
		else
			postInfoWithCount( "Send channel %d calibration type", channel+1, 0 );
		// Set the cal type (need to remove the ? from the string)
		gchar *ts = g_malloc0( strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ) + 1 );
		for(int i=0, j=0; i < strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ); i++ )
			if( optCalType[ pGlobal->HP8753cal.perChannelCal[channel].iCalType ].code[ i ] != '?' )
				ts[ j++ ] = optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code[ i ];
		GPIBwrite( descGPIB_HP8753, ts, pGPIBstatus );
		g_free( ts );

		// Send the cal arrays
		for( i=0; i < MAX_CAL_ARRAYS && pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ; i++ ) {
			if ( pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[ i ] != NULL ) {
				if( pGlobal->HP8753cal.settings.bSourceCoupled )
					postInfoWithCount( "Send calibration array %d", i+1, 0 );
				else
					postInfoWithCount( "Send channel %d calibration array %d", channel+1, i+1 );
				g_snprintf( sCommand, MAX_OUTPCAL_LEN, "INPUCALC%02d;", i+1);
				GPIBwrite( descGPIB_HP8753, sCommand, pGPIBstatus);
				GPIBwriteBinary( descGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ],
						GUINT16_FROM_BE(*(guint16 *)(pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ]+2)) + 4, pGPIBstatus );
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ) {
			if( pGlobal->HP8753cal.settings.bSourceCoupled )
				postInfo ( "Save the calibration arrays" );
			else
				postInfoWithCount( "Save channel %d calibration arrays", channel+1, 0 );

			GPIBwrite( descGPIB_HP8753, "OPC?;SAVC;", pGPIBstatus);
			// read "1" for complete
			if( GPIBasyncRead( descGPIB_HP8753, &complete, 1, pGPIBstatus, TIMEOUT_SWEEP ) != eRDWT_OK ) {
				*pGPIBstatus = ERR;
				break;
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			if( pGlobal->HP8753cal.settings.bSourceCoupled )
				postInfo ( "Enable interpolative correction" );
			else
				postInfoWithCount( "Enable channel %d interpolative correction", channel+1, 0 );
			GPIBwrite(descGPIB_HP8753, "CORION;", pGPIBstatus);
		}

		// Continuous Sweep
		if( !pGlobal->HP8753cal.perChannelCal[channel].settings.bSweepHold ) {
			if( pGlobal->HP8753cal.settings.bSourceCoupled )
				postInfo ( "Start sweeping" );
			else
				postInfoWithCount( "Start sweeping channel %d", channel+1, 0 );
			GPIBwrite( descGPIB_HP8753, "CONT;", pGPIBstatus );
		}

		// if coupled we need to do this only for channel one
		if( pGlobal->HP8753cal.settings.bSourceCoupled )
			break;
	}

	// if the source is coupled, then we did not change channel
	if ( !pGlobal->HP8753cal.settings.bSourceCoupled && pGlobal->HP8753cal.settings.bActiveChannel != channel )
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel, pGPIBstatus );

	GPIBwrite( descGPIB_HP8753, "OPC?;WAIT;", pGPIBstatus);
	GPIBasyncRead( descGPIB_HP8753, &complete, 1, pGPIBstatus, TIMEOUT_SWEEP );

	GPIBwrite( descGPIB_HP8753, "MENUOFF;", pGPIBstatus );
	// beep
	GPIBwrite( descGPIB_HP8753, "EMIB;", pGPIBstatus );

	return GPIBfailed( *pGPIBstatus );

}
