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

/*!     \brief  Callback / Options page / enable Bezier spline interpolation GtkButton
 *
 * Callback (NOPT 1) when the "spline" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  udata        unused
 */
static void
CB_cbtn_BezierSpline (GtkCheckButton *wBtnSpline, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnSpline ), "data");

    pGlobal->flags.bSmithSpline = gtk_check_button_get_active( GTK_CHECK_BUTTON( wBtnSpline ) );
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ));
}


/*!     \brief  Callback / Options page / "Show Date/Time" GtkCheckButton
 *
 * Callback (NOPT 2) when the "Show Date/Time" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton    pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_cbtn_ShowDateTime (GtkCheckButton *wCkButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCkButton ), "data");

	pGlobal->flags.bShowDateTime = gtk_check_button_get_active( GTK_CHECK_BUTTON( wCkButton ) );
	gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
}

/*!     \brief  Callback / Options page / "Admittance/Susceptance" GtkCheckButton
 *
 * Callback (NOPT 3) when the "Admittance/Susceptance" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton    pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_cbtn_SmithGBnotRX (GtkCheckButton *wCheckBtn, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCheckBtn ), "data");

    pGlobal->flags.bAdmitanceSmith = gtk_check_button_get_active( GTK_CHECK_BUTTON( wCheckBtn ) );
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ));
}

/*!     \brief  Callback / Options page / "Delta Marker Actual" GtkCheckButton
 *
 * Callback (NOPT 4) when the "Delta Marker Actual" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton    pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_cbtn_DeltaMarkerActual (GtkCheckButton *wCheckBtn, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCheckBtn ), "data");

	pGlobal->flags.bDeltaMarkerZero = !gtk_check_button_get_active( GTK_CHECK_BUTTON( wCheckBtn ) );
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ));
}

/*!     \brief  Callback - Option do not retrieve HPGL screen plot
 *
 * Callback (NOPT 5) when the "Do not retrieve HPGL screen plot" GtkChkButton is changed
 *
 * \param  wCkButton    pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_cbtn_DoNotRetrieveHPGL(GtkCheckButton *wCheckBtn, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCheckBtn ), "data");
	pGlobal->flags.bDoNotRetrieveHPGLdata = gtk_check_button_get_active( GTK_CHECK_BUTTON( wCheckBtn ) );
}


/*!     \brief  Show HP logo on Channel 1 plot
 *
 * Callback (NOPT 6) when the "Show HP logo" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  tGlobal      pointer global data
 */
void
CB_cbtn_ShowHPlogo (GtkCheckButton *wBtnHPlogo, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnHPlogo ), "data");

    pGlobal->flags.bHPlogo = gtk_check_button_get_active( GTK_CHECK_BUTTON( wBtnHPlogo ) );
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
    gtk_widget_queue_draw(GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ));
}

/*!     \brief  Callback / Options page / "Analyze Learn String" GtkButton
 *
 * Callback (NOPT 7) when the "Analyze Learn String" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_btn_AnalyzeLS( GtkButton *wBtnAnalyzeLS, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnAnalyzeLS ), "data");

    sensitiseControlsInUse( pGlobal, FALSE );
    postDataToGPIBThread (TG_ANALYZE_LEARN_STRING, NULL);
}



/*!     \brief  Callback / Options page / "Analyze Learn String" GtkButton
 *
 * Callback (NOPT 8) when the "Analyze Learn String" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  udata        unused
 */
void
CB_cbtn_PDFpageSize( GtkCheckButton *wBtnPageSize, gpointer gpSize)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnPageSize ), "data");

    // only take notice of the checked radio button
    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wBtnPageSize ) ) == TRUE )
        pGlobal->PDFpaperSize = (tPaperSize)GPOINTER_TO_INT(gpSize);
}

/*!     \brief  Initialize the widgets and callbacks on the 'Options' page of the notebook
 *
 * Initialize the widgets and callbacks on the 'Options' page of the notebook
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeNotebookPageOptions( tGlobal *pGlobal, tInitFn purpose )
{
    widgetIDs ewPDFsize, ewGPIBinterface;

    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbGPIB_cbtn_UseGPIB_PID ]), pGlobal->flags.bGPIB_UseCardNoAndPID);
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_SmithBezier] ), pGlobal->flags.bSmithSpline );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_ShowDateTime] ), pGlobal->flags.bShowDateTime );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_SmithGBnotRX] ), pGlobal->flags.bAdmitanceSmith );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_DeltaMarkerAbsolute] ), !pGlobal->flags.bDeltaMarkerZero );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_DoNotRetrieveHPGL] ), pGlobal->flags.bDoNotRetrieveHPGLdata );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbOpts_cbtn_ShowHPlogo] ), pGlobal->flags.bHPlogo );

        // Set widget states based upon recovered settings
        gchar *sFWlabel = g_strdup_printf( "Firmware %d.%d", pGlobal->HP8753.analyzedLSindexes.version/100,
                pGlobal->HP8753.analyzedLSindexes.version % 100 );
        gtk_label_set_label( GTK_LABEL( pGlobal->widgets[ eW_nbOpts_lbl_Firmware ] ),
                pGlobal->HP8753.analyzedLSindexes.version != 0 ? sFWlabel : "Firmware unknown");
        g_free( sFWlabel );

        switch( pGlobal->PDFpaperSize ) {
        case 0:
            ewPDFsize = eW_nbOpts_rbtn_PDF_A4;
            break;
        case 1:
        default:
            ewPDFsize = eW_nbOpts_rbtn_PDF_LTR;
            break;
        case 2:
            ewPDFsize = eW_nbOpts_rbtn_PDF_A3;
            break;
        case 3:
            ewPDFsize = eW_nbOpts_rbtn_PDF_TBL;
            break;
        }
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ ewPDFsize ] ), TRUE );

        switch( pGlobal->flags.bbGPIBinterfaceType ) {
        case eGPIB:
        default:
            ewGPIBinterface = eW_nbGPIB_rbtn_interfaceGPIB;
            break;
        case eUSBTMC:
            ewGPIBinterface = eW_nbGPIB_rbtn_interfaceUSBTMC;
            break;
        case ePrologix:
            ewGPIBinterface = eW_nbGPIB_rbtn_interfacePrologix;
            break;
        }
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ ewGPIBinterface] ), TRUE );
    }

    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        // NOPT 1 - Signal for callback of check button to to use Use Bezier splines on polar and Smith plots
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_SmithBezier ], "toggled", G_CALLBACK (CB_cbtn_BezierSpline), NULL );
        // NOPT 2 - Signal for callback of check button to show the date and time on plots
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_ShowDateTime ], "toggled", G_CALLBACK (CB_cbtn_ShowDateTime), NULL );
        // NOPT 3 - Signal for callback of check button to use resistance/impedance or conductance/susceptance
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_SmithGBnotRX ], "toggled", G_CALLBACK (CB_cbtn_SmithGBnotRX), NULL );
        // NOPT 4 - Signal for callback of check button to not retrieve HPGL plot
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_DoNotRetrieveHPGL ], "toggled", G_CALLBACK (CB_cbtn_DoNotRetrieveHPGL), NULL );
        // NOPT 5 - Signal for callback of check button to use absolute or zero measurements for delta marker
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_DeltaMarkerAbsolute ], "toggled", G_CALLBACK (CB_cbtn_DeltaMarkerActual), NULL );
        // NOPT 6 - Signal for callback of check button to show the HP logo in the plot
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_cbtn_ShowHPlogo ], "toggled", G_CALLBACK (CB_cbtn_ShowHPlogo), NULL );
        // NOPT 7 - Signal for callback of button to analyze the learn string
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_btn_AnalyzeLS ], "clicked", G_CALLBACK (CB_btn_AnalyzeLS), NULL );

        // NOPT 8 - Signal for callback of check buttons to set the PDF page size
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_rbtn_PDF_A4 ], "toggled", G_CALLBACK (CB_cbtn_PDFpageSize), GINT_TO_POINTER( eA4 ) );
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_rbtn_PDF_LTR ], "toggled", G_CALLBACK (CB_cbtn_PDFpageSize), GINT_TO_POINTER( eLetter ) );
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_rbtn_PDF_A3 ], "toggled", G_CALLBACK (CB_cbtn_PDFpageSize), GINT_TO_POINTER( eA3 ) );
        g_signal_connect ( pGlobal->widgets[ eW_nbOpts_rbtn_PDF_TBL ], "toggled", G_CALLBACK (CB_cbtn_PDFpageSize), GINT_TO_POINTER( eTabloid ) );
    }
}

