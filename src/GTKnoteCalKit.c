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

// Blast ... the combobox is deprecated without a suitable replacement
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/*!     \brief  Callback / Cal Kit page / change calibration kit selection of GtkComboBoxText
 *
 * Callback (NCK 1) when the calibration kit GtkComboBoxText on the Cal Kit notebook page is changed
 *
 * \param  wCalKitProfile      pointer to GtkComboBox widget
 * \param  udata	           unused
 */
void
CB_nbCalKit_cbt_CalKitSelection (GtkComboBoxText *wCalKitProfile, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCalKitProfile ), "data");

	gint n = gtk_combo_box_get_active( GTK_COMBO_BOX( wCalKitProfile ));

	if( n != INVALID ) {
		tCalibrationKitIdentifier *pCalKitIentifier = (tCalibrationKitIdentifier *)g_list_nth( pGlobal->pCalKitList, n )->data;
		gtk_label_set_label( GTK_LABEL( pGlobal->widgets[ eW_nbCalKit_lbl_Desc ] ), pCalKitIentifier->sDescription );
	}
}

//  Suggest to open the same file as last time
static gchar *lastXKTfilename = NULL;
/*!     \brief  Callback when opening .xkt file from system file selection dialog
 *
 * Callback when opening file from system file selection dialog
 *
 * \param  source_object     GtkFileDialog object
 * \param  res               result of opening file for write
 * \param  gpGlobal          pointer to global data
 */
static void
CB_fdlg_XKTfileOpen( GObject *source_object, GAsyncResult *res, gpointer gpGlobal )
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GError *err = NULL;
    GtkAlertDialog *alert_dialog;

    GtkComboBoxText *wComboBoxCalKit = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] );

    g_autoptr (GFile) file = gtk_file_dialog_open_finish (dialog, res, &err);

    if ( file != NULL ) {
        gchar *sChosenFilename = g_file_get_path( file );
        __attribute__((unused)) gchar  *selectedFileBasename =  g_file_get_basename( file );


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

            gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[eW_nbCalKit_btn_SendKit ] ), TRUE );
        } else {
            alert_dialog = gtk_alert_dialog_new ("Cannot parse this file:\n%s", sChosenFilename);
            gtk_alert_dialog_show (alert_dialog, NULL);
            g_object_unref (alert_dialog);
        }

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_free( lastXKTfilename );
        lastXKTfilename = g_strdup( sChosenFilename );

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
    } else {
        // probably cancelled
        gtk_label_set_text( GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ] ),
                "XKT file selection cancelled");
        g_clear_error (&err);
    }
}

/*!     \brief  Callback / Cal Kit page / "Read XKT" calibration kit GtkButton
 *
 * Callback (NCK 2) when the "Read XKT" calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wReadXKT     pointer to send button widget
 * \param  udate        unused
 */
void
CB_nbCalKit_btn_ImportXKT ( GtkButton* wbtnImportXKT, gpointer user_data )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wbtnImportXKT ), "data");

    GtkFileDialog *fileDialogRecall = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wbtnImportXKT), GTK_TYPE_WINDOW);
    GDateTime *now = g_date_time_new_now_local ();

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_autoptr (GtkFileFilter) filter = NULL;
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*.[Xx][Kk][Tt]");
    gtk_file_filter_set_name (filter, "XKT");
    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);

    gtk_file_dialog_set_filters (fileDialogRecall, G_LIST_MODEL (filters));

    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, lastXKTfilename, NULL );
    gtk_file_dialog_set_initial_file( fileDialogRecall, fPath );

    gtk_file_dialog_open ( fileDialogRecall, GTK_WINDOW (win), NULL, CB_fdlg_XKTfileOpen, pGlobal);

    g_object_unref (fileDialogRecall);
    g_object_unref( fPath );
    g_date_time_unref( now );
}

/*!     \brief  Callback when deleting .calibration kit from system file selection dialog
 *
 * Callback when deleting .calibration kit from system file selection dialog
 *
 * \param  source_object     GtkFileDialog object
 * \param  res               result of opening file for write
 * \param  gpGlobal          pointer to global data
 * */
void CB_fdlg_deleteCalKit_choice (GObject *source_object, GAsyncResult *res, gpointer gpGlobal)
{
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG (source_object);
    GError *err = NULL;
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    gint n;
    GtkComboBoxText *wCalKitCombo;
    gchar *sCalKitName = NULL;
    gboolean bFound = FALSE;
    GtkTreeIter iter;
    gchar *string;

    int button = gtk_alert_dialog_choose_finish (dialog, res, &err);

    if (err) {
        gtk_label_set_text( GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ] ),
                      "Error from dialog");
        return;
    }

    wCalKitCombo = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] );
    sCalKitName = gtk_combo_box_text_get_active_text( wCalKitCombo );

    if( button == 1 && deleteDBentry( pGlobal, pGlobal->sProject, sCalKitName, eDB_CALKIT ) == 0 ) {
        // look through all the combobox labels to see if the selected text matches.
        GtkTreeModel *tm = gtk_combo_box_get_model(GTK_COMBO_BOX(wCalKitCombo));

        n = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(wCalKitCombo)), NULL);
        if (n > 0) {
            gtk_tree_model_get_iter_first(tm, &iter);
            for (gint pos = 0; !bFound && pos < n; pos++, gtk_tree_model_iter_next(tm, &iter)) {
                gtk_tree_model_get(tm, &iter, 0, &string, -1);
                if (g_strcmp0( g_markup_escape_text ( sCalKitName, -1 ), string) == 0) {
                    gtk_combo_box_text_remove(wCalKitCombo, pos);
                    bFound = TRUE;
                }
            }
        }
        if (bFound) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(wCalKitCombo), 0);
        }
    }
    g_free( sCalKitName );
}

/*!     \brief  Callback / Cal Kit page / delete calibration kit GtkButton
 *
 * Callback (NCK 3) when the delete calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wDeleteButton    pointer to send button widget
 * \param  udata	        unused
 */
void
CB_nbCalKit_btn_DeleteCalKit (GtkButton *wDeleteButton, gpointer udata)
{
	GtkComboBoxText *wCombo;
	gchar *name = NULL;
	GtkAlertDialog *dialog;
	gchar *sQuestion = NULL;
	gchar *sCalKitName = NULL;

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wDeleteButton ), "data");

	wCombo = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] );
	sCalKitName = gtk_combo_box_text_get_active_text( wCombo );
	name = g_markup_escape_text ( sCalKitName, -1 );
	sQuestion = g_strdup_printf(
			"You look as though you know what you are doing but..."
			"\n\t\t...are you sure you want to delete the:\n\n"
			"\t\"%s\"\n\n⚖️ calibration kit?", name);

	if ( name != NULL && strlen( name ) != 0 ) {
	    dialog = gtk_alert_dialog_new ("Caution");
	    const char* buttons[] = {"Cancel", "Proceed", NULL};
	    gtk_alert_dialog_set_detail(dialog, sQuestion);
	    gtk_alert_dialog_set_buttons (dialog, buttons);
	    gtk_alert_dialog_set_cancel_button (dialog, 0);   // (ESC) equivalent to button 0 (Cancel)
	    gtk_alert_dialog_set_default_button (dialog, 1);  // (Enter) equivalent to button 1 (Proceed)
	    gtk_window_present (GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]));
	    gtk_alert_dialog_choose (dialog, GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]), NULL, CB_fdlg_deleteCalKit_choice, (gpointer) pGlobal);
	} else {
		gtk_label_set_text( GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ] ),
				"No calibration kit selected");
	}
	g_free( sCalKitName );
	g_free( name );
	g_free( sQuestion );
}

/*!     \brief  Callback / Cal Kit page / send calibration kit GtkButton
 *
 * Callback (NCK 4) when the send calibration kit GtkButton on the Cal Kit notebook page is pressed
 *
 * \param  wButton      pointer to send button widget
 * \param  udata	    unused
 */
void
CB_nbCalKit_btn_SendCalKit(GtkButton *wSendButton, gpointer udata)
{
	gint index;
	gchar *sLabel;
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wSendButton ), "data");
	GtkComboBoxText *wComboBoxCalKit = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_nbCalKit_btn_SendKit ] );

	if( pGlobal->pCalKitList &&
	        (index = gtk_combo_box_get_active ( GTK_COMBO_BOX( wComboBoxCalKit ))) != -1 ) {
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
 * Callback (NCK 5) when the "+ user kit" GtkCheckButton on the Cal Kit notebook page is changed
 *
 * \param  wChkButton	pointer to check button widget
 * \param  udata	    unused
 */
void
CB_nbCalKit_cbtn_SaveCalKit(GtkCheckButton *wSaveUserKit, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wSaveUserKit ), "data");
	pGlobal->flags.bSaveUserKit = gtk_check_button_get_active( GTK_CHECK_BUTTON( wSaveUserKit ) );
}


/*!     \brief  Initialize the widgets on the Calibration Kit notebook page
 *
 * Initialize the widgets on the Calibration Kit notebook page page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializeNotebookPageCalKit( tGlobal *pGlobal, tInitFn purpose )
{
    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        // Populate the combobox drop down with the calibration kit names
        gtk_combo_box_text_remove_all( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] );

        for( GList *item = g_list_first( pGlobal->pCalKitList ); item != NULL ; item = item->next ) {
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] ),
                    ((tCalibrationKitIdentifier *)item->data)->sLabel );
        }

        if( g_list_length( pGlobal->pCalKitList ) > 0 ) {
            gtk_combo_box_set_active ( GTK_COMBO_BOX( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ] ), 0 );
            tCalibrationKitIdentifier *pCalKitIentifier = (tCalibrationKitIdentifier *)g_list_nth( pGlobal->pCalKitList, 0 )->data;
            // We are using the CSS to detect whether we have a dark or light desktop
            // see the code in hp8753c on_activate
            // change color based on dark or light theme
            gtk_label_set_label( GTK_LABEL( pGlobal->widgets[ eW_nbCalKit_lbl_Desc ] ),
                    pCalKitIentifier->sDescription );
            gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_nbCalKit_btn_DeleteKit ] ), TRUE);
        } else {
            gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_nbCalKit_btn_DeleteKit ] ), FALSE);
        }

        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbCalKit_cbtn_SaveUserKit] ), pGlobal->flags.bSaveUserKit );
    }



    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        // NCK 1 - This is called if either the Cal Kit combobox is changed or the entry is changed
        g_signal_connect ( pGlobal->widgets[ eW_nbCalKit_cbt_Kit ], "changed", G_CALLBACK (CB_nbCalKit_cbt_CalKitSelection), NULL );

        // NCK 2 - connect callback for Import XKT button
        g_signal_connect ( pGlobal->widgets[ eW_nbCalKit_btn_ImportXKT ], "clicked", G_CALLBACK (CB_nbCalKit_btn_ImportXKT), NULL );

        // NCK 3 - connect callback for Import XKT button
        g_signal_connect ( pGlobal->widgets[ eW_nbCalKit_btn_DeleteKit ], "clicked", G_CALLBACK (CB_nbCalKit_btn_DeleteCalKit), NULL );

        // NCK4 - connect signal for Send calibration kit button callback
        g_signal_connect ( pGlobal->widgets[ eW_nbCalKit_btn_SendKit ], "clicked", G_CALLBACK (CB_nbCalKit_btn_SendCalKit), NULL );

        // NCK 5 - connect signal for callback on 'Save User Calibration Kit' checkbox
        g_signal_connect ( pGlobal->widgets[ eW_nbCalKit_cbtn_SaveUserKit ], "toggled", G_CALLBACK (CB_nbCalKit_cbtn_SaveCalKit), NULL );
    }
}

#pragma GCC diagnostic pop
