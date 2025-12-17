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
#include <glib-2.0/glib.h>
#include <math.h>

#include "hp8753.h"

// Blast ... the combobox is deprecated without a suitable replacement
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


static gboolean bResize = FALSE, bFocus = FALSE;

/*!     \brief  Notification that focus has been given to the application
 *
 * focus-in-event signal from GtkApplicationWindow WID_hp8753c_main
 * occurs on mouse button release of GtkApplicationWindow
 *
 * \param controller        pointer to GtkEventControllerFocus
 * \param gpwApplication    pointer to application widget
 */
void CB_AppFocusIn(GtkEventControllerFocus *controller, gpointer gpwApplication)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(gpwApplication))), "data");
    if( bResize ) {
        GtkWidget *wFramePlotB = pGlobal->widgets[ eW_frame_Plot_B ];
        visibilityFramePlot_B (pGlobal, gtk_widget_get_visible(wFramePlotB));
    }
   bResize = FALSE;
   bFocus = TRUE;
}

/*!     \brief  Notification that focus has been taken from the application
 *
 *
 * focus-out-event signal from GtkApplicationWindow WID_hp8753_main
 * occurs on mouse button release of GtkApplicationWindow
 *
 * \param controller        pointer to GtkEventControllerFocus
 * \param gpwApplication    pointer to application widget
 */
void CB_AppFocusOut(GtkEventControllerFocus *controller, gpointer gpwApplication)
{
    bFocus = FALSE;
}

/*!     \brief  Process the mouse button clicks in the drawing areas
*
* Process the mouse button clicks in the drawing areas
*
* \param  GtkGesture        gesture widget for mouse press
* \param  n_presses         number of presses
* \param  x                 x position within the widget
* \param  y                 y position within the widget
* \param  gpwDrawingArea    pointer to the drawing widget
*/
void
CB_gesture_DrawingArea_MousePress (GtkGesture *gesture, int n_press, double x, double y, gpointer areaAnotB)
{
    GtkDrawingArea *wDrawingArea= GTK_DRAWING_AREA( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( gesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDrawingArea))), "data");

    gdouble fractionX = 0.0;

    guint areaWidth   = gtk_widget_get_allocated_width ( GTK_WIDGET( wDrawingArea ));
    // areawidth gives a number slightly bigger than that given in the drawing callback
#define EDGE_WIDTH  2
    fractionX = x / (gdouble)(areaWidth - EDGE_WIDTH);

    if( gtk_gesture_single_get_current_button( GTK_GESTURE_SINGLE( gesture ) ) == GDK_BUTTON_PRIMARY ) {
        pGlobal->mouseXpercentHeld = fractionX;
        pGlobal->flags.bHoldLiveMarker = TRUE;
    } else {
        pGlobal->flags.bHoldLiveMarker = FALSE;
    }

    gtk_widget_queue_draw(GTK_WIDGET(pGlobal->widgets[ eW_drawingArea_Plot_A ]));
    gtk_widget_queue_draw(GTK_WIDGET(pGlobal->widgets[ eW_drawingArea_Plot_B ]));

}

void
sensitiseControlsInUse( tGlobal *pGlobal, gboolean bSensitive ) {
    GtkWidget *wBBsaveRecall = GTK_WIDGET( pGlobal->widgets[ eW_box_SaveRecallDelete ] );
    GtkWidget *wBBgetTrace = GTK_WIDGET( pGlobal->widgets[ eW_box_GetTrace ] );
    GtkWidget *wBtnAnalyzeLS = GTK_WIDGET( pGlobal->widgets[ eW_nbOpts_btn_AnalyzeLS ] );
    GtkWidget *wBtnS2P = GTK_WIDGET( pGlobal->widgets[ eW_nbData_btn_S2P ] );
    GtkWidget *wBtnS1P = GTK_WIDGET( pGlobal->widgets[ eW_nbData_btn_S1P ] );
    GtkWidget *wBtnSendCalKit = GTK_WIDGET( pGlobal->widgets[ eW_nbCalKit_btn_SendKit ] );

    gtk_widget_set_sensitive ( wBBsaveRecall, bSensitive );
    gtk_widget_set_sensitive ( wBBgetTrace, bSensitive );
    gtk_widget_set_sensitive ( wBtnAnalyzeLS, bSensitive );
    gtk_widget_set_sensitive ( wBtnS1P, bSensitive );
    gtk_widget_set_sensitive ( wBtnS2P, bSensitive );
    gtk_widget_set_sensitive ( wBtnSendCalKit, g_list_length( pGlobal->pCalKitList ) > 0 ? bSensitive : FALSE );
}

/*!     \brief  set the combobox to the string (if found in list)
 *
 * Set the combobox to the string
 *
 * \param wCombo pointer to GtkComboBox widget
 * \param sMatch pointer string to find
 * \return  true if string found in dropdown list
 */
gboolean
setGtkComboBox( GtkComboBox *wCombo, gchar *sMatch ) {

    GtkTreeModel *tm;
    GtkTreeIter iter;
    gint n;
    gboolean bFound = FALSE;

    if( !sMatch )
        return FALSE;

    tm = gtk_combo_box_get_model(wCombo );
    n  = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(wCombo), NULL);
    if (n > 0) {
        gtk_tree_model_get_iter_first(tm, &iter);
        for (gint pos = 0; !bFound && pos < n; pos++, gtk_tree_model_iter_next(tm, &iter)) {
            gchar *string;
            gtk_tree_model_get(tm, &iter, 0, &string, -1);
            if (g_strcmp0( sMatch, string ) == 0) {
                gtk_combo_box_set_active_iter ( wCombo, &iter );
                bFound = TRUE;
            }
            g_free(string);
        }
    }
    return bFound;
}

#pragma GCC diagnostic pop
