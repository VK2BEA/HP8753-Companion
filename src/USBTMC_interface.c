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
#include <poll.h>
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


/*!     \brief  Write data from the USBTC USB488 device asynchronously
 *
 * Write data to the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753   pointer GPIB device data
 * \param sData          pointer to data to write
 * \param length         number of bytes to write
 * \param timeout        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
IF_USBTMC_asyncWrite( tGPIBinterface *pGPIB_HP8753, const void *pData, size_t length,
        gdouble timeoutSecs) {

    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint    currentTimeout = 5000;
    gint    result = 0;
    gint    nbytes = 0;

    struct aiocb cb;
    memset(&cb, 0, sizeof(cb));
    cb.aio_fildes = pGPIB_HP8753->descriptor;
    cb.aio_buf = (void *)pData;
    cb.aio_nbytes = length;
    cb.aio_sigevent.sigev_notify = SIGEV_NONE;

    const struct aiocb *const aiocb_list[] = { &cb };
    struct timespec thirtyMS = { .tv_nsec = 30 * 1000 * 1000 };  // 30ms

    pGPIB_HP8753->status = ERR_TIMEOUT;

    GPIBtimeout( pGPIB_HP8753, TNONE, &currentTimeout, eTMO_SAVE_AND_SET );
    // schedule asynchronous write
    if ( aio_write(&cb) != 0 ) {
        pGPIB_HP8753->status = ERR;
        return eRDWT_ERROR;
    }

    do {
        // synchronously wait for the operation to complete or time out in 30ms
        aio_suspend( aiocb_list, 1, &thirtyMS );
        // Check for error
        result = aio_error(&cb);

        if (result == EINPROGRESS ) {
            // Timeout
            rtn = eRDWT_CONTINUE;
            waitTime += THIRTY_MS;
            if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
                gchar *sMessage = g_strdup_printf("✍🏻 Waiting for HP8753: %ds", (gint) (waitTime));
                postInfo(sMessage);
                g_free(sMessage);
            }
        } else if( result != 0  ) {
            pGPIB_HP8753->status = ERR;
            rtn = eRDWT_ERROR;
        // or did we complete the read
        } else {
            // check what the actually write returned
            nbytes = aio_return(&cb);
            if( nbytes >= 0 ) {
                pGPIB_HP8753->status = CMPL;
                rtn = eRDWT_OK;
            } else {
                nbytes = 0;
                pGPIB_HP8753->status = ERR;
                rtn = eRDWT_ERROR;
            }
        }
        // If we get a message on the queue, it is assumed to be an abort
        if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
            // This will stop future GPIB commands for this sequence
            pGPIB_HP8753->status |= ERR;
            rtn = eRDWT_ABORT;
        }
    } while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn != eRDWT_OK)
        aio_cancel( pGPIB_HP8753->descriptor, &cb );

    pGPIB_HP8753->nChars = nbytes;

    DBG(eDEBUG_EXTREME, "🖊 HP8753: %d / %d bytes", pGPIB_HP8753->nChars, length);

    if (( pGPIB_HP8753->status & CMPL) != CMPL) {
        if (waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "USBTMC async write timeout after %.2f sec. status %04X",
                    timeoutSecs, pGPIB_HP8753->status );
        else
            LOG(G_LOG_LEVEL_CRITICAL, "USBTMC async write status/errno: %04X/%d",
                    pGPIB_HP8753->status, errno);
    }

    GPIBtimeout( pGPIB_HP8753, TNONE, &currentTimeout, eTMO_RESTORE );

    if ( waitTime > FIVE_SECONDS )
        postInfo("");

    if( rtn == eRDWT_CONTINUE ) {
        pGPIB_HP8753->status |= ERR_TIMEOUT;
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
 * \param pGPIB_HP8753   GPIB device descriptor
 * \param readBuffer     pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param timeout        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
IF_USBTMC_asyncRead(  tGPIBinterface *pGPIB_HP8753, void *readBuffer, long maxBytes, gdouble timeoutSecs) {
    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint    currentTimeout;
    gint    result = 0;
    gint    nbytes = 0;
    gboolean bMore = FALSE;

    struct aiocb cb;
    memset(&cb, 0, sizeof(cb));
    cb.aio_fildes = pGPIB_HP8753->descriptor;
    cb.aio_buf = (void *)readBuffer;
    cb.aio_nbytes = maxBytes;
    cb.aio_sigevent.sigev_notify = SIGEV_NONE;

    const struct aiocb *const aiocb_list[] = { &cb };
    struct timespec thirtyMS = { .tv_nsec = 30 * 1000 * 1000 };  // 30ms

#define EOI	0x01
	guint8 msgStatus;

    pGPIB_HP8753->status = ERR_TIMEOUT;
    GPIBtimeout( pGPIB_HP8753, TNONE, &currentTimeout, eTMO_SAVE_AND_SET );

    // schedule asynchronous read
    pGPIB_HP8753->nChars = 0;

    do {
		if ( aio_read(&cb) != 0 ) {
			pGPIB_HP8753->status = ERR;
			return eRDWT_ERROR;
		}
		// wait for read to complete or for elapsed time to expire
		// actual read suspend timeout is 30ms, so we don't stall (and can check for abort)
		do {
			bMore = FALSE;
			// synchronously wait for the operation to complete or time out in 30ms
			aio_suspend( aiocb_list, 1, &thirtyMS );
			// Check for error
			result = aio_error(&cb);
			if (result == EINPROGRESS ) {
				// Timeout
				rtn = eRDWT_CONTINUE;
				waitTime += THIRTY_MS;
				if (waitTime > FIVE_SECONDS && fmod(waitTime, 1.0) < THIRTY_MS) {
					gchar *sMessage = g_strdup_printf("👀 Waiting for HP8753: %ds", (gint) (waitTime));
					postInfo(sMessage);
					g_free(sMessage);
				}
			} else if( result != 0  ) {
				pGPIB_HP8753->status = ERR;
				rtn = eRDWT_ERROR;
			// or did we complete the read
			} else {
				nbytes = aio_return(&cb);
				if( nbytes >= 0 ) {
					pGPIB_HP8753->nChars += nbytes;

				    ioctl( cb.aio_fildes, USBTMC_IOCTL_MSG_IN_ATTR, &msgStatus);

					if( (msgStatus  & EOI) || nbytes ==  cb.aio_nbytes ) {
						pGPIB_HP8753->status = CMPL;
						rtn = eRDWT_OK;
					} else {
						bMore = TRUE;
						cb.aio_buf += nbytes;
						cb.aio_nbytes -= nbytes;
						rtn = eRDWT_CONTINUE;
					}
				} else {
					nbytes = 0;
					pGPIB_HP8753->status = ERR;
					rtn = eRDWT_ERROR;
				}
			}

			// If we get a message on the queue, it is assumed to be an abort
			if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
				// This will stop future GPIB commands for this sequence
				pGPIB_HP8753->status |= ERR;
				rtn = eRDWT_ABORT;
			}
		} while (!bMore && rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));
    } while ( bMore );

    if (rtn != eRDWT_OK)
        aio_cancel( pGPIB_HP8753->descriptor, &cb );

    DBG(eDEBUG_EXTREME, "👓 HP8753: %d bytes (%d max)", nbytes, maxBytes);

    if (( pGPIB_HP8753->status & CMPL) != CMPL) {
        if (waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "USBTMC async read timeout after %.2f sec. status %04X",
                    timeoutSecs, pGPIB_HP8753->status );
        else
            LOG(G_LOG_LEVEL_CRITICAL, "USBTMC async read status/errno: %04X/%d",
                    pGPIB_HP8753->status, errno);
    }

    if ( waitTime > FIVE_SECONDS )
        postInfo("");

    GPIBtimeout( pGPIB_HP8753, TNONE, &currentTimeout, eTMO_RESTORE );

    // Indicate end with the same bit as the linux GPIB does
    if( msgStatus & EOI )
    	pGPIB_HP8753->status |= END;

    if( rtn == eRDWT_CONTINUE ) {
        pGPIB_HP8753->status |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}


/*!     \brief  open the USBTMC USB488 device
 *
 * Get the file descriptor for the USBTMC USB488 device using the
 * controller number as the minor number (e.g. /dev/usbtmc0
 *
 * \param pGlobal             pointer to global data structure
 * \param pGPIB_HP8753        pointer to GPIB device structure
 * \return                    0 on sucess or ERROR on failure
 */
gint
IF_USBTMC_open( tGlobal *pGlobal, tGPIBinterface *pGPIB_HP8753 ) {

   if ( pGlobal->GPIBcontrollerIndex < 0 )
       return ERROR;

   if( pGPIB_HP8753->descriptor >= 0 ) {
       close( pGPIB_HP8753->descriptor );
   }

   pGPIB_HP8753->descriptor = INVALID;

   gchar *devicePath = g_strdup_printf( "/dev/usbtmc%d", pGlobal->GPIBcontrollerIndex );
   gint fileDescriptor = open( devicePath, O_RDWR);
   g_free( devicePath );

   if( fileDescriptor == -1 ) {
       postError("Cannot open USBTMC device");
       return ERROR;
   } else {
	   pGPIB_HP8753->descriptor = fileDescriptor;
       postInfo("Contact with HP8753 established via USBTMC");
       GPIBlocal( pGPIB_HP8753  );
       usleep( LOCAL_DELAYms * 1000);
   }
   return OK;
}

/*!     \brief  close the USBTMC interface
 *
 * Close the controller if it was opened and close the device
 *
 * \param pDescGPIB_HP8753    pointer to GPIB device structure
 */
gint
IF_USBTMC_close( tGPIBinterface *pGPIB_HP8753) {
    gint rtn = OK;
    pGPIB_HP8753->status = 0;

    if( pGPIB_HP8753->descriptor >= 0 )
        if( close( pGPIB_HP8753->descriptor ) == ERROR ) {
            pGPIB_HP8753->status = ERR;
            rtn = ERROR;
        }

    pGPIB_HP8753->descriptor = INVALID;
    return rtn;
}


/*!     \brief  Ping GPIB interface
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param pGPIB_HP8753   pointer to GPIB interface structure
 * \return               TRUE if device responds or FALSE if not
 */
gboolean
IF_USBTMC_ping( tGPIBinterface *pGPIB_HP8753 ) {
    unsigned char statusByte = 0;
    gint status = ioctl( pGPIB_HP8753->descriptor, USBTMC488_IOCTL_READ_STB, &statusByte );
    if ( status == ERROR )
        return FALSE;
    else
        return TRUE;

    // return( fcntl( pGPIB_HP8753->descriptor, F_GETFD) != -1 || errno != EBADF );
}

gint GPIB_TO_USBTMC_TIMEOUT[] = { 100000,
                                     100,    100,    100,    100,    100,                   // us
                                     100,    100,    100,    100,    100,    300,         // ms
                                    1000,   3000,  10000,  30000, 100000, 300000, 1000000
};


/*!     \brief  Set or restore timeout
 *
 * Sets a new USBTMC timeout and optionally saves the current value
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIBinterface    pointer to USBTMC interface structure
 * \param value             new timeout value
 * \param savedTimeout      pointer to where to save current timeout
 * \param purpose           enum command
 * \return                  status result
 */
gint
IF_USBTMC_timeout( tGPIBinterface *pGPIB_HP8753, gint value, gint *savedTimeout, tTimeoutPurpose purpose ) {
    gint rtn = ERROR;

    switch( purpose ) {
    case eTMO_SAVE_AND_SET:
        if( savedTimeout != NULL )
            rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC_IOCTL_GET_TIMEOUT, savedTimeout );
        if ( rtn != ERROR )
            rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC_IOCTL_SET_TIMEOUT, &GPIB_TO_USBTMC_TIMEOUT[ value ] );
        pGPIB_HP8753->status = (rtn == ERROR ? ERR : 0 );
        break;
    default:
    case eTMO_SET:
        rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC_IOCTL_SET_TIMEOUT, &GPIB_TO_USBTMC_TIMEOUT[ value ] );
        pGPIB_HP8753->status = (rtn == ERROR ? ERR : 0 );
        break;
    case eTMO_RESTORE:
        rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC_IOCTL_GET_TIMEOUT, &savedTimeout );
        pGPIB_HP8753->status = (rtn == ERROR ? ERR : 0 );
        break;
    }

    return pGPIB_HP8753->status;
}

/*!     \brief  Set USBTMC GPIB device to local control
 *
 * Set GPIB device to local control
 *
 * \param pGPIB_HP8753   pointer to GPIB device structure
 * \return               read status result
 */
gint
IF_USBTMC_local( tGPIBinterface *pGPIB_HP8753 ) {
    usleep( ms(40) );
    gint rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC488_IOCTL_GOTO_LOCAL );
    pGPIB_HP8753->status = (rtn == ERROR ? ERR : 0 );

    return pGPIB_HP8753->status;
}

/*!     \brief  Send clear to the USBTMC GPIB interface
 *
 * Sent the clear command to the GPIB interface
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \return               read status result
 */
gint
IF_USBTMC_clear( tGPIBinterface *pGPIB_HP8753 ) {
    gint    rtn = 0;

    rtn = ioctl( pGPIB_HP8753->descriptor, USBTMC_IOCTL_CLEAR );
    pGPIB_HP8753->status = (rtn == ERROR ? ERR : 0 );

    return pGPIB_HP8753->status;
}

/*!     \brief  Write string preceeded with OPC or binary adding OPC;NOOP;, then wait for SRQ
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 * After a command that sets the OPC, wait for the event without tying up the GPIB.
 *
 * \param pGPIB_HP8753      pointer to HP8753 device
 * \param pData             ointer to command to send (OPC permitted) or binary data
 * \param nBytes            number of bytes or -1 for NULL terminated string
 * \param timeoutSecs        timout period to wait
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
IF_USBTMC_asyncSRQwrite( tGPIBinterface *pGPIB_HP8753, void *pData,
        gint nBytes, gdouble timeoutSecs ) {

#define SRQ_EVENT       1
#define TIMEOUT_EVENT   0
#define THIRTY_MS 0.030

    gchar *pPayload = NULL;

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    gint currentTimeoutDevice;
    gdouble waitTime = 0.0;
    gint nTotalBytes = 0;
    gint status = 0;
    unsigned char statusByte;

#define SIZE_OPC_NOOP    9    // # bytes in OPC;NOOP;

    IF_USBTMC_timeout( pGPIB_HP8753, T1s, &currentTimeoutDevice, eTMO_SAVE_AND_SET );

    if( nBytes < 0 ) {
        pPayload = g_strdup_printf( "OPC;%s", (gchar *)pData );
        nTotalBytes = strlen( pPayload );
    } else {
        pPayload = g_malloc( nBytes + SIZE_OPC_NOOP );
        memcpy( pPayload, (guchar *)pData, nBytes );
        memcpy( pPayload + nBytes, "OPC;NOOP;", SIZE_OPC_NOOP );
        nTotalBytes = nBytes + SIZE_OPC_NOOP;
    }

    DBG(eDEBUG_EXTREME, "🖊 HP8753: %s", pPayload);
    if( IF_USBTMC_asyncWrite( pGPIB_HP8753, pPayload, nTotalBytes, timeoutSecs ) != eRDWT_OK ) {
        g_free( pPayload );
        return eRDWT_ERROR;
    } else {
        g_free( pPayload );
    }

    struct pollfd sFD = {.fd = pGPIB_HP8753->descriptor, .events = POLLPRI };
    DBG( eDEBUG_EXTENSIVE, "Waiting for SRQ" );
    do {
        // This will timeout every 30ms waiting for the SR
        gint rtnPoll = poll( &sFD, 1, 30 );
        if( rtnPoll == ERROR ) {
            if (errno == EINTR) continue;
        } else if( rtnPoll == 0 ) {
            // timeout
            // If we get a message on the queue, it is assumed to be an abort
            if (checkMessageQueue( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
                // This will stop future GPIB commands for this sequence
                pGPIB_HP8753->status |= ERR;
                rtn = eRDWT_ABORT;
            }
        } else if ( sFD.revents & POLLPRI ) {
            // SRQ complete
        	status = ioctl( pGPIB_HP8753->descriptor, USBTMC488_IOCTL_READ_STB, &statusByte );
            if (ERROR == status ) {
                LOG(G_LOG_LEVEL_CRITICAL, "HPIB serial poll fail: %d", errno);
                pGPIB_HP8753->status = ERR;
                rtn = eRDWT_ERROR;
            } else if( statusByte & ST_SRQ ) {
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

#define SIZE_ESR_QUERY    5    // # bytes in ESR?;
                DBG(eDEBUG_EXTREME, "🖊 HP8753: %s", "ESR?;");
                if( IF_USBTMC_asyncWrite( pGPIB_HP8753 , "ESR?;", SIZE_ESR_QUERY,
                                            10 * TIMEOUT_RW_1SEC) == eRDWT_OK
                    && IF_USBTMC_asyncRead( pGPIB_HP8753, sESR, ESR_RESPONSE_MAXSIZE,
                                            10 * TIMEOUT_RW_1SEC ) == eRDWT_OK ) {
                    gint ESR = atoi( sESR );
                    if( ESR & ESE_OPC ) {
                        DBG(eDEBUG_EXTREME, "ESE_OPC set (%s)", sESR);
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
            // its not the HP8753 ... some other GPIB device is requesting service (should not happen with USBTMC)
        } else if (sFD.revents & (POLLERR | POLLNVAL)) {
            // problem
            rtn = eRDWT_ERROR;
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

    IF_USBTMC_timeout( pGPIB_HP8753, T1s, &currentTimeoutDevice, eTMO_RESTORE );

    if( rtn == eRDWT_CONTINUE ) {
        pGPIB_HP8753->status |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}


