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

static GtkCssProvider *cssItalic;

void CB_RadioBtn_ScreenPlot (GtkRadioButton *wRadioBtn, tGlobal *pGlobal);

/*!     \brief  set the combobox to the string (if found in list)
 *
 * Set the combobox to the string
 *
 * \param wCombo pointer to GtkComboBox widget
 * \param sMatch pointer string to find
 * \return	true if string found in dropdown list
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

gboolean
sensitizeRecallSaveDeleteButtons( tGlobal *pGlobal ) {

	gboolean bFound = FALSE;
	GtkButton 		*wBtnRecall = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" );
	GtkButton 		*wBtnSave   = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Save" );
	GtkButton 		*wBtnDelete = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" );
	gchar 			*sString;
	GtkComboBoxText *wComboText = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable,
	            (gconstpointer)(pGlobal->flags.bCalibrationOrTrace ? "WID_Combo_CalibrationProfile" : "WID_Combo_TraceProfile") ) );

	sString = gtk_combo_box_text_get_active_text(wComboText);
	bFound = setGtkComboBox( GTK_COMBO_BOX( wComboText ), sString );

 	gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );

	if( pGlobal->flags.bCalibrationOrTrace ) {
		gtk_widget_set_sensitive( GTK_WIDGET( wBtnSave ), strlen(sString) );
	} else {
		gtk_widget_set_sensitive(
			GTK_WIDGET( wBtnSave ), strlen(sString)
				&& (pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData
						|| pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData) );
	}
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );

	g_free( sString );
	return bFound;
}

void
sensitiseControlsInUse( tGlobal *pGlobal, gboolean bSensitive ) {
	GtkWidget *wBBsaveRecall = GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_Box_SaveRecallDelete")) ;
	GtkWidget *wBBgetTrace = GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_Box_GetTrace") );
	GtkWidget *wBtnAnalyzeLS = GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_Btn_AnalyzeLS") );
	GtkWidget *wBtnS2P = GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_S2P") );
	GtkWidget *wBtnSendCalKit = GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_Btn_SendCalKit") );

	gtk_widget_set_sensitive ( wBBsaveRecall, bSensitive );
	gtk_widget_set_sensitive ( wBBgetTrace, bSensitive );
	gtk_widget_set_sensitive ( wBtnAnalyzeLS, bSensitive );
	gtk_widget_set_sensitive ( wBtnS2P, bSensitive );
	gtk_widget_set_sensitive ( wBtnSendCalKit, g_list_length( pGlobal->pCalKitList ) > 0 ? bSensitive : FALSE );

}

/*!     \brief  Populate the setup & trace combo box widgets
 *
 * populate the setup & trace combobox widgets based on the selected project
 *
 * \param pGlobal pointer to global data
 * \return		  number of setups in this project
 */
int
populateCalComboBoxWidget( tGlobal *pGlobal ) {
	GtkComboBoxText *wComboBoxCalibration = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
	GtkNotebook *wNotebook = GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note") );
	void CB_ComboBoxCalibrationProfileName (GtkComboBoxText *, tGlobal *);
	void CB_Notebook_Select ( GtkNotebook *,   GtkWidget*, guint, tGlobal *);

	gint nPos, nItems;
	gboolean bFound;
	GList *l;

	// Block signals while we populate the widgets programatically
	g_signal_handlers_block_by_func(G_OBJECT(wComboBoxCalibration), CB_ComboBoxCalibrationProfileName, pGlobal);
	g_signal_handlers_block_by_func(G_OBJECT(wNotebook), CB_Notebook_Select, pGlobal);

	gtk_combo_box_text_remove_all( wComboBoxCalibration );
	for( l = pGlobal->pCalList, nPos=0, nItems=0, bFound=FALSE; l != NULL; l = l->next ){
		tProjectAndName *pProjectAndName = &(((tHP8753cal *)l->data)->projectAndName);
		if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
		    nItems++;
			gtk_combo_box_text_append_text( wComboBoxCalibration, pProjectAndName->sName );
			if( !bFound ) {
				if( pProjectAndName->bSelected )
					bFound = TRUE;
				else
					nPos++;
			}
		}
	}

	if( !bFound && nItems > 0 ) {

	}
	if( bFound ) {
		gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxCalibration), nPos);
	} else if ( nItems > 0 ) {
	    gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxCalibration), 0);
	    ((tHP8753cal *)(pGlobal->pCalList->data))->projectAndName.bSelected = TRUE;
	} else {
		gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(wComboBoxCalibration) )), "");
	}

	// Re-enable "changed" signals for drop comboboxes
	g_signal_handlers_unblock_by_func(G_OBJECT(wComboBoxCalibration), CB_ComboBoxCalibrationProfileName, pGlobal);
	g_signal_handlers_unblock_by_func(G_OBJECT(wNotebook), CB_Notebook_Select, pGlobal);

	return( gtk_tree_model_iter_n_children(gtk_combo_box_get_model( GTK_COMBO_BOX(wComboBoxCalibration)), NULL) );
}

/*!     \brief  Populate the setup & trace combo box widgets
 *
 * populate the setup & trace combobox widgets based on the selected project
 *
 * \param pGlobal pointer to global data
 * \return		  number of traces in this project
 */
int
populateTraceComboBoxWidget( tGlobal *pGlobal ) {
	GtkComboBoxText *wComboBoxTrace = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
	GtkNotebook *wNotebook = GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note") );
	void CB_ComboBoxTraceProfileName (GtkComboBoxText *, tGlobal *);
	void CB_Notebook_Select ( GtkNotebook *,   GtkWidget*, guint, tGlobal *);

	gint nPos, nItems;
	gboolean bFound;
	GList *l;

	// Block signals while we populate the widgets programatically
	g_signal_handlers_block_by_func(G_OBJECT(wComboBoxTrace), CB_ComboBoxTraceProfileName, pGlobal);
	g_signal_handlers_block_by_func(G_OBJECT(wNotebook), CB_Notebook_Select, pGlobal);

	// Fill in combobox with the available trace names recovered from the sqlite3 database
	gtk_combo_box_text_remove_all( wComboBoxTrace );
	for( l = pGlobal->pTraceList, nPos=0, nItems=0, bFound=FALSE; l != NULL; l = l->next ){
		tProjectAndName *pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);
		if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
		    nItems++;
			gtk_combo_box_text_append_text( wComboBoxTrace, pProjectAndName->sName );
			if( !bFound ) {
				if( pProjectAndName->bSelected )
					bFound = TRUE;
				else
					nPos++;
			}
		}
	}
    if( bFound ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxTrace), nPos);
    } else if ( nItems > 0 ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxTrace), 0);
        ((tHP8753traceAbstract *)(pGlobal->pTraceList->data))->projectAndName.bSelected = TRUE;
    } else {
        gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(wComboBoxTrace) )), "");
    }

	// Re-enable "changed" signals for drop comboboxes
	g_signal_handlers_unblock_by_func(G_OBJECT(wComboBoxTrace), CB_ComboBoxTraceProfileName, pGlobal);
	g_signal_handlers_unblock_by_func(G_OBJECT(wNotebook), CB_Notebook_Select, pGlobal);

	return( gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(wComboBoxTrace)), NULL) );
}

/*!     \brief  Populate the project combo box widget
 *
 * populate the project combobox widget
 *
 * \param pGlobal pointer to global data
 * \return		  number of projects
 */
int
populateProjectComboBoxWidget( tGlobal * pGlobal ) {
    GList *l;
    gint nProjects = 0, nPos=0;
	GtkComboBoxText *wComboBoxProject
		= GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_Project") );
	gtk_combo_box_text_remove_all( wComboBoxProject );
	for( l = pGlobal->pProjectList, nPos = 0, nProjects = 0; l != NULL; l = l->next, nProjects++ ){
		gtk_combo_box_text_append_text( wComboBoxProject, (gchar *)l->data );
		if( g_strcmp0( (gchar *)l->data, pGlobal->sProject) == 0 )
		    nPos = nProjects;
	}
	gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxProject), nPos );
	return( nProjects );
}

/*!     \brief  Set events for plot A widget
 *
 * Set events so that the plot A widget receives mouse movement and focus signals
 * realize signal from GtkDrawingArea plot A
 *
 * \param wDrawingAreaA GtkDrawingArea widget pointer
 * \param pGlobal pointer to global data
 */
void
CB_DrawingArea_Plot_A_Realize (GtkDrawingArea * wDrawingAreaA, tGlobal *pGlobal)
{
    gdk_window_set_events ( gtk_widget_get_window( GTK_WIDGET(wDrawingAreaA) ),
    		GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
//    g_signal_connect (GTK_WIDGET(wDrawingAreaA), "button-press-event", G_CALLBACK (CB_DrawingArea_Plot_A_MouseButton), (gpointer)&globalData);
}

/*!     \brief  Set events for plot B widget
 *
 * Set events so that the plot A widget receives mouse movement and focus signals
 * realize signal from GtkDrawingArea plot B
 *
 * \param wDrawingAreaB GtkDrawingArea widget pointer
 * \param pGlobal pointer to global data
 */
void
CB_DrawingArea_Plot_B_Realize (GtkDrawingArea * wDrawingAreaB, tGlobal *pGlobal)
{
    gdk_window_set_events ( gtk_widget_get_window( GTK_WIDGET(wDrawingAreaB) ),
    		GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
}

/*!     \brief  Show or hide plot b
 *
 * Show or hide plot b and shrink wrap around the drawing widgets
 *
 * \param wDrawingAreaA GtkDrawingArea widget pointer
 * \param pGlobal  pointer to global data
 * \param bVisible make or hide plot b
 */
#define MIN_WIDGET_SIZE 1
void
visibilityFramePlot_B (tGlobal *pGlobal, gint visible)
{
#define YES_NO_MASK 0x01
#define REDISPLAY   0x02
	GtkWidget *wApplication = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_hp8753c_main");
	GtkWidget *wFramePlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frame_Plot_B");
	GtkWidget *wFramePlotA = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frame_Plot_A");
	GtkWidget *wDrawingAreaPlotA = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A");

	GtkWidget *wControls = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Controls");

    gint widthA, heightA;
    gint frameWidth;
    gint widthApp, heightApp;
    GdkRectangle screenArea = {0};
    gboolean bWasVisible = gtk_widget_get_visible( wFramePlotB );
    gboolean bControlsVisible = gtk_widget_get_visible( wControls );

    static gint widthExtra = 0, widthControls = 0;
    static gint __attribute__((unused)) heightExtra = 0;
    static gboolean bFirst = TRUE;
    static gint frameThickness;
    static gint marginLeftFrameB, marginRightFrameB;

    // What's the size of Drawing Area A
    GtkAllocation alloc = {0};
    gtk_widget_get_allocation(wDrawingAreaPlotA, &alloc);
    widthA = alloc.width;
    heightA = alloc.height;
    gtk_widget_get_allocation(wFramePlotA, &alloc);
    frameWidth = alloc.width;


    // What's the size of the monitor
    gdk_monitor_get_workarea(
        gdk_display_get_primary_monitor( gdk_display_get_default()), &screenArea);
    // What's the size of the app window
    gtk_window_get_size( GTK_WINDOW(wApplication), &widthApp, &heightApp );

    // On first pass we note the additional hight and width (that remain fixed)
    // so that we can later determing the size of the whole application window
    if( bFirst ) {
        widthExtra = widthApp - widthA; // left control box & frame A
        heightExtra = heightApp - heightA;
        bFirst = FALSE;
        frameThickness = frameWidth - widthA;
        marginLeftFrameB = gtk_widget_get_margin_start( wFramePlotB );
        marginRightFrameB = gtk_widget_get_margin_end( wFramePlotB );
        gtk_widget_get_allocation(wControls, &alloc);
        widthControls = alloc.width;
    }

    // We shrink wrap the app around the drawing areas but we have to go through
    // a complicated calculation to avoid the app shrinking to it's minimum
    // Also handle the case when going from a single to dual plots that will be
    // larger than the screen
	if( !(visible & YES_NO_MASK)) {   // hiding plot B
        gtk_widget_hide( wFramePlotB );
	    gtk_window_resize( GTK_WINDOW(wApplication), widthA + widthExtra -
	            (bControlsVisible ?  0 : widthControls), heightA + heightExtra);
	} else {            // showing plot B
        // WidthA and WidthB are the same & because B is not visible
        //       it will does not give a width with gtk_widget_get_allocation
        // We must account for frame B width and the frame margins
	    gint newWidthApp = widthA + widthExtra // Width of control box and plotA
                + widthA + frameThickness + marginLeftFrameB + marginRightFrameB
                - (bControlsVisible ?  0 : widthControls);

	    gint newHeightApp = heightA + heightExtra;

	    gtk_widget_show( wFramePlotB );
	    if( !bWasVisible || (visible & REDISPLAY)) {
            if( newWidthApp <= screenArea.width ) {
                gtk_window_resize( GTK_WINDOW(wApplication), newWidthApp, newHeightApp );
            } else {
                gint newDrawingAreaWidth = ( screenArea.width - (widthExtra - (bControlsVisible ?  0 : widthControls))
                        - frameThickness + marginLeftFrameB + marginRightFrameB ) / 2;

                gint newHeightApp = (gint)(((gdouble)heightA/widthA) * newDrawingAreaWidth + 0.5 + heightExtra);
                gtk_window_resize( GTK_WINDOW(wApplication), screenArea.width, newHeightApp );
            }
	    }
	}
	// make sure the resize happens ASAP
	 while (gtk_events_pending ())
	   gtk_main_iteration ();
}


static gboolean bResize = FALSE, bFocus = FALSE;
/*!     \brief  Notification that the application window has resized
 *
 * Notification that the application window has resized or moved
 *
 * size-allocate signal from GthApplicationWindow WID_hp8753c_main
 *
 * \param wApp GtkApplicationWindow widget pointer
 * \param pAllocation Allocation structure
 * \param pGlobal pointer to global data
 */
void
CB_AppSizeAllocate(   GtkWidget* wApp, GtkAllocation* pAllocation,
        tGlobal *pGlobal) {
    static gint prevWidth = 0, prevHeight = 0;

    if( pAllocation->width != prevWidth || pAllocation->height != prevHeight) {
        if( !bFocus )
            bResize = TRUE;
    }
    prevWidth = pAllocation->width;
    prevHeight = pAllocation->height;
}

/*!     \brief  Notification that focus has been given to the application
 *
 * focus-in-event signal from GtkApplicationWindow WID_hp8753c_main
 * occurs on mouse button release of GtkApplicationWindow
 *
 * \param wApp GtkApplicationWindow widget pointer
 * \param event the event
 * \param pGlobal pointer to global data
 * \return  FALSE (propagate signal)
 */
gboolean
CB_AppFocusIn(GtkWidget* wApp, GdkEventFocus* event, tGlobal *pGlobal){
    if( bResize ) {
        GtkWidget *wFramePlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frame_Plot_B");
        visibilityFramePlot_B (pGlobal, gtk_widget_get_visible(wFramePlotB) | REDISPLAY);
    }
    bResize = FALSE;
    bFocus = TRUE;
    return FALSE;
}

/*!     \brief  Notification that focus has been taken from the application
 *
 *
 * focus-out-event signal from GtkApplicationWindow WID_hp8753c_main
 * occurs on mouse button release of GtkApplicationWindow
 *
 * \param wApp GtkApplicationWindow widget pointer
 * \param event the event
 * \param pGlobal pointer to global data
 * \return  FALSE (propagate signal)
 */
gboolean
CB_AppFocusOut(GtkWidget* wApp, GdkEventFocus* event, tGlobal *pGlobal){
    bFocus = FALSE;
    return FALSE;
}

/*!     \brief  Setup when main widget is created
 *
 * Setup when main widget is created (hide plot B initially and create a text style)
 * realize signal from GtkApplicationWindow
 *
 * \param wApplicationWindow GtkApplicationWindow widget pointer
 * \param pGlobal pointer to global data
 */
void
CB_hp8753c_main_Realize (GtkApplicationWindow * wApplicationWindow, tGlobal *pGlobal)
{
    cssItalic = gtk_css_provider_new ();
    gtk_css_provider_load_from_data( cssItalic, " entry { font-style: italic; } textview { font-style: italic; } ", -1, NULL);

	visibilityFramePlot_B( pGlobal, FALSE );
}

void
CB_BtnRecall (GtkButton * button, tGlobal *pGlobal)
{
	GtkComboBoxText *cbSetup;
	gchar *name;
	gint rtn;

	// Release live marker
    pGlobal->flags.bHoldLiveMarker = FALSE;

	if( pGlobal->flags.bCalibrationOrTrace )
		cbSetup = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
	else
		cbSetup = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
	name = gtk_combo_box_text_get_active_text( cbSetup );

	if ( name && strlen( name ) != 0 ) {
		if( pGlobal->flags.bCalibrationOrTrace ) {
			if( recoverCalibrationAndSetup( pGlobal, pGlobal->sProject, (gchar *)name ) != ERROR ) {
				GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
											(gconstpointer )"WID_TextView_CalibrationNote")));
				gtk_text_buffer_set_text( wTBnote,
						pGlobal->HP8753cal.sNote ? pGlobal->HP8753cal.sNote : "", STRLENGTH );
	            GtkWidget *wCalNote = g_hash_table_lookup(pGlobal->widgetHashTable,
	                    (gconstpointer )"WID_TextView_CalibrationNote");
	            gtk_style_context_remove_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wCalNote )),
	                    GTK_STYLE_PROVIDER( cssItalic ));
				postDataToGPIBThread (TG_SEND_SETUPandCAL_to_HP8753, NULL );
			}
			sensitiseControlsInUse( pGlobal, FALSE );
			gtk_widget_set_sensitive ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_CalInfo"), TRUE);
		} else {
			if( (rtn = recoverTraceData( pGlobal, pGlobal->sProject, (gchar *)name )) != ERROR ) {
				if( rtn == FALSE )
					clearHP8753traces( &pGlobal->HP8753 );
				GtkWidget *wTraceNote = g_hash_table_lookup(pGlobal->widgetHashTable,
                        (gconstpointer )"WID_TextView_TraceNote");
				GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote ));
				gtk_text_buffer_set_text( wTBnote, pGlobal->HP8753.sNote ? pGlobal->HP8753.sNote : "", STRLENGTH );
				GtkWidget *wEntryTitle = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Entry_Title");
				gtk_entry_set_text( GTK_ENTRY( wEntryTitle ), pGlobal->HP8753.sTitle ? pGlobal->HP8753.sTitle : "");
				if (!pGlobal->HP8753.flags.bDualChannel || !pGlobal->HP8753.flags.bSplitChannels) {
					postDataToMainLoop(TM_REFRESH_TRACE, 0);
				} else {
					postDataToMainLoop(TM_REFRESH_TRACE, 0);
					postDataToMainLoop(TM_REFRESH_TRACE, (void*) 1);
				}

				// Show whichever trace was showing when saved (High Resolution or HPGL)
				GtkWidget *wRadioHPGLplot = g_hash_table_lookup(pGlobal->widgetHashTable,
						(gconstpointer )"WID_RadioBtn_PlotTypeHPGL");
                GtkWidget *wRadioHIRESplot = g_hash_table_lookup(pGlobal->widgetHashTable,
                        (gconstpointer )"WID_RadioBtn_PlotTypeHighRes");
				GtkWidget *wBoxPlotType = g_hash_table_lookup(pGlobal->widgetHashTable,
				                        (gconstpointer )"WID_BoxPlotType");
				if( pGlobal->HP8753.plotHPGL && pGlobal->HP8753.flags.bShowHPGLplot )
				    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(wRadioHPGLplot), TRUE );
				else
                    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(wRadioHIRESplot), TRUE );

				if( pGlobal->HP8753.plotHPGL == NULL )
				    gtk_widget_hide (GTK_WIDGET( wBoxPlotType ));
				else
				    gtk_widget_show (GTK_WIDGET( wBoxPlotType ));

				// Restore the color of the title entry window
		        gtk_style_context_remove_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wEntryTitle )),
		                GTK_STYLE_PROVIDER( cssItalic ));
                gtk_style_context_remove_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wTraceNote )),
                        GTK_STYLE_PROVIDER( cssItalic ));
			}
		}
		gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable,
				(gconstpointer )"WID_Note")), pGlobal->flags.bCalibrationOrTrace ? NPAGE_CALIBRATION : NPAGE_TRACE );
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable,
						(gconstpointer)"WID_Lbl_Status")), "Please provide profile name.");
	}
	g_free( name );

}

void
CB_BtnSave (GtkButton *wButton, tGlobal *pGlobal)
{
	GtkComboBoxText *wComboBoxName;
	gchar *sName, *sNote;
	GtkWidget *wTraceNote = NULL, *wTitle = NULL;
	GtkTextBuffer* wTBnote;
	GtkTextIter start, end;

	if( pGlobal->flags.bCalibrationOrTrace ) {
		wComboBoxName = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
		wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
												(gconstpointer )"WID_TextView_CalibrationNote")));
	} else {
	    wTraceNote = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_TextView_TraceNote");
	    wTitle = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Entry_Title");
		wComboBoxName = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
		wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote ));
	}
	// This may be a new name or one selected from the combobox list
	sName = gtk_combo_box_text_get_active_text( wComboBoxName );
	// The text in the note field
	gtk_text_buffer_get_start_iter(wTBnote, &start);
	gtk_text_buffer_get_end_iter(wTBnote, &end);
	sNote = gtk_text_buffer_get_text(wTBnote, &start, &end, FALSE);

	if ( strlen( sName ) != 0 ) {
		GtkWidget *dialog;
		gboolean bFound;
		gboolean bAuthorized = TRUE;
		tProjectAndName projectAndName = { pGlobal->sProject, sName, FALSE };

		// Look to see if we are replacing an existing profile
		if( pGlobal->flags.bCalibrationOrTrace ) {
			bFound = (g_list_find_custom( pGlobal->pCalList, &projectAndName,
					(GCompareFunc)compareCalItemsForFind ) != NULL);
		} else {
			bFound = (g_list_find_custom( pGlobal->pTraceList, &projectAndName,
					(GCompareFunc)compareTraceItemsForFind ) != NULL);
		}

		// Warn before overwitting
		if( bFound ) {
			dialog = gtk_message_dialog_new( NULL,
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_YES_NO, " ");
			gtk_window_set_title(GTK_WINDOW(dialog), "Caution");
			gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG( dialog ),
					"<b>This profile already exists.</b>\n\nAre you sure you want to replace it?");

			if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES ) {
				bAuthorized = TRUE;
			} else {
				bAuthorized = FALSE;
			}
			gtk_widget_destroy(dialog);
		}

		if( bAuthorized ) {
			if( pGlobal->flags.bCalibrationOrTrace ) {
				g_free( pGlobal->HP8753cal.sNote );
				pGlobal->HP8753cal.sNote = sNote;
				// send message to GPIB tread to get calibration data.
				// If this completes correctly, the main thrtead receives a message that will save the data
				postDataToGPIBThread (TG_RETRIEVE_SETUPandCAL_from_HP8753, g_strdup( sName ));
				sensitiseControlsInUse( pGlobal, FALSE );
				// The abstract list is updated if the data is retrieved from the analyzer correctly
			} else {
				g_free( pGlobal->HP8753.sNote );
				pGlobal->HP8753.sNote = sNote;
				int saveStatus = saveTraceData(pGlobal, pGlobal->sProject, sName);
				// add to the list
				GList *liTraceAbstract = g_list_find_custom( pGlobal->pTraceList, &projectAndName,
						(GCompareFunc)compareTraceItemsForFind );
				if( liTraceAbstract ) {
				    // This is an existing profile ... just update the abstract
					tHP8753traceAbstract *pTraceAbstract = (tHP8753traceAbstract *)liTraceAbstract->data;
					g_free( pTraceAbstract->sTitle );
					pTraceAbstract->sTitle = g_strdup( pGlobal->HP8753.sTitle );
					g_free( pTraceAbstract->sNote );
					pTraceAbstract->sNote = g_strdup( pGlobal->HP8753.sNote );
					g_free( pTraceAbstract->sDateTime );
					pTraceAbstract->sDateTime = g_strdup( pGlobal->HP8753.dateTime );
				} else {
				    // This is a new profile ... create the abstract
					tHP8753traceAbstract *pTraceAbstract = g_malloc0( sizeof( tHP8753traceAbstract ));
					pTraceAbstract->projectAndName.sProject = g_strdup( pGlobal->sProject );
					pTraceAbstract->projectAndName.sName = g_strdup( sName );
					pTraceAbstract->sTitle = g_strdup( pGlobal->HP8753.sTitle );
					pTraceAbstract->sNote = g_strdup( pGlobal->HP8753.sNote );
					pTraceAbstract->sDateTime = g_strdup( pGlobal->HP8753.dateTime );
					pGlobal->pTraceList = g_list_prepend(pGlobal->pTraceList, pTraceAbstract);
					pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);
					gtk_combo_box_text_remove_all ( wComboBoxName  );
					for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
						if( g_strcmp0( ((tHP8753traceAbstract *)l->data)->projectAndName.sProject, pGlobal->sProject ) == 0 )
							gtk_combo_box_text_append_text( wComboBoxName, ((tHP8753traceAbstract *)l->data)->projectAndName.sName );
					}
					if( !g_list_find( pGlobal->pProjectList, pGlobal->sProject ) ) {
					    // This is also a new project
						pGlobal->pProjectList = g_list_prepend( pGlobal->pProjectList, g_strdup( pGlobal->sProject ) );
						pGlobal->pProjectList = g_list_sort (pGlobal->pProjectList, (GCompareFunc)g_strcmp0);
						populateProjectComboBoxWidget( pGlobal );
					}
				}
				pGlobal->pTraceAbstract = g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareTraceItemsForFind )->data;

				gtk_widget_set_sensitive( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" ), TRUE);
				gtk_widget_set_sensitive( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" ), TRUE);

				if( saveStatus == 0 ) { // successful save
                    // Restore the color of the title entry window
                    gtk_style_context_remove_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wTitle )),
                            GTK_STYLE_PROVIDER( cssItalic ));
                    gtk_style_context_remove_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wTraceNote )),
                            GTK_STYLE_PROVIDER( cssItalic ));
				}
			}
		}
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Status")),
				"Please provide profile name.");
	}

	g_free( sName );
}

void
CB_BtnRemove (GtkButton * button, tGlobal *pGlobal)
{
	GtkComboBoxText *wCombo;
	gchar *sanitizedName = NULL, *name=NULL;
	GtkWidget *dialog;
	gboolean bAuthorized = TRUE;
	gchar *sQuestion = NULL;

	if( pGlobal->flags.bCalibrationOrTrace ) {
		wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
		name = pGlobal->pCalibrationAbstract->projectAndName.sName;
		sanitizedName = g_markup_escape_text ( name, -1 );
		sQuestion = g_strdup_printf(
				"You look as though you know what you are doing but..."
				"\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
				"\t\"<b>%s</b>\"\n\n⚖️ calibration profile?", sanitizedName);
	} else {
		wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
		name = pGlobal->pTraceAbstract->projectAndName.sName;
		sanitizedName = g_markup_escape_text ( name, -1 );
		sQuestion = g_strdup_printf(
				"You look as though you know what you are doing but..."
				"\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
				"\t\"<b>%s</b>\"\n\n📈 trace profile?", sanitizedName);
	}

	if ( sanitizedName != NULL && strlen( sanitizedName ) != 0 ) {
		dialog = gtk_message_dialog_new( NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_YES_NO, " ");
		gtk_window_set_title(GTK_WINDOW(dialog), "Caution");
		gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG( dialog ), sQuestion );

		if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES ) {
			bAuthorized = TRUE;
		} else {
			bAuthorized = FALSE;
		}
		gtk_widget_destroy(dialog);

		if( bAuthorized && deleteDBentry( pGlobal, pGlobal->sProject, name,
				pGlobal->flags.bCalibrationOrTrace ? eDB_CALandSETUP : eDB_TRACE ) == 0 ) {
		    // deleteDBentry deletes the actual item, so we have to locate the list entry now
			if( pGlobal->flags.bCalibrationOrTrace ) {
			    pGlobal->pCalibrationAbstract = NULL;

				populateCalComboBoxWidget( pGlobal );
                // We've deleted the calibration profile from this project
				// so we are not looking at, so just select the first cal item
                gtk_combo_box_set_active( GTK_COMBO_BOX(wCombo), 0);
                // also update the calibration pointer to the first profile in the list
                // that matches the project
                pGlobal->pCalibrationAbstract = selectFirstCalibrationProfileInProject( pGlobal );
			} else {
                pGlobal->pTraceAbstract = NULL;

                populateTraceComboBoxWidget( pGlobal );
                // We've deleted the calibration profile from this project
                // so we are not looking at, so just select the first cal item
                gtk_combo_box_set_active( GTK_COMBO_BOX(wCombo), 0);
                // also update the calibration pointer to the first profile in the list
                // that matches the project
                pGlobal->pTraceAbstract = selectFirstTraceProfileInProject( pGlobal );
			}
		}
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Status")),
				"Please provide profile name.");
	}

    g_free( sanitizedName );
	g_free( sQuestion );
}

void
CB_BtnGetTrace (GtkButton * button, tGlobal *pGlobal)
{
    // Release held live marker
    pGlobal->flags.bHoldLiveMarker = FALSE;

    if( pGlobal->flags.bDoNotRetrieveHPGLdata ) {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                    g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_RadioBtn_PlotTypeHighRes") ), TRUE);
        g_free( pGlobal->HP8753.plotHPGL );
        pGlobal->HP8753.plotHPGL = NULL;
    }
	postDataToGPIBThread (TG_RETRIEVE_TRACE_from_HP8753, NULL);
	gtk_widget_set_sensitive (GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_SaveRecallDelete") ), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_GetTrace") ), FALSE);
	// Show the trace notebook page
    gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable,
            (gconstpointer )"WID_Note")), NPAGE_TRACE );
}


// handler for the 1 second timer tick
gboolean timer_handler(tGlobal *pGlobal)
{
    GDateTime *date_time;
    gchar *dt_format;

    date_time = g_date_time_new_now_local();                        // get local time
    dt_format = g_date_time_format(date_time, "%d %b %y %H:%M:%S");            // 24hr time format
    gtk_label_set_text(GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Status")),
    		dt_format);    // update label
    g_free (dt_format);

    return TRUE;
}


void
CB_EntryTitle_Changed (GtkEditable* wEditable, tGlobal *pGlobal)
{
	gchar *sTitle = gtk_editable_get_chars(wEditable, 0, -1);

	g_free( pGlobal->HP8753.sTitle );
	pGlobal->HP8753.sTitle = sTitle;
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A")));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_B")));
}


void
CB_Edit_NumberFilter (GtkEditable *editable,
                  const gchar *text,
                  gint        length,
                  gint        *position,
                  gpointer    user_data)
{
	gunichar thisChar = g_utf8_get_char (text);
	gchar *sUpToNow = gtk_editable_get_chars (editable, 0, -1);
	gboolean bDecimalExists = FALSE;

	for( int i=0; i < strlen( sUpToNow ); i++)
		if( sUpToNow[i] == '.' )
			bDecimalExists = TRUE;

	if (g_unichar_isdigit ( thisChar )
			|| (thisChar == '-' && *position == 0)
			|| (thisChar == '.' && !bDecimalExists) ) {
		  g_signal_handlers_block_by_func (editable,
				  (gpointer) CB_Edit_NumberFilter, user_data);

		  gtk_editable_insert_text (editable, text, length, position);

		  g_signal_handlers_unblock_by_func (editable,
				  (gpointer) CB_Edit_NumberFilter, user_data);

	}

	g_signal_stop_emission_by_name (editable, "insert_text");

	g_free( sUpToNow );
}

static void
drawingAreaMouseButton(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal, gboolean bAnotB)
{
	guint button;
	gdouble x, y;
	gdouble fractionX = 0.0;

	guint areaWidth   = gtk_widget_get_allocated_width (widget);

    // We have only connected the press signal GDK_BUTTON_PRESS
    // GdkEventType eventType = gdk_event_get_event_type ( event );

	gdk_event_get_button ( event, &button );
	gdk_event_get_coords ( event, &x, &y );

	fractionX = x / (gdouble)(areaWidth);

	if( button == 2 ) {
	    pGlobal->mouseXpercentHeld = fractionX;
	    pGlobal->flags.bHoldLiveMarker = TRUE;
	} else {
	    pGlobal->flags.bHoldLiveMarker = FALSE;
	}

    gtk_widget_queue_draw( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A") ));
    gtk_widget_queue_draw( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_B") ));

}

void CB_DrawingArea_Plot_A_MouseButton(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal)
{
	drawingAreaMouseButton( widget, event, pGlobal, TRUE );
}

void CB_DrawingArea_Plot_B_MouseButton(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal)
{
	drawingAreaMouseButton( widget, event, pGlobal, FALSE );
}


void
SensitizeWidgets ( tGlobal *pGlobal ) {
	GtkWidget   *wCalibrationProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile"));
	GtkWidget   *wTraceProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile"));
	// GtkButton   *wBtnRecall = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" );
	// GtkButton   *wBtnDelete = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" );
	GtkButton   *wBtnSave = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Save" );

	gtk_widget_set_sensitive( wCalibrationProfile,	pGlobal->flags.bCalibrationOrTrace );
	gtk_widget_set_sensitive( wTraceProfile, !pGlobal->flags.bCalibrationOrTrace );

	if( pGlobal->flags.bCalibrationOrTrace ) {
		gtk_widget_set_sensitive( GTK_WIDGET( wBtnSave ), g_list_length( pGlobal->pCalList ) > 0 );
	} else {
		gtk_widget_set_sensitive(
			GTK_WIDGET( wBtnSave ), (g_list_length( pGlobal->pTraceList ) > 0)
				&& (globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
						|| globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData) );
	}
}

/*!     \brief  Callback when selects Calibration or Trace GtkRadioButton
 *
 * Callback when selects Calibration or Trace GtkRadioButton.
 * This will also sensitize the appropriate GtkComboxBoxText &
 * change the GtkNoteBook page to the calibration or trace information / notes.
 *
 * \param  wCalibration pointer to radio button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Radio_Calibration ( GtkRadioButton *wCalibration, tGlobal *pGlobal ) {

	gboolean bFound = FALSE;
	GtkWidget *wCalibrationProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile"));
	GtkWidget *wTraceProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile"));
	GtkButton 		*wBtnRecall = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" );
	GtkButton 		*wBtnDelete = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" );
	GtkButton 		*wBtnSave = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Save" );
	GtkNotebook 	*wNotebook = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note");
	GtkLabel *wLabel;

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCalibration )) == 0) {
		return;
	}
	pGlobal->flags.bCalibrationOrTrace = TRUE;
	pGlobal->flags.bProject = FALSE;

	gtk_widget_set_sensitive( wCalibrationProfile,	TRUE );
	gtk_widget_set_sensitive( wTraceProfile, FALSE );

	if ( pGlobal->pCalibrationAbstract )
	    bFound = setGtkComboBox( GTK_COMBO_BOX(wCalibrationProfile),
	            pGlobal->pCalibrationAbstract->projectAndName.sName );

	gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnSave ), pGlobal->pCalibrationAbstract != NULL );

	gtk_notebook_set_current_page( wNotebook, NPAGE_CALIBRATION );

	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnRecall ) ) );
	gtk_label_set_markup (wLabel, "restore ⚙︎" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnSave ) ) );
	gtk_label_set_markup (wLabel, "save ⚙︎" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnDelete ) ) );
	gtk_label_set_markup (wLabel, "delete ⚙︎" );

    sensitizeRecallSaveDeleteButtons( pGlobal ) ;
}

/*!     \brief  Callback when selects Trace GtkRadioButton
 *
 * Callback when selects Trace GtkRadioButton.
 * This will also sensitize the appropriate GtkComboxBoxText &
 * change the GtkNoteBook page to the calibration or trace information / notes.
 *
 * \param  wTrace pointer to radio button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Radio_Trace ( GtkRadioButton *wTrace, tGlobal *pGlobal ) {

	gboolean bFound = FALSE;
	GtkWidget *wCalibrationProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile"));
	GtkWidget *wTraceProfile = GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile"));
	GtkButton 		*wBtnRecall = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" );
	GtkButton 		*wBtnDelete = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" );
	GtkButton 		*wBtnSave = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Save" );
	GtkNotebook 	*wNotebook = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note");
	GtkLabel *wLabel;

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wTrace )) == 0) {
		return;
	}
	pGlobal->flags.bCalibrationOrTrace = FALSE;
	pGlobal->flags.bProject = FALSE;

	gtk_widget_set_sensitive( wCalibrationProfile,	FALSE );
	gtk_widget_set_sensitive( wTraceProfile, TRUE );

	if ( pGlobal->pTraceAbstract )
	    bFound = setGtkComboBox( GTK_COMBO_BOX(wTraceProfile),
	            pGlobal->pTraceAbstract->projectAndName.sName );

	gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );

	gtk_widget_set_sensitive(
		GTK_WIDGET( wBtnSave ), pGlobal->pTraceAbstract &&
		(globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
					|| globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData) );

	gtk_notebook_set_current_page( wNotebook,
			pGlobal->flags.bCalibrationOrTrace ? NPAGE_CALIBRATION:NPAGE_TRACE);

	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnRecall ) ) );
	gtk_label_set_markup (wLabel, "recall 📈" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnSave ) ) );
	gtk_label_set_markup (wLabel, "save 📈" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnDelete ) ) );
	gtk_label_set_markup (wLabel, "delete 📈" );

	sensitizeRecallSaveDeleteButtons( pGlobal ) ;
}


/*!     \brief  Callback when user types in the ComboBoxText (editable) for the project name
 *
 * Callback when user types in the ComboBoxText (editable) for the project name.
 * If the profile name matches an existing profile .. display the info for that profile
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  tGlobal	    pointer global data
 */
void
CB_EditableProjectName( GtkEditable *wEditable, tGlobal *pGlobal ) {
	gchar *pProjectName = gtk_editable_get_chars( wEditable, 0, -1 ); // and allocated string
	g_free(pGlobal->sProject);
	if( strlen( pProjectName ) )
		pGlobal->sProject = pProjectName;
	else {
		pGlobal->sProject = (gchar *)0;
		g_free( pProjectName );
	}
	populateCalComboBoxWidget( pGlobal );
	populateTraceComboBoxWidget( pGlobal );
}

void
CB_ComboProjectSelect( GtkComboBoxText *wComboBoxProject, tGlobal *pGlobal ) {
	gint n = gtk_combo_box_get_active( GTK_COMBO_BOX( wComboBoxProject ));
    GtkWidget *wCalCombo =
            g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile" );
    GtkWidget *wTraceCombo =
            g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile" );
    gchar *sProfileName = NULL;

	if( n != INVALID ) {
	}
	GtkRadioButton *wRadioCal = GTK_RADIO_BUTTON( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_RadioCal") );
	GtkNotebook *wNotebook = GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note") );

	// Ususlly the not will be set in CB_EditableCalibrationProfileName and CB_EditableTraceProfileName
	// but if there is no profile the note will be left in the last state

	sProfileName = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(wCalCombo));
	if( strlen( sProfileName ) == 0 ) {
        GtkWidget *wCalNote = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_TextView_CalibrationNote");
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW( wCalNote) ), "", STRLENGTH);
	}
	g_free( sProfileName );
	sProfileName = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(wTraceCombo));
    if( strlen( sProfileName ) == 0 ) {
        GtkWidget *wTraceNote = g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_TextView_TraceNote");
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote) ), "", STRLENGTH);
	}
    g_free( sProfileName );

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wRadioCal )) )
	    gtk_notebook_set_current_page( wNotebook, NPAGE_CALIBRATION );
	else
	    gtk_notebook_set_current_page( wNotebook, NPAGE_TRACE );

	sensitizeRecallSaveDeleteButtons( pGlobal );
}

/*!     \brief  Callback Calibration/Setup selection of GtkComboBoxText
 *
 * Callback when the Calibration/Setup profile GtkComboBoxText is changed
 *
 * \param  wCalSelection      pointer to GtkComboBox widget
 * \param  tGlobal	          pointer global data
 */
void
CB_ComboBoxCalibrationProfileName (GtkComboBoxText *wCalSelection, tGlobal *pGlobal) {
    gchar *sCalProfileName = NULL;
    tProjectAndName *pProjectAndName;
	for( GList *l = pGlobal->pCalList; l != NULL; l = l->next ){
		pProjectAndName = &((tHP8753cal *)l->data)->projectAndName;

		if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
			pProjectAndName->bSelected = FALSE;
			sCalProfileName = gtk_combo_box_text_get_active_text(wCalSelection);
			if( g_strcmp0(  pProjectAndName->sName, sCalProfileName ) == 0 ) {
				pProjectAndName->bSelected = TRUE;
			}
			g_free( sCalProfileName );
		}
	}
}

/*!     \brief  Callback Trace selection of GtkComboBoxText
 *
 * Callback when the Trace profile GtkComboBoxText is changed
 *
 * \param  wTraceSelection    pointer to GtkComboBox widget
 * \param  tGlobal	          pointer global data
 */
void
CB_ComboBoxTraceProfileName (GtkComboBoxText *wTraceSelection, tGlobal *pGlobal) {
	tProjectAndName *pProjectAndName;
	gchar *sTraceProfileName = NULL;
	for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
		pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);

		if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
			pProjectAndName->bSelected = FALSE;
			sTraceProfileName = gtk_combo_box_text_get_active_text(wTraceSelection);
			if( g_strcmp0(  pProjectAndName->sName, gtk_combo_box_text_get_active_text(wTraceSelection) ) == 0 ) {
				pProjectAndName->bSelected = TRUE;
			}
			g_free( sTraceProfileName );
		}
	}
}

/*!     \brief  Callback when user types in the ComboBoxText (editable) for the calibration profile name
 *
 * Callback when user types in the ComboBoxText (editable) for the calibration profile name.
 * Sensitize the 'save' and 'delete' buttons if the text matches a saved profile, otherwise
 * desensitize the buttons.
 * If the profile name matches an existing profile .. display the info for that profile
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  tGlobal	    pointer global data
 */
void
CB_EditableCalibrationProfileName( GtkEditable *wEditable, tGlobal *pGlobal ) {

    GtkWidget *wCalNote = g_hash_table_lookup(pGlobal->widgetHashTable,
            (gconstpointer )"WID_TextView_CalibrationNote");
	GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wCalNote ));
	        gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")),
	                NPAGE_CALIBRATION );

    GtkComboBoxText *wCalibrationComboBox =
            GTK_COMBO_BOX_TEXT(gtk_widget_get_parent( gtk_widget_get_parent( GTK_WIDGET(wEditable))));
    gchar *sCalProfleName = gtk_combo_box_text_get_active_text(wCalibrationComboBox);

    // setGtkComboBox will return TRUE if the profile exists
    sensitizeRecallSaveDeleteButtons( pGlobal );

    if( setGtkComboBox( GTK_COMBO_BOX( wCalibrationComboBox ), sCalProfleName ) ) {
        pGlobal->pCalibrationAbstract = selectCalibrationProfile( pGlobal, pGlobal->sProject,
                sCalProfleName );

		// show the calibration information relevent to the selection
        showCalInfo( pGlobal->pCalibrationAbstract, pGlobal );
		gtk_widget_set_sensitive ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_CalInfo"), FALSE);
		gtk_text_buffer_set_text( wTBnote, pGlobal->pCalibrationAbstract->sNote, STRLENGTH);
	} else {
		// Clear the calibration information area (as the edit box text does not relate to a saved profile
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer( GTK_TEXT_VIEW(
						g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh1"))), "", 0);						;
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer( GTK_TEXT_VIEW(
						g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh2"))), "", 0);

	}
    gtk_style_context_add_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wCalNote )),
            GTK_STYLE_PROVIDER( cssItalic ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

    g_free( sCalProfleName );
}

/*!     \brief  Callback when user types in the ComboBoxText (editable) for the trace name
 *
 * Callback when user types in the ComboBoxText (editable) for the trace name
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  tGlobal	    pointer global data
 */
void
CB_EditableTraceProfileName( GtkEditable *wEditable, tGlobal *pGlobal ) {
    GtkWidget *wTraceNote = g_hash_table_lookup(pGlobal->widgetHashTable,
            (gconstpointer )"WID_TextView_TraceNote");
	GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote));
	GtkEntry* wTEtitle = GTK_ENTRY( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Entry_Title"));
	gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")), NPAGE_TRACE );

	GtkComboBoxText *wTraceComboBox =
	        GTK_COMBO_BOX_TEXT(gtk_widget_get_parent( gtk_widget_get_parent( GTK_WIDGET(wEditable))));
	gchar *sTraceProfileName = gtk_combo_box_text_get_active_text(wTraceComboBox);
	sensitizeRecallSaveDeleteButtons( pGlobal );
	// setGtkComboBox will return TRUE if the profile exists
	if( setGtkComboBox( GTK_COMBO_BOX( wTraceComboBox ), sTraceProfileName ) ) {
        pGlobal->pTraceAbstract = selectTraceProfile( pGlobal, pGlobal->sProject, sTraceProfileName );

		// show the trace information relevent to the selection
		gtk_text_buffer_set_text( wTBnote, pGlobal->pTraceAbstract->sNote, STRLENGTH);

		// Block signals while we populate the widgets programatically
		// So the title on the currently displayed plot doesn't change
		g_signal_handlers_block_by_func(G_OBJECT(wTEtitle), CB_EntryTitle_Changed, pGlobal);
		// Don't update pGlobal->HP8753.sTitle until we 'recall' otherwise resizing will show the wrong title on the plot
		// pGlobal->HP8753.sTitle = titleAbstract->sTitle;
		gtk_entry_set_text( GTK_ENTRY( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Entry_Title")),
		        pGlobal->pTraceAbstract->sTitle == NULL ? "" : pGlobal->pTraceAbstract->sTitle);

		g_signal_handlers_unblock_by_func(G_OBJECT(wTEtitle), CB_EntryTitle_Changed, pGlobal);

		gtk_label_set_label ( GTK_LABEL( g_hash_table_lookup(pGlobal->widgetHashTable,
				(gconstpointer )"WID_LblTraceTime")), pGlobal->pTraceAbstract->sDateTime );
	}

    gtk_style_context_add_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wTEtitle )),
            GTK_STYLE_PROVIDER( cssItalic ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    gtk_style_context_add_provider ( gtk_widget_get_style_context ( GTK_WIDGET( wTraceNote )),
            GTK_STYLE_PROVIDER( cssItalic ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

    g_free( sTraceProfileName );
}

/*!     \brief  Callback - show HPGL screen plot or enhanced display
 *
 * Callback when the "Show Plot" GtkButton is changed
 *
 * \param  wCheckBtn    pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_RadioBtn_ScreenPlot (GtkRadioButton *wRadioBtn, tGlobal *pGlobal) {
	pGlobal->HP8753.flags.bShowHPGLplot = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wRadioBtn ));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A")));

    if ( !globalData.HP8753.flags.bDualChannel
            || !pGlobal->HP8753.flags.bSplitChannels
            || !pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData
            || ( pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid ) ) {
        visibilityFramePlot_B( pGlobal, FALSE );
    } else {
        visibilityFramePlot_B( pGlobal, TRUE);
        gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                                    (gconstpointer )"WID_DrawingArea_Plot_B")));
    }
}

/*!     \brief  Callback / GtkNotebook page changed
 *
 * Callback when the GtkNotebook page has changed (Calibration/Trace/Data/Options/GPIB/Cal. Kits)
 *
 * \param  wNotebook      pointer to GtkNotebook widget
 * \param  wPage          pointer to the current page (box or scrolled widget)
 * \param  nPage          which page was selected
 * \param  tGlobal	      pointer global data
 */
void
CB_Notebook_Select( GtkNotebook *wNotebook,   GtkWidget* wPage,
		  guint nPage, tGlobal *pGlobal ) {
	if( nPage == 0 ) {
		gtk_button_clicked( GTK_BUTTON( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_RadioCal")) );
	} else if ( nPage == 1 ) {
		gtk_button_clicked( GTK_BUTTON( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_RadioTraces")) );


		GtkEntry *wEntryTitle = GTK_ENTRY(g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_EntryTitle"));
        gtk_entry_grab_focus_without_selecting (wEntryTitle);
	}
}



