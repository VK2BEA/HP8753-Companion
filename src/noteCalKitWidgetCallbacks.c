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
			g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_CalKitDescription") );
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
	gchar *sCalKitName = NULL;

	wCombo = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalKit") );
	sCalKitName = gtk_combo_box_text_get_active_text( wCombo );
	name = g_markup_escape_text ( sCalKitName, -1 );
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

		if( bAuthorized && deleteDBentry( pGlobal, pGlobal->sProject, sCalKitName, eDB_CALKIT ) == 0 ) {
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

	g_free( sCalKitName );
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
