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

#ifndef HP8753_H_
#define HP8753_H_

#ifndef VERSION
   #define VERSION "1.31-1"
#endif

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include "calibrationKit.h"

/*!     \brief Debugging levels
 */
enum _debug
{
        eDEBUG_ALWAYS    = 0,
        eDEBUG_INFO      = 1,
        eDEBUG_MINOR     = 3,
        eDEBUG_TESTING   = 4,
        eDEBUG_EXTENSIVE = 5,
		eDEBUG_EXTREME   = 6,
        eDEBUG_MAXIMUM   = 7
};
#include <gtk/gtk.h>
#include <cairo/cairo.h>

#define INVALID 	(-1)
#define STRLENGTH	(-1)	// calculate it
#define ms2us(x) 		((x) * 1000)
#define DEG2RAD(x)		((x) * M_PI / 180.0)
#define RAD2DEG(x)		((x) / M_PI * 180.0)
#define DBtoRATIO(x)	pow( 10.0, (x) / 20.0 )
#define RATIOtoDB(x)	(20.0 * log10(x))
#define Z0			50.0
#define KILO		1.0e3
#define MEGA		1.0e6
#define GIGA		1.0e9
#define kHz(x)	((gdouble)(x) * KILO)
#define MHz(x)	((gdouble)(x) * MEGA)
#define GHz(x)	((gdouble)(x) * GIGA)
#define SQ(x) ({ typeof (x) _x = (x); _x * _x; })
#define POLAR2COMPLEX(x,y)	((x) * cos(y) + (x) * sin(y) * I)

#define	BUFFER_SIZE	2048

#define MAX_PRIMARY_DEVICES			30
#define MAX_SECONDARY_DEVICES		10

#define ADDR_HP8753C				16

#define MAX_CAL_ARRAYS	12
#define HEADER_SIZE		4

typedef struct {
	Addr4882_t GPIBaddress;
	const gchar *name;
	struct {
		guint bActive	:1;
	} flags;

} GPIBdevice;

typedef struct {
	double r;
	double i;
} tComplex;

typedef struct line
{
	tComplex A, B;
} tLine;

typedef enum {
	eMEAS_S11=0, eMEAS_S12=1, eMEAS_S21=2, eMEAS_A_R=3, eMEAS_B_R=4, eMEAS_A_B=5, eMEAS_A=6, eMEAS_B=7, eMEAS_R=8
} tMeasurement;

typedef enum {
	eFMT_LOGM=0, eFMT_PHASE=1, eFMT_DELAY=2, eFMT_SMITH=3, eFMT_POLAR=4, eFMT_LINM=5, eFMT_SWR=6, eFMT_REAL=7, eFMT_IMAG=8
} tFormat;

typedef enum {
	eSWP_LINFREQ=0, eSWP_LOGFREQ=1, eSWP_LSTFREQ=2, eSWP_CWTIME=3, eSWP_PWR=4
} tSweepType;

typedef enum {
	eGridCartesian=0, eGridPolar=1, eGridSmith=2
} tGrid;

typedef enum {
	eLeft, eRight, eCenter, eTopLeft, eTopRight, eBottomLeft, eBottomRight
} tTxtPosn;

typedef enum {
	eENG_NORMAL, eENG_SEPARATE, eNEG_NUMERIC
} tEngNotation;

typedef enum { eACTIVE_MKR, eNONACTIVE_MKR, eFIXED_MKR } tMkrStyle;

typedef enum {
	eColorBlack,
	eColorWhite,
	eColorYellow,
	eColorLightBlue,
	eColorLightPeach,
	eColorPurple,
	eColorLightPurple,
	eColorBlue,
	eColorDarkBlue,
	eColorGreen,
	eColorDarkGreen,
	eColorRed,
	eColorDarkRed,
	eColorGray,
	eColorBrown,
	eColorDarkBrown,
	eColorLast
} eColor;

typedef enum { eDB_CALandSETUP, eDB_TRACE, eDB_CALKIT } tDBtable;

typedef enum { eA4 = 0, eLetter = 1, eA3 = 2, eTabloid = 3, eNumPaperSizes = 4 } tPaperSize;

extern const tGrid gridType[];

#define MKR_FREQ		0
#define MKR_REAL		1
#define MKR_IMAG		2
#define MKR_NUM_VALUES	3

#define MAX_MKRS			5
#define MAX_NUMBERED_MKRS	4
#define FIXED_MARKER		4
#define MAX_CHANNELS		2

typedef struct {
	gdouble   sourceValue;
	tComplex  point;
} tMarker;

typedef struct {
	gint 	nPoints;
	gdouble startFreq, stopFreq;
} tSegment;

// This must match the positions in optMeasurementType
#define S11_MEAS  0
#define S22_MEAS  3

typedef struct {
	gdouble *freq;
	tComplex *S11, *S21, *S22, *S12;
	gint nPoints;
	enum { S2P, S1P_S11, S1P_S22 } SnPtype;
}tS2P;

typedef enum {
	eMkrLinear = 0,
	eMkrLog    = 1,
	eMkrReIm   = 2,
	eMkrRjX    = 3,
	eMkrGjB    = 4,
	eMkrDefault
} tMkrType;

// Index to HP8753 learn string for items that we cannot
// get with conventional queries.
// This will no doubt be different for every firmware version, so if in doubt
// we don't access markers
typedef struct {
	gint version;			// version valid for the data below
// indexes into Learn String for various settings
	guint iActiveChannel;	// active channel (0x01 or 0x02)
	guint iStartStop   [2];	// stimulus start/stop or center/span (0x01 is start/stop)

	guint iMarkerActive[2];	// current marker (0x02 (marker 1) to 0x10 (marker 4))
	guint iMarkersOn   [2];	// markers on     (bit or of 0x02 (marker 1) to 0x10 (marker 4) with 0x20 as all off)
	guint iMarkerDelta [2];	// markers relative to marker x  (bit or of 0x02 (marker 1) to 0x10 (marker 4) with 0x20 as delta fixed)
	guint iSmithMkrType[2]; // Smith marker type 0x00 - Lin / 0x01 - Log / 0x02 - Re-Im / 0x04 - R+jX / 0x08 - G+jB
	guint iPolarMkrType[2]; // Polar marker type 0x10 - Lin / 0x20 - log / 0x40 - Re-Im
	guint iNumSegments [2]; // number of segments defined
} tLearnStringIndexes;

typedef struct {
	tComplex *responsePoints;
	gdouble  *stimulusPoints;
	struct {
		guint32 bSweepHold      : 1;
		guint32 bValidData      : 1;
		guint32 bbMkrs           : MAX_MKRS;
		guint32 bMkrsDelta      : 1;
		guint32 bCenterSpan     : 1;
		guint32 bBandwidth      : 1;
		guint32 bAllSegments    : 1;
		guint32 bValidSegments  : 1;
		guint32 bAdmitanceSmith : 1;
		guint32 bAveraging      : 1;
		guint32 bbTBA           : 18;    // unused
	} chFlags;

	gdouble 	sweepStart;
	gdouble 	sweepStop;
	tSweepType 	sweepType;
	gdouble 	IFbandwidth;
	gdouble 	CWfrequency;

	int activeMarker;
	int deltaMarker;

	tMarker numberedMarkers[ MAX_MKRS ];
#define BW_WIDTH  0
#define BW_CENTER 1
#define BW_Q      2
#define MAX_BW_ELEMENTS 3
	gdouble bandwidth[ MAX_BW_ELEMENTS ];		// BW, Center, Q
	tMkrType mkrType;

	guint 	nPoints;
	tFormat format;

	gdouble scaleVal;
	gdouble scaleRefPos;
	gdouble scaleRefVal;

#define MAX_SEGMENTS	30
	gint nSegments;
	tSegment segments[MAX_SEGMENTS];

	tMeasurement measurementType;
} tChannel;

typedef enum { eCH_ONE = 0, eCH_SINGLE = 0, eCH_TWO = 1, eNUM_CH = 2, eCH_BOTH = 2 } eChannel;
#define otherChannel(x)	((x+1) % eNUM_CH)
typedef enum { eProjectName = 0, eCalibrationName = 1, eTraceName = 2 } tRMCtarget;
typedef enum { eRename = 0, eMove = 1, eCopy = 2 } tRMCpurpose;

typedef enum {  eColorTrace1 = 0, eColorTrace2 = 1, eColorTraceSeparate = 2,
                eColorGrid = 3, eColorGridPolarOverlay = 4, eColorSmithGridAnnotations = 5,
                eColorTextSpanPerDivCoupled = 6, eColorTextTitle = 7, eColorRefLine1 = 8,
                eColorRefLine2 = 9, eColorLiveMkrCursor = 10, eColorLiveMkrFreqTicks = 11,
                eMAX_COLORS = 12
} tElementColor;

#define NUM_HPGL_PENS   11      // limit by HP8753

extern GdkRGBA plotElementColors[ eMAX_COLORS ];
extern GdkRGBA HPGLpens[ NUM_HPGL_PENS ];
extern GdkRGBA plotElementColorsFactory[ eMAX_COLORS ];
extern GdkRGBA HPGLpensFactory[ NUM_HPGL_PENS ];

#define COLOR_HPGL_DEFAULT              eColorBlack     // HPGL from 8753 always sets the color .. so not useful
#define COLOR_50ohm_SMITH               eColorGreen     // This is disabled by FIFTY_OHM_GREEN


enum { eNoInterplativeCalibration = 0, eInterplativeCalibration = 1, eInterplativeCalibrationButNotEnabled = 2 };
typedef struct {
	guint length;
	struct {
		guint16 bDualChannel     		: 1;
		guint16 bSplitChannels   		: 1;
		guint16 bSourceCoupled			: 1;
		guint16 bMarkersCoupled			: 1;
		guint16 bLearnStringParsed      : 1;
		guint16 bLearnedStringIndexes	: 1;	// if locally obtained
		guint16 bHPGLdataValid			: 1;
		guint16 bShowHPGLplot           : 1;	// display HPGL plot instead of enhanced plots
	} flags;

	struct {
	    guint16 bDualChannel     		: 1;
	    guint16 bSplitChannels   		: 1;
	    guint16 bSourceCoupled			: 1;
	    guint16 bMarkersCoupled			: 1;
	    guint16 bActiveChannel			: 1;
	} calSettings;

	tChannel channels[ eNUM_CH ];
	// compiled HPGL plot data
	// initial integer is the length of the malloced data
	void *plotHPGL;
	tS2P S2P;

	tLearnStringIndexes analyzedLSindexes;
	tLearnStringIndexes *pLSindexes;

	gchar *sTitle;
	gchar *sNote;
	gchar *dateTime;

	gint 	activeChannel;
	gint    firmwareVersion;
	gchar	*sProduct;

} tHP8753;

typedef struct {
	gchar *sProject;
	gchar *sName;
	gboolean bSelected;
} tProjectAndName;

typedef struct {

	struct {
	    guint16 bSourceCoupled			: 1;
	    guint16 bActiveChannel			: 1;
	    guint16 bDualChannel			: 1;
	} settings;

	struct {
		gint 	    iCalType;
		guchar     *pCalArrays[ MAX_CAL_ARRAYS ];
		gdouble 	sweepStart;
		gdouble 	sweepStop;
		tSweepType 	sweepType;
		gdouble 	IFbandwidth;
		gdouble		CWfrequency;
		gint		nPoints;
		struct {
		    guint16 bSweepHold					: 1;
		    guint16 bbInterplativeCalibration	: 2;
		    guint16 bValid						: 1;
		    guint16 bAveraging					: 1;
		    guint16 bUseOtherChannelCalArrays	: 1;
		} settings;
	} perChannelCal[ eNUM_CH ];

	gchar *sDateTime;
	gchar *sNote;
	tProjectAndName projectAndName;

	guchar *pHP8753_learn;
	gint    firmwareVersion;
} tHP8753cal;

typedef struct {
	gchar *sTitle;
	gchar *sNote;
	gchar *sDateTime;
	tProjectAndName projectAndName;
} tHP8753traceAbstract;

typedef struct {
	tHP8753 HP8753;
	tHP8753cal HP8753cal;
	tHP8753calibrationKit HP8753calibrationKit;

	struct {
	    guint32 bSmithSpline     		: 1;
	    guint32 bGPIB_UseCardNoAndPID	: 1;
	    guint32 bCalibrationOrTrace		: 1;
	    guint32 bShowDateTime			: 1;
	    guint32 bAdmitanceSmith			: 1;
	    guint32 bDeltaMarkerZero        : 1;
	    guint32 bSaveUserKit            : 1;
	    guint32 bRunning         		: 1;
	    guint32 bbDebug					: 3;
	    guint32 bGPIBcommsActive        : 1;
	    guint32 bProject                : 1;
	    guint32 bNoGPIBtimeout			: 1;
	    guint32 bDoNotRetrieveHPGLdata  : 1;
	    guint32 bHPlogo                 : 1;
	    guint32 bHoldLiveMarker         : 1;
	} flags;

	tRMCtarget RMCdialogTarget;
	tRMCpurpose RMCdialogPurpose;

	GHashTable *widgetHashTable;

	gint                GPIBcontrollerIndex,  GPIBdevicePID;
	gchar               *sGPIBdeviceName;
	gint                GPIBversion;
	GtkPrintSettings    *printSettings;
	GtkPageSetup        *pageSetup;
	tPaperSize          PDFpaperSize;
	gchar			    *sLastDirectory;

	// names of the currently selected objects
	tHP8753traceAbstract    *pTraceAbstract;
	tHP8753cal              *pCalibrationAbstract;
	gchar			    *sProject;
	gchar			    *sCalKit;

	GSource         *messageEventSource;
	GAsyncQueue     *messageQueueToMain;
	GAsyncQueue     *messageQueueToGPIB;

	GList *pProjectList;
	GList *pCalList;		// list containing tHP8753cal objects
	GList *pTraceList;		// list containing tHP8753traceAbstract objects
	GList *pCalKitList;

	GThread * pGThread;

	tComplex mousePosition[ eNUM_CH ];
	gdouble  mouseXpercentHeld;

} tGlobal;



typedef union {
	 struct {
		gdouble r;       // ∈ [0, 1]
		gdouble g;       // ∈ [0, 1]
		gdouble b;       // ∈ [0, 1]
	} RGB;
	struct {
	    gdouble h;       // ∈ [0, 360]
	    gdouble s;       // ∈ [0, 1]
	    gdouble v;       // ∈ [0, 1]
	} HSV;
} tColor;


typedef struct {
	struct {
		guint16     bAny                        : 1;
		guint16     bCartesian                  : 1;
		guint16     bPolar                      : 1;
		guint16     bPolarWithDiferentScaling   : 1;
		guint16     bSmith                      : 1;
		guint16     bSmithWithDiferentScaling   : 1;
		guint16     bPolarSmith                 : 1;
	} overlay;


	gboolean bSourceCoupled;

	guint areaWidth;
	guint areaHeight;

	gdouble margin;

	gdouble leftGridPosn;
	gdouble rightGridPosn;

	gdouble gridWidth;
	gdouble gridHeight;

	gdouble bottomGridPosn;
	gdouble topGridPosn;

	gdouble textMargin;
	gdouble makerAreaWidth;

	gdouble fontSize;
	gdouble lineSpacing;
	gdouble scale;

	cairo_matrix_t initialMatrix;
} tGridParameters;

typedef struct {
    guint   height, width;
    gdouble margin;
} tPaperDimensions;

extern tPaperDimensions paperDimensions[];

#define DEFAULT_GPIB_DEVICE_ID		16
#define DEFAULT_GPIB_HP8753C_DEVICE_NAME "hp8753c"

#define A4882( pad, sad )	( (pad & 0xFF) | ((sad & 0xFF) << 8) )

extern GPIBdevice devices[];
extern GHashTable *widgetHashTable;

#define ERROR	(-1)
#define OK      ( 0)
#define CLEAR	( 0)

#define NPAGE_CALIBRATION	0
#define NPAGE_TRACE			1
#define NPAGE_DATA			2
#define NPAGE_OPTIONS		3
#define NPAGE_GPIB			4
#define NPAGE_CALKITS		5


// Bits in the HP8753 status register (and bits for SRE (mask for signaling an SRQ))
#define ST_RevGET   1   // waiting for operator to connect device (reverse)
#define ST_FwdGET   2   // waiting for operator to connect device (forward)
#define ST_ESRB     4   // event status register b signal
#define ST_CEQ      8   // check error queue
#define ST_MOQ     16   // message in output queue
#define ST_ESR     32   // event status register signal
#define ST_SRQ     64   // request service
#define ST_TBA    128   // unused

// Bits in the HP8753 event status register
#define ESE_OPC     1   // OPC
#define ESE_RQC     2   // Request Control
#define ESE_QERR    4   // Query Error
#define ESE_SEQ     8   // Sequence Bit
#define ESE_EERR   16   // Execution Error
#define ESE_SERR   32   // Syntax Error
#define ESE_USER   64   // User Request
#define ESE_PWR   128   // Power On

#define SEVER_DIPLOMATIC_RELATIONS -1
#define THIRTY_MS 0.030
#define FIVE_SECONDS 5.0

gboolean    addToComboBox( GtkComboBox *, gchar * );
void        bezierControlPoints( const tLine *, const tLine *, tComplex *, tComplex * );
void        CB_EditableCalibrationProfileName( GtkEditable *, tGlobal * );
void        CB_EditableProjectName( GtkEditable *, tGlobal * );
void        CB_EditableTraceProfileName( GtkEditable *, tGlobal * );
void        CB_Radio_Calibration ( GtkRadioButton *, tGlobal * );
void        cairo_renderHewlettPackardLogo(cairo_t *, gboolean, gboolean, gdouble, gdouble);
gint        checkMessageQueue(GAsyncQueue *);
void        clearHP8753traces ( tHP8753 * );
tHP8753cal* cloneCalibrationProfile( tHP8753cal *, gchar * );
tHP8753traceAbstract*   cloneTraceProfileAbstract( tHP8753traceAbstract *, gchar * );
void        closeDB ( void );
gint        compareCalItemsForFind ( gpointer , gpointer );
gint        compareCalItemsForSort ( gpointer , gpointer );
gint        compareCalKitIdentifierItem ( gpointer, gpointer );
gint        compareTraceItemsForFind ( gpointer , gpointer );
gint        compareTraceItemsForSort ( gpointer , gpointer );
GList*      createIconList( void );
guint       deleteDBentry ( tGlobal *, gchar *, gchar *, tDBtable );
gchar*      doubleToStringWithSpaces( gdouble, gchar * );
void        drawBezierSpline( cairo_t *, const tComplex *, gint );
void        drawHPlogo (cairo_t *, gchar *, gdouble , gdouble , gdouble );
void        drawMarkers( cairo_t *, tGlobal *, tGridParameters *, eChannel , gdouble, gdouble );
gchar*      engNotation ( gdouble, gint, tEngNotation, gchar ** );
void        flipCairoText( cairo_t * );
gint        getTimeStamp( gchar ** );
void        freeCalListItem ( gpointer );
void        freeTraceListItem ( gpointer );
void        initializeFORM1exponentTable ( void );
gint        inventoryProjects ( tGlobal * );
gint        inventorySavedCalibrationKits ( tGlobal * );
gint        inventorySavedSetupsAndCal ( tGlobal * );
guint       inventorySavedTraceNames( tGlobal * );
void        logVersion( void );
gint        openOrCreateDB ( void ) ;
gboolean    plotA( guint, guint, gdouble, cairo_t *, tGlobal * );
gboolean    plotB( guint, guint, gdouble, cairo_t *, tGlobal * );
gint        populateCalComboBoxWidget( tGlobal * );
gint        populateProjectComboBoxWidget( tGlobal * );
gint        populateTraceComboBoxWidget( tGlobal * );
gint        recoverCalibrationAndSetup ( tGlobal *, gchar *, gchar * );
gint        recoverCalibrationKit ( tGlobal *, gchar * );
gint        recoverProgramOptions( tGlobal * );
gint        recoverTraceData ( tGlobal *, gchar *, gchar * );
gint        renameMoveCopyDBitems(tGlobal *, tRMCtarget, tRMCpurpose, gchar *, gchar *, gchar *);
void        rightJustifiedCairoText( cairo_t *, gchar *, gdouble, gdouble );
gint        saveCalibrationAndSetup ( tGlobal *, gchar *, gchar * );
gint        saveCalKit ( tGlobal *pGlobal );
gint        saveLearnStringAnalysis ( tGlobal *, tLearnStringIndexes * );
gint        saveProgramOptions ( tGlobal * );
tHP8753cal* selectCalibrationProfile( tGlobal *, gchar *, gchar * );
gint        saveTraceData ( tGlobal *, gchar *, gchar * );
tHP8753cal* selectFirstCalibrationProfileInProject( tGlobal * );
tHP8753traceAbstract*   selectFirstTraceProfileInProject( tGlobal * );
tHP8753traceAbstract*   selectTraceProfile( tGlobal *, gchar *, gchar * );
gint        sendHP8753calibrationKit (gint, tGlobal *, gint * );
void        sensitiseControlsInUse( tGlobal *, gboolean );
void        setCairoColor( cairo_t *, eColor );
void        setCairoFontSize( cairo_t *, gdouble );
gboolean    setGtkComboBox( GtkComboBox *, gchar * );
gint        setNotePageColorButton (tGlobal *, gboolean );
void        setUseGPIBcardNoAndPID( tGlobal *, gboolean );
void        showCalInfo( tHP8753cal *, tGlobal * );
void        showRenameMoveCopyDialog( tGlobal * );
gint        smithHighResPDF( tGlobal *, gchar *, eChannel );
gint        splineInterpolate( gint, tComplex [], gdouble, tComplex * );
gpointer    threadGPIB (gpointer);
void        updateCalComboBox( gpointer , gpointer );
void        visibilityFramePlot_B ( tGlobal *, gint );

extern tGlobal globalData;

typedef struct { gchar *code, *desc; } HP8753_option;
extern const HP8753_option optMeasurementType[];
extern const HP8753_option optFormat[];
extern const HP8753_option optCalType[];
extern const HP8753_option optSweepType[];

extern const gchar *formatSymbols[];
extern const gchar *formatSmithOrPolarSymbols[][2];
extern const gchar *sweepSymbols[];
extern const gint numOfCalArrays[];

#define lengthFORM1data(x) (GUINT16_FROM_BE(*(guint16 *)((x)+2)) + 4)
#define DISCOVERED_LS_INDEXES	0
extern tLearnStringIndexes learnStringIndexes[];

#undef SINGLE_SWEEP_BEFORE_SAVE
#define CLEAN_SWEEP_AFTER_RECALL

#define PERCENT(x,y) ((x) * (y)/100.0)
#define SQU(x) ((x) * (x))
#define fontSize( h, w ) (((h)<(w)*0.75 ? (h):(w)*0.75) / 50.0)
#define LIN_INTERP( lower, upper, fract) ((upper) * (fract) + (lower) * (1.0 - (fract)))

#define DATETIME_SIZE  64

#define NHGRIDS   10
#define NVGRIDS   10

#define INFO_LEN	 	50
#define BUFFER_SIZE_20	20
#define BUFFER_SIZE_100	100
#define	BUFFER_SIZE_250	250
#define	BUFFER_SIZE_500	500
#define BYTES_PER_CALPOINT 6;

#define ms(x) ((x)*1000)

#define LABEL_FONT	"Nimbus Sans"
#define CURSOR_FONT	"Nimbus Sans"
#define MARKER_FONT	"Nimbus Sans"
#define MARKER_FONT_NARROW	"Nimbus Sans Narrow"
#define MARKER_SYMBOL_FONT	"Nimbus Sans"
#define HP_LOGO_FONT "Nimbus Sans"
#define STIMULUS_LEGEND_FONT "Nimbus Sans"
#define HPGL_FONT "Noto Sans Mono Light"   // OR "Noto Sans Mono ExtraLight"

#define TIMEOUT_SWEEP	200		// if 10Hz RBW and 1601 points, it may take a long time to sweep
#define LOCAL_DELAYms   50		// Delay after going to local from remote

#if !GLIB_CHECK_VERSION(2, 67, 3)
static inline gpointer
g_memdup2(gconstpointer mem, gsize byte_size) {
	gpointer new_mem = NULL;
		if(mem && byte_size != 0) {
			new_mem = g_malloc (byte_size);
			memcpy (new_mem, mem, byte_size);
		}
		return new_mem;
}
#endif /* !GLIB_CHECK_VERSION(2, 67, 3) */

#define LOG( level, message, ...) \
    g_log_structured (G_LOG_DOMAIN, level, \
		  "SYSLOG_IDENTIFIER", "hp8753", \
		  "CODE_FUNC", __FUNCTION__, \
		  "CODE_LINE", G_STRINGIFY(__LINE__), \
		  "MESSAGE", message, ## __VA_ARGS__)

#define DBG( level, message, ... ) \
	if( globalData.flags.bbDebug >= level ) \
		LOG( G_LOG_LEVEL_DEBUG, message, ## __VA_ARGS__)

#define CURRENT_DB_SCHEMA	2
// This character separates project name from item name in database
// ... its more complicated to ensure compatability with older database schemas
#define ETX 0x03

#undef GPIB_PRE_4_3_6

#undef DELTA_MARKER_ZERO
#undef USE_PRECAUTIONARY_DEVICE_IBCLR

#ifndef GPIB_CHECK_VERSION
 #define GPIB_CHECK_VERSION(ma,mi,mic) 0
 #pragma message "You should update to a newer version of linux-gpib"
#endif

#endif /* HP8753COMMS_H_ */
