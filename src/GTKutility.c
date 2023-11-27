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

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib-2.0/glib.h>
#include <hp8753.h>
#include <math.h>

/*!     \brief  Show the calibration info on the GTK page widget
 *
 * Show the calibration info on the GTK page widget
 *
 *
 * \param tHP8753cal        pointer to the cairo structure
 * \param pGlobal   pointer to the gloabel data
 */
void
showCalInfo( tHP8753cal *pChannelCal, tGlobal *pGlobal ) {
	GtkTextBuffer *wTBcalInfo;
	GtkTextIter   iter;
	gchar sBuffer[ BUFFER_SIZE_500 ];
	gchar *sStart = NULL;
	gchar *sStop = NULL;
	gchar *sIFBW = NULL;
	gchar *sCWfreq = NULL;
	gchar *sUnitPrefix;
	gchar *sString;
	eChannel channel;

	for( channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		if( channel == eCH_ONE )
			wTBcalInfo = gtk_text_view_get_buffer( GTK_TEXT_VIEW(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh1")));
		else
			wTBcalInfo = gtk_text_view_get_buffer( GTK_TEXT_VIEW(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh2")));

		tSweepType sweepType = pChannelCal->perChannelCal[ channel ].sweepType;
		switch ( sweepType ) {
		case eSWP_CWTIME:
			sString = engNotation( pChannelCal->perChannelCal[ channel ].sweepStart, 2, eENG_SEPARATE, &sUnitPrefix );
			sStart = g_strdup_printf("%s %ss", sString, sUnitPrefix);
			g_free ( sString );
			sString = engNotation( pChannelCal->perChannelCal[ channel ].sweepStop, 2, eENG_SEPARATE, &sUnitPrefix );
			sStop = g_strdup_printf("%s %ss", sString, sUnitPrefix);
			g_free ( sString );
			sString = engNotation( pChannelCal->perChannelCal[ channel ].CWfrequency, 2, eENG_SEPARATE, &sUnitPrefix );
			sCWfreq = g_strdup_printf("%s %sHz", sString, sUnitPrefix);
			g_free ( sString );
			break;
		case eSWP_PWR:
			sStart = g_strdup_printf( "%.3f dbm", pChannelCal->perChannelCal[ channel ].sweepStart);
			sStop  = g_strdup_printf( "%.3f dbm", pChannelCal->perChannelCal[ channel ].sweepStop);
			sString = engNotation( pChannelCal->perChannelCal[ channel ].CWfrequency, 2, eENG_SEPARATE, &sUnitPrefix );
			sCWfreq = g_strdup_printf("%s %sHz", sString, sUnitPrefix);
			g_free ( sString );
			break;
		case eSWP_LOGFREQ:
		case eSWP_LINFREQ:
		case eSWP_LSTFREQ:
		default:
			sStart = g_strdup_printf( "%.6g MHz", pChannelCal->perChannelCal[ channel ].sweepStart/1.0e6 );
			sStop  = g_strdup_printf( "%.6g MHz", pChannelCal->perChannelCal[ channel ].sweepStop/1.0e6 );
			break;
		}

		sString = engNotation( pChannelCal->perChannelCal[ channel ].IFbandwidth, 2, eENG_SEPARATE, &sUnitPrefix );
		sIFBW = g_strdup_printf("%s %sHz", sString, sUnitPrefix);
		g_free ( sString );

		gtk_text_buffer_set_text( wTBcalInfo, "", 0);
		if( pChannelCal->perChannelCal[ channel ].settings.bValid ) {

			g_snprintf( sBuffer, BUFFER_SIZE_500, "<span color='darkblue'>%s</span>\n",
					optCalType[ pChannelCal->perChannelCal[ channel ].iCalType ].desc);
			gtk_text_buffer_get_end_iter( wTBcalInfo, &iter );
			gtk_text_buffer_insert_markup( wTBcalInfo, &iter, sBuffer, -1 );

			if( sweepType == eSWP_CWTIME || sweepType == eSWP_PWR ) {
				g_snprintf( sBuffer, BUFFER_SIZE_500,
						"<b>Start:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>Stop:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>IF BW:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>CW:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>Points:</b>\t<span color='darkgreen'>%d</span>"
							"<span color='darkblue'>  %s</span>",
						sStart, sStop, sIFBW, sCWfreq, pChannelCal->perChannelCal[ channel ].nPoints,
						pChannelCal->perChannelCal[ channel ].settings.bAveraging ? "(avg.)" : "" );
			} else {
				g_snprintf( sBuffer, BUFFER_SIZE_500,
						"<b>Start:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>Stop:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>IF BW:</b>\t<span color='darkgreen'>%s</span>\n"
						"<b>Points:</b>\t<span color='darkgreen'>%d</span>"
							"<span color='darkblue'>  %s</span>",
						sStart, sStop, sIFBW, pChannelCal->perChannelCal[ channel ].nPoints,
						pChannelCal->perChannelCal[ channel ].settings.bAveraging ? "(avg.)" : "" );
			}
			gtk_text_buffer_get_end_iter( wTBcalInfo, &iter );
			gtk_text_buffer_insert_markup( wTBcalInfo, &iter, sBuffer, -1 );
		}
		g_free( sStart );
		g_free( sStop );
		g_free( sCWfreq );
		g_free( sIFBW );
		sStart = NULL; sStop = NULL; sCWfreq = NULL, sIFBW = NULL;
	}

}

/*!     \brief  Add a string to the calibration pull down selector
 *
 * Add a string to the calibration pull down selector
 *
 * \ingroup drawing
 *
 * \param cal          pointer to the calibration data structure
 * \param wCalComboBox pointer calkibration combobox
 */
void
updateCalComboBox( gpointer cal, gpointer wCalComboBox ) {
	tHP8753cal *pCal = (tHP8753cal *)cal;
	GtkComboBoxText *wComboSetup = (GTK_COMBO_BOX_TEXT( wCalComboBox ));
	tGlobal *pGlobal = &globalData;

	if( g_strcmp0( pGlobal->sProject, pCal->projectAndName.sProject ) == 0  )
		gtk_combo_box_text_append_text( wComboSetup, pCal->projectAndName.sName );
}

/*!     \brief  Add a string to the combobox only if not there
 *
 * Add a string to the combobox only if not there otherwise highlight
 *
 * \ingroup drawing
 *
 * \param wComboBox  pointer to combobox widget
 * \param sName      name to search combobox list for
 */
gboolean
addTcomboBoxOrSelect( GtkComboBox *wComboBox, gchar *sName )
{
	gint n, pos;
	gboolean bFound = FALSE;
	GtkTreeModel *tm;
	GtkTreeIter iter;

// look through all the combobox labels to see if the selected text matches.
// If no, add to combobox otherwise set the active to it
	tm = gtk_combo_box_get_model(wComboBox);

	n = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(wComboBox),
			NULL);
	if (n > 0) {
		gtk_tree_model_get_iter_first(tm, &iter);
		for (pos = 0; !bFound && pos < n;
				pos++, gtk_tree_model_iter_next(tm, &iter)) {
			gchar *string;
			gtk_tree_model_get(tm, &iter, 0, &string, -1);
			if (g_strcmp0(sName, string) == 0)
				bFound = TRUE;
			g_free(string);
		}
	}

	if (!bFound) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wComboBox), sName);
		gtk_combo_box_set_active(wComboBox,
				gtk_tree_model_iter_n_children(
						gtk_combo_box_get_model(wComboBox), NULL) - 1);
	} else {
		gtk_combo_box_set_active(wComboBox, pos - 1);
	}

	return (bFound);
}
