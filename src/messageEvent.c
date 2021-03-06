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

#include <hp8753.h>
#include "messageEvent.h"

static gint clearTimerID = 0;
gboolean
clearNotification( gpointer labelWidget ) {
	gtk_label_set_text(GTK_LABEL( labelWidget ), "");
	clearTimerID = 0;
	return G_SOURCE_REMOVE;
}

GSourceFuncs messageEventFunctions = { messageEventPrepare, messageEventCheck,
		messageEventDispatch, NULL, };

/*!     \brief  Dispatch message posted by a GPIB thread
 *
 * Only the main event loop can update screen widgets.
 * Other threads post messages that are accepted here.
 *
 * Display messages (strings) are individually pulled from a queue.
 *
 * \param source   : GSource for the message event
 * \param callback : callback defined for this source (unused)
 * \param udata    : other data (unused)
 * \return G_SOURCE_CONTINUE to keep this source in the main loop
 */
gboolean
messageEventDispatch(GSource *source, GSourceFunc callback, gpointer udata) {
	messageEventData *message;

	tGlobal *pGlobal = &globalData;

	GtkLabel *wLabel = GTK_LABEL(
			g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Lbl_Status"));
	gchar *sMarkup;

	while ((message = g_async_queue_try_pop(pGlobal->messageQueueToMain))) {
		switch (message->command) {
		case TM_INFO:
		case TM_INFO_HIGHLIGHT:
			if( clearTimerID != 0 )
				g_source_remove( clearTimerID );
			clearTimerID = g_timeout_add ( 10000, clearNotification, wLabel );

			if( message->command == TM_INFO )
				sMarkup = g_markup_printf_escaped("<i>??%s</i>", message->sMessage);
			else
				sMarkup = g_markup_printf_escaped("<span color='darkgreen'>??<i>??%s</i></span>", message->sMessage);
			gtk_label_set_markup(wLabel, sMarkup);
			g_free(sMarkup);
//    		gtk_label_set_text( wLabel, message->sMessage);
			break;

		case TM_ERROR:
			if( clearTimerID != 0 )
				g_source_remove( clearTimerID );
			clearTimerID = g_timeout_add ( 15000, clearNotification, wLabel );

			gtk_label_set_text(wLabel, message->sMessage);

			sMarkup = g_markup_printf_escaped(
					"<span color='darkred'>??%s</span>", message->sMessage);
			gtk_label_set_markup(wLabel, sMarkup);
			g_free(sMarkup);

			break;

		case TM_SAVE_SETUPandCAL:
			if( saveCalibrationAndSetup( pGlobal, (gchar *)message->data ) != ERROR ) {
				GtkComboBoxText *wCalComboBox = GTK_COMBO_BOX_TEXT(
						g_hash_table_lookup(pGlobal->widgetHashTable,
								(gconstpointer )"WID_Combo_CalibrationProfile"));
				gtk_combo_box_text_remove_all ( wCalComboBox );
				g_list_foreach ( pGlobal->pCalList, updateCalCombobox, wCalComboBox );
				// choose the entry
				g_free( pGlobal->sCalProfile );
				pGlobal->sCalProfile = g_strdup( (gchar *)message->data );
				setGtkComboBox( GTK_COMBO_BOX(wCalComboBox), pGlobal->sCalProfile );
			}
			showCalInfo( &(pGlobal->HP8753cal), pGlobal );
			gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")), 0 );

			gtk_widget_set_sensitive(
					GTK_WIDGET( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall")),
					TRUE );
			gtk_widget_set_sensitive(
					GTK_WIDGET( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete")),
					TRUE );
			g_free( message->data );
			break;
		case TM_SAVE_LEARN_STRING_ANALYSIS:
			saveLearnStringAnalysis( pGlobal, (tLearnStringIndexes *)message->data );
			gchar *sFWlabel = g_strdup_printf( "Firmware %d.%d", pGlobal->HP8753.analyzedLSindexes.version/100,
					pGlobal->HP8753.analyzedLSindexes.version % 100 );
			gtk_label_set_label( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Firmware"),
					pGlobal->HP8753.analyzedLSindexes.version != 0 ? sFWlabel : "Firmware unknown");
			g_free( sFWlabel );
			break;

		case TM_SAVE_S2P:
			sensitiseControlsInUse( pGlobal, TRUE );
			FILE *fS2P;
			if( (fS2P = fopen( message->data, "w" )) == NULL ) {
				gchar *sError = g_strdup_printf( "Cannot write: %s", (gchar *)message->data);
				postError( sError );
				g_free( sError );
			} else {
				fprintf( fS2P,
						"! 2-port S-paramater data, multiple frequency points\n"
						"! from HP8753 Network analyzer\n"
						"# MHz S RI R 50.0\n"
						"! freq\tReS11\tImS11\tReS21\tImS21\tReS12\tImS12\tReS22\tImS22\n" );
				for( int i=0; i < pGlobal->HP8753.S2P.nPoints; i++ ) {
					fprintf( fS2P, "%lf\t%lg\t%lg\t%lg\t%lg\t%lg\t%lg\t%lg\t%lg\n",
							pGlobal->HP8753.S2P.freq[i]/1.0e6,
							pGlobal->HP8753.S2P.S11[i].r, pGlobal->HP8753.S2P.S11[i].i,
							pGlobal->HP8753.S2P.S21[i].r, pGlobal->HP8753.S2P.S21[i].i,
							pGlobal->HP8753.S2P.S12[i].r, pGlobal->HP8753.S2P.S12[i].i,
							pGlobal->HP8753.S2P.S22[i].r, pGlobal->HP8753.S2P.S22[i].i );
				}
				fclose( fS2P );
				postInfo( "S2P saved" );
			}
			g_free( message->data );
			break;
		case TM_COMPLETE_GPIB:
			sensitiseControlsInUse( pGlobal, TRUE );
			break;
		case TM_REFRESH_TRACE:
			if (message->data == 0) {
				gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
										(gconstpointer )"WID_DrawingArea_Plot_A")));
				if (!globalData.HP8753.flags.bDualChannel || !globalData.HP8753.flags.bSplitChannels
						|| !globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData ) {
					hide_Frame_Plot_B( &globalData );
				}
			} else {
				if ( globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData ) {
					gtk_widget_show( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer) "WID_Frame_Plot_B"));
					gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
											(gconstpointer )"WID_DrawingArea_Plot_B")));
				} else {
					hide_Frame_Plot_B( &globalData );
				}
			}
			gtk_label_set_label ( GTK_LABEL( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_LblTraceTime")),
					globalData.HP8753.dateTime );

			if( globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
					|| globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData)
				gtk_widget_set_sensitive(
					GTK_WIDGET( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Save")),
					TRUE );

			break;
		default:
			break;
		}

		g_free(message->sMessage);
		g_free(message);
	}

	return G_SOURCE_CONTINUE;
}

/*!     \brief  Prepare source event
 *
 * check for messages sent from duplication threads
 * to update the GUI.
 * We can only (safely) update the widgets from the main tread loop
 *
 * \param source : GSource for the message event
 * \param pTimeout : pointer to return timeout value
 * \return TRUE if we have a message to dispatch
 */
gboolean messageEventPrepare(GSource *source, gint *pTimeout) {
	*pTimeout = -1;
	return g_async_queue_length(globalData.messageQueueToMain) > 0;
}

/*!     \brief  Check source event
 *
 * check for messages sent from duplication threads
 * to update the GUI.
 * We can only (safely) update the widgets from the main tread loop
 *
 * \param source : GSource for the message event
 * \return TRUE if we have a message to dispatch
 */
gboolean messageEventCheck(GSource *source) {
	return g_async_queue_length(globalData.messageQueueToMain) > 0;
}

/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void
postMessageToMainLoop(enum _threadmessage Command, gchar *sMessage) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)

	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->sMessage = g_strdup(sMessage); // g_free() in threadEventsDispatch
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToMain, messageData);
	g_main_context_wakeup( NULL);
}

/*!     \brief  Send message with number from thread to the main loop
 *
 * Send message with number to the main loop
 *
 * \param Command	enumerated state to indicate action
 * \param sMessageWithFormat message with printf formatting
 * \param number
 */
void
postInfoWithCount(gchar *sMessageWithFormat, gint number, gint number2) {
	gchar *sLabel = g_strdup_printf( sMessageWithFormat, number, number2 );
	postMessageToMainLoop( TM_INFO, sLabel );
	g_free (sLabel);

}


/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void postDataToMainLoop(enum _threadmessage Command, void *data) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)

	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->data = data;
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToMain, messageData);
	g_main_context_wakeup( NULL);
}

/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void postDataToGPIBThread(enum _threadmessage Command, void *data) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)

	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->data = data;
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToGPIB, messageData);
}
