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

#ifndef HP8753COMMS_H_
#define HP8753COMMS_H_

gboolean askOption( gint descGPIB_HP8753, gchar *option, gint *pGPIBstatus );

gint askHP8753C_dbl(gint descGPIB_HP8753, gchar *mnemonic, gdouble *dresult, gint *pGPIBstatus);

gint getHP8753channelListFreqSegments(gint descGPIB_HP8753, tGlobal *pGlobal, eChannel channel, gint *pGPIBstatus );
gint getHP8753channelTrace(gint descGPIB_HP8753, tGlobal *pGlobal, eChannel channel, gint *pGPIBstatus );
gint acquireHPGLplot( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );
gint get8753firmwareVersion(gint descGPIB_HP8753, gchar **psProduct, gint *pGPIBstatus);

gint getHP8753markersAndSegments( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );
gint get8753setupAndCal( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );
gint send8753setupAndCal( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );
gint getHP8753switchOnOrOff( gint GPIBdescriptor, gchar *sRequest, gint *pGPIBstatus );

gint getHP8753format( gint descGPIB_HP8753, gint *pGPIBstatus );
gint getHP8753sweepType( gint descGPIB_HP8753, gint *pGPIBstatus );
gint getHP8753measurementType( gint descGPIB_HP8753, gint *pGPIBstatus );
gint getHP8753smithMkrType( gint descGPIB_HP8753, gint *pGPIBstatus );
gint getHP8753polarMkrType( gint descGPIB_HP8753, gint *pGPIBstatus );
gint getHP8753calType( gint descGPIB_HP8753, gint *pGPIBstatus );

gint get8753learnString( gint descGPIB_HP8753, guchar **ppLearnString, gint *pGPIBstatus );
gboolean analyze8753learnString( gint descGPIB_HP8753, tLearnStringIndexes *pLSindex, gint *pGPIBstatus );
gint process8753learnString( gint descGPIB_HP8753, guchar *, tGlobal *pGlobal, gint *pGPIBstatus );
gboolean getStartStopOrCenterSpanFrom8753learnString( guchar *pLearn, tGlobal *pGlobal, eChannel channel );
eChannel getActiveChannelFrom8753learnString( guchar *pLearn, tGlobal *pGlobal );

gint findHP8753option(gint descGPIB_HP8753, const HP8753C_option *optList,
		gint maxOptions, gint *pGPIBstatus);

gboolean selectLearningStringIndexes( tGlobal *pGlobal );

gint setHP8753channel( gint descGPIB_HP8753, eChannel channel, gint *pGPIBstatus );

gint getHP3753_S2P( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );
gint getHP3753_S1P( gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );

#define MAX_OUTPCAL_LEN	15

enum { eCALtypeNONE = 0, eCALtypeRESPONSE = 1, eCALtypeRESPONSEandISOLATION = 2, eCALtypeS11onePort = 3,
	   eCALtypeS22onePort = 4, eCALtypeFullTwoPort = 5, eCALtype1pathTwoPort = 6, eCALtypeTRL_LRM_TwoPort = 8
};

#endif /* HP8753_H_ */
