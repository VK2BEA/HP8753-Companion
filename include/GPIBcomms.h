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

#ifndef GPIBCOMMS_H_
#define GPIBCOMMS_H_

typedef enum { eRDWT_OK=0, eRDWT_ERROR, eRDWT_TIMEOUT, eRDWT_ABORT, eRDWT_CONTINUE, eRDWT_PREVIOUS_ERROR } tGPIBReadWriteStatus;
typedef enum { eTMO_SET, eTMO_SAVE_AND_SET, eTMO_RESTORE } tTimeoutPurpose;


typedef struct {
    tGPIBtype   interfaceType;
    gint descriptor;
    gint status;
    gint nChars;
} tGPIBinterface;

gint GPIBwriteBinary(  tGPIBinterface *, const void *, gint, gint * );
gint GPIBread(  tGPIBinterface *, void *sData, gint, gint * );
gint GPIBwrite(  tGPIBinterface *, const void *, gint * );
gint GPIBwriteOneOfN(  tGPIBinterface *, const void *, gint, gint * );
tGPIBReadWriteStatus GPIBasyncWriteOneOfN(  tGPIBinterface *, const void *, gint, gdouble );
tGPIBReadWriteStatus GPIBasyncRead(  tGPIBinterface * , void *, glong , gdouble );
tGPIBReadWriteStatus GPIBasyncWrite(  tGPIBinterface * , const void *, gdouble );
tGPIBReadWriteStatus GPIBasyncWriteBinary(  tGPIBinterface *, const void *, size_t, gdouble  );

tGPIBReadWriteStatus IF_GPIB_asyncWrite( tGPIBinterface *, const void *, size_t, gdouble );
tGPIBReadWriteStatus IF_USBTMC_asyncWrite( tGPIBinterface *, const void *, size_t, gdouble );
tGPIBReadWriteStatus IF_Prologix_asyncWrite( tGPIBinterface *, const void *, size_t, gdouble );
tGPIBReadWriteStatus IF_GPIB_asyncRead(  tGPIBinterface *, void *, long, gdouble);
tGPIBReadWriteStatus IF_USBTMC_asyncRead( tGPIBinterface *, void *, long, gdouble);
tGPIBReadWriteStatus IF_Prologix_asyncRead( tGPIBinterface *, void *, long, gdouble);
gint IF_USBTMC_open( tGlobal *, tGPIBinterface * );
gint IF_GPIB_open( tGlobal *, tGPIBinterface * );
gint IF_Prologix_open( tGlobal *, tGPIBinterface * );
gint IF_GPIB_close( tGPIBinterface *);
gint IF_USBTMC_close( tGPIBinterface *);
gint IF_Prologix_close( tGPIBinterface *);
gboolean IF_GPIB_ping( tGPIBinterface * );
gboolean IF_USBTMC_ping( tGPIBinterface * );
gboolean IF_Prologix_ping( tGPIBinterface * );
gint IF_GPIB_timeout( tGPIBinterface *, gint, gint *, tTimeoutPurpose );
gint IF_USBTMC_timeout( tGPIBinterface *, gint, gint *, tTimeoutPurpose );
gint IF_Prologix_timeout( tGPIBinterface *, gint, gint *, tTimeoutPurpose );
gint IF_GPIB_local( tGPIBinterface * );
gint IF_USBTMC_local( tGPIBinterface * );
gint IF_Prologix_local( tGPIBinterface * );
gint IF_GPIB_clear( tGPIBinterface * );
gint IF_USBTMC_clear( tGPIBinterface * );
gint IF_Prologix_clear( tGPIBinterface * );
gint IF_GPIB_readConfiguration( tGPIBinterface *, gint, gint *, gint * );
tGPIBReadWriteStatus IF_GPIB_asyncSRQwrite( tGPIBinterface *, void *, gint, gdouble );
tGPIBReadWriteStatus IF_USBTMC_asyncSRQwrite( tGPIBinterface *, void *, gint, gdouble );
tGPIBReadWriteStatus IF_Prologix_asyncSRQwrite( tGPIBinterface *, void *, gint, gdouble );

tGPIBReadWriteStatus GPIBasyncSRQwrite(  tGPIBinterface * , void *, gint, gdouble );
tGPIBReadWriteStatus GPIBenableSRQonOPC(  tGPIBinterface * );

gint GPIBtimeout( tGPIBinterface *, gint, gint *, tTimeoutPurpose );
gint GPIBclear( tGPIBinterface * );
gint GPIBlocal( tGPIBinterface * );

#define NULL_STR	-1
#define WAIT_STR	-2
#define TIMEOUT_SAFETY_FACTOR	1.5

#define ERR_TIMEOUT (0x1000)
#define GPIBfailed(x) (((x) & (ERR | ERR_TIMEOUT)) != 0)
#define GPIBsucceeded(x) (((x) & (ERR | ERR_TIMEOUT)) == 0)

#define TIMEOUT_RW_1SEC   1.0
#define TIMEOUT_RW_1MIN  60.0

#endif /* GPIBCOMMS_H_ */
