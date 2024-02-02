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

/*!     \brief  Write the S2P or S1P file
 *
 * Determine the filename to use for the SXP data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal	pointer to data
 */
void
BtnSXP (GtkButton *wButton, tGlobal *pGlobal, gboolean S2PnotS1P )
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    gchar *sFilename = NULL;
    GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new (
	        S2PnotS1P ? "Acquire S-paramater data and save to S2P file"
	                : "Acquire S-paramater data and save to S1P file",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Save", GTK_RESPONSE_ACCEPT,
					NULL);
	chooser = GTK_FILE_CHOOSER (dialog);
	filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, S2PnotS1P ? ".s2p" : ".s1p" );
    gtk_file_filter_add_pattern (filter, S2PnotS1P ? "*.[sS][2][pP]" :  "*.[sS][1][pP]");
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
		sFilename = g_date_time_format( now,
		        S2PnotS1P ? "HP8753.%d%b%y.%H%M%S.s2p" : "HP8753.%d%b%y.%H%M%S.s1p");
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
		extPos = g_strrstr( sFilename->str, S2PnotS1P ? ".s2p" : ".s1p" );
		if( !extPos )
			g_string_append( sFilename, S2PnotS1P ? ".s2p" : ".s1p" );

		g_free( lastFilename );
		lastFilename = g_strdup(sFilename->str);

		sensitiseControlsInUse( pGlobal, FALSE );
#if GLIB_CHECK_VERSION(2,76,0)
		postDataToGPIBThread (
		        S2PnotS1P ? TG_MEASURE_and_RETRIEVE_S2P_from_HP8753 : TG_MEASURE_and_RETRIEVE_S1P_from_HP8753,
		                g_string_free_and_steal (sFilename));
#else
		postDataToGPIBThread (
		        S2PnotS1P ? TG_MEASURE_and_RETRIEVE_S2P_from_HP8753 : TG_MEASURE_and_RETRIEVE_S1P_from_HP8753,
		                g_string_free (sFilename, FALSE));
#endif
		// sFilename->str is freed by thread
	}

	g_free( sFilename );
	gtk_widget_destroy (dialog);
}

/*!     \brief  Write the S2P file
 *
 * Determine the filename to use for the S2P data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal  pointer to data
 */
void
CB_BtnS2P (GtkButton *wButton, tGlobal *pGlobal)
{
    BtnSXP (wButton, pGlobal, TRUE);
}

/*!     \brief  Write the S1P file
 *
 * Determine the filename to use for the S1P data file and
 * send a message to the HP8753 comms thread to make the measurements.
 * On completion, a message to the main thread will write the data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal  pointer to data
 */
void
CB_BtnS1P (GtkButton *wButton, tGlobal *pGlobal)
{
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
		GString *strFilename = g_string_new( sChosenFilename );
		g_free( sChosenFilename );
		extPos = g_strrstr( strFilename->str, ".csv" );
		if( !extPos )
			g_string_append( strFilename, ".csv");

		g_free( lastFilename );
		lastFilename = g_strdup( strFilename->str );

		// todo ... save CSV data
		if( (fCSV = fopen( strFilename->str, "w" )) == NULL ) {
			gchar *sError = g_strdup_printf( "Cannot write: %s", (gchar *)strFilename->str);
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
			postInfo( "CSV saved" );
		}

		postInfo( "Traces saved to csv file");
		g_string_free (strFilename, TRUE);
	}

	g_free( sFilename );
	g_free( sSuggestedFilename );

	gtk_widget_destroy (dialog);
}

