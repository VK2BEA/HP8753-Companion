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

/*!     \brief  Send signal to GPIB communication thread for S2P or S1P
 *
 * Once a file name is chosen for the SxP file to write, sent it with
 * the command to the GPIB communication thread to initiate the measurement and
 * saving of the file.
 *
 * \param  source_object  file dialog object
 * \param  res            result (cancel or save)
 * \param  gpGlobal       pointer to global data
 * \param  bS21notS12     which type of file do we want
 */
static void
SxP_initiate( GObject *source_object, GAsyncResult *res, gpointer gpGlobal, gboolean bS2PnotS1P )
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    static gchar *lastSXPfilename = NULL;

    GFile *file;
    GError *err = NULL;
    gchar *extPos = NULL;

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        GString *sFilename = g_string_new( sChosenFilename );

        extPos = g_strrstr( sFilename->str, bS2PnotS1P ? ".s2p" : ".s1p");
        if( !extPos )
            g_string_append( sFilename, bS2PnotS1P ? ".s2p" : ".s1p");

        g_free( lastSXPfilename );
        lastSXPfilename = g_strdup(sFilename->str);

        sensitiseControlsInUse( pGlobal, FALSE );

        postDataToGPIBThread (
                bS2PnotS1P ? TG_MEASURE_and_RETRIEVE_S2P_from_HP8753 : TG_MEASURE_and_RETRIEVE_S1P_from_HP8753,
                        g_string_free_and_steal (sFilename));
        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
    }
}

// Call back when file is selected
static void
CB_dialog_S2P( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    SxP_initiate( source_object, res, gpGlobal, TRUE );
}

// Call back when file is selected
static void
CB_dialog_S1P( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    SxP_initiate( source_object, res, gpGlobal, FALSE );
}

/*!     \brief  Write the S2P or S1P file
 *
 * Determine the filename to use for the SXP data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal  pointer to data
 */
void
BtnSXP (GtkButton *wButton, tGlobal *pGlobal, gboolean bS2PnotS1P )
{

    GtkFileDialog *fileDialogSave = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wButton), GTK_TYPE_WINDOW);

    gchar *pUserFileName = NULL;

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);

    g_autoptr (GtkFileFilter) filter = NULL;

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "text/plain");
    if( bS2PnotS1P ) {
        gtk_file_filter_add_pattern( filter, "*.[Ss]2[Pp]");
        gtk_file_filter_set_name (filter, "S2P");
    } else {
        gtk_file_filter_add_pattern( filter, "*.[Ss]1[Pp]");
        gtk_file_filter_set_name (filter, "S1P");
    }
    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);


    gtk_file_dialog_set_filters (fileDialogSave, G_LIST_MODEL (filters));

    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, pUserFileName, NULL );
    gtk_file_dialog_set_initial_file( fileDialogSave, fPath );

    gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL,
            bS2PnotS1P ? CB_dialog_S2P : CB_dialog_S1P, pGlobal);

}


/*!     \brief  Write the S2P file
 *
 * Determine the filename to use for the S2P data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  udata    unused
 */
void
CB_btn_S2P (GtkButton *wButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");
    BtnSXP (wButton, pGlobal, TRUE);
}

/*!     \brief  Write the S1P file
 *
 * Determine the filename to use for the S1P data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  udata    unused
 */
void
CB_btn_S1P (GtkButton *wButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");
    BtnSXP (wButton, pGlobal, FALSE);
}


/*!     \brief  Write the CSV header to the file
 *
 * The comma separated variabls (CSV) data file has a header lin
 * listing the data in each column.
 *
 * \param  file    	file pointer to the open, writable file
 * \param  sweepCh1	channel 1 sweep type
 * \param  sweepCh2	channel 2 sweep type
 * \param  fmtCh1	channel 1 format
 * \param  fmtCh2	channel 2 format
 * \param  measCh1	measurement channel 1 (S11, S21, SWR etc)
 * \param  measCh2	measurement channel 2
 * \param  bCoupled	boolean indicating if channels coupled
 * \param  bCoupled	boolean indicating if there is data for two channels
 *
 */
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

/*!     \brief  Write a CSV data point
 *
 * The comma separated variabls (CSV) data file has a header lin
 * listing the data in each column.
 *
 * \param  file    	file pointer to the open, writable file
 * \param  tFormat	type of data - (Smith/Polar) has real/imaginary pairs
 * \param  point	complex data point
 * \param  bLF  	add a line feed
 *
 */
void
writeCSVpoint( FILE *file, tFormat format, tComplex *point, gboolean bLF ) {
	switch( format ) {
	case eFMT_SMITH:
	case eFMT_POLAR:
		fprintf( file, ",%.16lg,%.16lg", point->r,  point->i );
		break;
	default:
		fprintf( file, ",%.16lg", point->r );
		break;
	}
	if( bLF )
		fprintf( file, "\n" );
}



static gchar *sCSVfileName = NULL;

/*!     \brief  Callback from CSV file selection dialog
 *
 * Once a file name is chosen for the CSV file to write,
 * write the CSV data to thje selected file.
 *
 * \param  source_object  file dialog object
 * \param  res            result (cancel or save)
 * \param  gpGlobal       pointer to global data
 */
static void
CB_dialog_CSV( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        GString *sFilename = g_string_new( sChosenFilename );

        FILE *fCSV = NULL;
        tFormat fmtCh1 = pGlobal->HP8753.channels[ eCH_ONE ].format,
                fmtCh2 = pGlobal->HP8753.channels[ eCH_TWO ].format;
        tSweepType sweepCh1 = pGlobal->HP8753.channels[ eCH_ONE ].sweepType,
                   sweepCh2 = pGlobal->HP8753.channels[ eCH_TWO ].sweepType;
        tMeasurement measCh1 = pGlobal->HP8753.channels[ eCH_ONE ].measurementType,
                     measCh2 = pGlobal->HP8753.channels[ eCH_TWO ].measurementType;

        g_free( sCSVfileName );
        sCSVfileName = g_strdup(sFilename->str);

        if( (fCSV = fopen( sChosenFilename, "w" )) == NULL ) {
            gchar *sError = g_strdup_printf( "Cannot write: %s", sChosenFilename);
            postError( sError );
            g_free( sError );
        } else {
            writeCSVheader( fCSV,  sweepCh1, sweepCh2, fmtCh1, fmtCh2, measCh1, measCh2,
                    pGlobal->HP8753.flags.bSourceCoupled, pGlobal->HP8753.flags.bDualChannel );
            if( pGlobal->HP8753.flags.bDualChannel ) {
                if( pGlobal->HP8753.flags.bSourceCoupled ) {
                    for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints; i++ ) {
                        fprintf( fCSV, "%.0lf",
                                pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
                        writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], FALSE );
                        writeCSVpoint( fCSV, fmtCh2, &pGlobal->HP8753.channels[ eCH_TWO ].responsePoints[i], TRUE );
                    }
                } else {
                    for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints
                                    || i < pGlobal->HP8753.channels[ eCH_TWO ].nPoints; i++ ) {
                        if( i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints ) {
                            fprintf( fCSV, "%.0lf",
                                    pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
                            writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], FALSE );
                        } else {
                            fprintf( fCSV, ",,,");
                        }
                        if( i < pGlobal->HP8753.channels[ eCH_TWO ].nPoints ) {
                            fprintf( fCSV, ",%.0lf",
                                    pGlobal->HP8753.channels[ eCH_TWO ].stimulusPoints[i] );
                            writeCSVpoint( fCSV, fmtCh2, &pGlobal->HP8753.channels[ eCH_TWO ].responsePoints[i], TRUE );
                        } else {
                            fprintf( fCSV, ",,\n");
                        }
                    }
                }
            } else {
                for( int i=0; i < pGlobal->HP8753.channels[ eCH_ONE ].nPoints; i++ ) {
                    fprintf( fCSV, "%.0lf",
                            pGlobal->HP8753.channels[ eCH_ONE ].stimulusPoints[i] );
                    writeCSVpoint( fCSV, fmtCh1, &pGlobal->HP8753.channels[ eCH_ONE ].responsePoints[i], TRUE );
                }
            }
            fclose( fCSV );
            postInfo( "Traces saved to csv file" );
        }

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
    }
}

/*!     \brief  Write the CSV file
 *
 * Determine the filename to use for the S2P data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  CSV button widget
 * \param  udata    unused
 */
void
CB_btn_SaveCSV (GtkButton *wButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");
    GDateTime *now = g_date_time_new_now_local ();

    if( sCSVfileName == NULL )
        sCSVfileName = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S.csv");

    if( !pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData ) {
        postError( "No trace data to export!" );
        return;
    }

    GtkFileDialog *fileDialogSave = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wButton), GTK_TYPE_WINDOW);

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);

    g_autoptr (GtkFileFilter) filter = NULL;

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "text/plain");

        gtk_file_filter_add_pattern( filter, "*.[Cc]1[Ss][Vv]");
        gtk_file_filter_set_name (filter, "CSV");

    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);


    gtk_file_dialog_set_filters (fileDialogSave, G_LIST_MODEL (filters));

    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, sCSVfileName, NULL );
    gtk_file_dialog_set_initial_file( fileDialogSave, fPath );

    gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_dialog_CSV, pGlobal);
}

/*!     \brief  Initialize the 'Data' page of the notebook widget
 *
 * Initialize the 'Data' page of the notebook widget
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeNotebookPageData( tGlobal *pGlobal, tInitFn purpose )
{

    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        // NDTA 1 - signal for callback of button to initiate S2P measurement and saving of data
        g_signal_connect ( pGlobal->widgets[ eW_nbData_btn_S2P ], "clicked", G_CALLBACK (CB_btn_S2P), NULL );
        // NDTA 1 - signal for callback of button to initiate S1P measurement and saving of data
        g_signal_connect ( pGlobal->widgets[ eW_nbData_btn_S1P ], "clicked", G_CALLBACK (CB_btn_S1P), NULL );
        // NDTA 1 - signal for callback of button to save CSV data
        g_signal_connect ( pGlobal->widgets[ eW_nbData_btn_CSV ], "clicked", G_CALLBACK (CB_btn_SaveCSV), NULL );
        // Only active when there is data
        gtk_widget_set_sensitive( pGlobal->widgets[ eW_nbData_btn_CSV ], FALSE );
    }
}
