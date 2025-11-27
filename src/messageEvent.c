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

#include "hp8753.h"
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
    GtkLabel *wLblStatus = GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ]);
    GtkWidget *wBoxPlotType;
    gchar *sMarkup;
    FILE *fSXP;

	while ((message = g_async_queue_try_pop(pGlobal->messageQueueToMain))) {
		switch (message->command) {
		case TM_INFO:
		case TM_INFO_HIGHLIGHT:
		    if( clearTimerID != 0 )
		        g_source_remove( clearTimerID );
		    clearTimerID = g_timeout_add ( 10000, clearNotification, wLblStatus );

		    if( message->command == TM_INFO )
		        sMarkup = g_markup_printf_escaped("<i>%s</i>", message->sMessage);
		    else
		        sMarkup = g_markup_printf_escaped("<span color='darkgreen'><i>َ%s</i></span>", message->sMessage);
		    gtk_label_set_markup(wLblStatus, sMarkup);
		    g_free(sMarkup);
			break;

		case TM_ERROR:
            if( clearTimerID != 0 )
                    g_source_remove( clearTimerID );
            clearTimerID = g_timeout_add ( 15000, clearNotification, wLblStatus );

            sMarkup = g_markup_printf_escaped(
                            "<span color=\"darkred\">%s</span>", message->sMessage);
            gtk_label_set_markup( wLblStatus, sMarkup );
            g_free(sMarkup);


			break;

		case TM_SAVE_SETUPandCAL:
		    if( saveCalibrationAndSetup( pGlobal, pGlobal->sProject, (gchar *)message->data ) != ERROR ) {
		        populateCalComboBoxWidget( pGlobal );
                // If this is a new project, also update the project combobox list
		        if( !g_list_find_custom (pGlobal->pProjectList, pGlobal->sProject, (GCompareFunc) strcmp ) ) {
		            pGlobal->pProjectList = g_list_prepend( pGlobal->pProjectList, g_strdup( pGlobal->sProject ) );
		            pGlobal->pProjectList = g_list_sort (pGlobal->pProjectList, (GCompareFunc)g_strcmp0);
		            populateProjectComboBoxWidget( pGlobal );
		        }
		        showCalInfo( &(pGlobal->HP8753cal), pGlobal );
		        gtk_widget_set_sensitive(GTK_WIDGET( pGlobal->widgets[ eW_btn_Recall]), TRUE );
		        gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_btn_Delete]), TRUE );
	            gtk_widget_remove_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] ), "italicFont" );
            }
		    gtk_notebook_set_current_page( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] ), NPAGE_CALIBRATION);
            g_free( message->data );
            break;

		case TM_SAVE_LEARN_STRING_ANALYSIS:
            saveLearnStringAnalysis( pGlobal, (tLearnStringIndexes *)message->data );
            gchar *sFWlabel = g_strdup_printf( "Firmware %d.%d", pGlobal->HP8753.analyzedLSindexes.version/100,
                            pGlobal->HP8753.analyzedLSindexes.version % 100 );
            gtk_label_set_label( GTK_LABEL(pGlobal->widgets[ eW_nbOpts_lbl_Firmware ]),
                            pGlobal->HP8753.analyzedLSindexes.version != 0 ? sFWlabel : "Firmware unknown");
            g_free( sFWlabel );

			break;

		case TM_SAVE_S2P:
		    sensitiseControlsInUse( pGlobal, TRUE );
		    if( (fSXP = fopen( message->data, "w" )) == NULL ) {
		        gchar *sError = g_strdup_printf( "Cannot write: %s", (gchar *)message->data);
		        postError( sError );
		        g_free( sError );
		    } else {
		        fprintf( fSXP, "! 2-port S-paramater data, multiple frequency points\n"
		                "! from HP8753 Network analyzer\n"
		                "# MHz S RI R 50.0\n"
		                "! freq\tReS11\tImS11\tReS21\tImS21\tReS12\tImS12\tReS22\tImS22\n" );
		        for( gint i=0; i < pGlobal->HP8753.S2P.nPoints; i++ ) {
		            fprintf( fSXP, "%.16lg\t%.16lg\t%.16lg\t%.16lg\t%.16lg\t%.16lg\t%.16lg\t%.16lg\t%.16lg\n",
		                    pGlobal->HP8753.S2P.freq[i]/1.0e6,
		                    pGlobal->HP8753.S2P.S11[i].r, pGlobal->HP8753.S2P.S11[i].i,
		                    pGlobal->HP8753.S2P.S21[i].r, pGlobal->HP8753.S2P.S21[i].i,
		                    pGlobal->HP8753.S2P.S12[i].r, pGlobal->HP8753.S2P.S12[i].i,
		                    pGlobal->HP8753.S2P.S22[i].r, pGlobal->HP8753.S2P.S22[i].i );
		        }
		        fclose( fSXP );
		        postInfo( "S2P saved" );
		    }
		    g_free( message->data );

			break;
		case TM_SAVE_S1P:
		    sensitiseControlsInUse( pGlobal, TRUE );
		    if( (fSXP = fopen( message->data, "w" )) == NULL ) {
		        gchar *sError = g_strdup_printf( "Cannot write: %s", (gchar *)message->data);
		        postError( sError );
		        g_free( sError );
		    } else {
		        fprintf( fSXP, "! 1-port S-paramater data, multiple frequency points\n"
		                "! from HP8753 Network analyzer\n"
		                "# MHz S RI R 50.0\n" );
		        if( pGlobal->HP8753.S2P.SnPtype == S1P_S11 ) {
		            fprintf( fSXP, "! freq\tReS11\tImS11\n"         );
		            for( gint i=0; i < pGlobal->HP8753.S2P.nPoints; i++ ) {
		                fprintf( fSXP, "%.16lg\t%.16lg\t%.16lg\n",
		                        pGlobal->HP8753.S2P.freq[i]/1.0e6,
		                        pGlobal->HP8753.S2P.S11[i].r, pGlobal->HP8753.S2P.S11[i].i );
		            }
		        } else {
		            fprintf( fSXP, "! freq\tReS22\tImS22\n" );
		            for( int i=0; i < pGlobal->HP8753.S2P.nPoints; i++ ) {
		                fprintf( fSXP, "%.16g\t%.16lg\t%.16lg\n",
		                        pGlobal->HP8753.S2P.freq[i]/1.0e6,
		                        pGlobal->HP8753.S2P.S22[i].r, pGlobal->HP8753.S2P.S22[i].i );
		                }
		        }
		        fclose( fSXP );
		        postInfo( "S2P saved" );
		    }
            g_free( message->data );

			break;

		case TM_COMPLETE_GPIB:
            sensitiseControlsInUse( pGlobal, TRUE );
			break;
		case TM_REFRESH_TRACE:
            wBoxPlotType = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_box_PlotType ]);
            if( pGlobal->HP8753.plotHPGL == NULL )
                gtk_widget_set_visible (GTK_WIDGET( wBoxPlotType ), FALSE);
            else
                gtk_widget_set_visible (GTK_WIDGET( wBoxPlotType ), TRUE);

            if (message->data == 0 ) {
                gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ) );
                if ( !globalData.HP8753.flags.bDualChannel
                        || !pGlobal->HP8753.flags.bSplitChannels
//                      || !pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData
                        || ( pGlobal->HP8753.flags.bShowHPGLplot /* && pGlobal->HP8753.flags.bHPGLdataValid */ ) ) {
                    visibilityFramePlot_B( pGlobal, FALSE );
                }
            } else {
                if ( // pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData &&
                        (globalData.HP8753.flags.bDualChannel && pGlobal->HP8753.flags.bSplitChannels)
                        && ! ( pGlobal->HP8753.flags.bShowHPGLplot /* && pGlobal->HP8753.flags.bHPGLdataValid */ ) ) {
                    visibilityFramePlot_B( pGlobal, TRUE);
                    gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ) );
                } else {
                    visibilityFramePlot_B( pGlobal, FALSE );
                }
            }
            gtk_label_set_label ( GTK_LABEL( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ),
                                        globalData.HP8753.dateTime );

            if( globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
                    || globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData) {
                gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_btn_Save] ), TRUE );
                gtk_widget_set_sensitive( pGlobal->widgets[ eW_nbData_btn_CSV ], TRUE );
            }

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
	messageData = g_new0( messageEventData, 1 );

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
	messageData = g_new0( messageEventData, 1 );

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
	messageData = g_new0( messageEventData, 1 );

	messageData->data = data;
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToGPIB, messageData);
}
