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
   #define VERSION "1.13-1"
#endif

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include "calibrationKit.h"

/*!     \brief Debugging levels
 */
enum _debug
{
        eDEBUG_NONE      = 0,
        eDEBUG_INFO      = 1,
        eDEBUG_MINOR     = 3,
        eDEBUG_EXTENSIVE = 5,
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
#define ADDR_HP33401A_MULTIMETER	22

#define MAX_CAL_ARRAYS	12
#define HEADER_SIZE		4

typedef struct {
	struct {
		unsigned short bSmithSpline     : 1;
	} flags;
} _global;

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
	eColorLightPuple,
	eColorBlue,
	eColorDarkBlue,
	eColorGreen,
	eColorDarkGreen,
	eColorRed,
	eColorGray,
	eColorBrown,
	eColorLast
} eColor;

typedef enum { eDB_CALandSETUP, eDB_TRACE, eDB_CALKIT } tDBtable;

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

typedef struct {
	gdouble *freq;
	tComplex *S11, *S21, *S22, *S12;
	gint nPoints;
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
		unsigned short bUnused         				: 1;
		unsigned short bValidData					: 1;
		unsigned short bMkrs						: MAX_MKRS;
		unsigned short bMkrsDelta					: 1;
		unsigned short bCenterSpan					: 1;
		unsigned short bBandwidth					: 1;
		unsigned short bAllSegments					: 1;
		unsigned short bValidSegments				: 1;
		unsigned short bAdmitanceSmith				: 1;
		unsigned short bAveraging					: 1;
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

#define CH_ONE_COLOR				eColorDarkGreen
#define CH_TWO_COLOR				eColorDarkBlue
#define CH_ALL_COLOR				eColorBlack
#define COLOR_GRID					eColorLightBlue
#define COLOR_GRID_OVERLAY	eColorLightPeach

enum { eNoInterplativeCalibration = 0, eInterplativeCalibration = 1, eInterplativeCalibrationButNotEnabled = 2 };
typedef struct {
	guint length;
	struct {
		unsigned short bDualChannel     		: 1;
		unsigned short bSplitChannels   		: 1;
		unsigned short bSourceCoupled			: 1;
		unsigned short bMarkersCoupled			: 1;
		unsigned short bLearnStringParsed       : 1;
		unsigned short bLearnedStringIndexes	: 1;	// if locally obtained
	} flags;

	struct {
		unsigned short bDualChannel     		: 1;
		unsigned short bSplitChannels   		: 1;
		unsigned short bSourceCoupled			: 1;
		unsigned short bMarkersCoupled			: 1;
		unsigned short bActiveChannel			: 1;
	} calSettings;

	tChannel channels[ eNUM_CH ];
	tS2P S2P;

	tLearnStringIndexes analyzedLSindexes;
	tLearnStringIndexes *pLSindexes;

	gchar *sTitle;
	gchar *sNote;
	gchar *dateTime;

	guchar *pHP8753C_learn;
	gint 	activeChannel;
	gint    firmwareVersion;
	gchar	*sProduct;

} tHP8753;

typedef struct {

	struct {
		unsigned short bSourceCoupled			: 1;
		unsigned short bActiveChannel			: 1;
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
			unsigned short bSweepHold					: 1;
			unsigned short bbInterplativeCalibration	: 2;
			unsigned short bValid						: 1;
			unsigned short bAveraging					: 1;
		} settings;
	} perChannelCal[ eNUM_CH ];

	gchar *dateTime;
	gchar *sNote;
	gchar *sName;

	guchar *pHP8753C_learn;
	gint    firmwareVersion;
} tHP8753cal;


typedef struct {
	tHP8753 HP8753;
	tHP8753cal HP8753cal;
	tHP8753calibrationKit HP8753calibrationKit;

	struct {
		unsigned short bSmithSpline     		: 1;
		unsigned short bGPIB_UseCardNoAndPID	: 1;
		unsigned short bCalibrationOrTrace		: 1;
		unsigned short bShowDateTime			: 1;
		unsigned short bAdmitanceSmith			: 1;
		unsigned short bDeltaMarkerZero			: 1;
		unsigned short bSaveUserKit             : 1;
		unsigned short bRunning         		: 1;
		unsigned short bbDebug					: 3;
	} flags;

	GHashTable *widgetHashTable;

	gint	 GPIBcontrollerIndex,  GPIBdevicePID;
	gchar 	*sGPIBcontrollerName, *sGPIBdeviceName;
	GtkPrintSettings *printSettings;
	GtkPageSetup 	 *pageSetup;
	gchar			 *sLastDirectory;
	gchar			 *sCalProfile;
	gchar			 *sTraceProfile;
	gchar			 *sCalKit;

	GSource *		messageEventSource;
	GAsyncQueue *	messageQueueToMain;
	GAsyncQueue *	messageQueueToGPIB;

	GList *pCalList;
	GList *pTraceList;
	GList *pCalKitList;

	GThread * pGThread;
	tComplex mousePosition[ eNUM_CH ];

} tGlobal;



typedef union {
	 struct {
		double r;       // ∈ [0, 1]
		double g;       // ∈ [0, 1]
		double b;       // ∈ [0, 1]
	} RGB;
	struct {
	    double h;       // ∈ [0, 360]
	    double s;       // ∈ [0, 1]
	    double v;       // ∈ [0, 1]
	} HSV;
} tColor;


typedef struct {
	struct {
		unsigned short     bAny   : 1;
		unsigned short     bCartesian : 1;
		unsigned short     bPolar : 1;
		unsigned short     bPolarWithDiferentScaling : 1;
		unsigned short     bSmith : 1;
		unsigned short     bSmithWithDiferentScaling : 1;
		unsigned short     bPolarSmith : 1;
	} overlay;


	gboolean bSourceCoupled;

	guint areaWidth;
	guint areaHeight;

	gdouble leftMargin;
	gdouble rightMargin;

	gdouble gridWidth;
	gdouble gridHeight;

	gdouble bottomMargin;
	gdouble topMargin;

	gdouble textMargin;
	gdouble makerAreaWidth;

	gdouble fontSize;
	gdouble lineSpacing;
	gdouble scale;

	cairo_matrix_t initialMatrix;
} tGridParameters;

#define DEFAULT_GPIB_DEVICE_ID		16
#define DEFAULT_GPIB_CONTROLLER_NAME "hp82357b"
#define DEFAULT_GPIB_HP8753C_DEVICE_NAME "hp8753c"

#define D_HP8553C				0
#define D_HP33401A_MULTIMETER	1
#define A4882( pad, sad )	( (pad & 0xFF) | ((sad & 0xFF) << 8) )

extern GPIBdevice devices[];
extern GHashTable *widgetHashTable;

#define ERROR	(-1)
#define OK      ( 0)
#define CLEAR	( 0)

gpointer	threadGPIB (gpointer);

int openOrCreateDB();
void closeDB(void);

gint recoverCalibrationAndSetup( tGlobal *, gchar * );
gint recoverTraceData( tGlobal *, gchar * );
gint saveCalibrationAndSetup( tGlobal *, gchar * );
gint saveLearnStringAnalysis( tGlobal *, tLearnStringIndexes * );
gint saveTraceData( tGlobal *, gchar * );
gint inventorySavedSetupsAndCal(tGlobal *);
gint compareCalItem( gpointer , gpointer );
guint inventorySavedTraceNames( tGlobal * );
gint inventorySavedCalibrationKits( tGlobal * );
gint compareCalKitIdentifierItem( gpointer pCalKitIdentfierItem, gpointer sLabel );
gint recoverCalibrationKit(tGlobal *pGlobal, gchar *sLabel);
gint saveCalKit(tGlobal *pGlobal);
guint deleteDBentry( tGlobal *, gchar *, tDBtable );
void hide_Frame_Plot_B (tGlobal *);

void FORM1toDouble( guint8 *, gdouble *, gdouble * );
void initializeFORM1exponentTable( void );
void drawBezierSpline( cairo_t *ctx, const tComplex *pt, int cnt );
gboolean plotA( guint, guint, cairo_t *, tGlobal * );
gboolean plotB( guint, guint, cairo_t *, tGlobal * );
gint smithHighResPDF( tGlobal *, gchar *, eChannel );
void bezierControlPoints( const tLine *, const tLine *, tComplex *, tComplex * );
gchar* doubleToStringWithSpaces( gdouble freq, gchar *sUnits );
gchar* engNotation( gdouble, gint, tEngNotation, gchar ** );
void drawMarkers( cairo_t *, tGlobal *, tGridParameters *, eChannel , gdouble, gdouble );
void flipCairoText( cairo_t * );
void rightJustifiedCairoText( cairo_t *, gchar *, gdouble, gdouble );
void setCairoFontSize( cairo_t *, gdouble );
void setCairoColor( cairo_t *, eColor );
void drawHPlogo (cairo_t *, gchar *, gdouble , gdouble , gdouble );
void clearHP8753traces( tHP8753 * );
gint saveProgramOptions( tGlobal * );
gint recoverProgramOptions( tGlobal * ) ;
gboolean setGtkComboBox( GtkComboBox *, gchar * );
gint compareCalItemForSort( gpointer pCalItem1, gpointer pCalItem2 );
void setUseGPIBcardNoAndPID( tGlobal *, gboolean );
gint sendHP8753calibrationKit(gint descGPIB_HP8753, tGlobal *pGlobal, gint *pGPIBstatus );

extern tGlobal globalData;

typedef struct { gchar *code, *desc; } HP8753C_option;
extern const HP8753C_option optMeasurementType[];
extern const HP8753C_option optFormat[];
extern const HP8753C_option optCalType[];
extern const HP8753C_option optSweepType[];

extern const gchar *formatSymbols[];
extern const gchar *formatSmithOrPolarSymbols[][2];
extern const gchar *sweepSymbols[];
extern const gint numOfCalArrays[];

#define DISCOVERED_LS_INDEXES	0
extern tLearnStringIndexes learnStringIndexes[];

#undef SINGLE_SWEEP_BEFORE_SAVE
#define CLEAN_SWEEP_AFTER_RECALL

#define PERCENT(x,y) ((x) * (y)/100.0)
#define SQU(x) ((x) * (x))
#define fontSize( h, w ) (((h)<(w)*0.75 ? (h):(w)*0.75) / 50.0)
#define LIN_INTERP( lower, upper, fract) ((upper) * (fract) + (lower) * (1.0 - (fract)))
gint splineInterpolate( gint, tComplex [], double, tComplex * );
void CB_Radio_Calibration ( GtkRadioButton *, tGlobal * );
void showCalInfo( tHP8753cal *, tGlobal * );
gboolean addToComboBox( GtkComboBox *, gchar * );
void updateCalCombobox( gpointer , gpointer  );
void sensitiseControlsInUse( tGlobal *pGlobal, gboolean bSensitive );
GList *createIconList( void );
gint getTimeStamp( gchar ** );
void logVersion(void);
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

#define TIMEOUT_SWEEP	200		// if 10Hz RBW and 1601 points, it may take a long time to sweep

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

#undef DELTA_MARKER_ZERO
#endif /* HP8753COMMS_H_ */
