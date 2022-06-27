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

typedef enum { eRD_OK=0, eRD_ERROR, eRD_TIMEOUT, eRD_ABORT, eRD_CONTINUE } tReadStatus;

tReadStatus GPIB_AsyncRead( gint GPIBdescriptor, void *readBuffer, long maxBytes, gint *pGPIBstatus,
		gdouble timeout, GAsyncQueue *queue );

#define GPIBfailed(x) (((x) & ERR) == ERR)
#define GPIBsucceeded(x) (((x) & ERR) != ERR)

#endif /* GPIBCOMMS_H_ */
