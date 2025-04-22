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
get8753setupAndCal( tGPIBinterface *pGPIB_HP8753, tGlobal *pGlobal  ) {

	guint16 CALheaderAndSize[2], CALsize;
	gchar sCommand[ MAX_OUTPCAL_LEN ];
	eChannel channel = eCH_ONE;
	gdouble nPoints = 0;
	gint i, nchannel;

	// clear the status registers and preset the HP8753
	GPIBclear( pGPIB_HP8753 );
	GPIBasyncWrite( pGPIB_HP8753, "CLS;", 30 * TIMEOUT_RW_1SEC);
	usleep( ms(20) );
	GPIBasyncSRQwrite( pGPIB_HP8753, "ESE1;SRE32;NOOP;", NULL_STR, 10 * TIMEOUT_RW_1SEC );

	// abort if we can't get this far
	if( GPIBfailed( pGPIB_HP8753->status ))
		return( FALSE );

	postInfo("Retrieve learn string");
	// Request Learn string (probably don't need to set the format as the LS is always in 'form1')
	GPIBasyncWrite( pGPIB_HP8753, "FORM1;", 	10 * TIMEOUT_RW_1SEC);
	if ( get8753learnString( pGPIB_HP8753, &pGlobal->HP8753cal.pHP8753_learn ))
		goto err;
	// find active channel from learn string
	pGlobal->HP8753cal.settings.bActiveChannel =
			getActiveChannelFrom8753learnString( pGlobal->HP8753cal.pHP8753_learn, pGlobal );

	postInfo("Determine channel configuration");
	// See of we have a coupled source. If uncoupled, we will need to get both sets of calibration correction arrays.
	pGlobal->HP8753cal.settings.bSourceCoupled  = getHP8753switchOnOrOff( pGPIB_HP8753, "COUC" );
	pGlobal->HP8753cal.settings.bDualChannel  = getHP8753switchOnOrOff( pGPIB_HP8753, "DUAC" );
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
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bUseOtherChannelCalArrays = FALSE;
	}

	// Its faster if we stop sweeping ... less for the microprocessor to handle
    channel = pGlobal->HP8753cal.settings.bActiveChannel;
    pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
                        = getHP8753switchOnOrOff( pGPIB_HP8753, "HOLD" );
    GPIBasyncWrite( pGPIB_HP8753, "HOLD;", 10 * TIMEOUT_RW_1SEC);
    // If coupled we need go no further
    if( pGlobal->HP8753cal.settings.bSourceCoupled ) {
            pGlobal->HP8753cal.perChannelCal[ otherChannel( channel ) ].settings.bSweepHold
                = pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold;
    } else {
    	channel = otherChannel( channel );
        // change channel if we are not coupled
        setHP8753channel( pGPIB_HP8753, channel );
        pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
                            = getHP8753switchOnOrOff( pGPIB_HP8753, "HOLD" );
        GPIBasyncWrite( pGPIB_HP8753, "HOLD;", 10 * TIMEOUT_RW_1SEC);
    }

    for( nchannel = 0; nchannel < eNUM_CH; nchannel++ ) {
        // maximum of two sets of calibration correction arrays.
        // if we have a coupled source we break out after retrieving one set

		// Calibration data
		// Depending on the calibration mode, no, 1, 2, 3 or 8 calibration error arrays are retrieved
		postInfo("Determine the type of calibration");
		pGlobal->HP8753cal.perChannelCal[ channel ].iCalType
			= getHP8753calType( pGPIB_HP8753 );
		// If we ask for start/stop this actually changes the display (from start/stop to center/span say)
		// so we ask for the appropriate settings based on the learn string
		if( getStartStopOrCenterSpanFrom8753learnString( pGlobal->HP8753cal.pHP8753_learn, pGlobal, channel ) ) {
			askHP8753_dbl( pGPIB_HP8753, "STAR", &pGlobal->HP8753cal.perChannelCal[ channel ].sweepStart );
			askHP8753_dbl( pGPIB_HP8753, "STOP", &pGlobal->HP8753cal.perChannelCal[ channel ].sweepStop );
		} else {
			gdouble sweepCenter=1500.15e6, sweepSpan=2999.70e6;
			askHP8753_dbl( pGPIB_HP8753, "CENT", &sweepCenter );
			askHP8753_dbl( pGPIB_HP8753, "SPAN", &sweepSpan );
			pGlobal->HP8753cal.perChannelCal[ channel ].sweepStart = sweepCenter - sweepSpan/2.0;
			pGlobal->HP8753cal.perChannelCal[ channel ].sweepStop = sweepCenter + sweepSpan/2.0;
		}
		// Ask for the IF resolution BW
		askHP8753_dbl( pGPIB_HP8753, "IFBW", &pGlobal->HP8753cal.perChannelCal[ channel ].IFbandwidth );
		// Ask for number of points
		askHP8753_dbl( pGPIB_HP8753, "POIN", &nPoints );
		pGlobal->HP8753cal.perChannelCal[ channel ].nPoints = (gint)nPoints;

		pGlobal->HP8753cal.perChannelCal[ channel ].sweepType
			= getHP8753sweepType( pGPIB_HP8753  );
		// Get CW frequency
		askHP8753_dbl( pGPIB_HP8753, "CWFREQ", &pGlobal->HP8753cal.perChannelCal[ channel ].CWfrequency );
		// Are we averaging ?
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bAveraging = askOption( pGPIB_HP8753, "AVERO?;" );

		postInfo("Retrieve the calibration arrays");
		// Get each error coefficient array (up to 12) based on the calibration type
		for (i = 0; i < numOfCalArrays[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ]; i++) {
			// First see if we are using interpolated calibration coefficients
			// If so, take those rather than the regular coefficients
			if( i == 0 && pGlobal->HP8753.firmwareVersion >= 411 ) {	    // OUTPICALnn only available in FW 4.11 and above
				GPIBasyncWrite( pGPIB_HP8753, "OUTPICAL01;", 10 * TIMEOUT_RW_1SEC);	    // https://na.support.keysight.com/8753/firmware/history.htm#53c413
				GPIBasyncRead( pGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, TIMEOUT_RW_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);

				// Flag as being interpolated if there is data in first interpolated array
				if (CALsize > 0) {
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibration;
					postInfo( "Retrieve the interpolated calibration arrays");
				} else {
					GPIBclear( pGPIB_HP8753 );
					pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eNoInterplativeCalibration;
					// Get measured calibration arrays if there are no interpolated arrays
					GPIBasyncWrite( pGPIB_HP8753, "OUTPCALC01;", 10 * TIMEOUT_RW_1SEC);
					GPIBasyncRead( pGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, TIMEOUT_RW_1MIN);
					CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
				}
			} else {
				// Get the other arrays 02 .. 02 (where applicable)
				g_snprintf(sCommand, MAX_OUTPCAL_LEN,
						(pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration)
							? "OUTPICAL%02d;" :"OUTPCALC%02d;", i + 1);
				GPIBasyncWrite( pGPIB_HP8753, sCommand, 10 * TIMEOUT_RW_1SEC);
				// first read header and size of data
				GPIBasyncRead( pGPIB_HP8753, &CALheaderAndSize, HEADER_SIZE, TIMEOUT_RW_1MIN);
				CALsize = GUINT16_FROM_BE(CALheaderAndSize[1]);
			}
			// allocate and read calibration error coefficient array
			if (CALsize > 0) {
				pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i] = g_malloc( CALsize + HEADER_SIZE);
				memmove(pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[i], CALheaderAndSize, HEADER_SIZE);
				GPIBasyncRead( pGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ] + HEADER_SIZE,
						CALsize, TIMEOUT_RW_1MIN);

				if( pGlobal->HP8753cal.settings.bSourceCoupled )
					postInfoWithCount( "Retrieve calibration array %d", i+1, 0 );
				else
					postInfoWithCount( "Retrieve channel %d calibration array %d", channel+1, i+1 );
			}
		}

		// We don't want to have the interpolation on when we send back the learn string
		// because it can cause a long  delay. We will re-enable them after the cal is restored.
		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			if( getHP8753switchOnOrOff( pGPIB_HP8753, "CORI" ) == FALSE ) {
				pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration = eInterplativeCalibrationButNotEnabled;
			} else {
				GPIBasyncWrite( pGPIB_HP8753, "CORIOFF;", 10 * TIMEOUT_RW_1SEC);
			}
		}
		pGlobal->HP8753cal.perChannelCal[ channel ].settings.bValid = TRUE;

		// No need to interrogate the other channel if we are coupled .. just copy the data
		// except for the cal arrays
        if( pGlobal->HP8753cal.settings.bSourceCoupled ) {
			// copy the whole structure
            pGlobal->HP8753cal.perChannelCal[ otherChannel( channel ) ] =
                    pGlobal->HP8753cal.perChannelCal[ channel ];
            // don't copy pointers .. just indicate to use the ones in the other channel
            for (i = 0; i < numOfCalArrays[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ]; i++) {
                pGlobal->HP8753cal.perChannelCal[ otherChannel( channel ) ].pCalArrays[i] = NULL;
            }
            pGlobal->HP8753cal.perChannelCal[ otherChannel( channel ) ].settings.bUseOtherChannelCalArrays = TRUE;
            nchannel++;     // we are done, just skip over the next channel
        } else if( nchannel == 0 ) {
            // change channel if we are not coupled (just the first time)
            channel = otherChannel( channel );
            setHP8753channel( pGPIB_HP8753, channel );
        }
	}

	if( channel != pGlobal->HP8753cal.settings.bActiveChannel )
		setHP8753channel( pGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel );

	// Request modified learn string (without interpolative correction and with hold)
	// so that when we restore we do so efficiently.
	// We will re-enable cal after loading learn string.
	if( get8753learnString( pGPIB_HP8753, &pGlobal->HP8753cal.pHP8753_learn ) != 0)
		goto err;

	// Re-enable interpolative correction and sweeping if it was on previously
	// maximum of two sets of calibration correction arrays.
	// if we have a coupled source we break out after retrieving one set
	for( nchannel = 0; nchannel < eNUM_CH; nchannel++ ) {

		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration )
			GPIBasyncWrite( pGPIB_HP8753, "CORION;", 10 * TIMEOUT_RW_1SEC);

		if( !pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold
				&& ( nchannel == 0 || !pGlobal->HP8753cal.settings.bSourceCoupled ))
			GPIBasyncWrite( pGPIB_HP8753, "CONT;", 10 * TIMEOUT_RW_1SEC);

		if( nchannel == 0 ) {
		    // change channel if we are not coupled (just the first time)
		    channel = otherChannel( channel );
		    setHP8753channel( pGPIB_HP8753, channel );
		}
	}

	// We may have changed channels - return to regularly scheduled programming
	if( pGlobal->HP8753cal.settings.bActiveChannel != channel )
		setHP8753channel( pGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel );

	// beep
	GPIBasyncWrite( pGPIB_HP8753, "EMIB;", 10 * TIMEOUT_RW_1SEC );
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
send8753setupAndCal( tGPIBinterface *pGPIB_HP8753, tGlobal *pGlobal )  {

	gchar sCommand[ MAX_OUTPCAL_LEN ];
	gdouble sweepTime[ eNUM_CH ] = {0};
	eChannel channel = eCH_ONE;
	gdouble totalSweepTime;
	gdouble bUncertainSweepTime = FALSE;
	int i, nchannel;

	// clear the status registers and preset the HP8753
	GPIBclear( pGPIB_HP8753 );

	GPIBasyncWrite( pGPIB_HP8753, "CLS;", 30 * TIMEOUT_RW_1SEC);
    usleep( ms(20) );

	GPIBasyncSRQwrite( pGPIB_HP8753, "PRES;ESE1;SRE32;NOOP;", NULL_STR, 10 * TIMEOUT_RW_1SEC);

	// abort if we can't get this far
	if( GPIBfailed( pGPIB_HP8753->status ))
		return( FALSE );

	GPIBasyncWrite( pGPIB_HP8753, "FORM1;INPULEAS;", 10 * TIMEOUT_RW_1SEC);
	// Includes the 4 byte header with size in bytes (big endian)
	gint LSsize = GUINT16_FROM_BE(*(guint16 *)(pGlobal->HP8753cal.pHP8753_learn+2)) + 4;

	GPIBasyncSRQwrite( pGPIB_HP8753, (gchar *)pGlobal->HP8753cal.pHP8753_learn, LSsize,
			10 * TIMEOUT_RW_1MIN );
	// Restoring the setup seems to reset the ESR and SRQ enable ... so do it here
	GPIBenableSRQonOPC( pGPIB_HP8753 );

	// If the calibration needs to be interpolated, the processing of the learn string can be over a minute
	// A long sweep (narrow IFBW) can take 5 min for both channels
	// on restoration the trigger is in hold and the interpolative cal is disabled
	//  these are restored after cal arrays are sent to the 8753

	// the restored learn string will set the active channel but if the source is uncoupled
	// we need to restore cal arrays for both channels
	if( GPIBfailed( pGPIB_HP8753->status ) )
	    return TRUE;
	GPIBasyncWrite( pGPIB_HP8753, "MENUOFF;", 10 * TIMEOUT_RW_1SEC  );
	for( nchannel = 0, channel = pGlobal->HP8753cal.settings.bActiveChannel; nchannel < eNUM_CH;
			nchannel++ ) {

		if( channel != pGlobal->HP8753cal.settings.bActiveChannel )
			setHP8753channel( pGPIB_HP8753, channel );

		postInfoWithCount( pGlobal->HP8753cal.settings.bSourceCoupled ?
				"Send calibration type" : "Send channel %d calibration type", channel+1, 0 );

		// Set the cal type (need to remove the ? from the string)
		gchar *ts = g_malloc0( strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ) + 1 );
		for(int i=0, j=0; i < strlen( optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code ); i++ )
			if( optCalType[ pGlobal->HP8753cal.perChannelCal[channel].iCalType ].code[ i ] != '?' )
				ts[ j++ ] = optCalType[ pGlobal->HP8753cal.perChannelCal[ channel ].iCalType ].code[ i ];
		// If the channels are coupled, then the cal on / cal off is also coupled
		if( nchannel == 0 || !pGlobal->HP8753cal.settings.bSourceCoupled ) {
			GPIBasyncWrite( pGPIB_HP8753, "CALN", 10 * TIMEOUT_RW_1SEC  );
			GPIBasyncWrite( pGPIB_HP8753, ts, 10 * TIMEOUT_RW_1SEC  );
		}
		g_free( ts );

		// Send the cal arrays
		for( i=0; i < MAX_CAL_ARRAYS && pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ; i++ ) {
			if ( pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[ i ] != NULL ) {
				if ( pGlobal->HP8753cal.settings.bSourceCoupled )
					postInfoWithCount( "Send calibration array %d", i+1, 0 );
				else
					postInfoWithCount( "Send channel %d calibration array %d", channel+1, i+1 );
				g_snprintf( sCommand, MAX_OUTPCAL_LEN, "INPUCALC%02d;", i+1);
				GPIBasyncWrite( pGPIB_HP8753, sCommand, 10 *TIMEOUT_RW_1SEC );

				GPIBasyncSRQwrite( pGPIB_HP8753, pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ],
						lengthFORM1data( pGlobal->HP8753cal.perChannelCal[ channel ].pCalArrays[ i ] ),
						30 * TIMEOUT_RW_1SEC );
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].iCalType != eCALtypeNONE ) {
			postInfoWithCount( pGlobal->HP8753cal.settings.bSourceCoupled ?
					"Save calibration arrays" : "Save channel %d calibration arrays", channel+1, 0 );
		    GPIBasyncWrite( pGPIB_HP8753, "ESE1;SRE32;", 10 * TIMEOUT_RW_1SEC);
		    if( GPIBasyncSRQwrite( pGPIB_HP8753, "SAVC;", NULL_STR,
		    		4 * TIMEOUT_RW_1MIN ) != eRDWT_OK ) {
		        pGPIB_HP8753->status = ERR;
				break;
			}
		}

		if( pGlobal->HP8753cal.perChannelCal[ channel ].settings.bbInterplativeCalibration == eInterplativeCalibration ) {
			postInfoWithCount( pGlobal->HP8753cal.settings.bSourceCoupled ?
					"Enable interpolative correction" : "Enable channel %d interpolative correction", channel+1, 0 );
			GPIBasyncWrite( pGPIB_HP8753, "CORION;", 10 * TIMEOUT_RW_1SEC );
		}
		// Find the sweep time
		askHP8753_dbl( pGPIB_HP8753, "SWET", &sweepTime[ channel ] );

		if( GPIBfailed( pGPIB_HP8753->status ) )
		    return TRUE;

		// If the source is coupled then the settings on both channels are the same
		if( pGlobal->HP8753cal.settings.bSourceCoupled ) {
			nchannel++;	// no need to do the other channel.. this will cause the for loop to end
			sweepTime[ otherChannel( channel ) ] = sweepTime[ channel ];
		} else if ( nchannel == 0 ) {
			// we will only do another loop if we are not coupled
			// and we do not want to change channel if we are going to exit the llop
			channel = otherChannel( channel );
		}
	}

    // Re-enable sweeping if it was on previously
	// we come here on whatever channel we were on from above but because of the for loop
	// channel is set incorrectly ... so adjust accordingly

    if( !pGlobal->HP8753cal.perChannelCal[ channel ].settings.bSweepHold ) {
    	postInfoWithCount( pGlobal->HP8753cal.settings.bSourceCoupled ?
        			"Resume sweeping" : "Resume sweeping channel %d", channel+1, 0 );
        GPIBasyncWrite( pGPIB_HP8753, "CONT;", 10 * TIMEOUT_RW_1SEC);
    }
    if( !pGlobal->HP8753cal.settings.bSourceCoupled
    		&& !pGlobal->HP8753cal.perChannelCal[ otherChannel( channel ) ].settings.bSweepHold ) {
    	channel = otherChannel( channel );
    	setHP8753channel( pGPIB_HP8753, channel );
    	postInfoWithCount( pGlobal->HP8753cal.settings.bSourceCoupled ?
        			"Resume sweeping" : "Resume sweeping channel %d", channel+1, 0 );
        GPIBasyncWrite( pGPIB_HP8753, "CONT;", 10 * TIMEOUT_RW_1SEC);
    }

	// if the source is coupled, then we did not change channel
	if ( pGlobal->HP8753cal.settings.bActiveChannel != channel ) {
		setHP8753channel( pGPIB_HP8753, pGlobal->HP8753cal.settings.bActiveChannel );
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

	postInfo( "Waiting for clean sweep" );
	// Show estimated sweep time in the status line unless we have some doubt as to it's accuracy.
	GPIBasyncSRQwrite( pGPIB_HP8753, "WAIT;", bUncertainSweepTime ? NULL_STR : WAIT_STR,
			(gint) (totalSweepTime * TIMEOUT_SAFETY_FACTOR) );

	// beep
	GPIBasyncWrite( pGPIB_HP8753, "MENUOFF;EMIB", 10 * TIMEOUT_RW_1SEC );

	return GPIBfailed( pGPIB_HP8753->status );

}
