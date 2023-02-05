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
sensitizeRecallAndDeleteButtons( GtkEditable *editable, tGlobal *pGlobal ) {

	gboolean bFound = FALSE;
	GtkComboBoxText *wComboText = GTK_COMBO_BOX_TEXT( gtk_widget_get_parent( gtk_widget_get_parent ( GTK_WIDGET(editable)) ));
	GtkButton 		*wBtnRecall = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" );
	GtkButton 		*wBtnDelete = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" );
	gchar 			*sString = gtk_editable_get_chars( editable, 0, -1 );

	bFound = setGtkComboBox( GTK_COMBO_BOX( wComboText ), sString );

	gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );
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

void
CB_DrawingArea_Plot_A_Realize (GtkDrawingArea * wDrawingAreaA, tGlobal *pGlobal)
{
    gdk_window_set_events ( gtk_widget_get_window( GTK_WIDGET(wDrawingAreaA) ),
    		GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
//    g_signal_connect (GTK_WIDGET(wDrawingAreaA), "button-press-event", G_CALLBACK (CB_DrawingArea_Plot_A_MouseButton), (gpointer)&globalData);
}

void
CB_DrawingArea_Plot_B_Realize (GtkDrawingArea * wDrawingAreaB, tGlobal *pGlobal)
{
    gdk_window_set_events ( gtk_widget_get_window( GTK_WIDGET(wDrawingAreaB) ),
    		GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
}

#define MIN_WIDGET_SIZE 1
void
hide_Frame_Plot_B (tGlobal *pGlobal)
{
	GtkWidget *wApplication = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_hp8753c_main");
	GtkWidget *wFramePlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frame_Plot_B");

	gtk_widget_hide( wFramePlotB );
   	gtk_window_resize( GTK_WINDOW(wApplication), MIN_WIDGET_SIZE, MIN_WIDGET_SIZE);
}

void
CB_hp8753c_main_Realize (GtkApplicationWindow * wApplicationWindow, tGlobal *pGlobal)
{
	hide_Frame_Plot_B( pGlobal );
}

void
CB_BtnRecall (GtkButton * button, tGlobal *pGlobal)
{
	GtkComboBoxText *cbSetup;
	gchar *name;
	gint rtn;

	if( pGlobal->flags.bCalibrationOrTrace )
		cbSetup = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
	else
		cbSetup = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
	name = gtk_combo_box_text_get_active_text( cbSetup );

	if ( name && strlen( name ) != 0 ) {
		if( pGlobal->flags.bCalibrationOrTrace ) {
			if( recoverCalibrationAndSetup( pGlobal, (gchar *)name ) != ERROR ) {
				GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
											(gconstpointer )"WID_TextView_CalibrationNote")));
				gtk_text_buffer_set_text( wTBnote,
						pGlobal->HP8753cal.sNote ? pGlobal->HP8753cal.sNote : "", STRLENGTH );
				postDataToGPIBThread (TG_SEND_SETUPandCAL_to_HP8753, NULL );
			}
			sensitiseControlsInUse( pGlobal, FALSE );
			gtk_widget_set_sensitive ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_CalInfo"), TRUE);
		} else {
			if( (rtn = recoverTraceData( pGlobal, (gchar *)name )) != ERROR ) {
				if( rtn == FALSE )
					clearHP8753traces( &pGlobal->HP8753 );
				GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
											(gconstpointer )"WID_TextView_TraceNote")));

				gtk_text_buffer_set_text( wTBnote,
						pGlobal->HP8753.sNote ? pGlobal->HP8753.sNote : "", STRLENGTH );
				gtk_entry_set_text( GTK_ENTRY( g_hash_table_lookup(pGlobal->widgetHashTable,
											(gconstpointer )"WID_Entry_Title")),
							pGlobal->HP8753.sTitle ? pGlobal->HP8753.sTitle : "");
				if (!pGlobal->HP8753.flags.bDualChannel || !pGlobal->HP8753.flags.bSplitChannels) {
					postDataToMainLoop(TM_REFRESH_TRACE, 0);
				} else {
					postDataToMainLoop(TM_REFRESH_TRACE, 0);
					postDataToMainLoop(TM_REFRESH_TRACE, (void*) 1);
				}
			}
		}
		gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")), pGlobal->flags.bCalibrationOrTrace ? 0 : 1 );
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable,
						(gconstpointer)"WID_Lbl_Status")), "Please provide profile name.");
	}
	g_free( name );

}

void
CB_BtnSave (GtkButton * button, tGlobal *pGlobal)
{
	GtkComboBoxText *wComboBoxName;
	gchar *sName, *sNote;
	GtkTextBuffer* wTBnote;
	GtkTextIter start, end;

	if( pGlobal->flags.bCalibrationOrTrace ) {
		wComboBoxName = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
		wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
												(gconstpointer )"WID_TextView_CalibrationNote")));
	} else {
		wComboBoxName = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
		wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
									(gconstpointer )"WID_TextView_TraceNote")));
	}
	sName = gtk_combo_box_text_get_active_text( wComboBoxName );
	gtk_text_buffer_get_start_iter(wTBnote, &start);
	gtk_text_buffer_get_end_iter(wTBnote, &end);
	sNote = gtk_text_buffer_get_text(wTBnote, &start, &end, FALSE);

	if ( strlen( sName ) != 0 ) {
		GtkWidget *dialog;
		gboolean bFound;
		gboolean bAuthorized = TRUE;

		if( pGlobal->flags.bCalibrationOrTrace ) {
			bFound = (g_list_find_custom( pGlobal->pCalList, sName, (GCompareFunc)compareCalItem ) != NULL);
		} else {
			bFound = (g_list_find_custom( pGlobal->pTraceList, sName, (GCompareFunc)strcmp ) != NULL);
		}

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
				// send message to GPIB tread to get calibration data and then save
				postDataToGPIBThread (TG_RETRIEVE_SETUPandCAL_from_HP8753, (guchar *)g_strdup( sName ));
				sensitiseControlsInUse( pGlobal, FALSE );
			} else {
				g_free( pGlobal->HP8753.sNote );
				pGlobal->HP8753.sNote = sNote;
				saveTraceData(pGlobal, sName);
				// add to the list
				GList *traceName = g_list_find_custom( pGlobal->pTraceList, sName, (GCompareFunc)strcmp );
				if( !traceName )
					pGlobal->pTraceList = g_list_prepend(pGlobal->pTraceList, g_strdup(sName));
				pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)strcmp);

				gtk_combo_box_text_remove_all ( wComboBoxName  );
				for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
					gtk_combo_box_text_append_text( wComboBoxName, l->data );
				}
				g_free( pGlobal->sTraceProfile );
				pGlobal->sTraceProfile = g_strdup( sName );
				setGtkComboBox( GTK_COMBO_BOX( wComboBoxName ), pGlobal->sTraceProfile );
				gtk_widget_set_sensitive( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Recall" ), TRUE);
				gtk_widget_set_sensitive( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_Delete" ), TRUE);
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
	gchar *name = NULL;
	GtkWidget *dialog;
	gboolean bAuthorized = TRUE;
	gchar *sQuestion = NULL;

	if( pGlobal->flags.bCalibrationOrTrace ) {
		wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
		name = g_markup_escape_text ( gtk_combo_box_text_get_active_text( wCombo ), -1 );
		sQuestion = g_strdup_printf(
				"You look as though you know what you are doing but..."
				"\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
				"\t\"<b>%s</b>\"\n\n⚖️ calibration profile?", name);
	} else {
		wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
		name = g_markup_escape_text ( gtk_combo_box_text_get_active_text( wCombo ), -1 );
		sQuestion = g_strdup_printf(
				"You look as though you know what you are doing but..."
				"\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
				"\t\"<b>%s</b>\"\n\n📈 trace profile?", name);
	}


	if ( name != NULL && strlen( name ) != 0 ) {
		gboolean bFound = FALSE;
		GtkTreeIter iter;
		gint n;
		gchar *string;

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

		if( bAuthorized && deleteDBentry( pGlobal, name,
				pGlobal->flags.bCalibrationOrTrace ? eDB_CALandSETUP : eDB_TRACE ) == 0 ) {
			// look through all the combobox labels to see if the selected text matches.
			GtkTreeModel *tm = gtk_combo_box_get_model(GTK_COMBO_BOX(wCombo));

			n = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(wCombo)), NULL);
			if (n > 0) {
				gtk_tree_model_get_iter_first(tm, &iter);
				for (gint pos = 0; !bFound && pos < n; pos++, gtk_tree_model_iter_next(tm, &iter)) {
					gtk_tree_model_get(tm, &iter, 0, &string, -1);
					if (g_strcmp0(name, string) == 0) {
						gtk_combo_box_text_remove(wCombo, pos);
						bFound = TRUE;
					}
				}
			}
			if (bFound) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(wCombo), 0);
			}
		}
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Status")),
				"Please provide profile name.");
	}

	g_free( name );
	g_free( sQuestion );
}

void
CB_BtnGetTrace (GtkButton * button, tGlobal *pGlobal)
{
	postDataToGPIBThread (TG_RETRIEVE_TRACE_from_HP8753, NULL);
	gtk_widget_set_sensitive (GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_SaveRecallDelete") ), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(
			g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_GetTrace") ), FALSE);
}


void
CB_BtnS2P (GtkButton * button, tGlobal *pGlobal)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    gchar *sFilename = NULL;
    GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new ("Acquire S-paramater data and save to S2P file",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Save", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);
	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".s2p" );
    gtk_file_filter_add_pattern (filter, "*.[sS][12][pP]");
	gtk_file_chooser_add_filter ( chooser, filter );
	//gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (chooser, filter);

	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	if( lastFilename )
		sFilename = g_strdup( lastFilename);
	else
		sFilename = g_date_time_format( now, "HP8753C.%d%b%y.%H%M%S.s2p");
	gtk_file_chooser_set_current_name (chooser, sFilename);

	if( pGlobal->sLastDirectory )
		gtk_file_chooser_set_current_folder( chooser, pGlobal->sLastDirectory );

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename, *extPos = NULL;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( chooser );

		GString *sFilename = g_string_new( filename );
		g_free( filename );	// don't need this
		extPos = g_strrstr( sFilename->str, ".s2p" );
		if( !extPos )
			g_string_append( sFilename, ".s2p");

		g_free( lastFilename );
		lastFilename = g_strdup(sFilename->str);

		sensitiseControlsInUse( pGlobal, FALSE );
		postDataToGPIBThread (TG_MEASURE_and_RETRIEVe_S2P_from_HP8753, sFilename->str);

		// sFilename->str is freed by thread
		g_string_free (sFilename, FALSE);
	}

	g_free( sFilename );
	gtk_widget_destroy (dialog);
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
CB_ChkBtn_Spline (GtkCheckButton * button, tGlobal *pGlobal)
{
	pGlobal->flags.bSmithSpline = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button ) );
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_A")));
	gtk_widget_queue_draw(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
			(gconstpointer)"WID_DrawingArea_Plot_B")));
}

#define PNG_WIDTH	3300
#define PNG_HEIGHT	2550
void
CB_BtnSavePNG (GtkButton * button, tGlobal *pGlobal)
{
	cairo_t *cr;
    cairo_surface_t *cs;
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    gchar *sFilename = NULL;
    gchar *sSuggestedFilename = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S.png");
    static gboolean bUsedSuggested = FALSE;
	gboolean bBoth = pGlobal->HP8753.flags.bDualChannel
			&& pGlobal->HP8753.flags.bSplitChannels;

	cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, PNG_WIDTH, PNG_HEIGHT);
	cr = cairo_create (cs);
    // clear the screen
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
	cairo_paint( cr );
	cairo_save( cr ); {
		plotA(PNG_WIDTH, PNG_HEIGHT, cr, pGlobal);
	} cairo_restore( cr );


 //g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_hp8753c_main")
	dialog = gtk_file_chooser_dialog_new ("Open File",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Save", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);

	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".png" );
    gtk_file_filter_add_pattern (filter, "*.[pP][nN][gG]");
	gtk_file_chooser_add_filter ( chooser, filter );
	//gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (chooser, filter);

	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	if( lastFilename && !bUsedSuggested) {
		sFilename = g_strdup( lastFilename);
		gtk_file_chooser_set_filename( chooser, sFilename );
	} else {
		sFilename = g_strdup( sSuggestedFilename );
		gtk_file_chooser_set_current_name (chooser, sFilename);
	}

	if( pGlobal->sLastDirectory)
		gtk_file_chooser_set_current_folder( chooser, pGlobal->sLastDirectory );

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *sChosenFilename;

		sChosenFilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		// see if they used our suggestion
		gchar *sBaseName = g_path_get_basename( sChosenFilename );
		bUsedSuggested = (g_strcmp0( sBaseName, sSuggestedFilename ) == 0);
		g_free( sBaseName );

		g_free( lastFilename );
		lastFilename = g_strdup( sChosenFilename );

		cairo_surface_flush (cs);
		if ( bBoth ) {
			gchar *extPos = NULL;
			// create two filenames from the provided name 'name.1.png and name.2.png'
			GString *sFilename = g_string_new( sChosenFilename );
			extPos = g_strrstr( sFilename->str, ".png" );
			if( extPos )
				g_string_insert( sFilename, extPos  - sFilename->str, ".1" );
			else
				g_string_append( sFilename, ".1.png");

			cairo_surface_write_to_png (cs, sFilename->str);

			extPos = g_strrstr( sFilename->str, ".1.png" );
			*(extPos+1) = '2';
		    // clear the screen
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
			cairo_paint( cr );
			plotB(PNG_WIDTH, PNG_HEIGHT, cr, pGlobal);

			cairo_surface_write_to_png (cs, sFilename->str);

			g_string_free( sFilename, TRUE );
		} else {
			cairo_surface_write_to_png (cs, sChosenFilename);
		}

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( chooser );

		g_free (sChosenFilename);
	}

	g_free( sFilename );
	gtk_widget_destroy (dialog);

	cairo_destroy( cr );
	cairo_surface_destroy ( cs );
}

#define PDF_WIDTH	(11.5 * 72)
#define PDF_HEIGHT	(8 * 72)

void
CB_BtnSavePDF (GtkButton * button, tGlobal *pGlobal)
{
	cairo_t *cr;
    cairo_surface_t *cs;
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    gchar *sFilename = NULL;
    gchar *sSuggestedFilename = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S.pdf");
    static gboolean bUsedSuggested = FALSE;
	gboolean bBoth = pGlobal->HP8753.flags.bDualChannel
			&& pGlobal->HP8753.flags.bSplitChannels;

 //g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_hp8753c_main")
	dialog = gtk_file_chooser_dialog_new ("Open File",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Save", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);

	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".pdf" );
    gtk_file_filter_add_pattern (filter, "*.[pP][dD][fF]");
	gtk_file_chooser_add_filter ( chooser, filter );
	//gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (chooser, filter);

	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	if( lastFilename && !bUsedSuggested) {
		sFilename = g_strdup( lastFilename);
		gtk_file_chooser_set_filename( chooser, sFilename );
	} else {
		sFilename = g_strdup( sSuggestedFilename );
		gtk_file_chooser_set_current_name (chooser, sFilename);
	}

	if( pGlobal->sLastDirectory )
		gtk_file_chooser_set_current_folder( chooser, pGlobal->sLastDirectory );

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *sChosenFilename = NULL, *extPos = NULL;

		sChosenFilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		// see if they used our suggestion
		gchar *sBaseName = g_path_get_basename( sChosenFilename );
		bUsedSuggested = (g_strcmp0( sBaseName, sSuggestedFilename ) == 0);
		g_free( sBaseName );

		g_free( lastFilename );
		lastFilename = g_strdup( sChosenFilename );

		cs = cairo_pdf_surface_create (sChosenFilename, PDF_WIDTH, PDF_HEIGHT );
		cr = cairo_create (cs);
		cairo_save( cr ); {
			plotA(PDF_WIDTH, PDF_HEIGHT, cr, pGlobal);
		} cairo_restore( cr );
		cairo_show_page( cr );
		if ( bBoth ) {
			plotB(PDF_WIDTH, PDF_HEIGHT, cr, pGlobal);
			cairo_show_page( cr );
		}

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( chooser );

		cairo_surface_destroy ( cs );
		cairo_destroy( cr );

		// now do high resolution smith charts
		// create two filenames from the provided name 'name.1.png and name.2.png'
		GString *sFilename = g_string_new( sChosenFilename );
		extPos = g_strrstr( sFilename->str, ".pdf" );
		if( extPos )
			g_string_insert( sFilename, extPos  - sFilename->str, ".HR" );
		else
			g_string_append( sFilename, ".HR.pdf");

		if( pGlobal->HP8753.channels[eCH_ONE].format == eFMT_SMITH )
			if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH )
				smithHighResPDF(pGlobal, sFilename->str, eCH_BOTH );
			else
				smithHighResPDF(pGlobal, sFilename->str, eCH_ONE );
		else if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH )
							smithHighResPDF(pGlobal, sFilename->str, eCH_TWO );

		g_string_free( sFilename, TRUE );
		g_free (sChosenFilename);
	}

	g_free( sFilename );
	gtk_widget_destroy (dialog);
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
writeCSVheader( FILE *file,
		tSweepType sweepCh1, tSweepType sweepCh2,
		tFormat fmtCh1, tFormat fmtCh2,
		tMeasurement measCh1, tMeasurement measCh2,
		gboolean bCoupled, gboolean bDualChannel ) {
	fprintf( file, "%s", optSweepType[ sweepCh1 ].desc );
	switch( fmtCh1 ) {
	case eFMT_SMITH:
	case eFMT_POLAR:
		fprintf( file, ",%s (re),%s (im)", optMeasurementType[ measCh1 ].desc,  optMeasurementType[ measCh1 ].desc );
		break;
	default:
		fprintf( file, ",%s (%s)", optMeasurementType[ measCh1 ].desc, formatSymbols[ fmtCh1 ] );
		break;
	}
	if( bDualChannel ) {
		if( !bCoupled )
			fprintf( file, ",%s", optSweepType[ sweepCh2 ].desc );
		switch( fmtCh2 ) {
		case eFMT_SMITH:
		case eFMT_POLAR:
			fprintf( file, ",%s (re),%s (im)", optMeasurementType[ measCh2 ].desc,  optMeasurementType[ measCh2 ].desc );
			break;
		default:
			fprintf( file, ",%s (%s)", optMeasurementType[ measCh2 ].desc, formatSymbols[ fmtCh2 ] );
			break;
		}
	}
	fprintf( file, "\n" );
}
void
writeCSVpoint( FILE *file, tFormat format, tComplex *point, gboolean bLF ) {
	switch( format ) {
	case eFMT_SMITH:
	case eFMT_POLAR:
		fprintf( file, ",%le,%le", point->r,  point->i );
		break;
	default:
		fprintf( file, ",%le", point->r );
		break;
	}
	if( bLF )
		fprintf( file, "\n" );
}

void
CB_BtnSaveCSV (GtkButton *wButton, tGlobal *pGlobal)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    static gboolean bUsedSuggested = FALSE;

    gchar *sFilename = NULL;
    gchar *sSuggestedFilename = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S.csv");

    FILE *fCSV = NULL;
    tFormat	fmtCh1 = pGlobal->HP8753.channels[ eCH_ONE ].format,
    		fmtCh2 = pGlobal->HP8753.channels[ eCH_TWO ].format;
    tSweepType sweepCh1 = pGlobal->HP8753.channels[ eCH_ONE ].sweepType,
    		   sweepCh2 = pGlobal->HP8753.channels[ eCH_TWO ].sweepType;
    tMeasurement measCh1 = pGlobal->HP8753.channels[ eCH_ONE ].measurementType,
    		     measCh2 = pGlobal->HP8753.channels[ eCH_TWO ].measurementType;

	if( !pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData ) {
		postError( "No trace data to export!" );
		return;
	}

	dialog = gtk_file_chooser_dialog_new ("Save trace data to CSV file",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Save", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);

	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".csv" );
    gtk_file_filter_add_pattern (filter, "*.[cC][sS][vV]");
	gtk_file_chooser_add_filter ( chooser, filter );
	//gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (chooser, filter);

	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	if( lastFilename && !bUsedSuggested) {
		sFilename = g_strdup( lastFilename);
		gtk_file_chooser_set_filename( chooser, sFilename );
	} else {
		sFilename = g_strdup( sSuggestedFilename );
		gtk_file_chooser_set_current_name (chooser, sFilename);
	}

	if( pGlobal->sLastDirectory )
		gtk_file_chooser_set_current_folder( chooser, pGlobal->sLastDirectory );

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *sChosenFilename, *extPos = NULL;

		sChosenFilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		// see if they used our suggestion
		gchar *sBaseName = g_path_get_basename( sChosenFilename );
		bUsedSuggested = (g_strcmp0( sBaseName, sSuggestedFilename ) == 0);
		g_free( sBaseName );

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( chooser );

		// now do high resolution smith charts
		// create two filenames from the provided name 'name.1.png and name.2.png'
		GString *sFilename = g_string_new( sChosenFilename );
		g_free( sChosenFilename );
		extPos = g_strrstr( sFilename->str, ".csv" );
		if( !extPos )
			g_string_append( sFilename, ".csv");

		g_free( lastFilename );
		lastFilename = g_strdup( sFilename->str );

		// todo ... save CSV data
		if( (fCSV = fopen( sFilename->str, "w" )) == NULL ) {
			gchar *sError = g_strdup_printf( "Cannot write: %s", (gchar *)sChosenFilename);
			postError( sError );
			g_free( sError );
		} else {
			writeCSVheader( fCSV,  sweepCh1, sweepCh2, fmtCh1, fmtCh2, measCh1, measCh2,
					pGlobal->HP8753.flags.bSourceCoupled, pGlobal->HP8753.flags.bDualChannel );
			if( pGlobal->HP8753.flags.bDualChannel ) {
				if( pGlobal->HP8753.flags.bSourceCoupled ) {
					for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints; i++ ) {
						fprintf( fCSV, "%le",
								pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
						writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], FALSE );
						writeCSVpoint( fCSV, fmtCh2, &pGlobal->HP8753.channels[ eCH_TWO ].responsePoints[i], TRUE );
					}
				} else {
					for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints
									|| i < pGlobal->HP8753.channels[ eCH_TWO ].nPoints; i++ ) {
						if( i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints ) {
							fprintf( fCSV, "%le",
									pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
							writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], FALSE );
						} else {
							fprintf( fCSV, ",,,");
						}
						if( i < pGlobal->HP8753.channels[ eCH_TWO ].nPoints ) {
							fprintf( fCSV, ",%le",
									pGlobal->HP8753.channels[ eCH_TWO ].stimulusPoints[i] );
							writeCSVpoint( fCSV, fmtCh2, &pGlobal->HP8753.channels[ eCH_TWO ].responsePoints[i], TRUE );
						} else {
							fprintf( fCSV, ",,\n");
						}
					}
				}
			} else {
				for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints; i++ ) {
					fprintf( fCSV, "%le",
							pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
					writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], TRUE );
				}
			}
			fclose( fCSV );
			postInfo( "CSV saved" );
		}

		postInfo( "Traces saved to csv file");
		g_free( sFilename->str );
		g_string_free (sFilename, FALSE);
	}

	g_free( sFilename );
	g_free( sSuggestedFilename );

	gtk_widget_destroy (dialog);
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
	gdk_event_get_button ( event, &button );
	gdk_event_get_coords ( event, &x, &y );
}

void CB_DrawingArea_Plot_A_MouseButton(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal)
{
	drawingAreaMouseButton( widget, event, pGlobal, TRUE );
}

void CB_DrawingArea_Plot_B_MouseButton(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal)
{
	drawingAreaMouseButton( widget, event, pGlobal, FALSE );
}


#define HEADER_HEIGHT 10
static void
CB_PrintDrawPage (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               pageNo,
		   tGlobal *          pGlobal)
{
	  cairo_t *cr;
	  gdouble height, width;

	  cr = gtk_print_context_get_cairo_context (context);
	  height = gtk_print_context_get_height( context );
	  width = gtk_print_context_get_width (context);
	  if( pageNo == 0)
		  plotA ( width, height, cr, pGlobal);
	  else
		  plotB ( width, height, cr, pGlobal);
}

static void
CB_PrintBegin (GtkPrintOperation *printOp,
           GtkPrintContext   *context, tGlobal *pGlobal)
{
	gint pageNos = 1;
	if( pGlobal->HP8753.flags.bDualChannel && pGlobal->HP8753.flags.bSplitChannels )
		pageNos = 2;
	gtk_print_operation_set_n_pages( printOp, pageNos );
}

static void
CB_PrintDone (GtkPrintOperation *printOp,
        GtkPrintContext   *context, tGlobal *pGlobal)
{

}


static void
CB_PrintRequestPageSetup(GtkPrintOperation* printOp,
                            GtkPrintContext* context, gint page_number, GtkPageSetup* setup,
							tGlobal *pGlobal)
{
#if 0
	GtkPageOrientation orientation;
	orientation = gtk_page_setup_get_orientation(setup);

	GtkPaperSize *paperSize = gtk_paper_size_new ( GTK_PAPER_NAME_LETTER );
	gtk_page_setup_set_paper_size( setup, paperSize );
#endif
}

void
CB_BtnMPrint (GtkButton *wButton, tGlobal *pGlobal)
{
	  GtkPrintOperation *printOp;
	  GtkPrintOperationResult res;

	  printOp = gtk_print_operation_new ();

	  if (pGlobal->printSettings != NULL)
	    gtk_print_operation_set_print_settings (printOp, pGlobal->printSettings);

	  if (pGlobal->pageSetup != NULL)
		  gtk_print_operation_set_default_page_setup (printOp, pGlobal->pageSetup);

	  g_signal_connect (printOp, "begin_print", G_CALLBACK (CB_PrintBegin), pGlobal);
	  g_signal_connect (printOp, "draw_page", G_CALLBACK (CB_PrintDrawPage), pGlobal);
	  g_signal_connect(printOp, "request-page-setup", G_CALLBACK(CB_PrintRequestPageSetup), pGlobal);
	  g_signal_connect(printOp, "done", G_CALLBACK(CB_PrintDone), pGlobal);

	  gtk_print_operation_set_embed_page_setup ( printOp, TRUE );
	  gtk_print_operation_set_use_full_page ( printOp, FALSE );

	 gtk_print_operation_set_n_pages ( printOp, 2 );

	  res = gtk_print_operation_run (printOp, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
			  GTK_WINDOW(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_hp8753c_main") ), NULL);

	  if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
	    {
	      if (pGlobal->printSettings != NULL)
	        g_object_unref (pGlobal->printSettings);
	      pGlobal->printSettings = g_object_ref (gtk_print_operation_get_print_settings (printOp));

	      if (pGlobal->pageSetup != NULL)
	        g_object_unref (pGlobal->pageSetup);
	      pGlobal->pageSetup = g_object_ref (gtk_print_operation_get_default_page_setup (printOp));
	    }

	  g_object_unref (printOp);
}

void setUseGPIBcardNoAndPID( tGlobal *pGlobal, gboolean bPID ) {
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_Controller_Identifier")), !bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_HP8753_Identifier")), !bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_Controler_CardNo")), bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_HP8753_PID")), bPID );
}

void
CB_Toggle_UseGPIB_SlotAndID (GtkToggleButton* wToggle, tGlobal *pGlobal) {
	pGlobal->flags.bGPIB_UseCardNoAndPID = gtk_toggle_button_get_active( wToggle);
	setUseGPIBcardNoAndPID( pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

void
CP_Entry_GPIBname_HP8753 (GtkEditable* wEditable, tGlobal *pGlobal)
{
	g_free( pGlobal->sGPIBdeviceName );
	pGlobal->sGPIBdeviceName = g_strdup( gtk_editable_get_chars( wEditable, 0, -1 ) );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

void
CB_Spin_GPIBcontrollerCard (GtkSpinButton* wSpin, tGlobal *pGlobal) {
	pGlobal->GPIBcontrollerIndex = (gint)gtk_spin_button_get_value( wSpin );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

void
CB_Spin_GPIB_HP8753_PID (GtkSpinButton* wSpin, tGlobal *pGlobal) {
	pGlobal->GPIBdevicePID = (gint)gtk_spin_button_get_value( wSpin );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
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

	pGlobal->flags.bCalibrationOrTrace = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wCalibration ));

	gtk_widget_set_sensitive( wCalibrationProfile,	pGlobal->flags.bCalibrationOrTrace );
	gtk_widget_set_sensitive( wTraceProfile, !pGlobal->flags.bCalibrationOrTrace );

	if( pGlobal->flags.bCalibrationOrTrace ) {
		bFound = setGtkComboBox( GTK_COMBO_BOX(wCalibrationProfile), pGlobal->sCalProfile );
	} else {
		bFound = setGtkComboBox( GTK_COMBO_BOX(wTraceProfile), pGlobal->sTraceProfile );
	}

	gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
	gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );

	if( pGlobal->flags.bCalibrationOrTrace )
		gtk_widget_set_sensitive(
			GTK_WIDGET( wBtnSave ),
			TRUE );
	else
		gtk_widget_set_sensitive(
			GTK_WIDGET( wBtnSave ),
			(globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
						|| globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData) );

	gtk_notebook_set_current_page( wNotebook,
			pGlobal->flags.bCalibrationOrTrace ? 0:1);

	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnRecall ) ) );
	gtk_label_set_markup (wLabel, pGlobal->flags.bCalibrationOrTrace ? "restore ⚖" : "recall 📈" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnSave ) ) );
	gtk_label_set_markup (wLabel, pGlobal->flags.bCalibrationOrTrace ? "save ⚖" : "save 📈" );
	wLabel = GTK_LABEL ( gtk_bin_get_child( GTK_BIN( wBtnDelete ) ) );
	gtk_label_set_markup (wLabel, pGlobal->flags.bCalibrationOrTrace ? "delete ⚖" : "delete 📈" );
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
CB_EditableCalibrationProfileName( GtkEditable *editable, tGlobal *pGlobal ) {
	GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( g_hash_table_lookup(pGlobal->widgetHashTable,
										(gconstpointer )"WID_TextView_CalibrationNote")));
	gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")), 0 );

	if( sensitizeRecallAndDeleteButtons( editable, pGlobal ) ) {
		g_free( pGlobal->sCalProfile );
		pGlobal->sCalProfile = g_strdup( gtk_editable_get_chars( editable, 0, STRLENGTH));

		// show the calibration information relevent to the selection
		GList *calPreviewElement = g_list_find_custom( pGlobal->pCalList, pGlobal->sCalProfile, (GCompareFunc)compareCalItem );
		showCalInfo( calPreviewElement->data, pGlobal );
		gtk_widget_set_sensitive ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Box_CalInfo"), FALSE);
		gtk_text_buffer_set_text( wTBnote, ((tHP8753cal *)calPreviewElement->data)->sNote, STRLENGTH);
	} else {
		// Clear the calibration information area (as the edit box text does not relate to a saved profile
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer( GTK_TEXT_VIEW(
						g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh1"))), "", 0);						;
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer( GTK_TEXT_VIEW(
						g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_TextView_CalInfoCh2"))), "", 0);
	}
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
	gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_Note")), 1 );
	if( sensitizeRecallAndDeleteButtons( wEditable, pGlobal ) ) {
		g_free( pGlobal->sTraceProfile );
		pGlobal->sTraceProfile = g_strdup( gtk_editable_get_chars( wEditable, 0, -1 ));
	}
}

/*!     \brief  Callback / Cal Kit page / "Options" "Show Date/Time" GtkCheckButton
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

/*!     \brief  Callback / Cal Kit page / "Options" "Admittance/Susceptance" GtkCheckButton
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

/*!     \brief  Callback / Cal Kit page / "Options" "Delta Marker Actual" GtkCheckButton
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

/*!     \brief  Callback / Cal Kit page / "Options" "Analyze Learn String" GtkButton
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
	}
}

/*!     \brief  Callback / Cal Kit page / change calibration kit selection of GtkComboBoxText
 *
 * Callback when the calibration kit GtkComboBoxText on the Cal Kit notebook page is changed
 *
 * \param  wCalKitProfile      pointer to GtkComboBox widget
 * \param  tGlobal	           pointer global data
 */
void
CB_ComboBoxCalKitSelection (GtkComboBoxText *wCalKitProfile, tGlobal *pGlobal) {
	GtkLabel *wCalKitDescription = GTK_LABEL(
			g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_Lbl_CalKitDescription") );
	gint n = gtk_combo_box_get_active( GTK_COMBO_BOX( wCalKitProfile ));

	if( n != INVALID ) {
		tCalibrationKitIdentifier *pCalKitIentifier = (tCalibrationKitIdentifier *)g_list_nth( pGlobal->pCalKitList, n )->data;
		gtk_label_set_label( wCalKitDescription, pCalKitIentifier->sDescription );
	}
}

/*!     \brief  Callback / Cal Kit page / "Read XKT" calibration kit GtkButton
 *
 * Callback when the "Read XKT" calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wButton      pointer to send button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ReadXKT (GtkButton *wButton, tGlobal *pGlobal)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    static gchar *lastFilename = NULL;
    GtkFileFilter *filter;
	GtkComboBoxText *wComboBoxCalKit = GTK_COMBO_BOX_TEXT(
			g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_Combo_CalKit") );

	dialog = gtk_file_chooser_dialog_new ("Import Calibration Kit",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Open", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);
	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".xkt" );
    gtk_file_filter_add_pattern (filter, "*.[xX][kK][tT]");
	gtk_file_chooser_add_filter ( chooser, filter );
	//gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (chooser, filter);
	if( pGlobal->sLastDirectory )
		gtk_file_chooser_set_current_folder( chooser, pGlobal->sLastDirectory );

	if( lastFilename )
		gtk_file_chooser_set_filename (chooser, lastFilename);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *sChosenFilename = NULL;

		sChosenFilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		g_free( lastFilename );
		lastFilename = g_strdup( sChosenFilename );

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( chooser );

    	if( parseCalibrationKit( sChosenFilename, &pGlobal->HP8753calibrationKit ) == 0 ) {
    		saveCalKit( pGlobal );

    		GList *listElement = g_list_find_custom( pGlobal->pCalKitList,
    				pGlobal->HP8753calibrationKit.label, (GCompareFunc)compareCalKitIdentifierItem );
    		// remove all combo box text items, then add themn back from the GList
    		gtk_list_store_clear (GTK_LIST_STORE( gtk_combo_box_get_model(GTK_COMBO_BOX(wComboBoxCalKit))));
    		for( GList *l = pGlobal->pCalKitList; l != NULL; l = l->next ){
    			gtk_combo_box_text_append_text( wComboBoxCalKit, ((tCalibrationKitIdentifier *)l->data)->sLabel );
    		}

    		gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxCalKit), g_list_position( pGlobal->pCalKitList, listElement ));

    		GtkWidget *wBtnSendCalKit = GTK_WIDGET(
    				g_hash_table_lookup ( pGlobal->widgetHashTable,	(gconstpointer)"WID_Btn_SendCalKit") );
    		gtk_widget_set_sensitive( GTK_WIDGET( wBtnSendCalKit ), TRUE );

    	}

    	g_free( sChosenFilename );
	}

	gtk_widget_destroy (dialog);
}

/*!     \brief  Callback / Cal Kit page / delete calibration kit GtkButton
 *
 * Callback when the delete calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wButton      pointer to send button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_BtnDeleteCalKit (GtkButton * button, tGlobal *pGlobal)
{
	GtkComboBoxText *wCombo;
	gchar *name = NULL;
	GtkWidget *dialog;
	gboolean bAuthorized = TRUE;
	gchar *sQuestion = NULL;

	wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalKit") );
	name = g_markup_escape_text ( gtk_combo_box_text_get_active_text( wCombo ), -1 );
	sQuestion = g_strdup_printf(
			"You look as though you know what you are doing but..."
			"\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
			"\t\"<b>%s</b>\"\n\n⚖️ calibration kit?", name);


	if ( name != NULL && strlen( name ) != 0 ) {
		gboolean bFound = FALSE;
		GtkTreeIter iter;
		gint n;
		gchar *string;

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

		if( bAuthorized && deleteDBentry( pGlobal, name, eDB_CALKIT ) == 0 ) {
			// look through all the combobox labels to see if the selected text matches.
			GtkTreeModel *tm = gtk_combo_box_get_model(GTK_COMBO_BOX(wCombo));

			n = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(wCombo)), NULL);
			if (n > 0) {
				gtk_tree_model_get_iter_first(tm, &iter);
				for (gint pos = 0; !bFound && pos < n; pos++, gtk_tree_model_iter_next(tm, &iter)) {
					gtk_tree_model_get(tm, &iter, 0, &string, -1);
					if (g_strcmp0(name, string) == 0) {
						gtk_combo_box_text_remove(wCombo, pos);
						bFound = TRUE;
					}
				}
			}
			if (bFound) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(wCombo), 0);
			}
		}
	} else {
		gtk_label_set_text( GTK_LABEL(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Status")),
				"No calibration kit selected");
	}

	g_free( name );
	g_free( sQuestion );
}

/*!     \brief  Callback / Cal Kit page / send calibration kit GtkButton
 *
 * Callback when the send calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wButton      pointer to send button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_BtnSendCalKit(GtkButton *wButton, tGlobal *pGlobal) {
	gint index;
	gchar *sLabel;
	GtkComboBoxText *wComboBoxCalKit = GTK_COMBO_BOX_TEXT(
			g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_Combo_CalKit") );

	if( (index = gtk_combo_box_get_active ( GTK_COMBO_BOX( wComboBoxCalKit ))) != -1 ) {
		sLabel = ((tCalibrationKitIdentifier *)g_list_nth_data( pGlobal->pCalKitList, index ))->sLabel;

		if( recoverCalibrationKit(pGlobal, sLabel) == 0 ) {
			postDataToGPIBThread (TG_SEND_CALKIT_to_HP8753, NULL);
			sensitiseControlsInUse( pGlobal, FALSE );
		} else {
			postError( "Cannot recover calibration kit");
		}
	}
}

/*!     \brief  Callback / Cal Kit page / GtkCheckButton
 *
 * Callback when the "+ user kit" GtkCheckButton on the Cal Kit notebook page is changed
 *
 * \param  wChkButton	pointer to check button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_ChkUserCalKit(GtkCheckButton *wChkButton, tGlobal *pGlobal) {
	pGlobal->flags.bSaveUserKit = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wChkButton ) );
}

