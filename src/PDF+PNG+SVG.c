/*
 * Copyright (c) 2024 Michael G. Katzmann
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
#include <gio/gio.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>
#include <cairo/cairo-svg.h>

#include "hp8753.h"
#include "messageEvent.h"
#include <math.h>


#define PNG_WIDTH       3300

tPaperDimensions paperDimensions[ eNumPaperSizes ] = {
        {595,  842,  7.2},  // A4
        {612,  792,  7.2},  // Letter
        {842, 1190, 10.0},  // A3
        {792, 1224, 10.0}   // Tabloid
};

enum eWhichPlot { eOnlyPlot = 0, ePlotA = 1, ePlotB = 2 };

const static gchar *sSuffix[] = { ".pdf", ".svg", ".png", ".csv" };

static gboolean bUsedSynthesizedName = TRUE;
static gchar   *sSynthesizedName = NULL;
static gchar   *sChosenName = NULL;

/*!     \brief  Suggest a filename based on previous filenames or a synthesized one
 *
 * Suggest a filename based on previous filenames or a synthesized one & save previously selected name
 *
 * \param  pGlobal          pointer to global data
 * \param  chosenFileName   filename to strip suffix and save or NULL if asking for a suggestion
 * \param  suffix of filename (i.e. .json, .pdf, .svg, .png
 * \return pointer to filename string (caller must free)
 */
gchar *
suggestFilename( tGlobal *pGlobal, gchar *sPreviousFileName, gchar *suffix ) {
    gchar *sFilename = NULL;
    gchar *sFilenameStem = NULL;

    // synthesize a name
    GDateTime *now = g_date_time_new_now_local ();
    g_free( sSynthesizedName );
    sSynthesizedName = g_date_time_format( now, "HP8753.%d%b%y.%H%M%S" );
    g_date_time_unref (now);

    if( bUsedSynthesizedName || sPreviousFileName == NULL  ) {
        sFilename = strdup( sSynthesizedName );
    } else {
        // The user has previously chosen his/her own .. so suggest that
        gchar *dot = strrchr( sPreviousFileName, '.' );

        sFilenameStem = g_strdup( sPreviousFileName );
        if( dot ) {
            for( int i = sizeof( sSuffix ) / sizeof( gchar *); i >= 0; i-- ) {
                if( g_str_has_suffix( sFilenameStem, sSuffix[ i ] ) ) {
                    *strrchr( sFilenameStem, '.' ) = 0;
                    break;
                }
            }
        }
        sFilename = g_strdup_printf( "%s.%s", sFilenameStem, suffix );
        g_free( sFilenameStem );
    }

    // This must be freed by the calling program
    return sFilename;
}


/*!     \brief  Add the appropriate file name suffix
 *
 * Add the appropriate file name suffix based on type and whether it
 * is page one or two of a two plot file
 *
 * \param  sChosenFilename  file name as selected (may have a suffix already)
 * \param  fileType         enum of file type
 * \param  page             enum of page (sole page, page one or page two)
 * \param  sExtra           additional suffix like .HR
 * \return                  newly allocated string with modified file name
 */
gchar *
addFileNameSuffix( gchar *sChosenFilename, tFileType fileType, enum eWhichPlot page, gchar *sExtra ) {
    gchar *sLCchosenFilename, *sModifiedFilename, *sWorkingFilename;
    const static gchar *sPageNo[] = { "", ".1", ".2" };

    sWorkingFilename = g_strdup( sChosenFilename );
    // lower case for comparison
    sLCchosenFilename = g_ascii_strdown( sWorkingFilename, STRLENGTH );
    // if we have a suffix already ... remove it first (e.g.: .pdf, .1.pdf or .2.pdf)
    if( g_str_has_suffix( sLCchosenFilename, sSuffix[ fileType ] ) ) {
        gint len = strlen( sWorkingFilename ) - strlen( sSuffix[ fileType ] );
        sWorkingFilename[ len ] = 0;
        if( g_strcmp0( sWorkingFilename + len - strlen( sPageNo[ 1 ] ), sPageNo[ 1 ] ) == 0 )
            sWorkingFilename[ len - strlen( sPageNo[ 1 ] ) ] = 0;
        else if( g_strcmp0( sWorkingFilename + len - strlen( sPageNo[ 2 ] ), sPageNo[ 2 ] ) == 0 )
            sWorkingFilename[ len - strlen( sPageNo[ 1 ] ) ] = 0;
    }

    sModifiedFilename = g_strdup_printf( "%s%s%s%s", sWorkingFilename, sPageNo[ page ],
            sExtra ? sExtra : "", sSuffix[ fileType ]  );

    g_free( sLCchosenFilename );
    g_free( sWorkingFilename );

    return( sModifiedFilename );
}

/*!     \brief  Write the PDF image to a file
 *
 * Determine the filename to use for the PNG / PDF / SVG file and
 * write image(s) of plot using the already retrieved data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal  pointer to data
 */

static gchar *sSuggestedFilename = NULL;
// Call back when file is selected
static void
plotAndSaveFile( GObject *source_object, GAsyncResult *res, gpointer gpGlobal, tFileType fileType ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;
    GtkAlertDialog *alert_dialog;
    gdouble width, height, margin = 0.0;

    gboolean bHPGL = (pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid);
    gboolean bBoth = pGlobal->HP8753.flags.bDualChannel
                && pGlobal->HP8753.flags.bSplitChannels && !bHPGL;

    cairo_t *cr;
    cairo_surface_t *cs;

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {

        gchar *sChosenFilename = g_file_get_path( file );
        gchar *sAugmentedFilename = NULL;
        gchar *selectedFileBasename = g_file_get_basename( file );

        // did we use the synthesized name or did we choose another
        bUsedSynthesizedName = g_str_has_prefix( selectedFileBasename, sSynthesizedName );

        if( !bUsedSynthesizedName ) {
            // if we chose a name .. strip the suffix (if provided)
            // and save, so we can suggest that next time
            GString *filenameLower = g_string_ascii_down( g_string_new( selectedFileBasename ) );
            gchar *dot = strrchr( filenameLower->str, '.' );

            if( dot ) {
                if( g_strcmp0( dot+1, sSuffix[fileType] ) == 0 ) {
                    selectedFileBasename[ dot - filenameLower->str ] = 0;
                }
            }

            g_free( sChosenName );
            sChosenName = selectedFileBasename;
        } else {
            g_free( selectedFileBasename );
        }

        for( gint page = (bBoth ? ePlotA : eOnlyPlot); page <= ePlotB; page++ ) {
            // We place both plots into the one PDF (so only one file name needed)
            sAugmentedFilename = addFileNameSuffix( sChosenFilename, fileType,
                    fileType == ePDF ? eOnlyPlot : page, NULL );
            switch( fileType ) {
            case ePDF:
            default:
                width  = paperDimensions[pGlobal->PDFpaperSize].width;
                height = paperDimensions[pGlobal->PDFpaperSize].height;
                margin = paperDimensions[pGlobal->PDFpaperSize].margin;
                // Only create the surface once
                if( page != ePlotB ) {
                    cs = cairo_pdf_surface_create ( sAugmentedFilename, width, height );
                    cairo_pdf_surface_set_metadata (cs, CAIRO_PDF_METADATA_CREATOR, "HP8753 Network Analyzer");
                }
                break;
            case eSVG:
                width  = paperDimensions[pGlobal->PDFpaperSize].width;
                height = paperDimensions[pGlobal->PDFpaperSize].height;
                margin = paperDimensions[pGlobal->PDFpaperSize].margin;
                cs = cairo_svg_surface_create ( sAugmentedFilename, width, height );
                break;
            case ePNG:
                width  = PNG_WIDTH;
                height = PNG_WIDTH / sqrt( 2.0 );
                cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
                break;
            }

            cr = cairo_create (cs);

            // Letter and Tabloid size are not in the ratio of our data ( height = width / sqrt( 2 ) )
            // we need to adjust
            if( fileType != ePNG )  {   // we know PNG is the right aspect ratio
                // aspect ratio is sqrt( 2 )
                if( (height / width) / sqrt( 2.0 ) > 1.01 ) {// this should leave A4 and A3 untouched
                    cairo_translate( cr, width - (height * sqrt(2.0)) / 2.0, 0.0  );
                    width = height * sqrt( 2.0 );
                } else if( (height / width) / sqrt( 2.0 ) < 0.99 ) {   // wider
                    cairo_translate( cr, 0.0, (height - width / sqrt( 2.0 )) / 2.0  );
                    height = width / sqrt( 2.0 );
                }
            }

            cairo_save( cr ); {
                if( page < ePlotB)
                    plotA( width, height, margin, cr, pGlobal );
                else
                    plotB( width, height, margin, cr, pGlobal );
            } cairo_restore( cr );

            cairo_show_page( cr );

            if( fileType  == ePNG )
                cairo_surface_write_to_png (cs, sAugmentedFilename );

            // Don't destroy on the first page of a multi-page PDF
            if( fileType != ePDF || page == ePlotB || page == eOnlyPlot ) {
                cairo_surface_destroy ( cs );
            }
            cairo_destroy( cr );

            g_free( sAugmentedFilename );

            if( page == eOnlyPlot )
                break;
        }

        // now do high resolution smith charts if we are doing PDF & smith
        if( fileType == ePDF ) {
            if( bBoth && pGlobal->HP8753.channels[eCH_ONE].format == eFMT_SMITH
                    && pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH )
                bBoth = TRUE;
            else
                bBoth = FALSE;

            sAugmentedFilename = NULL;
            if( pGlobal->HP8753.channels[eCH_ONE].format == eFMT_SMITH ) {
                if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH ) {
                    sAugmentedFilename = addFileNameSuffix( sChosenFilename, ePDF, eOnlyPlot, ".HR" );
                    smithHighResPDF(pGlobal, sAugmentedFilename, eCH_BOTH );
                } else {
                    sAugmentedFilename = addFileNameSuffix( sChosenFilename, ePDF, bBoth ? ePlotA : eOnlyPlot, ".HR" );
                    smithHighResPDF(pGlobal, sAugmentedFilename, eCH_ONE );
                }
            } else if( pGlobal->HP8753.channels[eCH_TWO].format == eFMT_SMITH ) {
                    sAugmentedFilename = addFileNameSuffix( sChosenFilename, ePDF, bBoth ? ePlotB : eOnlyPlot, ".HR" );
                    smithHighResPDF(pGlobal, sAugmentedFilename, eCH_TWO );
            }
            g_free( sAugmentedFilename );
        }

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
   }

   if (err) {
     alert_dialog = gtk_alert_dialog_new ("%s", err->message);
     // gtk_alert_dialog_show (alert_dialog, GTK_WINDOW (win));
     g_object_unref (alert_dialog);
     g_clear_error (&err);
   }
    g_free( sSuggestedFilename );
}

// Call back when file is selected
static void
CB_PDFsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, ePDF );
}

// Call back when file is selected
static void
CB_SVGsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, eSVG );
}

// Call back when file is selected
static void
CB_PNGsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, ePNG );
}

void
presentFileSaveDialog ( GtkButton *wBtn, gpointer user_data, tFileType fileType ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wBtn), "data");

    GtkFileDialog *fileDialogSave = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wBtn), GTK_TYPE_WINDOW);

    GDateTime *now = g_date_time_new_now_local ();
    gchar *pUserFileName = NULL;

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);

    g_autoptr (GtkFileFilter) filter = NULL;

    // PDF files
    filter = gtk_file_filter_new ();
    switch( fileType ) {
    case ePDF:
        gtk_file_filter_add_mime_type (filter, "application/pdf");
        gtk_file_filter_set_name (filter, "PDF");
        pUserFileName = suggestFilename( pGlobal, sChosenName, "pdf" );
        break;
    case eSVG:
        gtk_file_filter_add_mime_type (filter, "image/svg+xml");
        gtk_file_filter_set_name (filter, "SVG");
        pUserFileName = suggestFilename( pGlobal, sChosenName, "svg" );
        break;
    case ePNG:
        gtk_file_filter_add_mime_type (filter, "image/png");
        gtk_file_filter_set_name (filter, "PNG");
        pUserFileName = suggestFilename( pGlobal, sChosenName, "png" );
        break;
    default: break;
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

    switch( fileType ) {
    case ePDF:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_PDFsave, pGlobal);
        break;
    case eSVG:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_SVGsave, pGlobal);
        break;
    case ePNG:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_PNGsave, pGlobal);
        break;
    default:
        break;
    }

    g_free( pUserFileName );
    g_object_unref (fileDialogSave);
    g_object_unref( fPath );
    g_date_time_unref( now );
}

// Call back for button on main dialog
void
CB_btn_PDF ( GtkButton *wBtnPDF, gpointer user_data ) {
    presentFileSaveDialog ( wBtnPDF, user_data, ePDF );
}


void
CB_btn_SVG ( GtkButton *wBtbSVG, gpointer user_data ) {
    presentFileSaveDialog ( wBtbSVG, user_data, eSVG );
}

void
CB_btn_PNG ( GtkButton *wBtnPNG, gpointer user_data ) {
    presentFileSaveDialog ( wBtnPNG, user_data, ePNG );
}

