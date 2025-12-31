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

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>
#include <glib-2.0/glib.h>

#include "hp8753.h"
#include "calibrationKit.h"
#include "messageEvent.h"

/*!     \brief  Show the calibration info on the GTK page widget
 *
 * Show the calibration info on the GTK page widget
 *
 *
 * \param tHP8753cal    pointer to the calibration data
 * \param pGlobal       pointer to the global data
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
    gchar *sGreen = pGlobal->flags.bDarkTheme ? "lightgreen" : "darkgreen";
    gchar *sBlue = pGlobal->flags.bDarkTheme ? "lightblue" : "darkblue";
    gchar *sString;
    eChannel channel;
    gint i;

    for( i=0, channel = pChannelCal->settings.bActiveChannel; i < eNUM_CH;
                    i++, channel = (channel == eCH_ONE ? eCH_TWO : eCH_ONE ) ) {
        if( channel == eCH_ONE )
            wTBcalInfo = gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalInfoCh1 ] ));
        else
            wTBcalInfo = gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalInfoCh2 ] ));

        // Only show the acive channel if there is only one channel shown (and the sources are coupled)
        if(!pChannelCal->settings.bDualChannel &&
            pChannelCal->settings.bSourceCoupled && channel != pChannelCal->settings.bActiveChannel ) {
            gtk_text_buffer_set_text( wTBcalInfo, "", 0);
            continue;
        }

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

            g_snprintf( sBuffer, BUFFER_SIZE_500, "<span color='%s'>%s</span>  %s\n",
                    sBlue, optCalType[ pChannelCal->perChannelCal[ channel ].iCalType ].desc,
                    pChannelCal->settings.bSourceCoupled ? "" : "⛓️‍💥");
            gtk_text_buffer_get_end_iter( wTBcalInfo, &iter );
            gtk_text_buffer_insert_markup( wTBcalInfo, &iter, sBuffer, -1 );

            if( sweepType == eSWP_CWTIME || sweepType == eSWP_PWR ) {
                g_snprintf( sBuffer, BUFFER_SIZE_500,
                        "<b>Start:</b>\t<span color='%s'>%s</span>\n"
                        "<b>Stop:</b>\t<span color='%s'>%s</span>\n"
                        "<b>IF BW:</b>\t<span color='%s'>%s</span>\n"
                        "<b>CW:</b>\t<span color='%s'>%s</span>\n"
                        "<b>Points:</b>\t<span color='%s'>%d</span>"
                            "<span color='%s'>  %s</span>",
                        sGreen, sStart, sGreen, sStop, sGreen, sIFBW, sGreen, sCWfreq, sGreen, pChannelCal->perChannelCal[ channel ].nPoints,
                        sBlue, pChannelCal->perChannelCal[ channel ].settings.bAveraging ? "(avg.)" : "" );
            } else {
                g_snprintf( sBuffer, BUFFER_SIZE_500,
                        "<b>Start:</b>\t<span color='%s'>%s</span>\n"
                        "<b>Stop:</b>\t<span color='%s'>%s</span>\n"
                        "<b>IF BW:</b>\t<span color='%s'>%s</span>\n"
                        "<b>Points:</b>\t<span color='%s'>%d</span>"
                            "<span color='%s'>  %s</span>",
                        sGreen, sStart, sGreen, sStop, sGreen, sIFBW, sGreen, pChannelCal->perChannelCal[ channel ].nPoints,
                        sBlue, pChannelCal->perChannelCal[ channel ].settings.bAveraging ? "(avg.)" : "" );
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
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


/*!     \brief  Initialize the widgets and callbacks on the 'Traces' page of the notebook
 *
 * Initialize the widgets and callbacks on the 'Traces' page of the notebook
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeNotebookPageCalibration( tGlobal *pGlobal, tInitFn purpose ) {

    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        // show the calibration information relevent to the selection
        if( pGlobal->pCalibrationAbstract != NULL ) {
            gtk_text_buffer_set_text( gtk_text_view_get_buffer(
                    GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] )),
                    pGlobal->pCalibrationAbstract->sNote, STRLENGTH);
            showCalInfo( pGlobal->pCalibrationAbstract, pGlobal );
        }
    }
}
#pragma GCC diagnostic pop
