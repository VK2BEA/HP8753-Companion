/*
 * Copyright (c) 2022-2026 Michael G. Katzmann
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
#include <locale.h>

#include "hp8753.h"
#include "GPIBcomms.h"
#include "hp8753comms.h"
#include "messageEvent.h"

/*!     \brief  Write data from the Prologix device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753   pointer GPIB device data
 * \param sData          pointer to data to write
 * \param length         number of bytes to write
 * \param timeout        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
IF_Prologix_asyncWrite( tGPIBinterface *pGPIB_HP8753, const void *pData, size_t length,
        gdouble timeoutSecs) {
    return eRDWT_OK;
}

/*!     \brief  Read data from the Prologix device asynchronously
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
IF_Prologix_asyncRead(  tGPIBinterface *pGPIB_HP8753, void *readBuffer, long maxBytes, gdouble timeoutSecs) {
    return eRDWT_OK;
}

/*!     \brief  Ping Prologix interface
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param pGPIB_HP8753   pointer to GPIB interface structure
 * \return               TRUE if device responds or FALSE if not
 */
gboolean
IF_Prologix_ping( tGPIBinterface *pGPIB_HP8753 ) {
    return TRUE;
}

/*!     \brief  open the Prologix device
 *
 * Get the device descriptors of the contraller and GPIB device
 * based on the paramaters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pGPIB_HP8753        pointer to GPIB interfcae structure
 * \return                    0 on sucess or ERROR on failure
 */
gint
IF_Prologix_open( tGlobal *pGlobal, tGPIBinterface *pGPIB_HP8753 ) {
    return OK;
}

/*!     \brief  close the Prologix devices
 *
 * Close the GPIB device
 *
 * \param pGPIB_HP8753      pointer to GPIB device structure
 */
gint
IF_Prologix_close( tGPIBinterface *pGPIB_HP8753) {
    return OK;
}

/*!     \brief  Set or restore timeout
 *
 * Sets a new Prologix timeout and optionally saves the current value
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param pGPIB_HP8753      pointer to GPIB interfcae structure
 * \param value             new timeout value
 * \param pSavedTimeout     pointer to where to save current timeout
 * \param purpose           enum command
 * \return                  status result
 */
gint
IF_Prologix_timeout( tGPIBinterface *pGPIB_HP8753, gint value, gint *pSavedTimeout, tTimeoutPurpose purpose ) {
    return 0;
}

/*!     \brief  Set Prologix device to local control
 *
 * Set GPIB device to local control
 *
 * \param pGPIB_HP8753   pointer to GPIB device structure
 * \return               read status result
 */
gint
IF_Prologix_local( tGPIBinterface *pGPIB_HP8753 ) {
    return 0;
}

/*!     \brief  Send clear to the Prologix interface
 *
 * Sent the clear command to the Prologix interface
 *
 * \param pGPIB_HP8753   pointer to GPIB interfcae structure
 * \return               read status result
 */
gint
IF_Prologix_clear( tGPIBinterface *pGPIB_HP8753 ) {
    return 0;
}

/*!     \brief  Write string preceeded with OPC or binary adding OPC;NOOP;, then wait for SRQ
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 * After a command that sets the OPC, wait for the event without tying up the GPIB.
 *
 * \param pGPIB_HP8753     GPIB descriptor for HP8753 device
 * \param pData            pointer to command to send (OPC permitted) or binary data
 * \param nBytes           number of bytes or -1 for NULL terminated string
 * \param timeoutSecs      timout period to wait
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
IF_Prologix_asyncSRQwrite( tGPIBinterface *pGPIB_HP8753, void *pData,
        gint nBytes, gdouble timeoutSecs ) {
    return eRDWT_OK;
}
