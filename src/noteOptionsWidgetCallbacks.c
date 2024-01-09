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

#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>
#include <glib-2.0/glib.h>
#include "hp8753.h"
#include "calibrationKit.h"

#include "messageEvent.h"

/*!     \brief  Callback / Options page / "Show Date/Time" GtkCheckButton
 *
 * Callback when the "Show Date/Time" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkBtn_ShowDateTime (GtkCheckButton *wCkButton, tGlobal *pGlobal) {
		pGlobal->flags.bShowDateTime = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCkButton ) );
		gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
				(gconstpointer)"WID_DrawingArea_Plot_A")));
}

/*!     \brief  Callback / Options page / "Admittance/Susceptance" GtkCheckButton
 *
 * Callback when the "Admittance/Susceptance" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkBtn_SmithGBnotRX (GtkCheckButton *wCkButton, tGlobal *pGlobal) {
	pGlobal->flags.bAdmitanceSmith = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCkButton ) );
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_A")));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_B")));
}

/*!     \brief  Callback / Options page / "Delta Marker Actual" GtkCheckButton
 *
 * Callback when the "Delta Marker Actual" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wCkButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkBtn_DeltaMarkerActual (GtkCheckButton *wCkButton, tGlobal *pGlobal) {
	pGlobal->flags.bDeltaMarkerZero = !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCkButton ) );
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_A")));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_B")));
}

/*!     \brief  Callback - Option do not retrieve HPGL screen plot
 *
 * Callback when the "Do not retrieve HPGL screen plot" GtkChkButton is changed
 *
 * \param  wCkButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkBtn_DoNotRetrieveHPGL(GtkCheckButton *wCheckBtn, tGlobal *pGlobal) {
	pGlobal->flags.bDoNotRetrieveHPGLdata = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCheckBtn ) );
}

/*!     \brief  Callback / Options page / "Analyze Learn String" GtkButton
 *
 * Callback when the "Analyze Learn String" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Btn_AnalyzeLS( GtkButton *wBtnAnalyzeLS, tGlobal *pGlobal ) {
	sensitiseControlsInUse( pGlobal, FALSE );
	postDataToGPIBThread (TG_ANALYZE_LEARN_STRING, NULL);
}

/*!     \brief  Callback / Options page / enable spline interpolation GtkButton
 *
 * Callback when the "spline" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkBtn_Spline (GtkCheckButton *wButton, tGlobal *pGlobal)
{
	pGlobal->flags.bSmithSpline = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wButton ) );
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_A")));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_B")));
}

/*!     \brief  Show HP logo on Channel 1 plot
 *
 * Callback when the "Show HP logo" GtkButton on the "Options" notebook page is pressed
 *
 * \param  wButton      pointer to analyze learn string button widget
 * \param  tGlobal      pointer global data
 */
void
CB_ChkBtn_ShowHPlogo (GtkCheckButton *wButton, tGlobal *pGlobal)
{
    pGlobal->flags.bHPlogo = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wButton ) );
    gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
            (gconstpointer)"WID_DrawingArea_Plot_A")));
    gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
            (gconstpointer)"WID_DrawingArea_Plot_B")));
}
