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

gboolean askOption( tGPIBinterface *, gchar * );

gint askHP8753_dbl( tGPIBinterface *, gchar *, gdouble * );

gint getHP8753channelListFreqSegments( tGPIBinterface *, tGlobal *, eChannel );
gint getHP8753channelTrace(tGPIBinterface *, tGlobal *, eChannel );
gint acquireHPGLplot( tGPIBinterface *, tGlobal * );
gint get8753firmwareVersion( tGPIBinterface *, gchar ** );

gint getHP8753markersAndSegments( tGPIBinterface *, tGlobal * );
gint get8753setupAndCal( tGPIBinterface *, tGlobal * );
gint send8753setupAndCal( tGPIBinterface *, tGlobal * );
gint getHP8753switchOnOrOff( tGPIBinterface *, gchar * );

gint getHP8753format( tGPIBinterface * );
gint getHP8753sweepType( tGPIBinterface * );
gint getHP8753measurementType( tGPIBinterface * );
gint getHP8753smithMkrType( tGPIBinterface * );
gint getHP8753polarMkrType( tGPIBinterface * );
gint getHP8753calType( tGPIBinterface * );

gint get8753learnString( tGPIBinterface *, guchar ** );
gboolean analyze8753learnString( tGPIBinterface *, tLearnStringIndexes * );
gint process8753learnString( tGPIBinterface *, guchar *, tGlobal * );
gboolean getStartStopOrCenterSpanFrom8753learnString( guchar *, tGlobal *, eChannel );
eChannel getActiveChannelFrom8753learnString( guchar *, tGlobal * );
gint sendHP8753calibrationKit (tGPIBinterface *, tGlobal * );

gint findHP8753option( tGPIBinterface *, const HP8753_option *, gint );

gboolean selectLearningStringIndexes( tGlobal * );

gint setHP8753channel( tGPIBinterface *, eChannel );

gint getHP3753_S2P( tGPIBinterface *, tGlobal * );
gint getHP3753_S1P( tGPIBinterface *, tGlobal * );

#define MAX_OUTPCAL_LEN	15

enum { eCALtypeNONE = 0, eCALtypeRESPONSE = 1, eCALtypeRESPONSEandISOLATION = 2, eCALtypeS11onePort = 3,
	   eCALtypeS22onePort = 4, eCALtypeFullTwoPort = 5, eCALtype1pathTwoPort = 6, eCALtypeTRL_LRM_TwoPort = 8
};

#endif /* HP8753_H_ */
