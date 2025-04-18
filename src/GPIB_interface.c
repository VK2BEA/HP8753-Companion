/*
 * Copyright (c) 2022-2024 Michael G. Katzmann
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
#include <fcntl.h>
#include <aio.h>
#include <errno.h>
#include <linux/usb/tmc.h>
#include <sys/ioctl.h>
#include <math.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <errno.h>
#include <hp8753.h>
#include <GPIBcomms.h>
#include <hp8753comms.h>
#include <locale.h>

#include "messageEvent.h"

/*!     \brief  Write data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIBinterface pointer GPIB device data
 * \param sData          pointer to data to write
 * \param length         number of bytes to write
 * \param timeout        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
IF_GPIB_asyncWrite( tGPIBinterface *pGPIBinterface, const void *pData, size_t length,
        gdouble timeoutSecs) {

    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint    currentTimeout;

    GPIBtimeout( pGPIBinterface, TNONE, &currentTimeout, eTMO_SAVE_AND_SET );

    pGPIBinterface->nChars = 0;
    pGPIBinterface->status = ibwrta( pGPIBinterface->descriptor, pData, length);

    if (GPIBfailed( pGPIBinterface->status ))
        return eRDWT_ERROR;
#if !GPIB_CHECK_VERSION(4,3,6)
    //todo - remove when linux GPIB driver fixed
    // a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
    // we delay, so that the timeout used is TNONE bedore changing to T30ms
    usleep(20 * 1000);
#endif

    // set the timout for the ibwait to 30ms
    GPIBtimeout( pGPIBinterface, T30ms, NULL, eTMO_SET );
    do {
        // Wait for read completion or timeout
        pGPIBinterface->status = ibwait( pGPIBinterface->descriptor, TIMO | CMPL | END);
        if (( pGPIBinterface->status & TIMO) == TIMO ) {
            // Timeout
            rtn = eRDWT_CONTINUE;
            waitTime += THIRTY_MS;
            if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
                gchar *sMessage = g_strdup_printf("✍🏻 Waiting for HP8753: %ds", (gint) (waitTime));
                postInfo(sMessage);
                g_free(sMessage);
            }
        } else {
            // did we have a read error
            if (( pGPIBinterface->status & ERR) == ERR)
                rtn = eRDWT_ERROR;
            // or did we complete the read
            else if (( pGPIBinterface->status & CMPL ) == CMPL || ( pGPIBinterface->status & END ) == END )
                rtn = eRDWT_OK;
        }
        // If we get a message on the queue, it is assumed to be an abort
        if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
            // This will stop future GPIB commands for this sequence
            pGPIBinterface->status |= ERR;
            rtn = eRDWT_ABORT;
        }
    } while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn != eRDWT_OK)
        ibstop( pGPIBinterface->descriptor );

    pGPIBinterface->status = AsyncIbsta();
	pGPIBinterface->nChars = AsyncIbcnt();

    DBG(eDEBUG_EXTREME, "🖊 HP8753: %d / %d bytes", pGPIBinterface->nChars, length);

    if (( pGPIBinterface->status & CMPL) != CMPL) {
        if (waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async write timeout after %.2f sec. status %04X",
                    timeoutSecs, pGPIBinterface->status );
        else
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async write status/error: %04X/%d", pGPIBinterface->status,
                    AsyncIberr());
    }

    GPIBtimeout( pGPIBinterface, TNONE, &currentTimeout, eTMO_RESTORE );

    if ( waitTime > FIVE_SECONDS )
        postInfo("");

    if( rtn == eRDWT_CONTINUE ) {
        pGPIBinterface->status |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
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
 * \return               read status result
 */
tGPIBReadWriteStatus
IF_GPIB_asyncRead(  tGPIBinterface *pGPIBinterface, void *readBuffer, long maxBytes, gdouble timeoutSecs) {
    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint    currentTimeout;

    GPIBtimeout( pGPIBinterface, TNONE, &currentTimeout, eTMO_SAVE_AND_SET );

    pGPIBinterface->nChars = 0;
    pGPIBinterface->status = ibrda( pGPIBinterface->descriptor, readBuffer, maxBytes);

    if (GPIBfailed( pGPIBinterface->status ))
        return eRDWT_ERROR;

#if !GPIB_CHECK_VERSION(4,3,6)
    //todo - remove when linux GPIB driver fixed
    // a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
    // we delay, so that the timeout used is TNONE bedore changing to T30ms
    usleep(20 * 1000);
#endif

    // set the timout for the ibwait to 30ms
    GPIBtimeout( pGPIBinterface, T30ms, NULL, eTMO_SET );
    do {
        // Wait for read completion or timeout
        pGPIBinterface->status = ibwait( pGPIBinterface->descriptor, TIMO | CMPL | END);
        if ((pGPIBinterface->status & TIMO) == TIMO) {
            // Timeout
            rtn = eRDWT_CONTINUE;
            waitTime += THIRTY_MS;
            if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
                gchar *sMessage = g_strdup_printf("👀 Waiting for HP8753: %ds", (gint) (waitTime));
                postInfo(sMessage);
                g_free(sMessage);
            }
        } else {
            // did we have a read error
            if ((pGPIBinterface->status & ERR) == ERR)
                rtn = eRDWT_ERROR;
            // or did we complete the read
            else if ((pGPIBinterface->status & CMPL) == CMPL || (pGPIBinterface->status & END) == END)
                rtn = eRDWT_OK;
        }
        // If we get a message on the queue, it is assumed to be an abort
        if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
            // This will stop future GPIB commands for this sequence
            pGPIBinterface->status |= ERR;
            rtn = eRDWT_ABORT;
        }
    } while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn != eRDWT_OK)
        ibstop( pGPIBinterface->descriptor);

    pGPIBinterface->status = AsyncIbsta();
	pGPIBinterface->nChars = AsyncIbcnt();


    DBG(eDEBUG_EXTREME, "👓 HP8753: %d bytes (%d max)", pGPIBinterface->nChars, maxBytes);

    if ((pGPIBinterface->status & CMPL) != CMPL) {
        if (waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async read timeout after %.2f sec. status %04X",
                    timeoutSecs, pGPIBinterface->status);
        else
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async read status/error: %04X/%d", pGPIBinterface->status,
                    AsyncIberr());
    }

    if ( waitTime > FIVE_SECONDS )
        postInfo("");

    GPIBtimeout( pGPIBinterface, TNONE, &currentTimeout, eTMO_RESTORE );

    if( rtn == eRDWT_CONTINUE ) {
        pGPIBinterface->status |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}

/*!     \brief  Ping GPIB interface
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param pGPIBinterface pointer to GPIB interface structure
 * \return               TRUE if device responds or FALSE if not
 */
gboolean
IF_GPIB_ping( tGPIBinterface *pGPIB_HP8753 ) {
    gint descGPIBdevice = pGPIB_HP8753->descriptor;
    gint PID = INVALID;
    gint timeout;
    gint descGPIBboard = INVALID;

    gshort bFound = FALSE;

    // Get the device PID
    if ((pGPIB_HP8753->status = ibask(descGPIBdevice, IbaPAD, &PID)) & ERR)
        goto err;
    // Get the board number
    if ((pGPIB_HP8753->status = ibask(descGPIBdevice, IbaBNA, &descGPIBboard)) & ERR)
        goto err;

    // save old timeout
    if ((pGPIB_HP8753->status = ibask(descGPIBboard, IbaTMO, &timeout)) & ERR)
        goto err;
    // set new timeout (for ping purpose only)
    if ((pGPIB_HP8753->status = ibtmo(descGPIBboard, T3s)) & ERR)
        goto err;

    // Actually do the ping
    if ((pGPIB_HP8753->status = ibln(descGPIBboard, PID, NO_SAD, &bFound)) & ERR) {
        DBG(eDEBUG_EXTENSIVE, "🖊 HP8753: ping to %d failed (status: %04x, error %04x)", PID,
                pGPIB_HP8753->status, ThreadIberr());
        goto err;
    }

    pGPIB_HP8753->status = ibtmo(descGPIBboard, timeout);

    err: return (bFound);
}


#define GPIB_EOI        TRUE
#define GPIB_EOS_NONE   0

/*!     \brief  open the GPIB device
 *
 * Get the device descriptors of the contraller and GPIB device
 * based on the paramaters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pGPIBinterface      pointer to GPIB interfcae structure
 * \return                    0 on sucess or ERROR on failure
 */
gint
IF_GPIB_open( tGlobal *pGlobal, tGPIBinterface *pGPIB_HP8753 ) {
    gint *pDescGPIB_HP8753 = &pGPIB_HP8753->descriptor;

    // The board index can be used as a device descriptor; however,
    // if a device desripter was returned from the ibfind, it mist be freed
    // with ibnol().
    // The board index corresponds to the minor number /dev/
    // $ ls -l /dev/gpib0
    // crw-rw----+ 1 root root 160, 0 May 12 09:24 /dev/gpib0
#define FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR 16
    // raise(SIGSEGV);

    if (*pDescGPIB_HP8753 != INVALID) {
        ibonl(*pDescGPIB_HP8753, 0);
    }

    *pDescGPIB_HP8753 = INVALID;

    // Look for the HP8753
    if (pGlobal->flags.bGPIB_UseCardNoAndPID) {
        if (pGlobal->GPIBcontrollerIndex >= 0 && pGlobal->GPIBdevicePID >= 0)
            *pDescGPIB_HP8753 = ibdev(pGlobal->GPIBcontrollerIndex, pGlobal->GPIBdevicePID, 0, T3s,
                    GPIB_EOI, GPIB_EOS_NONE);
        else {
            postError("Bad GPIB controller or device number");
            return ERROR;
        }
    } else {
        *pDescGPIB_HP8753 = ibfind(pGlobal->sGPIBdeviceName);
        ibeot(*pDescGPIB_HP8753, GPIB_EOI);
    }

    if (*pDescGPIB_HP8753 == ERROR) {
        postError("Cannot find HP8753 on GPIB");
        return ERROR;
    }

    if (!IF_GPIB_ping( pGPIB_HP8753 )) {
        postError("Cannot contact HP8753 on GPIB");
        return ERROR;
    } else {
        postInfo("Contact with HP8753 established on GPIB");
        GPIBlocal( pGPIB_HP8753 );
        usleep( LOCAL_DELAYms * 1000);
    }
    return OK;
}


/*!     \brief  close the GPIB devices
 *
 * Close the GPIB device
 *
 * \param tGPIBinterface      pointer to GPIB device structure
 */
gint
IF_GPIB_close( tGPIBinterface *pGPIB_HP8753) {

    pGPIB_HP8753->status = 0;

    if ( pGPIB_HP8753->descriptor != INVALID ) {
        pGPIB_HP8753->status = ibonl( pGPIB_HP8753->descriptor, 0 );
        pGPIB_HP8753->descriptor = INVALID;
    }

    return pGPIB_HP8753->status;
}

/*!     \brief  Set or restore timeout
 *
 * Sets a new GPIB timeout and optionally saves the current value
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIBinterface    pointer to GPIB interfcae structure
 * \param value             new timeout value
 * \param pSavedTimeout     pointer to where to save current timeout
 * \param purpose           enum command
 * \return                  status result
 */
gint
IF_GPIB_timeout( tGPIBinterface *pGPIBinterface, gint value, gint *pSavedTimeout, tTimeoutPurpose purpose ) {
    switch( purpose ) {
    case eTMO_SAVE_AND_SET:
        if( pSavedTimeout != NULL )
            pGPIBinterface->status = ibask( pGPIBinterface->descriptor, IbaTMO, pSavedTimeout);
        if( !(pGPIBinterface->status & ERR) )
            pGPIBinterface->status = ibtmo( pGPIBinterface->descriptor, value);
        break;
    default:
    case eTMO_SET:
        pGPIBinterface->status = ibtmo( pGPIBinterface->descriptor, value);
        break;
    case eTMO_RESTORE:
        pGPIBinterface->status = ibtmo( pGPIBinterface->descriptor, *pSavedTimeout);
        break;
    }

    return pGPIBinterface->status;
}

/*!     \brief  Set GPIB device to local control
 *
 * Set GPIB device to local control
 *
 * \param pGPIBinterface pointer to GPIB device structure
 * \return               read status result
 */
gint
IF_GPIB_local( tGPIBinterface *pGPIBinterface ) {
    pGPIBinterface->status = ibloc( pGPIBinterface->descriptor );
    return pGPIBinterface->status;
}

/*!     \brief  Send clear to the GPIB interface
 *
 * Sent the clear command to the GPIB interface
 *
 * \param pGPIBinterface pointer to GPIB interfcae structure
 * \return               read status result
 */
gint
IF_GPIB_clear( tGPIBinterface *pGPIBinterface ) {
    pGPIBinterface->status = ibclr( pGPIBinterface->descriptor );
    return pGPIBinterface->status;
}

/*!     \brief  Read configuration value from the GPIB device
 *
 * Read configuration value for GPIB device
 *
 * \param pGPIBinterface pointer to GPIB interfcae structure
 * \param option         the paramater to read
 * \param result         pointer to the integer receiving the value
 * \param pGPIBstatus    pointer to GPIB status
 * \return               OK or ERROR
 */
gint
IF_GPIB_readConfiguration( tGPIBinterface *pGPIBinterface, gint option, gint *result, gint *pGPIBstatus) {
    *pGPIBstatus = ibask( pGPIBinterface->descriptor, option, result);

    if (GPIBfailed(*pGPIBstatus))
        return ERROR;
    else
        return OK;
}

/*!     \brief  Write string preceeded with OPC or binary adding OPC;NOOP;, then wait for SRQ
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 * After a command that sets the OPC, wait for the event without tying up the GPIB.
 *
 * \param pGPIBinterface   GPIB descriptor for HP8753 device
 * \param pData            pointer to command to send (OPC permitted) or binary data
 * \param nBytes           number of bytes or -1 for NULL terminated string
 * \param pGPIBstatus      pointer to GPIB status
 * \param timeoutSecs      timout period to wait
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
IF_GPIB_asyncSRQwrite( tGPIBinterface *pGPIBinterface, void *pData,
        gint nBytes, gdouble timeoutSecs ) {

#define SRQ_EVENT       1
#define TIMEOUT_EVENT   0
#define THIRTY_MS 0.030

    gchar *pPayload = NULL;

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint currentTimeoutDevice, currentTimeoutController;
    gdouble waitTime = 0.0;
    gint GPIBcontrollerIndex = 0;
    gint nTotalBytes = 0;

#define SIZE_OPC_NOOP    9    // # bytes in OPC;NOOP;

    if( nBytes < 0 ) {
        pPayload = g_strdup_printf( "OPC;%s", (gchar *)pData );
        nTotalBytes = strlen( pPayload );
    } else {
        pPayload = g_malloc( nBytes + SIZE_OPC_NOOP );
        memcpy( pPayload, (guchar *)pData, nBytes );
        memcpy( pPayload + nBytes, "OPC;NOOP;", SIZE_OPC_NOOP );
        nTotalBytes = nBytes + SIZE_OPC_NOOP;
    }

    if( IF_GPIB_asyncWrite( pGPIBinterface, pPayload, nTotalBytes, timeoutSecs ) != eRDWT_OK ) {
        g_free( pPayload );
        return eRDWT_ERROR;
    } else {
        g_free( pPayload );
    }

    // get the controller index
    ibask( pGPIBinterface->descriptor, IbaBNA, &GPIBcontrollerIndex);
    ibask( pGPIBinterface->descriptor, IbaTMO, &currentTimeoutDevice);
    ibtmo( pGPIBinterface->descriptor, T1s);
    ibask( GPIBcontrollerIndex, IbaTMO, &currentTimeoutController);
    ibtmo( GPIBcontrollerIndex, T30ms);    // just to check if we've been ordered to abandon ship
    DBG( eDEBUG_EXTENSIVE, "Waiting for SRQ" );
    do {
        short waitResult = 0;
        char status = 0;
        // This will timeout every 30ms (the timeout we set for the controller)
        WaitSRQ( GPIBcontrollerIndex, &waitResult);

        if ( waitResult == SRQ_EVENT ) {
            // This actually is an SRQ ..  is it from the HP8753 ?
            // Serial poll for status to reset SRQ and find out if it was the HP8753
            if( ( pGPIBinterface->status = ibrsp( pGPIBinterface->descriptor, &status)) & ERR ) {
                LOG(G_LOG_LEVEL_CRITICAL, "HPIB serial poll fail %04X/%d", pGPIBinterface->status, AsyncIberr());
                rtn = eRDWT_ERROR;
            } else if( status & ST_SRQ ) {
                // there is but one condition that asserts the SRQ ... the OPC
                // so it's probably not necessary in our setup to read the ESR.
                // It also tends to re-enable the SRQ ... which is odd
#ifndef CLEAR_ESR
                // We've cleared the SRQ bit in the Status Register by the serial poll
                // now clear the ESR flag by reading it
#define ESR_RESPONSE_MAXSIZE    5        // more than enough
                gchar sESR[ ESR_RESPONSE_MAXSIZE ] = {0};
                // For some bazaar reason the HP8753C can raise SRQ again when the ESR?; is written and cleared when is read
                // so mask it out before that.

                if( GPIBasyncWrite(pGPIBinterface , "ESR?;",
                                            10 * TIMEOUT_RW_1SEC) == eRDWT_OK
                    && GPIBasyncRead( pGPIBinterface, sESR, ESR_RESPONSE_MAXSIZE,
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
                pGPIBinterface->status |= ERR;
                rtn = eRDWT_ABORT;
            }
        }
        waitTime += THIRTY_MS;
        if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
            gchar *sMessage;
            if( nBytes == WAIT_STR && timeoutSecs > 15 ) {    // this means we have a "WAIT;" message .. so show the estimated time
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
    ibtmo( pGPIBinterface->descriptor, currentTimeoutDevice);
    ibtmo( GPIBcontrollerIndex, currentTimeoutDevice);

    if( rtn == eRDWT_CONTINUE ) {
        pGPIBinterface->status |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}

