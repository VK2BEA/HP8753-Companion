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

#ifndef MESSAGEEVENT_H_
#define MESSAGEEVENT_H_

gboolean messageEventPrepare (GSource *, gint *);
gboolean messageEventCheck (GSource *);
gboolean messageEventDispatch (GSource *, GSourceFunc, gpointer);

#define MSG_STRING_SIZE 256


enum _threadmessage
{
	TM_INFO,							// show information
	TM_INFO_HIGHLIGHT,					// show information with a highlight (green)
    TM_ERROR,							// show error message (in red)

	TM_COMPLETE_GPIB,					// update widgets based on GPIB connection
	TM_REFRESH_TRACE,					// redraw trace(s)
	TM_SAVE_SETUPandCAL,				// save calibration and setup to database
	TM_SAVE_LEARN_STRING_ANALYSIS,		// save analyzed learn string indexes
	TM_SAVE_S2P,						// save calibration and setup to database

	TG_SETUP_GPIB,						// configure GPIB
	TG_RETRIEVE_SETUPandCAL_from_HP8753,// get current calibration and setup
	TG_SEND_SETUPandCAL_to_HP8753,		// restore calbration and setup
	TG_SEND_CALKIT_to_HP8753,		// restore calbration and setup
	TG_RETRIEVE_TRACE_from_HP8753,		// get traces
	TG_MEASURE_and_RETRIEVe_S2P_from_HP8753,	// S2P
	TG_ANALYZE_LEARN_STRING,			// get learn string and find the indexes to setup data
	TG_UTILITY,
	TG_ABORT,
	TG_END								// end thread
};

typedef struct
{
    enum _threadmessage command;
    gchar *		sMessage;
    void  *		data;
    gint		dataLength;
} messageEventData;


extern GSourceFuncs 	messageEventFunctions;


void postMessageToMainLoop (enum _threadmessage Command, gchar *sMessage);
void postInfoWithCount(gchar *sMessageWithFormat, gint number, gint number2);
void postDataToMainLoop (enum _threadmessage Command, void *data);
void postDataToGPIBThread (enum _threadmessage Command, void *data);

#define postInfo(x)		postMessageToMainLoop( TM_INFO, (x) )
#define postError(x)	postMessageToMainLoop( TM_ERROR, (x) )

#endif /*MESSAGEEVENT_H_*/
