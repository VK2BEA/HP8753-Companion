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

gint GPIBwriteBinary( gint GPIBdescriptor, const void *sRequest, gint length, gint *pGPIBstatus );
gint GPIBread( gint GPIBdescriptor, void *sResult, gint length, gint *pGPIBstatus );
gint GPIBwrite( gint GPIBdescriptor, const void *sRequest, gint *GPIBstatus );
gint GPIBwriteOneOfN( gint GPIBdescriptor, const void *sRequest, gint number, gint *GPIBstatus );

typedef enum { eRDWT_OK=0, eRDWT_ERROR, eRDWT_TIMEOUT, eRDWT_ABORT, eRDWT_CONTINUE, eRDWT_PREVIOUS_ERROR } tGPIBReadWriteStatus;

tGPIBReadWriteStatus GPIBasyncRead( gint GPIBdescriptor, void *readBuffer, glong maxBytes, gint *pGPIBstatus,
		gdouble timeout );
tGPIBReadWriteStatus GPIBasyncWrite( gint GPIBdescriptor, const void *sData, gint *GPIBstatus, gdouble timeoutSecs );

#define GPIBfailed(x) (((x) & ERR) == ERR)
#define GPIBsucceeded(x) (((x) & ERR) != ERR)

#define TIMEOUT_READ_1SEC   1.0
#define TIMEOUT_READ_1MIN  60.0

#endif /* GPIBCOMMS_H_ */
