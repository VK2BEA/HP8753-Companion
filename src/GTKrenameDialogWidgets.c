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
#include "hp8753.h"

/*!     \brief  Sensitise the OK button if data validated
 *
 * Sensitize the OK button on the rename/move/copy dialog box
 * if the operation is likely to succeed.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param pGlobal     pointer to global data
 */
void
SensitizeDR_OKbtn( tGlobal *pGlobal ) {
    GtkWidget *wOK = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_BtnOK");
    const gchar *sTargetName = gtk_entry_buffer_get_text( gtk_entry_get_buffer(
            GTK_ENTRY(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_Entry_To"))) );
    GtkWidget *wDRprojectCombo = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_ComboProject" );

    gchar *sTargetProject = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(wDRprojectCombo));
    gchar *sTargetNameSanitized = g_strchomp(g_strdup( sTargetName ));
    gchar *sTargetProjectSanitized = g_strchomp( sTargetProject );  // does not reallocate .. inplace chomp

    gboolean bSensitive = TRUE;
    tProjectAndName projectAndName = {pGlobal->sProject, 0};

    switch( pGlobal->RMCdialogTarget ) {
    case eProjectName:
        // A project cannot be renamed to an existing project
        // move and copy don't apply to project
        if( g_list_find_custom (pGlobal->pProjectList, sTargetName, (GCompareFunc) strcmp ) != NULL
                || strlen(sTargetNameSanitized) == 0)
            bSensitive = FALSE;
        break;
    case eCalibrationName:
        if( pGlobal->RMCdialogPurpose == eRename ) {
            // Cannot rename to a cal profile that already exists
            projectAndName.sName = (gchar *)sTargetName;
            if( g_list_find_custom( pGlobal->pCalList, &projectAndName, (GCompareFunc)compareCalItemsForFind ) != NULL
                    || strlen(sTargetNameSanitized) == 0 )
                            bSensitive = FALSE;
        } else {
            // Cannot move or copy to a project where the cal profile exists
            projectAndName.sProject = sTargetProject;
            projectAndName.sName = pGlobal->pCalibrationAbstract->projectAndName.sName;
            if( g_list_find_custom( pGlobal->pCalList, &projectAndName, (GCompareFunc)compareCalItemsForFind ) != NULL
                    || strlen( sTargetProjectSanitized ) == 0 )
                bSensitive = FALSE;
            else
                bSensitive = TRUE;
        }
        break;
    case eTraceName:
        if( pGlobal->RMCdialogPurpose == eRename ) {
            // Cannot rename to a trace profile that already exists
            projectAndName.sName = (gchar *)sTargetName;
            if( g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareTraceItemsForFind ) != NULL
                    || strlen(sTargetNameSanitized) == 0 )
                            bSensitive = FALSE;
        } else {
            // Cannot move or copy to a project where the cal profile exists
            projectAndName.sProject = sTargetProject;
            projectAndName.sName = pGlobal->pTraceAbstract->projectAndName.sName;
            if( g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareTraceItemsForFind ) != NULL
                    || strlen( sTargetProjectSanitized ) == 0 )
                bSensitive = FALSE;
            else
                bSensitive = TRUE;
        }
        break;
    default:
        break;
    }

    gtk_widget_set_sensitive( wOK, bSensitive );
    g_free( sTargetProject );
    g_free( sTargetNameSanitized );
}

/*!     \brief  Callback when user types in the 'to' edit box
 *
 * When the user types in the 'to' box of the rename/move/copy dialog box
 * we check if the operation is still valid. We sensitize or unsensitize the
 * OK button depending upon the validity of the operation with the new name.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param wEditable     GtkEditable widget from the GtkEdit widget
 * \param pGlobal       pointer to global data
 */
void
CB_DR_ToNameChanged (GtkEditable* wEditable, tGlobal *pGlobal)
{
    SensitizeDR_OKbtn( pGlobal );
}

/*!     \brief  Add to the project list (and combobox) if the project not already there
 *
 * If we move or copy a trace or calibration profile to a new project
 * we need to ensure that the project list is updated
 *
 * \ingroup Rename dialog widget callback
 *
 * \param possiblyNewProject        name of project
 * \param pGlobal                   pointer to global data
 */
static gboolean
keepProjectListUpdated( const gchar *possiblyNewProject, tGlobal *pGlobal ) {
    // see if this is a new project
    if( g_list_find_custom (pGlobal->pProjectList, possiblyNewProject, (GCompareFunc) strcmp ) == NULL ) {
        // This is a new project ... add it to the list
        pGlobal->pProjectList = g_list_prepend(pGlobal->pProjectList, g_strdup( possiblyNewProject ));
        pGlobal->pProjectList = g_list_sort (pGlobal->pProjectList, (GCompareFunc)g_strcmp0);
        PopulateProjectComboBoxWidget( pGlobal );
        return TRUE;
    }
    return FALSE;
}

/*!     \brief  Callback user presses the OK or Cancel buttons
 *
 * The rename/move/copy dialog concludes when the user presses the OK or Cancel
 * buttons. We take the requested action based on the options selected and the data
 * entered.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param wDialog       pointer to the rename/move/copy dialog widget
 * \param response      code returned from the OK or Cancel buttons
 * \param pGlobal       pointer to global data
 */
void
CB_DR_RenameResponse( GtkDialog *wDialog, int response, tGlobal *pGlobal ) {
    GtkEntry *wEntryTo = GTK_ENTRY(
            g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_Entry_To" ));
    GtkComboBoxText *wDRprojectCombo = GTK_COMBO_BOX_TEXT(
            g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_ComboProject" ));
    GtkComboBoxText *wComboBox;

    const gchar *sTo = gtk_entry_get_text( wEntryTo );
    gchar *sProjectTo = gtk_combo_box_text_get_active_text(wDRprojectCombo);
    gchar *sFrom = 0;
    GList *l;

    switch ( response ) {
    case GTK_RESPONSE_OK:
        switch( pGlobal->RMCdialogTarget ) {
        case eProjectName:
            if( pGlobal->RMCdialogPurpose  != eRename )
                break;  // something unexpected .. we can only rename a project
            sFrom = pGlobal->sProject;
            pGlobal->sProject = g_strdup( sTo );
            // rename the project in the database (cal and trace tables)
            if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                    NULL, sFrom, (gchar *)sTo ) == ERROR )
                break;
            // change the project name in the list
            for( l = pGlobal->pProjectList; l != NULL; l = l->next ){
                if( g_strcmp0( sFrom, (gchar *)l->data  )  == 0 ){
                    g_free( l->data );
                    l->data = g_strdup( sTo );
                }
            }

            // update the cal/setup list
            for( l = pGlobal->pCalList; l != NULL; l = l->next ){
                tProjectAndName *pProjectAndName = &(((tHP8753cal *)(l->data))->projectAndName);
                if( g_strcmp0( sFrom, pProjectAndName->sProject  )  == 0 ){
                    g_free( pProjectAndName->sProject );
                    pProjectAndName->sProject = g_strdup( sTo );
                }
            }

            // update the trace list
            for( l = pGlobal->pTraceList; l != NULL; l = l->next ){
                tProjectAndName *pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);
                if( g_strcmp0( sFrom, pProjectAndName->sProject  )  == 0 ){
                    g_free( pProjectAndName->sProject );
                    pProjectAndName->sProject = g_strdup( sTo );
                }
            }

            // update the combobox widget

            PopulateProjectComboBoxWidget( pGlobal );
            // Block signals while we populate the entry widget programatically
            wComboBox = GTK_COMBO_BOX_TEXT(
                        g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_Project") );
            g_signal_handlers_block_by_func(G_OBJECT(wComboBox), CB_EditableProjectName, pGlobal);
            gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(wComboBox) )), sTo);
            g_signal_handlers_unblock_by_func(G_OBJECT(wComboBox), CB_EditableProjectName, pGlobal);

            // free the string memory that we took
            g_free( sFrom );
            break;
        case eCalibrationName:
            wComboBox = GTK_COMBO_BOX_TEXT(
                        g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
            switch ( pGlobal->RMCdialogPurpose ) {
            case eRename:
                sFrom = pGlobal->pCalibrationAbstract->projectAndName.sName;
                // rename the project in the database (cal table)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->sProject, sFrom, (gchar *)sTo ) == ERROR )
                    break;
                    // Update the name in the list of calibration/setup profiles
                for( l = pGlobal->pCalList; l != NULL; l = l->next ){
                    tProjectAndName *pProjectAndName = &(((tHP8753cal *)(l->data))->projectAndName);
                    if( g_strcmp0( pGlobal->sProject, pProjectAndName->sProject  )  == 0
                            && g_strcmp0( sFrom, pProjectAndName->sName  )  == 0  ){
                        g_free( pProjectAndName->sName );
                        pProjectAndName->sName = g_strdup( sTo );
                    }
                }

                // Find the calibration in the list
                tProjectAndName projectAndName = { pGlobal->sProject, (gchar *)sTo };
                pGlobal->pCalibrationAbstract =
                        g_list_find_custom( pGlobal->pCalList, &projectAndName, (GCompareFunc)compareCalItemsForFind )->data;

                // update the combobox widget
                PopulateCalComboBoxWidget( pGlobal );
                // Block signals while we populate the entry widget programatically

                g_signal_handlers_block_by_func(G_OBJECT(wComboBox), CB_EditableCalibrationProfileName, pGlobal);
                gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(wComboBox) )), sTo);
                g_signal_handlers_unblock_by_func(G_OBJECT(wComboBox), CB_EditableCalibrationProfileName, pGlobal);
                g_free( sFrom );
                break;
            case eMove:
                sFrom = pGlobal->sProject;
                keepProjectListUpdated( sProjectTo, pGlobal );
                // move the calibration profile in the database (cal table)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->pCalibrationAbstract->projectAndName.sName, sFrom, sProjectTo ) == ERROR )
                    break;
                g_free(pGlobal->pCalibrationAbstract->projectAndName.sProject);
                pGlobal->pCalibrationAbstract->projectAndName.sProject = g_strdup(sProjectTo);
                // now resort because the project has changed
                pGlobal->pCalList = g_list_sort (pGlobal->pCalList, (GCompareFunc)compareCalItemsForSort);

                // also update the calibration pointer to the first profile in the list
                // that matches the project
                pGlobal->pCalibrationAbstract=selectFirstCalibrationProfileInProject( pGlobal );
                // populate the combobox and set the selected profile (0)
                PopulateCalComboBoxWidget( pGlobal );

                break;
            case eCopy:
                sFrom = pGlobal->sProject;
                keepProjectListUpdated( sProjectTo, pGlobal );
                // copy the calibration profile in the database (cal table)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->pCalibrationAbstract->projectAndName.sName, sFrom, sProjectTo ) == ERROR )
                    break;
                tHP8753cal *pCal = cloneCalibrationProfile( pGlobal->pCalibrationAbstract, sProjectTo );
                pGlobal->pCalList = g_list_prepend(pGlobal->pCalList, pCal);
                pGlobal->pCalList = g_list_sort (pGlobal->pCalList, (GCompareFunc)compareCalItemsForSort);
                break;
            }
        break;
        case eTraceName:
            wComboBox = GTK_COMBO_BOX_TEXT(
                        g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );
            switch ( pGlobal->RMCdialogPurpose ) {
            case eRename:
                sFrom = pGlobal->pTraceAbstract->projectAndName.sName;

                // rename the trace profile in the database (trace tables)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->sProject, sFrom, (gchar *)sTo ) == ERROR )
                    break;

                // Update the name in the list of trace profiles
                for( l = pGlobal->pTraceList; l != NULL; l = l->next ){
                    tProjectAndName *pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);
                    if( g_strcmp0( pGlobal->sProject, pProjectAndName->sProject  )  == 0
                            && g_strcmp0( sFrom, pProjectAndName->sName  )  == 0  ){
                        g_free( pProjectAndName->sName );
                        pProjectAndName->sName = g_strdup( sTo );
                    }
                }
                pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);

                // Find the trace in the list
                tProjectAndName projectAndName = { pGlobal->sProject, (gchar *)sTo };
                pGlobal->pTraceAbstract =
                        g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareCalItemsForFind )->data;
                // Update the widget
                PopulateTraceComboBoxWidget( pGlobal );

                // Block signals while we populate the entry widget programatically
                g_signal_handlers_block_by_func(G_OBJECT(wComboBox), CB_EditableTraceProfileName, pGlobal);
                gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(wComboBox) )), sTo);
                g_signal_handlers_unblock_by_func(G_OBJECT(wComboBox), CB_EditableTraceProfileName, pGlobal);
                g_free( sFrom );
                break;
            case eMove:
                sFrom = pGlobal->sProject;
                // see if this is a new project
                keepProjectListUpdated( sProjectTo, pGlobal );
                // move the calibration profile in the database (cal table)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->pTraceAbstract->projectAndName.sName, sFrom, sProjectTo ) == ERROR )
                    break;

                g_free(pGlobal->pTraceAbstract->projectAndName.sProject);
                pGlobal->pTraceAbstract->projectAndName.sProject = g_strdup(sProjectTo);
                // now resort because the project has changed
                pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);

                // Update the trace abstract pointer to the first profile in the list
                // that matches the project
                pGlobal->pTraceAbstract=selectFirstTraceProfileInProject( pGlobal );
                // populate the combobox and set the selected profile (0)
                PopulateTraceComboBoxWidget( pGlobal );

                break;
            case eCopy:
                sFrom = pGlobal->sProject;
                // see if this is a new project
                keepProjectListUpdated( sProjectTo, pGlobal );
                // copy the calibration profile in the database (cal table)
                if( RenameMoveCopyDBitems(pGlobal, pGlobal->RMCdialogTarget, pGlobal->RMCdialogPurpose,
                        pGlobal->pTraceAbstract->projectAndName.sName, sFrom, sProjectTo ) == ERROR )
                    break;

                tHP8753traceAbstract *pCal = cloneTraceProfileAbstract( pGlobal->pTraceAbstract, sProjectTo );
                pGlobal->pTraceList = g_list_prepend(pGlobal->pTraceList, pCal);
                pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);

                break;
            }
            break;
        default:
            break;
        }
        break;
    case GTK_RESPONSE_CANCEL:
        break;
    default:
        break;
    }
    g_free( sProjectTo );
}


#define PROGRAM_RADIO   2
#define CAL_RADIO       1
#define TRACE_RADIO     0

#define RENAME_RADIO    2
#define MOVE_RADIO      1
#define COPY_RADIO      0

/*!     \brief  Change the label in the dialog widget based on the action selected
 *
 * Change the 'from' label to better describe the action to be performed.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param pGlobal       pointer to global data
 */
static void
setFromLabel( tGlobal *pGlobal ) {
    gchar *sPurpose[] = {"Rename", "Move", "Copy"};
    gchar *sTarget[] = {"project", "calibration", "trace(s)"};
    gchar *sName = "";
    gchar *sFrom = "from";

    if( pGlobal->RMCdialogPurpose != eRename ) {
        if( pGlobal->RMCdialogTarget == eCalibrationName )
            sName = pGlobal->pCalibrationAbstract->projectAndName.sName;
        else
            sName = pGlobal->pTraceAbstract->projectAndName.sName;
        sFrom = " from";   // add leading space to separate name
    }
    GtkWidget *wLblFrom = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_Lbl_From" );
    sName = g_markup_escape_text( sName, -1 );  // this allocates a string that must be freed
    gchar *sFromLabel = g_strdup_printf( "%s %s <span style='italic' weight='bold'>%s</span>%s",
            sPurpose[pGlobal->RMCdialogPurpose], sTarget[pGlobal->RMCdialogTarget], sName, sFrom );
    gtk_label_set_label( GTK_LABEL( wLblFrom ), sFromLabel );
    g_free( sFromLabel );
    g_free( sName );
}

/*!     \brief  Change the 'from' entry box text to show where or what is being renamed/moved/copied
 *
 * Change the 'from' GtkEntry to show the project or profile that is being renamed or
 * the project from which the profile is sourced for move or copy.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param pGlobal       pointer to global data
 */
static void
setFromName( tGlobal *pGlobal ) {
    GtkWidget *wEntryFrom = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_Edit_From" );

    gchar *sFromName = 0;

    if( pGlobal->RMCdialogPurpose != eRename ) {
        sFromName = pGlobal->sProject;
    } else {
        switch( pGlobal->RMCdialogTarget ) {
        case eProjectName:
            sFromName = pGlobal->sProject;
            break;
        case eCalibrationName:
            sFromName = pGlobal->pCalibrationAbstract->projectAndName.sName;
            break;
        case eTraceName:
            sFromName = pGlobal->pTraceAbstract->projectAndName.sName;
            break;
        }
    }

    gtk_entry_buffer_set_text ( gtk_entry_get_buffer( GTK_ENTRY(wEntryFrom)), sFromName, -1);
}

/*!     \brief  Callback from radio buttons for rename, move and copy
 *
 * Callback when rename, move and copy buttons changed.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param purposeButton widget for button that has changed
 * \param pGlobal       pointer to global data
 */
void
CB_DR_RadioPurpose(GtkToggleButton *purposeButton, tGlobal *pGlobal) {

    // rename, move, copy

    GtkWidget *wDRprojectCombo = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_ComboProject" );
    GtkWidget *wDRtoEdit = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_Entry_To" );
    GtkWidget *wDRprojectToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioProject" );
    GtkWidget *wDRcalToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioCal" );
    GtkWidget *wDRtraceToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioTrace" );

    if ( ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (purposeButton)) )
        return;

    // The radio buttons are 2 - rename, 1 - move, 0 - copy
    // we correct to save
    pGlobal->RMCdialogPurpose = 2 - g_slist_index (gtk_radio_button_get_group (GTK_RADIO_BUTTON (purposeButton)),
                                                (gconstpointer)purposeButton);
    setFromLabel( pGlobal );
    setFromName( pGlobal );
    SensitizeDR_OKbtn( pGlobal );

    switch( pGlobal->RMCdialogPurpose ) {
    case eMove:
    case eCopy:
    default:
        // show the drop down and disable the editbox
        gtk_widget_set_visible( wDRprojectCombo, TRUE );
        gtk_widget_set_visible( wDRtoEdit, FALSE );

        // We cannot move or copy a project..
        // If we choose move or copy and project is also selected, change project to cal
        // unless it has been disabled because there are no entries
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wDRprojectToggleBtn ) ) ) {
            if ( gtk_widget_get_sensitive( GTK_WIDGET( wDRcalToggleBtn ) ) )
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( wDRcalToggleBtn ), TRUE );
            else
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( wDRtraceToggleBtn ), TRUE );
        }
        gtk_widget_set_sensitive( wDRprojectToggleBtn, FALSE );
        break;

    case eRename:
        gtk_widget_set_visible( wDRprojectCombo, FALSE );
        gtk_widget_set_visible( wDRtoEdit, TRUE );
        gtk_widget_set_sensitive( wDRprojectToggleBtn, TRUE );
        break;
    }
}

/*!     \brief  Callback from radio buttons for project, calibration profile and trace profile
 *
 * Callback when project, calibration and trace buttons changed.
 *
 * \ingroup Rename dialog widget callback
 *
 * \param targetButton  widget for button that has changed
 * \param pGlobal       pointer to global data
 */
void
CB_DR_RadioTarget(GtkToggleButton *targetButton, tGlobal *pGlobal) {

    if ( ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (targetButton)) )
        return;

    pGlobal->RMCdialogTarget = 2 - g_slist_index (gtk_radio_button_get_group (GTK_RADIO_BUTTON (targetButton)),
                                        (gconstpointer)targetButton);

    setFromLabel( pGlobal );
    setFromName( pGlobal );
    SensitizeDR_OKbtn( pGlobal );
}

/*!     \brief  Show the rename / move / copy dialog box
 *
 * Show the rename / move / copy dialog box.
 * This is initiated by pressing F2
 *
 * \ingroup Rename dialog widget callback
 *
 * \param pGlobal       pointer to global data
 */
void
ShowRenameMoveCopyDialog( tGlobal *pGlobal ) {
    int i, currentProjectIndex=-1;
    GList *l;

    GtkDialog *wDlgRename = GTK_DIALOG( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Dlg_Rename") );
    GtkComboBox *wCalCombo = GTK_COMBO_BOX(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile"));

    GtkComboBox *wTraceCombo = GTK_COMBO_BOX(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile"));

    GtkWidget *wDRmoveToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioMove" );
    GtkWidget *wDRcopyToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioCopy" );
    GtkWidget *wDRcalToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioCal" );
    GtkWidget *wDRtraceToggleBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_RadioTrace" );

    // Certain selections are disabled if there are no options (e.g. move cal profile if there are none)

    // We cannot move or copy a cal profile if there are none
    // We must have at least one cal or trace profile, so if there are no
    // cal profiles then ther must be at least one trace profile

    if( gtk_combo_box_get_active( GTK_COMBO_BOX( wCalCombo ) ) == -1  ) {
        gtk_widget_set_sensitive( wDRcalToggleBtn, FALSE );
        if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wDRcalToggleBtn ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( wDRtraceToggleBtn ), TRUE );
    } else {
        gtk_widget_set_sensitive( wDRcalToggleBtn, TRUE );
    }

    if( gtk_combo_box_get_active( GTK_COMBO_BOX( wTraceCombo ) ) == -1  ) {
        gtk_widget_set_sensitive( wDRtraceToggleBtn, FALSE );
        if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( wDRtraceToggleBtn ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( wDRcalToggleBtn ), TRUE );
    } else {
        gtk_widget_set_sensitive( wDRtraceToggleBtn, TRUE );
    }


    // This sets the "From" entry box in the dialog
    // i.e. we rename from the current project so display ne name of that project in the from box
    setFromName( pGlobal );

    // Fill in the project combobox in the
    // rename/move/copy dialog box (used when moving or copying cal or trace)
    GtkComboBoxText *wComboBoxProject
        = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DR_ComboProject") );
    gchar *currentTargetProject = g_strdup( gtk_combo_box_text_get_active_text( wComboBoxProject ) );
    gtk_combo_box_text_remove_all( wComboBoxProject );
    for( i=0, l = pGlobal->pProjectList; l != NULL; l = l->next ){
        // we don't move to the current project..so skip that one
        if( g_strcmp0( (gchar *)l->data, pGlobal->sProject ) != 0 ) {
            gtk_combo_box_text_append_text( wComboBoxProject, (gchar *)l->data );

            if( g_strcmp0(currentTargetProject, (gchar *)l->data) == 0 )
                currentProjectIndex = i;
            i++;
        }
    }

    gtk_widget_set_sensitive( wDRmoveToggleBtn, TRUE );
    gtk_widget_set_sensitive( wDRcopyToggleBtn, TRUE );

    if( i > 0 )
        gtk_combo_box_set_active( GTK_COMBO_BOX( g_hash_table_lookup ( pGlobal->widgetHashTable,
                    (gconstpointer)"WID_DR_ComboProject")  ), currentProjectIndex <= 0 ? 0 : currentProjectIndex );

    g_free( currentTargetProject );


    setFromLabel( pGlobal );

    int renameResponse = gtk_dialog_run(GTK_DIALOG(wDlgRename));
    switch (renameResponse)
      {
        case GTK_RESPONSE_OK:
           // do_application_specific_something ();
           break;
        default:
           // do_nothing_since_dialog_was_cancelled ();
           break;
      }
       gtk_widget_hide (GTK_WIDGET( wDlgRename ));
}
