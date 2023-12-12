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


/*!     \brief  Write the PNG image to a file
 *
 * Determine the filename to use for the PNG file and
 * write image(s) of plot using the already retrieved data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal	pointer to data
 */
#define PNG_WIDTH	3300
#define PNG_HEIGHT	2550
#define PNG_MARGIN  0.0
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
    gboolean bHPGL = (pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid);
	gboolean bBoth = pGlobal->HP8753.flags.bDualChannel
			&& pGlobal->HP8753.flags.bSplitChannels && !bHPGL;

	cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, PNG_WIDTH, PNG_HEIGHT);
	cr = cairo_create (cs);
    // clear the screen
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
	cairo_paint( cr );
	cairo_save( cr ); {
		plotA(PNG_WIDTH, PNG_HEIGHT, PNG_MARGIN, cr, pGlobal);
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
			plotB(PNG_WIDTH, PNG_HEIGHT, PNG_MARGIN, cr, pGlobal);

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


tPaperDimensions paperDimensions[ eNumPaperSizes ] = {
        {595,  842,  7.2},  // A4
        {612,  792,  7.2},  // Letter
        {842, 1190, 10.0},  // A3
        {792, 1224, 10.0}   // Legal
};
/*!     \brief  Write the PDF image to a file
 *
 * Determine the filename to use for the PNG file and
 * write image(s) of plot using the already retrieved data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal	pointer to data
 */
void
CB_BtnSavePDF (GtkButton * button, tGlobal *pGlobal)
{
	cairo_t *cr;
    cairo_surface_t *cs;

    GtkFileFilter *filter;
    GDateTime *now = g_date_time_new_now_local ();
    static gchar *lastFilename = NULL;
    gchar *sFilename = NULL;
    gchar *sSuggestedFilename = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S.pdf");
    static gboolean bUsedSuggested = FALSE;
    gboolean bHPGL = (pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid);
	gboolean bBoth = pGlobal->HP8753.flags.bDualChannel
			&& pGlobal->HP8753.flags.bSplitChannels && !bHPGL;

 //g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_hp8753c_main")
    GtkDialog       *wPDFfileDlg = GTK_DIALOG( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Dlg_PDFfileChooser" ) );
    GtkFileChooser  *wPDFfileChooser = GTK_FILE_CHOOSER (wPDFfileDlg);
    GtkComboBox     *wComboPDFpaperSize = GTK_COMBO_BOX ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_CB_PDFpaperSize" ) );

    gtk_file_chooser_set_action( wPDFfileChooser, GTK_FILE_CHOOSER_ACTION_SAVE );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, ".pdf" );
    gtk_file_filter_add_pattern (filter, "*.[pP][dD][fF]");
    gtk_file_chooser_add_filter ( wPDFfileChooser, filter );
    //gtk_file_chooser_set_filter ( chooser, filter );
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "All files");
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (wPDFfileChooser, filter);

	gtk_file_chooser_set_do_overwrite_confirmation (wPDFfileChooser, TRUE);

	if( lastFilename && !bUsedSuggested) {
		sFilename = g_strdup( lastFilename);
		gtk_file_chooser_set_filename( wPDFfileChooser, sFilename );
	} else {
		sFilename = g_strdup( sSuggestedFilename );
		gtk_file_chooser_set_current_name (wPDFfileChooser, sFilename);
	}

	if( pGlobal->sLastDirectory )
		gtk_file_chooser_set_current_folder( wPDFfileChooser, pGlobal->sLastDirectory );

	if (gtk_dialog_run (wPDFfileDlg) == GTK_RESPONSE_ACCEPT) {
	    tPaperSize id = eLetter;
		gchar *sChosenFilename = NULL, *extPos = NULL;
		const gchar *sID = gtk_combo_box_get_active_id( GTK_COMBO_BOX( wComboPDFpaperSize) );

		if( sID )
		    if ((tPaperSize)(id = atoi( sID )) < eNumPaperSizes )
		        pGlobal->PDFpaperSize = id;

		sChosenFilename = gtk_file_chooser_get_filename (wPDFfileChooser);

		// see if they used our suggestion
		gchar *sBaseName = g_path_get_basename( sChosenFilename );
		bUsedSuggested = (g_strcmp0( sBaseName, sSuggestedFilename ) == 0);
		g_free( sBaseName );

		g_free( lastFilename );
		lastFilename = g_strdup( sChosenFilename );

		cs = cairo_pdf_surface_create (sChosenFilename,
		        paperDimensions[pGlobal->PDFpaperSize].width,
		        paperDimensions[pGlobal->PDFpaperSize].height );
		cr = cairo_create (cs);
		cairo_save( cr ); {
			plotA(paperDimensions[pGlobal->PDFpaperSize].width,
			        paperDimensions[pGlobal->PDFpaperSize].height,
			        paperDimensions[pGlobal->PDFpaperSize].margin, cr, pGlobal);
		} cairo_restore( cr );
		cairo_show_page( cr );
		if ( bBoth ) {
			plotB(paperDimensions[pGlobal->PDFpaperSize].width,
			        paperDimensions[pGlobal->PDFpaperSize].height,
			        paperDimensions[pGlobal->PDFpaperSize].margin, cr, pGlobal);
			cairo_show_page( cr );
		}

		g_free( pGlobal->sLastDirectory );
		pGlobal->sLastDirectory = gtk_file_chooser_get_current_folder( wPDFfileChooser );

		cairo_surface_destroy ( cs );
		cairo_destroy( cr );

		// now do high resolution smith charts
		// create two filenames from the provided name 'name.1.png and name.2.png'
		GString *strFilename = g_string_new( sChosenFilename );
		extPos = g_strrstr( strFilename->str, ".pdf" );
		if( extPos )
			g_string_insert( strFilename, extPos  - strFilename->str, ".HR" );
		else
			g_string_append( strFilename, ".HR.pdf");

		if( pGlobal->HP8753.channels[eCH_ONE].format == eFMT_SMITH )
			if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH )
				smithHighResPDF(pGlobal, strFilename->str, eCH_BOTH );
			else
				smithHighResPDF(pGlobal, strFilename->str, eCH_ONE );
		else if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH )
							smithHighResPDF(pGlobal, strFilename->str, eCH_TWO );

		g_string_free( strFilename, TRUE );
		g_free (sChosenFilename);
	}

	g_free( sFilename );
	gtk_widget_hide (GTK_WIDGET(wPDFfileDlg));
}

/*!     \brief  Callback to draw plots for printing
 *
 *
 * \param  operation  pointer to GTK print operation structure
 * \param  context	  pointer to GTK print context structure
 * \param  pageNo	  page number
 * \param  pGlobal    pointer to data
 */
#define HEADER_HEIGHT 10
static void
CB_PrintDrawPage (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               pageNo,
		   tGlobal *          pGlobal)
{
	  cairo_t *cr;
	  gdouble height, width;
#define PRINT_MARGIN    (72.0 * 0.10)     // 0.10 inches
	  cr = gtk_print_context_get_cairo_context (context);
	  height = gtk_print_context_get_height( context );
	  width = gtk_print_context_get_width (context);
	  if( pageNo == 0)
		  plotA ( width, height, PRINT_MARGIN, cr, pGlobal);
	  else
		  plotB ( width, height, PRINT_MARGIN, cr, pGlobal);
}

/*!     \brief  Callback when printing commences
 *
 * Increment page number if it is the second plot
 *
 * \param  printOp  pointer to GTK print operation structure
 * \param  context	pointer to GTK print context structure
 * \param  pGlobal  pointer to data
 */
static void
CB_PrintBegin (GtkPrintOperation *printOp,
           GtkPrintContext   *context, tGlobal *pGlobal)
{
	gint pageNos = 1;
	gboolean bHPGL = (pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid);
	if( (pGlobal->HP8753.flags.bDualChannel && pGlobal->HP8753.flags.bSplitChannels)
	        && !bHPGL )
		pageNos = 2;
	gtk_print_operation_set_n_pages( printOp, pageNos );
}

/*!     \brief  Callback when printing completes
 *
 *
 * \param  printOp  pointer to GTK print operation structure
 * \param  context	pointer to GTK print context structure
 * \param  pGlobal  pointer to data
 */
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

/*!     \brief  Callback when print button pressed
 *
 * Setup the required callbacks and print settings
 *
 * \param  wButton  button widget
 * \param  pGlobal  pointer to data
 */
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

	  g_signal_connect(printOp, "begin_print", G_CALLBACK (CB_PrintBegin), pGlobal);
	  g_signal_connect(printOp, "draw_page", G_CALLBACK (CB_PrintDrawPage), pGlobal);
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


