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
 * \param  descGPIB_HP8753  GPIB descriptor for HP8753 device
 * \param  pGPIBstatus      pointer to GPIB status
 * \return TRUE on success or ERROR on problem
 */
gint
get8753setupAndCal( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus ) {

	guint16 CALheaderAndSize[2], CALsize;
	gchar sCommand[ MAX_OUTPCAL_LEN ];
	eChannel channel = eCH_ONE;
	gdouble nPoints = 0;
	int i;

	postInfo("Retrieve learn string");
	enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );
	// Request Learn string (probably don't need to set the format as the LS is always in 'form1')
	GPIBasyncWrite(descGPIB_HP8753, "FORM1;", pGPIBstatus, 	10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( descGPIB_HP8753, &pGlobal->HP8753cal.pHP8753C_learn, pGPIBstatus ))
		goto err;
	// find active channel from learn string
	pGlobal->HP8753cal.settings.bActiveChannel =
			getActiveChannelFrom8753learnString( pGlobal->HP8753cal.pHP8753C_learn, pGlobal );

	postInfo("Determine channel configuration");
	// See of we have a coupled source. If uncoupled, we will need to get both sets of calibration correction arrays.
	pGlobal->HP8753cal.settings.bSourceCoupled  = getHP8753switchOnOrOff( descGPIB_HP8753, "COUC", pGPIBstatus );
	pGlobal->HP8753cal.settings.bDualChannel  = getHP8753switchOnOrOff( descGPIB_HP8753, "DUAC", pGPIBstatus );
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

	// Its faster if we stop sweeping ... less for the microprocessor to handle
	for( channel = eCH_ONE;  channel < eNUM_CH; channel++ ) {
		// we separately save this state so we can selectively restart scanning on restoration
		setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
						= getHP8753switchOnOrOff( descGPIB_HP8753, "HOLD", pGPIBstatus );
		GPIBasyncWrite(descGPIB_HP8753, "HOLD;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

		// If coupled we need go no further
		if( pGlobal->HP8753cal.settings.bSourceCoupled ) {
			pGlobal->HP8753cal.perChannelCal[ eCH_TWO ].settings.bSweepHold
				= pGlobal->HP8753cal.perChannelCal[ eCH_ONE ].settings.bSweepHold;
			break;
		}
	}


	// maximum of two sets of calibration correction arrays.
	// if we have a coupled source we break out after retrieving one set
	for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
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
		// Ask for number of points
		askHP8753C_dbl(descGPIB_HP8753, "POIN", &nPoints, pGPIBstatus);
		pGlobal->HP8753cal.perChannelCal[ channel ].nPoints = (gint)nPoints;

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
				GPIBasyncWrite(descGPIB_HP8753, "OUTPICAL01;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);	    // https://na.support.keysight.com/8753/firmware/history.htm#53c413
				GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_RW_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);

				// Flag as being interpolated if there is data in first interpolated array
				if (CALsize > 0) {
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibration;
					postInfo( "Retrieve the interpolated calibration arrays");
				} else {
					ibclr( descGPIB_HP8753 );
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eNoInterplativeCalibration;
					// Get measured calibration arrays if there are no interpolated arrays
					GPIBasyncWrite(descGPIB_HP8753, "OUTPCALC01;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
					GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_RW_1MIN);
					CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
				}
			} else {
				// Get the other arrays 02 .. 02 (where applicable)
				g_snprintf(sCommand, MAX_OUTPCAL_LEN,
						(pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration)
							? "OUTPICAL%02d;" :"OUTPCALC%02d;", i + 1);
				GPIBasyncWrite(descGPIB_HP8753, sCommand, pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
				// first read header and size of data
				GPIBasyncRead(descGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, pGPIBstatus, TIMEOUT_RW_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
			}
			// allocate and read calibration error coefficient array
			if (CALsize > 0) {
				pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i] = g_malloc( CALsize + HEADER_SIZE);
				memmove(pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i], CALheaderAndSize, HEADER_SIZE);
				GPIBasyncRead(descGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ] + HEADER_SIZE,
						CALsize, pGPIBstatus, TIMEOUT_RW_1MIN);

				if( pGlobal->HP8753cal.settings.bSourceCoupled )
					postInfoWithCount( "Retrieve calibration array %d", i+1, 0 );
				else
					postInfoWithCount( "Retrieve channel %d calibration array %d", channel+1, i+1 );
			}
		}

		// We don't want to have the interpolation on when we send back the learn string
		// because it can cause a long  delay. We will re-enable them after the cal is restored.
		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			if( getHP8753switchOnOrOff( descGPIB_HP8753, "CORI", pGPIBstatus ) == FALSE ) {
				pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibrationButNotEnabled;
			} else {
				GPIBasyncWrite(descGPIB_HP8753, "CORIOFF;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
			}
		}
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bValid = TRUE;
	}

	if( channel != pGlobal->HP8753cal.settings.bActiveChannel )
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel, pGPIBstatus );
	// Request modified learn string (without interpolative correction and with hold)
	// so that when we restore we do so efficiently.
	// We will re-enable cal after loading learn string.
	if ( get8753learnString( descGPIB_HP8753, &pGlobal->HP8753cal.pHP8753C_learn, pGPIBstatus ) != 0)
		goto err;

	// Re-enable interpolative correction and sweeping if it was on previously
	// maximum of two sets of calibration correction arrays.
	// if we have a coupled source we break out after retrieving one set
	for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );

		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration )
			GPIBasyncWrite(descGPIB_HP8753, "CORION;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

		if( !pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
				&& ( channel == eCH_ONE || !pGlobal->HP8753cal.settings.bSourceCoupled ))
			GPIBasyncWrite(descGPIB_HP8753, "CONT;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	}

	// We may have changed channels - return to regularly scheduled programming
	if( pGlobal->HP8753cal.settings.bActiveChannel != channel )
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel, pGPIBstatus );

	// beep
	GPIBasyncWrite( descGPIB_HP8753, "EMIB;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC );
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
	gchar sCommand[ MAX_OUTPCAL_LEN ];
	gdouble sweepTime[ eNUM_CH ] = {0};
	eChannel channel = eCH_ONE;
	gdouble totalSweepTime;
	gdouble bUncertainSweepTime = FALSE;
	int i, nchannels;

	enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );
	GPIBasyncWrite( descGPIB_HP8753, "FORM1;INPULEAS;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
	// Includes the 4 byte header with size in bytes (big endian)
	gint LSsize = GUINT16_FROM_BE(*(guint16 *)(pGlobal->HP8753cal.pHP8753C_learn+2)) + 4;

	GPIBasyncSRQwrite( descGPIB_HP8753, (gchar *)pGlobal->HP8753cal.pHP8753C_learn, LSsize,
			pGPIBstatus, 10 * TIMEOUT_RW_1MIN );
	// Restoring the setup seems to reset the ESR and SRQ enable ... so do it here
	enableSRQonOPC( descGPIB_HP8753, pGPIBstatus );

	// If the calibration needs to be interpolated, the processing of the learn string can be over a minute
	// A long sweep (narrow IFBW) can take 5 min for both channels
	// on restoration the trigger is in hold and the interpolative cal is disabled
	//  these are restored after cal arrays are sent to the 8753

	// the restored learn string will set the active channel but if the source is uncoupled
	// we need to restore cal arrays for both channels
	if( GPIBfailed( *pGPIBstatus ) )
	    return TRUE;
	GPIBasyncWrite( descGPIB_HP8753, "MENUOFF;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC  );
	for( nchannels = 0, channel = pGlobal->HP8753cal.settings.bActiveChannel; nchannels < eNUM_CH;
			nchannels++, channel = (channel + 1) % eNUM_CH ) {
		if( channel != pGlobal->HP8753cal.settings.bActiveChannel )
			setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
		postInfoWithCount( "Send channel %d calibration type", channel+1, 0 );

		// Set the cal type (need to remove the ? from the string)
		gchar *ts = g_malloc0( strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ) + 1 );
		for(int i=0, j=0; i < strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ); i++ )
			if( optCalType[ pGlobal->HP8753cal.perChannelCal[channel].iCalType ].code[ i ] != '?' )
				ts[ j++ ] = optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code[ i ];
		// If the channels are coupled, then the cal on / cal off is also coupled
		if( nchannels == 0 || !pGlobal->HP8753cal.settings.bSourceCoupled ) {
			GPIBasyncWrite( descGPIB_HP8753, "CALN", pGPIBstatus, 10 * TIMEOUT_RW_1SEC  );
			GPIBasyncWrite( descGPIB_HP8753, ts, pGPIBstatus, 10 * TIMEOUT_RW_1SEC  );
		}
		g_free( ts );

		// Send the cal arrays
		for( i=0; i < MAX_CAL_ARRAYS && pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ; i++ ) {
			if ( pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[ i ] != NULL ) {
				postInfoWithCount( "Send channel %d calibration array %d", channel+1, i+1 );
				g_snprintf( sCommand, MAX_OUTPCAL_LEN, "INPUCALC%02d;", i+1);
				GPIBasyncWrite( descGPIB_HP8753, sCommand, pGPIBstatus, 10 *TIMEOUT_RW_1SEC );

				GPIBasyncSRQwrite( descGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ],
						lengthFORM1data( pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ] ),
						pGPIBstatus, 20 * TIMEOUT_RW_1SEC );
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ) {
			postInfoWithCount( "Save channel %d calibration arrays", channel+1, 0 );
		    GPIBasyncWrite(descGPIB_HP8753, "ESE1;SRE32;", pGPIBstatus,  10 * TIMEOUT_RW_1SEC);
		    if( GPIBasyncSRQwrite( descGPIB_HP8753, "SAVC;", NULL_STR,
		    		pGPIBstatus, 4 * TIMEOUT_RW_1MIN ) != eRDWT_OK ) {
				*pGPIBstatus = ERR;
				break;
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			postInfoWithCount( "Enable channel %d interpolative correction", channel+1, 0 );
			GPIBasyncWrite(descGPIB_HP8753, "CORION;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC );
		}
		// Find the sweep time
		askHP8753C_dbl(descGPIB_HP8753, "SWET", &sweepTime[ channel ], pGPIBstatus);

		if( GPIBfailed( *pGPIBstatus ) )
		    return TRUE;
	}

    // Re-enable sweeping if it was on previously
    for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
        setHP8753channel( descGPIB_HP8753, channel, pGPIBstatus );
        if( !pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold ) {
            if( pGlobal->HP8753cal.settings.bSourceCoupled )
            	postInfo( "Resume sweeping" );
            else
            	postInfoWithCount( "Resume sweeping channel %d", channel+1, 0 );
            GPIBasyncWrite(descGPIB_HP8753, "CONT;", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
        }
        // Sweep on one means a sweep on the other if coupled
        if( pGlobal->HP8753cal.settings.bSourceCoupled )
        	break;
    }

	// if the source is coupled, then we did not change channel
	if ( pGlobal->HP8753cal.settings.bActiveChannel != channel ) {
		setHP8753channel( descGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel, pGPIBstatus );
		channel = pGlobal->HP8753cal.settings.bActiveChannel;
	}

	// Estimate the time to do a complete sweep of
	// if its long (more than 10 seconds ... add this to the status indicator// active channel sweep time
	bUncertainSweepTime = FALSE;
	totalSweepTime = sweepTime[ channel ];

	// We have at least the active channel
	// If calibration correction is applied the HP8753 has to do a scan of the opposing port also
	switch ( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ) {
	case eCALtypeNONE:
	case eCALtypeS11onePort:
	case eCALtypeS22onePort:
	case eCALtypeRESPONSE:
	case eCALtypeRESPONSEandISOLATION:
		break;
	case eCALtypeFullTwoPort:
	case eCALtype1pathTwoPort:
	case eCALtypeTRL_LRM_TwoPort:
		totalSweepTime += sweepTime[ channel ];
		break;
	default:
		break;
	}

	if ( pGlobal->HP8753cal.settings.bDualChannel ) {
		channel = (channel == eCH_ONE ? eCH_TWO : eCH_ONE);
		if( !pGlobal->HP8753cal.settings.bSourceCoupled ) {	// uncoupled
			totalSweepTime += sweepTime[ channel ];
			switch ( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ) {
			case eCALtypeFullTwoPort:
			case eCALtype1pathTwoPort:
			case eCALtypeTRL_LRM_TwoPort:
				totalSweepTime += sweepTime[ channel ];
				break;
			default:
				break;
			}
		} else {
			// If the sources are coupled, the scan time may still increase if there is no calibration.
			// This is because one channel may be measuring Port 1 and the other Port2
			// if we measure S11 + S22 or S12 + S21 or S11 + S21 or S22 + S12 ( on either channel)
			switch ( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ) {
			case eCALtypeNONE:
			case eCALtypeS11onePort:
			case eCALtypeS22onePort:
			case eCALtypeRESPONSE:
			case eCALtypeRESPONSEandISOLATION:
				bUncertainSweepTime = TRUE;
				totalSweepTime += sweepTime[ channel ];
				break;
			default:
				break;
			}
		}
	}

	// We need to wait for a clean sweep  ... the 8753 is not useful until it does sweep in any case
	if( totalSweepTime < 5.0 )
		totalSweepTime = 10.0;

	// Show estimated sweep time in the status line unless we have some doubt as to it's accuracy.
	GPIBasyncSRQwrite( descGPIB_HP8753, "WAIT;", bUncertainSweepTime ? NULL_STR : WAIT_STR,
			pGPIBstatus, (gint) (totalSweepTime * TIMEOUT_SAFETY_FACTOR) );

	// beep
	GPIBasyncWrite( descGPIB_HP8753, "MENUOFF;EMIB", pGPIBstatus, 10 * TIMEOUT_RW_1SEC );

	return GPIBfailed( *pGPIBstatus );

}
