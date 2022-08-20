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

/*!     \brief  Write (possibly) binary data to the GPIB device
 *
 * Send binary data to the GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param length         number of bytes to send
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR
 */
gint
GPIBwriteBinary( gint GPIBdescriptor, const void *sData, gint length, gint *pGPIBstatus ) {
	if( GPIBfailed( *pGPIBstatus ) ) {
		return ERROR;
	} else {
		DBG( eDEBUG_EXTENSIVE, "🖊       : %d bytes", length );
		*pGPIBstatus = ibwrt( GPIBdescriptor, sData, length );
	}

	if( GPIBfailed( *pGPIBstatus ) ) {
		LOG( G_LOG_LEVEL_CRITICAL, "GPIB write status/error: %04X/%d", ibsta, iberr );
		return ERROR;
	} else {
		return (ibcnt);
	}
}

/*!     \brief  Read data from the GPIB device
 *
 * Read data from the GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR
 */
gint
GPIBread( gint GPIBdescriptor, void *sData, gint maxBytes, gint *pGPIBstatus ) {
	if( GPIBfailed( *pGPIBstatus ) ) {
		return ERROR;
	} else {
		*pGPIBstatus = ibrd( GPIBdescriptor, sData, maxBytes );
		DBG( eDEBUG_EXTENSIVE, "👓 HP8753: %d (%d max)", ibcnt, maxBytes );
	}

	if( GPIBfailed( *pGPIBstatus ) ) {
		LOG( G_LOG_LEVEL_CRITICAL, "GPIB read status/error: %04X/%d", ibsta, iberr );
		return ERROR;
	} else {
		return (ibcnt);
	}
}

/*!     \brief  Write string to the GPIB device
 *
 * Send NULL terminated string to the GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR
 */
gint
GPIBwrite( gint GPIBdescriptor, const void *sData, gint *GPIBstatus ) {
	DBG( eDEBUG_EXTENSIVE, "🖊 HP8753: %s", sData );
	return GPIBwriteBinary( GPIBdescriptor, sData, strlen( (gchar *)sData ), GPIBstatus );
}

/*!     \brief  Write string with a numeric value to the GPIB device
 *
 * Send NULL terminated string with numeric paramater to the GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param number         number to embed in string (%d)
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR (from GPIBwriteBinary)
 */
gint
GPIBwriteOneOfN( gint GPIBdescriptor, const void *sData, gint number, gint *GPIBstatus ) {
	gchar *sCmd = g_strdup_printf( sData, number );
	DBG( eDEBUG_EXTENSIVE, "👉 HP8753: %s", sCmd );
	return GPIBwrite( GPIBdescriptor, sCmd, GPIBstatus );
	g_free ( sCmd );
}

/*!     \brief  See if there are messages on the asynchronous queue
 *
 * If the argument contains a pointer to a queue, set the default queue to it
 *
 * Check the queue and report the number of messages
 *
 * \param asyncQueue  pointer to async queue (or NULL)
 * \return            number of messages in queue
 */
static gint
GPIB_checkQueue( GAsyncQueue *asyncQueue )
{
	static GAsyncQueue *queueToCheck = NULL;

	if( asyncQueue )
		queueToCheck = asyncQueue;

	if( !asyncQueue ) {
		return g_async_queue_length( queueToCheck );
	} else {
		return 0;
	}
}

#define THIRTY_MS 0.030
#define FIVE_SECONDS 5.0
/*!     \brief  Write data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param readBuffer     pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeout        the maximum time to wait before abandoning
 * \param queue          message queue from main thread
 * \return               read status result
 */
tGPIBReadWriteStatus
GPIBasyncWriteBinary( gint GPIBdescriptor, const void *sData, gint length, gint *pGPIBstatus, gdouble timeoutSecs )
{
	gint currentTimeout;
	gdouble waitTime = 0.0;
	tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

	if( GPIBfailed( *pGPIBstatus ) ) {
		return eRDWT_PREVIOUS_ERROR;
	}

	ibask(GPIBdescriptor, IbaTMO, &currentTimeout);
	ibtmo( GPIBdescriptor, TNONE );

	*pGPIBstatus = ibwrta( GPIBdescriptor, sData, length );

	if( GPIBfailed( *pGPIBstatus ) )
		return eRDWT_ERROR;
	//todo - remove when linux GPIB driver fixed
	// a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
	// we delay, so that the timeout used is TNONE bedore changing to T30ms
	usleep( 20 * 1000 );

	// set the timout for the ibwait to 30ms
	ibtmo( GPIBdescriptor, T30ms );
	do {
		// Wait for read completion or timeout
		*pGPIBstatus = ibwait(GPIBdescriptor, TIMO | CMPL | END);
		if( (*pGPIBstatus & TIMO) == TIMO ){
			// Timeout
			rtn = eRDWT_CONTINUE;
			waitTime += THIRTY_MS;
			if( waitTime > FIVE_SECONDS && fmod( waitTime, 1.0 ) < THIRTY_MS ) {
				gchar *sMessage =  g_strdup_printf( "Waiting for HP8753: %ds", (gint)(waitTime) );
				postInfo( sMessage );
				g_free( sMessage );
			}
		} else {
			// did we have a read error
			if((*pGPIBstatus & ERR) == ERR )
				rtn= eRDWT_ERROR;
			// or did we complete the read
			else if( (*pGPIBstatus & CMPL) == CMPL ||  (*pGPIBstatus & END) == END )
				rtn = eRDWT_OK;
		}
		// If we get a message on the queue, it is assumed to be an abort
		if( GPIB_checkQueue( NULL ) )
			rtn = eRDWT_ABORT;
	} while( rtn == eRDWT_CONTINUE && (waitTime < timeoutSecs)  );


	if( rtn != eRDWT_OK )
		ibstop( GPIBdescriptor );;

	*pGPIBstatus = AsyncIbsta();

	DBG( eDEBUG_EXTENSIVE, "🖊 HP8753: %d / %d bytes", AsyncIbcnt(), length );

	if( (*pGPIBstatus & CMPL) != CMPL )
		LOG( G_LOG_LEVEL_INFO, "GPIB async write status/error: %04X/%d", AsyncIbsta(), AsyncIberr() );

	ibtmo( GPIBdescriptor, currentTimeout);
	return ( rtn == eRDWT_CONTINUE ? eRDWT_TIMEOUT : rtn );
}

/*!     \brief  Write (async) string to the GPIB device
 *
 * Send NULL terminated string to the GPIB device (asynchronously)
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR
 */
tGPIBReadWriteStatus
GPIBasyncWrite( gint GPIBdescriptor, const void *sData, gint *pGPIBstatus, gdouble timeoutSecs ) {
	DBG( eDEBUG_EXTENSIVE, "🖊 HP8753: %s", sData );
	return GPIBasyncWriteBinary( GPIBdescriptor, sData, strlen( (gchar *)sData ), pGPIBstatus, timeoutSecs );
}


/*!     \brief  Read data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param readBuffer     pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeout        the maximum time to wait before abandoning
 * \param queue          message queue from main thread
 * \return               read status result
 */
tGPIBReadWriteStatus
GPIBasyncRead( gint GPIBdescriptor, void *readBuffer, long maxBytes, gint *pGPIBstatus, gdouble timeoutSecs )
{
	gint currentTimeout;
	gdouble waitTime = 0.0;
	tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

	if( GPIBfailed( *pGPIBstatus ) ) {
		return eRDWT_PREVIOUS_ERROR;
	}

	ibask(GPIBdescriptor, IbaTMO, &currentTimeout);
	// for the read itself we have no timeout .. we loop using ibwait with short timeout
	ibtmo( GPIBdescriptor, TNONE );
	*pGPIBstatus = ibrda( GPIBdescriptor, readBuffer, maxBytes );

	if( GPIBfailed( *pGPIBstatus ) )
		return eRDWT_ERROR;

	//todo - remove when linux GPIB driver fixed
	// a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
	// we delay, so that the timeout used is TNONE bedore changing to T30ms
	usleep( 20 * 1000 );

	// set the timout for the ibwait to 30ms
	ibtmo( GPIBdescriptor, T30ms );
	do {
		// Wait for read completion or timeout
		*pGPIBstatus = ibwait(GPIBdescriptor, TIMO | CMPL | END);
		if( (*pGPIBstatus & TIMO) == TIMO ){
			// Timeout
			rtn = eRDWT_CONTINUE;
			waitTime += THIRTY_MS;
			if( waitTime > FIVE_SECONDS && fmod( waitTime, 1.0 ) < THIRTY_MS ) {
				gchar *sMessage =  g_strdup_printf( "Waiting for HP8753: %ds", (gint)(waitTime) );
				postInfo( sMessage );
				g_free( sMessage );
			}
		} else {
			// did we have a read error
			if((*pGPIBstatus & ERR) == ERR )
				rtn= eRDWT_ERROR;
			// or did we complete the read
			else if( (*pGPIBstatus & CMPL) == CMPL ||  (*pGPIBstatus & END) == END )
				rtn = eRDWT_OK;
		}
		// If we get a message on the queue, it is assumed to be an abort
		if( GPIB_checkQueue( NULL ) )
			rtn = eRDWT_ABORT;
	} while( rtn == eRDWT_CONTINUE && (waitTime < timeoutSecs)  );

	if( rtn != eRDWT_OK )
		ibstop( GPIBdescriptor );;

	*pGPIBstatus = AsyncIbsta();

	DBG( eDEBUG_EXTENSIVE, "👓 HP8753: %d bytes (%d max)", AsyncIbcnt(), maxBytes );

	if( (*pGPIBstatus & CMPL) != CMPL )
		LOG( G_LOG_LEVEL_INFO, "GPIB async read status/error: %04X/%d", AsyncIbsta(), AsyncIberr() );

	ibtmo( GPIBdescriptor, currentTimeout);
	return ( rtn == eRDWT_CONTINUE ? eRDWT_TIMEOUT : rtn );
}

/*!     \brief  Read configuration value from the GPIB device
 *
 * Read configuration value for GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param option         the paramater to read
 * \param result         pointer to the integer receiving the value
 * \param pGPIBstatus    pointer to GPIB status
 * \return               OK or ERROR
 */
int
GPIBreadConfiguration( gint GPIBdescriptor, gint option, gint *result, gint *pGPIBstatus ) {
	*pGPIBstatus = ibask( GPIBdescriptor, option, result );

	if( GPIBfailed( *pGPIBstatus ) )
		return ERROR;
	else
		return OK;
}



const gint numOfCalArrays[] =
	{ 	0, 	// None
		1,	// Response
		2,	// Response & Isolation
		3,	// S11 1-port
		3,	// S11 1-port
		12,	// Full 2-port
		12, // One path 2-port
		12  // TRL*/LRM* 2-port
	};

/*!     \brief  Ping GPIB device
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param descGPIBboard  GPIB controller board descriptor
 * \param descGPIBdevice GPIB device descriptor
 * \param pGPIBstatus    pointer to GPIB status
 * \return               TRUE if device responds or FALSE if not
 */
static
gboolean pingGPIBdevice(gint descGPIBboard, gint descGPIBdevice, gint *pGPIBstatus) {
	gint PID = INVALID;
	gint timeout;

	gshort bFound = FALSE;
	// save old timeout
	if( (*pGPIBstatus = ibask(descGPIBboard, IbaTMO, &timeout)) & ERR )
		goto err;
	// set new timeout (for ping purpose only)
	if( (*pGPIBstatus = ibtmo(descGPIBboard, T1s)) & ERR )
		goto err;
	// Get the device PID
	if( (*pGPIBstatus = ibask(descGPIBdevice, IbaPAD, &PID)) & ERR )
			goto err;
	// Actually do the ping
	if ((*pGPIBstatus = ibln(descGPIBboard, PID,  NO_SAD, &bFound)) & ERR)
		goto err;

	*pGPIBstatus = ibtmo(descGPIBboard, timeout);

err:
	return (bFound);
}


#define GPIB_EOI		TRUE
#define	GPIB_EOS_NONE	0

/*!     \brief  open the GPIB device
 *
 * Get the device descriptors of the contraller and GPIB device
 * based on the paramaters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pDescGPIBcontroller pointer to GPIB controller descriptor
 * \param pDescGPIB_HP8753    pointer to GPIB device descriptor
 * \return                    0 on sucess or ERROR on failure
 */
gint
findGPIBdescriptors( tGlobal *pGlobal, gint *pDescGPIBcontroller, gint *pDescGPIB_HP8753 ) {
	gshort lineStatus;
	gint GPIBstatus = 0;

	// The board index can be used as a device descriptor; however,
	// if a device desripter was returned from the ibfind, it mist be freed
	// with ibnol().
	// The board index corresponds to the minor number /dev/
	// $ ls -l /dev/gpib0
	// crw-rw----+ 1 root root 160, 0 May 12 09:24 /dev/gpib0
#define FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR 16
	// raise(SIGSEGV);

	if( *pDescGPIBcontroller != INVALID
			&& *pDescGPIBcontroller >= FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR )
		ibonl(*pDescGPIBcontroller, 0);

	if( *pDescGPIB_HP8753 != INVALID ) {
		ibonl(*pDescGPIB_HP8753, 0);
	}

	*pDescGPIBcontroller = INVALID;
	*pDescGPIB_HP8753 = INVALID;

	gchar *string;

	if( pGlobal->flags.bGPIB_UseCardNoAndPID ) {
			if( pGlobal->GPIBcontrollerIndex >= 0 &&
			    pGlobal->GPIBcontrollerIndex < GPIB_MAX_NUM_BOARDS ) {
				*pDescGPIBcontroller = pGlobal->GPIBcontrollerIndex;
				ibtmo( *pDescGPIBcontroller, T30s );
			}
	} else {
		*pDescGPIBcontroller = ibfind(pGlobal->sGPIBcontrollerName);
	}

	if( *pDescGPIBcontroller == ERROR ) {
		string = g_strdup_printf("Cannot find GPIB controller (index %d)", pGlobal->GPIBcontrollerIndex);
		postError( string );
		g_free(string);
		return ERROR;
	} else {
		// check if it's really there
		if ( GPIBfailed( iblines(*pDescGPIBcontroller, &lineStatus) )) {
			if ( *pDescGPIBcontroller >= FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR )
				ibonl(*pDescGPIBcontroller, 0);
			*pDescGPIBcontroller = INVALID;
			postError("Cannot find GPIB controller");
			return ERROR;
		}
	}

	// Now look for the HP8753
	if( pGlobal->flags.bGPIB_UseCardNoAndPID ) {
		if ( pGlobal->GPIBdevicePID >= 0 )
			*pDescGPIB_HP8753 = ibdev(pGlobal->GPIBcontrollerIndex, pGlobal->GPIBdevicePID, 0, T3s, GPIB_EOI, GPIB_EOS_NONE);
	} else {
		*pDescGPIB_HP8753 = ibfind(pGlobal->sGPIBdeviceName);
		ibeot(*pDescGPIB_HP8753, GPIB_EOI);
	}

	if (*pDescGPIB_HP8753 == ERROR ) {
		postError( "Cannot find HP8753C" );
		return ERROR;
	}

	if( ! pingGPIBdevice( *pDescGPIBcontroller, *pDescGPIB_HP8753, &GPIBstatus) ) {
		postError( "Cannot contact HP8753C" );
		return ERROR;
	} else {
		postInfo( "Contact with HP8753 established");
        usleep( 20000 );
		ibloc( *pDescGPIB_HP8753 );
	}
	return 0;
}


/*!     \brief  Thread to communicate with GPIB
 *
 * Start thread berform asynchronous GPIB communication
 *
 * \param _pGlobal : pointer to structure holding global variables
 * \return       0 for success and ERROR on problem
 */
gpointer
threadGPIB(gpointer _pGlobal) {
	tGlobal *pGlobal = (tGlobal*) _pGlobal;

	gchar *sGPIBversion;
	gint GPIBstatus;
	gint timeoutHP8753C;   				// previous timeout

	gint descGPIBcontroller = ERROR, descGPIB_HP8753 = ERROR;
	messageEventData *message;
	gboolean bRunning = TRUE;
	gboolean bHoldThisChannel = FALSE, bHoldOtherChannel = FALSE;

	ibvers(&sGPIBversion);

	// g_print( "Linux GPIB version: %s\n", sGPIBversion );

	// look for the hp82357b GBIB controller
	// it is defined in /usr/local/etc/gpib.conf which can be overridden with the IB_CONFIG environment variable
	//
	//	interface {
	//    	minor = 0                       /* board index, minor = 0 uses /dev/gpib0, minor = 1 uses /dev/gpib1, etc.	*/
	//    	board_type = "agilent_82357b"   /* type of interface board being used 										*/
	//    	name = "hp82357b"               /* optional name, allows you to get a board descriptor using ibfind() 		*/
	//    	pad = 0                         /* primary address of interface             								*/
	//		sad = 0                         /* secondary address of interface          								    */
	//		timeout = T100ms                /* timeout for commands 												    */
	//		master = yes                    /* interface board is system controller 								    */
	//	}

	// loop waiting for messages from the main loop

	// Set the default queue to check for interruptions to async GPIB reads
	GPIB_checkQueue( pGlobal->messageQueueToGPIB );

	while (bRunning
			&& (message = g_async_queue_pop(pGlobal->messageQueueToGPIB ))) {

		// Reset the status ..  GPIB_AsyncRead & GBIPwrte will not proceed if this
		// shows an error
		GPIBstatus = 0;

		switch (message->command) {
		case TG_SETUP_GPIB:
			findGPIBdescriptors( pGlobal, &descGPIBcontroller, &descGPIB_HP8753 );
			continue;
		case TG_END:
			bRunning = FALSE;
			continue;
		default:
			if (descGPIBcontroller == INVALID || descGPIB_HP8753 == INVALID) {
				findGPIBdescriptors( pGlobal, &descGPIBcontroller, &descGPIB_HP8753 );
			}
			break;
		}

		// Most but not all commands require the GBIB
		if (descGPIB_HP8753 == INVALID ) {
			postError( "Cannot obtain GPIB controller or HP8753 descriptors");
		} else if( ! pingGPIBdevice( descGPIBcontroller, descGPIB_HP8753, &GPIBstatus) ) {
			postError( "HP8753C is not responding" );
		} else {
			GPIBstatus = ibask(descGPIB_HP8753, IbaTMO, &timeoutHP8753C); /* Remember old timeout */
			ibtmo(descGPIB_HP8753, T30s);
			// send a clear command to HP8753 ..
			GPIBstatus = ibclr( descGPIB_HP8753 );

			if( ! pGlobal->HP8753.firmwareVersion ) {
				if( (pGlobal->HP8753.firmwareVersion
						= get8753firmwareVersion( descGPIB_HP8753,
								&pGlobal->HP8753.sProduct, &GPIBstatus )) == INVALID ) {
					postError( "Cannot query identity - cannot proceed");
					postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);
					continue;
				}
				selectLearningStringIndexes( pGlobal );
			}
			// This must be an 8753 otherwise all bets are off
			if( strncmp( "8753", pGlobal->HP8753.sProduct, 4 ) != 0 ) {
				postError( "Not an HP8753 - cannot proceed");
				postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);
				pGlobal->HP8753.firmwareVersion = 0;
				continue;
			}

			switch (message->command) {
				// Get learn string and calibration arrays
				// If the channels are uncoupled, there are two sets of calibration arrays
			case TG_RETRIEVE_SETUPandCAL_from_HP8753:
				GPIBstatus = ibcmd(descGPIBcontroller, "_?", 2);
				if( get8753setupAndCal( descGPIB_HP8753, pGlobal, &GPIBstatus ) == OK
					&& GPIBsucceeded(GPIBstatus) ) {
					postInfo ( "Saving HP8753C setup to database" );
					postDataToMainLoop (TM_SAVE_SETUPandCAL, message->data );
					message->data = NULL;
				} else {
					postError( "Could not get setup/cal from HP8753" );
				}

				// restore timeout
				ibtmo(descGPIB_HP8753, timeoutHP8753C);
				// clear errors
				if( GPIBfailed( GPIBstatus ) ) {
					usleep( ms(250) );
					GPIBstatus = ibclr( descGPIB_HP8753 );
				}
				// local
				ibloc( descGPIB_HP8753 );

				usleep( ms(250) );

				break;
			case TG_SEND_SETUPandCAL_to_HP8753:
				// We have already obtained the data from the database
				// now send it to the network analyzer

				// This can take some time
				GPIBstatus = ibtmo(descGPIB_HP8753, T30s);
				postInfo( "Restore setup and calibration" );
				clearHP8753traces( &pGlobal->HP8753 );
				postDataToMainLoop( TM_REFRESH_TRACE, eCH_ONE );
				postDataToMainLoop( TM_REFRESH_TRACE, (void *)eCH_TWO );
				if( send8753setupAndCal( descGPIB_HP8753, pGlobal, &GPIBstatus ) == OK
						&& GPIBsucceeded(GPIBstatus) ) {
					postInfo ( "Setup and Calibration restored" );
				} else {
					postError ( "Setup and Calibration failed" );
				}
				// clear errors
				if( GPIBfailed( GPIBstatus ) ) {
					usleep( ms(250) );
					GPIBstatus = ibclr( descGPIB_HP8753 );
				}
				ibtmo(descGPIB_HP8753, timeoutHP8753C);
				ibloc( descGPIB_HP8753 );
				usleep( ms(250));
				break;

			case TG_RETRIEVE_TRACE_from_HP8753:
				// Clear the drawing areas
				clearHP8753traces( &pGlobal->HP8753 );
				postDataToMainLoop( TM_REFRESH_TRACE, eCH_ONE );
				postDataToMainLoop( TM_REFRESH_TRACE, (void *)eCH_TWO );

				postInfo( "Determine channel configuration");
				pGlobal->HP8753.flags.bDualChannel    = getHP8753switchOnOrOff( descGPIB_HP8753, "DUAC", &GPIBstatus );
				pGlobal->HP8753.flags.bSplitChannels  = getHP8753switchOnOrOff( descGPIB_HP8753, "SPLD", &GPIBstatus );
				pGlobal->HP8753.flags.bSourceCoupled  = getHP8753switchOnOrOff( descGPIB_HP8753, "COUC", &GPIBstatus );
				pGlobal->HP8753.flags.bMarkersCoupled = getHP8753switchOnOrOff( descGPIB_HP8753, "MARKCOUP", &GPIBstatus );

				if( GPIBfailed( GPIBstatus ) ) {
					postError( "Error (ask channel conf.)");
					break;
				}

				if ( get8753learnString( descGPIB_HP8753, &pGlobal->HP8753.pHP8753C_learn, &GPIBstatus ) != 0 ) {
					LOG( G_LOG_LEVEL_CRITICAL, "retrieve learn string" );
					break;
				}
				process8753learnString( descGPIB_HP8753, pGlobal, &GPIBstatus );

				// Hold this channel & see if we need to restart later
				// We stop sweeping so that the trace and markers give the same data
				// If the source is coupled, then a single hold works for both channels, if not, we
				// must hold when we change to the other channel
				bHoldThisChannel = getHP8753switchOnOrOff( descGPIB_HP8753, "HOLD", &GPIBstatus );
				GPIBwrite( descGPIB_HP8753, "HOLD;", &GPIBstatus );

				postInfo( "Get trace data channel");
				// if dual channel, then get both channels
				// otherwise just get the active channel
				if( pGlobal->HP8753.flags.bDualChannel ) {
					// start with the other channel because we will then return to the active
					// channel. We can only determine the active channel from the learn sting, so
					// where we can't determine the active channel, this is assumed to be channel 1
					for(int i=0, channel = (pGlobal->HP8753.activeChannel + 1) % eNUM_CH;
							i < eNUM_CH; i++, channel = (channel + 1) % eNUM_CH ) {
						setHP8753channel( descGPIB_HP8753, channel, &GPIBstatus );
						if( !pGlobal->HP8753.flags.bSourceCoupled && i==0 ) {
							// if uncoupled, we need to hold the other channel also
							bHoldOtherChannel = getHP8753switchOnOrOff( descGPIB_HP8753, "HOLD", &GPIBstatus );
							GPIBwrite( descGPIB_HP8753, "HOLD;", &GPIBstatus );
						}
						getHP8753channelTrace( descGPIB_HP8753, pGlobal, channel, &GPIBstatus );
					}
				} else {
					// just the active channel .. but data goes in channel 1
					getHP8753channelTrace( descGPIB_HP8753, pGlobal, eCH_ONE, &GPIBstatus );
				}
				postInfo( "Get marker data");
				getHP8753markersAndSegments( descGPIB_HP8753, pGlobal, &GPIBstatus );
				// timestamp this plot
				getTimeStamp( &pGlobal->HP8753.dateTime );

				// Display the new data
				postDataToMainLoop( TM_REFRESH_TRACE, eCH_ONE );
				if( pGlobal->HP8753.flags.bDualChannel &&  pGlobal->HP8753.flags.bSplitChannels )
					postDataToMainLoop( TM_REFRESH_TRACE, (void *)eCH_TWO );

				if( !bHoldThisChannel )
					GPIBwrite( descGPIB_HP8753, "CONT;", &GPIBstatus );

				if( pGlobal->HP8753.flags.bDualChannel ) {
					// if we are uncoupled, then we need to restart that trace separately
					if( !pGlobal->HP8753.flags.bSourceCoupled && !bHoldOtherChannel ) {
						setHP8753channel( descGPIB_HP8753, (pGlobal->HP8753.activeChannel+1) % eNUM_CH, &GPIBstatus );
						GPIBwrite( descGPIB_HP8753, "CONT;", &GPIBstatus );
						setHP8753channel( descGPIB_HP8753, pGlobal->HP8753.activeChannel, &GPIBstatus );
					}
				}
				GPIBwrite( descGPIB_HP8753, "MENUOFF;", &GPIBstatus );
				// beep
				GPIBwrite( descGPIB_HP8753, "EMIB;", &GPIBstatus );
				GPIBstatus = ibloc( descGPIB_HP8753 );
				usleep( 1000 * 500 );
				break;

			case TG_MEASURE_and_RETRIEVe_S2P_from_HP8753:
				// This can take some time
				GPIBstatus = ibtmo(descGPIB_HP8753, T30s);
				postInfo( "Measure and retrieve S2P" );

				if( getHP3753_S2P( descGPIB_HP8753, pGlobal, &GPIBstatus ) == OK
					&& GPIBsucceeded(GPIBstatus) ) {
					postInfo ( "Saving S2P to file" );
					postDataToMainLoop (TM_SAVE_S2P, message->data );
					message->data = NULL;
				}
				// restore timeout
				GPIBstatus = ibtmo(descGPIB_HP8753, timeoutHP8753C);
				// clear errors
				if( GPIBfailed( GPIBstatus ) ) {
					usleep( ms(250) );
					GPIBstatus = ibclr( descGPIB_HP8753 );
				}
				// local
				GPIBstatus = ibloc( descGPIB_HP8753 );
				usleep( ms(250) );
				break;
			case TG_ANALYZE_LEARN_STRING:
				postInfo( "Discovering Learn String indexes" );
				if( analyze8753learnString( descGPIB_HP8753, &pGlobal->HP8753.analyzedLSindexes, &GPIBstatus ) == 0 ) {
					postDataToMainLoop( TM_SAVE_LEARN_STRING_ANALYSIS, &pGlobal->HP8753.analyzedLSindexes );
					selectLearningStringIndexes( pGlobal );
				} else {
					postError( "Cannot analyze Learn String" );
				}
				GPIBwrite( descGPIB_HP8753, "EMIB;", &GPIBstatus );
				GPIBstatus = ibloc( descGPIB_HP8753 );
				break;
			case TG_UTILITY:
				{
					gint LSsize = 0;
					guchar *LearnString = NULL;
					get8753learnString( descGPIB_HP8753, &LearnString, &GPIBstatus );
					LSsize = GUINT16_FROM_BE(*((guint *)(LearnString+2)));
					for( int i=0; i < LSsize; i++ ) {
						if( pGlobal->HP8753.pHP8753C_learn[i] != LearnString[i] ) {
							g_print( "%-4d: 0x%02x  0x%02x\n", i, pGlobal->HP8753.pHP8753C_learn[i], LearnString[i] );
						}
					}
					g_print("\n");
				}
				break;
			case TG_SEND_CALKIT_to_HP8753:
				postInfo( "Send calibration kit" );
				if( sendHP8753calibrationKit( descGPIB_HP8753, pGlobal, &GPIBstatus ) == 0 ) {
					postInfo( "Calibration kit transfered" );
				} else {
					postError( "Cal kit transfer error" );
				}
				GPIBwrite( descGPIB_HP8753, "EMIB;", &GPIBstatus );
				GPIBstatus = ibloc( descGPIB_HP8753 );
				break;
			case TG_ABORT:
				postError( "Communication Aborted" );
				break;
			default:
				break;
			}
		}

		if( GPIBfailed(GPIBstatus) )
			postError("GPIB error or timeout");

		postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);

		g_free(message->sMessage);
		g_free(message->data);
		g_free(message);
	}

	return NULL;
}
