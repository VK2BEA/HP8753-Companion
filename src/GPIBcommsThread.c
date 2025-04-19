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
#include <linux/usb/tmc.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <hp8753.h>
#include <GPIBcomms.h>
#include <hp8753comms.h>
#include <locale.h>

#include "messageEvent.h"


/*!     \brief  See if there are messages on the asynchronous queue
 *
 * If the argument contains a pointer to a queue, set the default queue to it
 *
 * Check the queue and report the number of messages
 *
 * \param asyncQueue  pointer to async queue (or NULL)
 * \return            number of messages in queue
 */
gint
checkMessageQueue(GAsyncQueue *asyncQueue) {
    static GAsyncQueue *queueToCheck = NULL;
    int queueLength;

    if (asyncQueue)
        queueToCheck = asyncQueue;

    if (!asyncQueue && queueToCheck) {
        queueLength = g_async_queue_length(queueToCheck);
        if (queueLength > 0) {
            messageEventData *message = g_async_queue_pop(queueToCheck);
            gint command = message->command;
            g_async_queue_push_front(queueToCheck, message);
            if (command == TG_ABORT || command == TG_END)
                return SEVER_DIPLOMATIC_RELATIONS;
        } else {
            return queueLength;
        }
        return queueLength;
    } else {
        return 0;
    }
}

enum { TMO_SET, TMO_SAVE_AND_SET, TMO_RESTORE } eTIMEOUT_FN;
/*!     \brief  Set or restore timeout
 *
 * Sets a new GPIB timeout and optionally saves the current value
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753      pointer to GPIB interface structure
 * \param value             new timeout value
 * \param savedTimeout      pointer to where to save current timeout
 * \param purpose           enum command
 * \return                  status result
 */
gint
GPIBtimeout( tGPIBinterface *pGPIB_HP8753, gint value, gint *savedTimeout, tTimeoutPurpose purpose ) {
    static gboolean (*interfaceGPIBtimeout[]) (tGPIBinterface *, gint, gint *, tTimeoutPurpose) =
        { IF_GPIB_timeout, IF_USBTMC_timeout, IF_Prologix_timeout };
    gint rtn = interfaceGPIBtimeout[ pGPIB_HP8753->interfaceType ]
                                ( pGPIB_HP8753, value, savedTimeout, purpose );
    return rtn;
}

/*!     \brief  Set GPIB device to local control
 *
 * Set GPIB device to local control
 *
 * \param pGPIB_HP8753   pointer to GPIB device structure
 * \return               read status result
 */
gint
GPIBlocal( tGPIBinterface *pGPIB_HP8753 ) {
    static gboolean (*interfaceGPIBlocal[]) (tGPIBinterface *) =
        { IF_GPIB_local, IF_USBTMC_local, IF_Prologix_local };

    gint    rtn = interfaceGPIBlocal[ pGPIB_HP8753->interfaceType ] ( pGPIB_HP8753 );

    return rtn;
}

/*!     \brief  Send clear to the GPIB interface
 *
 * Sent the clear command to the GPIB interface
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \return               read status result
 */
gint
GPIBclear( tGPIBinterface *pGPIB_HP8753 ) {
    static gboolean (*interfaceGPIBclear[]) (tGPIBinterface *) =
        { IF_GPIB_clear, IF_USBTMC_clear, IF_Prologix_clear };
    gint rtn = interfaceGPIBclear[ pGPIB_HP8753->interfaceType ]( pGPIB_HP8753 );
    return rtn;
}

/*!     \brief  Write data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753  pointer to GPIB interfcae structure
 * \param sData         pointer to data
 * \param length        length of data in bytes
 * \param timeoutSecs   the maximum time to wait before abandoning
 * \return              read status result
 */
tGPIBReadWriteStatus
GPIBasyncWriteBinary( tGPIBinterface *pGPIB_HP8753, const void *pData, size_t length,
        gdouble timeoutSecs) {
    static tGPIBReadWriteStatus (*interfaceGPIBasyncWriteBinary[]) (tGPIBinterface *, const void *, size_t , gdouble) =
        { IF_GPIB_asyncWrite, IF_USBTMC_asyncWrite, IF_Prologix_asyncWrite };

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

    if (GPIBfailed( pGPIB_HP8753->status ))
        return eRDWT_PREVIOUS_ERROR;

    rtn = interfaceGPIBasyncWriteBinary[ pGPIB_HP8753->interfaceType ]
                                  ( pGPIB_HP8753, pData, length, timeoutSecs );

    return rtn;
}

/*!     \brief  Write (async) string to the GPIB device
 *
 * Send NULL terminated string to the GPIB device (asynchronously)
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \param sData          data to send
 * \param pGPIBstatus    pointer to GPIB status
 * \return               count or ERROR
 */
tGPIBReadWriteStatus
GPIBasyncWrite( tGPIBinterface *pGPIB_HP8753, const void *sData, gdouble timeoutSecs) {
    DBG(eDEBUG_EXTREME, "🖊 HP8753: %s", sData);
    return GPIBasyncWriteBinary( pGPIB_HP8753, sData, strlen((gchar*) sData),
            timeoutSecs);
}

/*!     \brief  Write string with a numeric value to the GPIB device
 *
 * Send NULL terminated string with numeric paramater to the GPIB device
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \param sData          data to send
 * \param number         number to embed in string (%d)
 * \param timeout        seconds to timeout
 * \return               count or ERROR (from GPIBasyncWriteBinary)
 */
tGPIBReadWriteStatus
GPIBasyncWriteOneOfN( tGPIBinterface *pGPIB_HP8753, const void *sData, gint number, double timeout) {
    gchar *sCmd = g_strdup_printf(sData, number);
    DBG(eDEBUG_EXTREME, "👉 HP8753: %s", sCmd);
    tGPIBReadWriteStatus rtnStatus = GPIBasyncWrite( pGPIB_HP8753, sCmd, timeout);
    g_free(sCmd);
    return rtnStatus;
}

/*!     \brief  Read data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \param readBuffer     pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param timeout        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
GPIBasyncRead(  tGPIBinterface *pGPIB_HP8753, void *readBuffer, glong maxBytes, gdouble timeoutSecs) {
    static tGPIBReadWriteStatus (*interfaceGPIBasyncRead[]) (tGPIBinterface *, void *, glong , gdouble) =
        { IF_GPIB_asyncRead, IF_USBTMC_asyncRead, IF_Prologix_asyncRead };

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

    if (GPIBfailed( pGPIB_HP8753->status ))
        return eRDWT_PREVIOUS_ERROR;

    rtn = interfaceGPIBasyncRead[ pGPIB_HP8753->interfaceType ]
                                  ( pGPIB_HP8753, readBuffer, maxBytes, timeoutSecs );
    return rtn;
}

const gint numOfCalArrays[] = { 0, 	// None
        1,	// Response
        2,	// Response & Isolation
        3,	// S11 1-port
        3,	// S22 1-port
        12,	// Full 2-port
        12, // One path 2-port
        12  // TRL*/LRM* 2-port
        };

/*!     \brief  Ping GPIB device
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \return               TRUE if device responds or FALSE if not
 */
static gboolean
pingGPIBdevice( tGPIBinterface *pGPIB_HP8753 ) {
    static gboolean (*interfacePingGPIB[]) (tGPIBinterface *) =
        { IF_GPIB_ping, IF_USBTMC_ping, IF_Prologix_ping };
    gint rtn;

    rtn = interfacePingGPIB[ pGPIB_HP8753->interfaceType ](pGPIB_HP8753);

    return rtn;
}

#define GPIB_EOI		TRUE
#define	GPIB_EOS_NONE	0

static gboolean (*interfaceGPIBopen[]) (tGlobal *, tGPIBinterface *) =
    { IF_GPIB_open, IF_USBTMC_open, IF_Prologix_open };
static gboolean (*interfaceGPIBclose[]) (tGPIBinterface *) =
    { IF_GPIB_close, IF_USBTMC_close, IF_Prologix_close };
/*!     \brief  open the GPIB device
 *
 * Get the device descriptors of the contraller and GPIB device
 * based on the paramaters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pGPIB_HP8753        pointer to GPIB interface structure
 * \return                    0 on success or ERROR on failure
 */
gint
GPIBopen( tGlobal *pGlobal, tGPIBinterface *pGPIB_HP8753 ) {
    gint rtn;
    // close interface (if open)
    rtn = interfaceGPIBclose[ pGPIB_HP8753->interfaceType ]( pGPIB_HP8753 );
    // open interface
    pGPIB_HP8753->interfaceType = pGlobal->flags.bbGPIBinterfaceType;
    rtn = interfaceGPIBopen[ pGPIB_HP8753->interfaceType ]( pGlobal, pGPIB_HP8753 );

    return rtn;
}

/*!     \brief  close the GPIB devices
 *
 * Close the controller if it was opened and close the device
 *
 * \param pGPIB_HP8753  pointer to GPIB device structure
 * \return				status
 */
gint
GPIBclose( tGPIBinterface *pGPIB_HP8753) {
    gint rtn;

    rtn = interfaceGPIBclose[ pGPIB_HP8753->interfaceType ]( pGPIB_HP8753 );

    return rtn;
}

/*!     \brief  Get real time in ms
 *
 * Get the time in milliseconds
 *
 * \return       the current time in milliseconds
 */
gulong
now_milliSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec * 1.0e3 + now.tv_nsec / 1.0e6;
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

    gchar *sGPIBversion = NULL;
    gint verMajor, verMinor, verMicro;
    gint currentTimeout = T1s;   				// previous timeout

    tGPIBinterface GPIBinterface = { .interfaceType = eGPIB, .descriptor = ERROR, .status=0 };
    messageEventData *message;
    gboolean bRunning = TRUE;
    gulong __attribute__((unused)) datum = 0;
    static guchar *pHP8753_learn = NULL;

    // The HP8753 formats numbers like 3.141 not, the continental European way 3,14159
    setlocale(LC_NUMERIC, "C");
    ibvers(&sGPIBversion);
    LOG(G_LOG_LEVEL_CRITICAL, sGPIBversion);
    if( sGPIBversion && sscanf( sGPIBversion, "%d.%d.%d", &verMajor, &verMinor, &verMicro ) == 3  ) {
        pGlobal->GPIBversion = verMajor * 10000 + verMinor * 100 + verMicro;
    }
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
    checkMessageQueue(pGlobal->messageQueueToGPIB);

    while (bRunning && (message = g_async_queue_pop(pGlobal->messageQueueToGPIB))) {

        // Reset the status ..  GPIB_AsyncRead & GBIPwrte will not proceed if this
        // shows an error
        GPIBinterface.status = 0;

        switch (message->command) {
        case TG_SETUP_GPIB:
            GPIBopen(pGlobal, &GPIBinterface);
            datum = now_milliSeconds();
            continue;
        case TG_END:
            GPIBclose(&GPIBinterface);
            bRunning = FALSE;
            continue;
        default:
            if (GPIBinterface.descriptor == INVALID) {
                GPIBopen(pGlobal, &GPIBinterface);
                datum = now_milliSeconds();
            }
            break;
        }
#define IBLOC(x, y) { GPIBlocal( x ); y = now_milliSeconds(); usleep( ms( LOCAL_DELAYms ) ); }
        // Most but not all commands require the GBIB
        if (GPIBinterface.descriptor == INVALID) {
            postError("Cannot obtain HP8753 descriptor");
        } else if (!pingGPIBdevice( &GPIBinterface )) {
            postError("HP8753 is not responding");
            // attempt to reopen if USBTMC
            if( GPIBinterface.interfaceType == eUSBTMC ) {
                GPIBopen(pGlobal, &GPIBinterface);
            } else {
                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                GPIBclear(  &GPIBinterface );
            }
            usleep(ms(250));
        } else {
            pGlobal->flags.bGPIBcommsActive = TRUE;
            GPIBtimeout( &GPIBinterface, T1s, &currentTimeout, eTMO_SAVE_AND_SET );
#ifdef USE_PRECAUTIONARY_DEVICE_IBCLR
			// send a clear command to HP8753 ..
			if( now_milliSeconds() - datum > 2000 )
			    GPIBstatus = GPIBclear( &GPIBinterface );
#endif
            if (!pGlobal->HP8753.firmwareVersion) {
                if ((pGlobal->HP8753.firmwareVersion = get8753firmwareVersion( &GPIBinterface,
                        &pGlobal->HP8753.sProduct )) == INVALID) {
                    postError("Cannot query identity - cannot proceed");
                    postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);
                    GPIBtimeout( &GPIBinterface, T1s, &currentTimeout, eTMO_RESTORE );
                    continue;
                }
                selectLearningStringIndexes(pGlobal);
            }
            // This must be an 8753 otherwise all bets are off
            if (strncmp("8753", pGlobal->HP8753.sProduct, 4) != 0) {
                postError("Not an HP8753 - cannot proceed");
                postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);
                pGlobal->HP8753.firmwareVersion = 0;
                GPIBtimeout( &GPIBinterface, T1s, &currentTimeout, eTMO_RESTORE );
                continue;
            }

            switch (message->command) {
            // Get learn string and calibration arrays
            // If the channels are uncoupled, there are two sets of calibration arrays
            case TG_RETRIEVE_SETUPandCAL_from_HP8753:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                if (get8753setupAndCal( &GPIBinterface, pGlobal ) == OK
                        && GPIBsucceeded( GPIBinterface.status )) {
                    postInfo("Saving HP8753 setup to database");
                    postDataToMainLoop(TM_SAVE_SETUPandCAL, message->data);
                    message->data = NULL;
                } else {
                    postError("Could not get setup/cal from HP8753");
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear(  &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "MENUOFF;EMIB;CLES;", 10 * TIMEOUT_RW_1SEC);
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;
            case TG_SEND_SETUPandCAL_to_HP8753:

//            	int test(tGPIBinterface *);

//            	test( &GPIBinterface );
//            	break;
                // We have already obtained the data from the database
                // now send it to the network analyzer

                // This can take some time
                GPIBtimeout( &GPIBinterface, T30s, NULL, eTMO_SET );
                postInfo("Restore setup and calibration");
                clearHP8753traces(&pGlobal->HP8753);
                postDataToMainLoop(TM_REFRESH_TRACE, (void*) eCH_ONE);
                postDataToMainLoop(TM_REFRESH_TRACE, (void*) eCH_TWO);

                if (send8753setupAndCal( &GPIBinterface, pGlobal ) == OK
                        && GPIBsucceeded( GPIBinterface.status )) {
                    postInfo("Setup and Calibration restored");
                } else {
                    postError("Setup and Calibration failed");
                }
                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear(  &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "MENUOFF;EMIB;CLES;", 10 * TIMEOUT_RW_1SEC);
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;

            case TG_RETRIEVE_TRACE_from_HP8753:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                // Clear the drawing areas
                clearHP8753traces(&pGlobal->HP8753);

                postInfo("Determine channel configuration");
                {
                    gint dualChannel = getHP8753switchOnOrOff( &GPIBinterface, "DUAC" );
                    if (GPIBfailed( GPIBinterface.status ) || dualChannel == ERROR ) {
                        postError("HP8753 not responding .. is it ready?");
                        postDataToMainLoop(TM_REFRESH_TRACE, eCH_ONE);
                        postDataToMainLoop(TM_REFRESH_TRACE, (void*) eCH_TWO);
                        break;
                    } else {
                        pGlobal->HP8753.flags.bDualChannel = dualChannel;
                    }
                }
                pGlobal->HP8753.flags.bSplitChannels = getHP8753switchOnOrOff( &GPIBinterface, "SPLD" );
                pGlobal->HP8753.flags.bSourceCoupled = getHP8753switchOnOrOff( &GPIBinterface, "COUC" );
                pGlobal->HP8753.flags.bMarkersCoupled = getHP8753switchOnOrOff( &GPIBinterface, "MARKCOUP" );

                postDataToMainLoop(TM_REFRESH_TRACE, eCH_ONE);
                postDataToMainLoop(TM_REFRESH_TRACE, (void*) eCH_TWO);

                if (GPIBfailed( GPIBinterface.status )) {
                    postError("Error (ask channel conf.)");
                    break;
                }

                if (get8753learnString( &GPIBinterface, &pHP8753_learn ) != 0) {
                    LOG(G_LOG_LEVEL_CRITICAL, "retrieve learn string");
                    break;
                }
                process8753learnString( &GPIBinterface, pHP8753_learn, pGlobal );

                // Hold this channel & see if we need to restart later
                // We stop sweeping so that the trace and markers give the same data
                // If the source is coupled, then a single hold works for both channels, if not, we
                // must hold when we change to the other channel
                pGlobal->HP8753.channels[ pGlobal->HP8753.activeChannel ].chFlags.bSweepHold = getHP8753switchOnOrOff( &GPIBinterface, "HOLD" );
                GPIBasyncWrite( &GPIBinterface, "HOLD;", 10.0);

                postInfo("Get trace data channel");
                // if dual channel, then get both channels
                // otherwise just get the active channel
                if (pGlobal->HP8753.flags.bDualChannel) {
                    // start with the other channel because we will then return to the active
                    // channel. We can only determine the active channel from the learn sting, so
                    // where we can't determine the active channel, this is assumed to be channel 1
                	GPIBenableSRQonOPC( &GPIBinterface );
                    for (int i = 0, channel = ( pGlobal->HP8753.activeChannel == eCH_ONE ? eCH_TWO : eCH_ONE );
                            i < eNUM_CH; i++, channel = (channel + 1) % eNUM_CH) {
                        setHP8753channel( &GPIBinterface, channel );
                        if (!pGlobal->HP8753.flags.bSourceCoupled && i == 0) {
                            // if uncoupled, we need to hold the other channel also
                            pGlobal->HP8753.channels[ channel ].chFlags.bSweepHold
                                    = getHP8753switchOnOrOff( &GPIBinterface, "HOLD" );
                            GPIBasyncWrite( &GPIBinterface, "HOLD;",  10.0);
                        }
                        getHP8753channelTrace( &GPIBinterface, pGlobal, channel );
                    }
                } else {
                    // just the active channel .. but data goes in channel 1
                    getHP8753channelTrace( &GPIBinterface, pGlobal, eCH_ONE );
                }

                // Get HPGL plot before querying segment data as selecting segments alters the display
                if (pGlobal->flags.bDoNotRetrieveHPGLdata) {
                    pGlobal->HP8753.flags.bHPGLdataValid = FALSE;
                } else {
                    postInfo("Acquire HPGL screen plot");
                    if (acquireHPGLplot( &GPIBinterface, pGlobal ) != 0)
                        postError("Cannot acquire HPGL plot");
                }

                postInfo("Get marker data");
                getHP8753markersAndSegments( &GPIBinterface, pGlobal );
                // timestamp this plot
                getTimeStamp(&pGlobal->HP8753.dateTime);

                if (GPIBfailed( GPIBinterface.status ))
                    break;

                // Display the new data
                if( !pGlobal->HP8753.flags.bShowHPGLplot )
                    postDataToMainLoop(TM_REFRESH_TRACE, eCH_ONE);

                if (pGlobal->HP8753.flags.bDualChannel && pGlobal->HP8753.flags.bSplitChannels
                        && !pGlobal->HP8753.flags.bShowHPGLplot )
                    postDataToMainLoop(TM_REFRESH_TRACE, (void*) eCH_TWO);

                if (pGlobal->HP8753.channels[ pGlobal->HP8753.activeChannel ].chFlags.bSweepHold == FALSE) {
                    GPIBasyncWrite( &GPIBinterface, "CONT;", 1.0);
                }

                if (pGlobal->HP8753.flags.bDualChannel) {
                    // if we are uncoupled, then we need to restart that trace separately
                    if (!pGlobal->HP8753.flags.bSourceCoupled &&
                            pGlobal->HP8753.channels[ pGlobal->HP8753.activeChannel == eCH_ONE ? eCH_TWO : eCH_ONE ].chFlags.bSweepHold == FALSE) {
                        setHP8753channel( &GPIBinterface,
                                pGlobal->HP8753.activeChannel == eCH_ONE ? eCH_TWO : eCH_ONE );
                        GPIBasyncWrite( &GPIBinterface, "CONT;", 1.0);
                        setHP8753channel( &GPIBinterface, pGlobal->HP8753.activeChannel );
                    }
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear( &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "MENUOFF;EMIB;", 10 * TIMEOUT_RW_1SEC);
                    postInfo("Trace(s) retrieved");
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;

            case TG_MEASURE_and_RETRIEVE_S2P_from_HP8753:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                postInfo("Measure and retrieve S2P");
                // This can take some time
                GPIBtimeout( &GPIBinterface, T30s, NULL, eTMO_SET );

                if ( getHP3753_S2P( &GPIBinterface, pGlobal ) == OK ) {
                    postInfo("Saving S2P to file");
                    postDataToMainLoop(TM_SAVE_S2P, message->data);
                    message->data = NULL;
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear( &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "EMIB;CLES;", 1.0);
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;

            case TG_MEASURE_and_RETRIEVE_S1P_from_HP8753:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                postInfo("Measure and retrieve S1P");
                // This can take some time
                GPIBtimeout( &GPIBinterface, T30s, NULL, eTMO_SET );

                if ( getHP3753_S1P( &GPIBinterface, pGlobal ) == OK ) {
                    postInfo("Saving S1P to file");
                    postDataToMainLoop(TM_SAVE_S1P, message->data);
                    message->data = NULL;
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear( &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "EMIB;CLES;", 1.0);
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;

            case TG_ANALYZE_LEARN_STRING:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                postInfo("Discovering Learn String indexes");

                if (analyze8753learnString( &GPIBinterface, &pGlobal->HP8753.analyzedLSindexes ) == 0) {
                    postDataToMainLoop(TM_SAVE_LEARN_STRING_ANALYSIS,
                            &pGlobal->HP8753.analyzedLSindexes);
                    selectLearningStringIndexes(pGlobal);
                } else {
                    postError("Cannot analyze Learn String");
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear( &GPIBinterface );
                    usleep(ms(250));
                } else {
                    // beep
                    GPIBasyncWrite( &GPIBinterface, "EMIB;CLES;", 1.0);
                }
                // local
                IBLOC( &GPIBinterface, datum );
                break;
            case TG_UTILITY:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                // see what's changed in the learn string after making some change on the HP8753
                //... for diagnostic reasons only
                {
                    guchar *LearnString = NULL;
                    gboolean bDifferent = FALSE;
                    if( pHP8753_learn == NULL && get8753learnString( &GPIBinterface, &pHP8753_learn ) !=  OK ) {
                        g_printerr( "Cannot get learn string from HP8753\n" );
                        break;
                    }
                    if( get8753learnString( &GPIBinterface, &LearnString ) != OK ) {
                        g_printerr( "Cannot get learn string from HP8753\n" );
                        break;
                    }
                    for (int i = 0; i < lengthFORM1data( LearnString ); i++) {
                        if (pHP8753_learn[i] != LearnString[i]) {
                            g_print("%-4d: 0x%02x  0x%02x\n", i, pHP8753_learn[i], LearnString[i]);
                            bDifferent = TRUE;
                        }
                    }
                    if( !bDifferent )
                        g_print( "No change in learn string\n");
                    else
                        g_print("\n");
                    g_free( LearnString );
                }
                IBLOC( &GPIBinterface, datum );
                break;
            case TG_EXPERIMENT:
                {
                    gint OUTPFORMsize = 0;
                    guint16 OUTPFORMheaderAndSize[2];
                    guint8 *pOUTPFORM = 0;
                    gdouble real, imag;

                    void
                    FORM1toDouble( guint8 *, gdouble *, gdouble *, gboolean );

                    GPIBasyncWrite( &GPIBinterface, "FORM1;OUTPFORM;", 1.0);
                    GPIBasyncRead( &GPIBinterface, &OUTPFORMheaderAndSize, HEADER_SIZE, 10 * TIMEOUT_RW_1SEC);
                    // Convert from big endian to local (Intel LE)
                    OUTPFORMsize = GUINT16_FROM_BE(OUTPFORMheaderAndSize[1]);
                    pOUTPFORM = g_malloc( OUTPFORMsize );
                    // read learn string into malloced space (not clobbering header and size)
                    GPIBasyncRead( &GPIBinterface, pOUTPFORM, OUTPFORMsize, 10 * TIMEOUT_RW_1SEC);

                    for( gint n=0; n < OUTPFORMsize / 6; n++ ) {
                        FORM1toDouble( pOUTPFORM + (n * 6), &real, &imag, FALSE );
                        g_printerr( "%20.8lf\n", real );
                    }

                    g_free( pOUTPFORM );
                }
                IBLOC( &GPIBinterface, datum );
                break;

            case TG_SEND_CALKIT_to_HP8753:
                GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                postInfo("Send calibration kit");

                if ( sendHP8753calibrationKit( &GPIBinterface, pGlobal ) == 0) {
                    postInfo("Calibration kit transfered");
                } else {
                    postError("Cal kit transfer error");
                }

                GPIBtimeout( &GPIBinterface, T1s, NULL, eTMO_SET );
                // clear errors
                if (GPIBfailed( GPIBinterface.status )) {
                    GPIBclear( &GPIBinterface );
                    usleep(ms(250));
                }

                GPIBasyncWrite( &GPIBinterface, "EMIB;CLES;", 1.0);
                IBLOC( &GPIBinterface, datum );
                break;
            case TG_ABORT:
                postError("Communication Aborted");
                {   // Clear the interface
                    if( GPIBinterface.interfaceType == eGPIB) {
                        gint boardIndex = 0;
                        ibask( GPIBinterface.descriptor, IbaBNA, &boardIndex);
                        ibsic( boardIndex );
                    }
                    GPIBclear( &GPIBinterface );
                    GPIBasyncWrite( &GPIBinterface, "CLES;",  10 * TIMEOUT_RW_1SEC);
                    IBLOC( &GPIBinterface, datum );
                }
                break;
            default:
                break;
            }
        }

        // restore timeout
        GPIBtimeout( &GPIBinterface, T1s, &currentTimeout, eTMO_RESTORE );

        if (GPIBfailed( GPIBinterface.status )) {
            postError("GPIB error or timeout");
        }
        postMessageToMainLoop(TM_COMPLETE_GPIB, NULL);

        g_free(message->sMessage);
        g_free(message->data);
        g_free(message);
        pGlobal->flags.bGPIBcommsActive = FALSE;
    }

    g_free( pHP8753_learn );

    return NULL;
}
