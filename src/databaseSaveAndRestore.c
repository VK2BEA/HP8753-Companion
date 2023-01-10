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

#include <hp8753.h>
#include <sqlite3.h>
#include <sys/stat.h>

#include "messageEvent.h"
#include "calibrationKit.h"

static sqlite3 *db = NULL;

/*!     \brief  Callback for every row in SQL query to fill combo box list
 *
 * An SQL query is made for trace profile names. This is called for each row
 * found. The name is then added to the combo box list of the trace selection control
 *
 * \ingroup database
 *
 * \param ppGList	  pointer to pointer to GList cairo context
 * \param nCols	      number of columns in row
 * \param columnData  array of column data text strings
 * \param columNames  array of names of columns
 * \return            0
 */
int
sqlCBtraceNames(void *ppGList, gint nCols, gchar **columnData, gchar **columnNames) {
	GList **ppList = ppGList;

	if( nCols >= 1 )
		*ppList = g_list_prepend(*ppList, g_strdup(columnData[0]));

	return 0;
}


/*!     \brief  Open Sqlite database (or create tables)
 *
 * Open the database and create tables if they do not exist.
 */
int
openOrCreateDB(void) {
	gchar *zErrMsg = 0;
	gint rc;
	gchar *sql[] = {
			"CREATE TABLE IF NOT EXISTS HP8753C_CALIBRATION("
				"name        NOT NULL,"
			    "channel     INTEGER,"
				"learn       BLOB, "
				"sweepStart  REAL,"
				"sweepStop   REAL,"
				"IFbandwidth REAL,"
				"CWfrequency REAL,"
				"sweepType   INTEGER,"
				"npoints     INTEGER,"
				"calType     INT,"
				"cal01       BLOB, cal02    BLOB, cal03    BLOB, cal04    BLOB,"
				"cal05       BLOB, cal06    BLOB, cal07    BLOB, cal08    BLOB,"
				"cal09       BLOB, cal10    BLOB, cal11    BLOB, cal12    BLOB,"
			    "notes       TEXT,"
			    "perChannelCalSettings    INTEGER,"
				"calSettings INTEGER,"
			    "PRIMARY KEY (name, channel)"
			");",
			"CREATE TABLE IF NOT EXISTS HP8753C_TRACEDATA("
				"name        	TEXT 	NOT NULL,"
				"channel     	INTEGER,"
				"sweepStart    	REAL,"
				"sweepStop     	REAL,"
				"IFbandwidth   	REAL,"
			    "CWfrequency    REAL,"
				"sweepType     	INTEGER,"
				"npoints       	INTEGER,"
				"points        	BLOB,"
				"stimulusPoints BLOB,"
				"format        	INTEGER,"
				"scaleVal      	REAL,"
				"scaleRefPos   	REAL,"
				"scaleRefVal   	REAL,"
				"sParamOrInputPort INTEGER,"
				"markers       	BLOB,"
				"activeMkr     	INTEGER,"
				"deltaMkr      	INTEGER,"
				"mkrType       	INTEGER,"
				"bandwidth     	BLOB,"
				"nSegments      INTEGER,"
			    "segments       BLOB,"
				"title			TEXT,"
			    "notes			TEXT,"
				"perChannelFlags    INTEGER,"
				"generalFlags       INTEGER,"
				"time			TEXT,"
				"PRIMARY KEY (name, channel)"
			");"
			"CREATE TABLE IF NOT EXISTS CAL_KITS("
				"label           TEXT,"
				"description     TEXT,"
				"standards       BLOB,"
			    "classes         BLOB,"
				"PRIMARY KEY (label)"
			");"
			"CREATE TABLE IF NOT EXISTS OPTIONS("
				"ID			 INTEGER NOT NULL DEFAULT 0,"
				"flags       INTEGER,"
				"GPIBcontrollerName	TEXT,"
				"GPIBdeviceName     TEXT,"
				"GPIBcontrollerCard INTEGER,"
				"GPIBdevicePID		INTEGER,"
				"GtkPrintSettings	BLOB,"
				"GtkPageSetup		BLOB,"
				"lastDirectory		TEXT,"
				"calProfile			TEXT,"
				"traceProfile		TEXT,"
				"learnStringIndexes BLOB,"
			    "product            TEXT,"
				"PRIMARY KEY (ID)"
			");"
	};
	gint i, rtn = ERROR;
	struct stat sb;
	gboolean bProblem = FALSE;
	gchar *DBdir = g_strdup_printf("%s/.local/share/hp8753c", getenv("HOME"));
	gchar *DBfile = g_strdup_printf("%s/hp8753c.db", DBdir);

	do {
		if ((rc = sqlite3_initialize()) != SQLITE_OK) {
			postMessageToMainLoop(TM_ERROR, "Cannot initialize Sqlite3");
			break;
		}

		if (stat(DBdir, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
			mkdir(DBdir, S_IRWXU);
		}

		if ( (rc = sqlite3_open_v2(DBfile,
				&db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) != SQLITE_OK ) {
			postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
			break;
		}

		// if the table(s) do not exist, create them
		for (i = 0; i < sizeof(sql) / sizeof(gchar*); i++) {
			if ((rc = sqlite3_exec(db, sql[i], NULL, 0, &zErrMsg)) != SQLITE_OK) {
				postMessageToMainLoop(TM_ERROR, zErrMsg);
				sqlite3_free(zErrMsg);
				bProblem = TRUE;
				break;
			}
		}

		if (bProblem)
			break;

		rtn = 0;
	} while ( FALSE);

	g_free(DBdir);
	g_free(DBfile);

	return rtn;
}

/*!     \brief  Free the g_malloced strings in the tHP8753cal structure
 *
 * When we delete a calibration profile we free the allocated memory.
 * When we recall from the database we also free the previously recalled
 * allocated strings
 *
 * \param pCalItem	pointer to tHP8753cal holding the allocated strings
 */
static void
freeCalItem( gpointer pCalItem ) {
	tHP8753cal *pCal = (tHP8753cal *)pCalItem;

	g_free( pCal->sName );
	g_free( pCal->sNote );

	g_free( pCal );
}

/*!     \brief  Comparison routine for sorting the calibration structures
 *
 * Determine if the name string in one tHP8753cal is greater, equal to or less than another.
 *
 * \param  pCalItem1	pointer to tHP8753cal holding the first name
 * \param  pCalItem1	pointer to tHP8753cal holding the second name
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareCalItemForSort( gpointer pCalItem1, gpointer pCalItem2 ) {
	if( pCalItem1 == NULL || pCalItem2 == NULL )
		return 0;
	else
		return strcmp(((tHP8753cal *)pCalItem1)->sName, ((tHP8753cal *)pCalItem2)->sName);
}

/*!     \brief  Comparison routine for searching for a calibration structures
 *
 * Determine if the name string in one tHP8753cal is greater, equal to or less than another.
 *
 * \param  pCalItem1    pointer to tHP8753cal holding the first name
 * \param  sName        pointer to name of the calibration profile
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareCalItem( gpointer pCalItem, gpointer sName ) {
	if( pCalItem == NULL || sName == NULL )
		return 0;
	else
		return strcmp(((tHP8753cal *)pCalItem)->sName, sName);
}

/*!     \brief  Get the skeleton data on setup and calibration profiles
 *
 * Populate a list of available setup and calibration profiles
 * including some basic data that is used to identify the profile.
 *
 * \param  pGlobal      pointer to tGlobal structure (containing the pointer to list)
 * \return 				completion status
 */
gint
inventorySavedSetupsAndCal(tGlobal *pGlobal) {

	g_list_free_full ( g_steal_pointer (&pGlobal->pCalList), (GDestroyNotify)freeCalItem );
	pGlobal->pCalList = NULL;

	tHP8753cal *pCal;
	sqlite3_stmt *stmt = NULL;
	gint queryIndex;
	gushort perChannelCalSettings, settings;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"   a.name, a.notes,"
			"   a.sweepStart, b.sweepStart, a.sweepStop, b.sweepStop,"
			"   a.IFbandwidth, b.IFbandwidth, a.CWfrequency, b.CWfrequency,"
			"   a.sweepType, b.sweepType, a.npoints, b.npoints, a.CalType, "
			"   b.CalType, a.perChannelCalSettings, b.perChannelCalSettings, a.calSettings "
			" FROM HP8753C_CALIBRATION a LEFT JOIN HP8753C_CALIBRATION b ON a.name=b.name"
			" WHERE a.channel=0 AND b.channel=1;", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			pCal = g_new0(tHP8753cal, 1);
			queryIndex = 0;

			// name
			pCal->sName = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			// note
			pCal->sNote = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			// sweepStart
			pCal->perChannelCal[ eCH_ONE ].sweepStart = sqlite3_column_double(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].sweepStart = sqlite3_column_double(stmt,  queryIndex++);
			// sweepStop
			pCal->perChannelCal[ eCH_ONE ].sweepStop = sqlite3_column_double(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].sweepStop = sqlite3_column_double(stmt,  queryIndex++);
			// IF bandwidth
			pCal->perChannelCal[ eCH_ONE ].IFbandwidth = sqlite3_column_double(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].IFbandwidth = sqlite3_column_double(stmt,  queryIndex++);
			// CW frequency
			pCal->perChannelCal[ eCH_ONE ].CWfrequency = sqlite3_column_double(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].CWfrequency = sqlite3_column_double(stmt,  queryIndex++);
			// SweepType
			pCal->perChannelCal[ eCH_ONE ].sweepType = sqlite3_column_int(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].sweepType = sqlite3_column_int(stmt,  queryIndex++);
			// nPoints
			pCal->perChannelCal[ eCH_ONE ].nPoints = sqlite3_column_int(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].nPoints = sqlite3_column_int(stmt,  queryIndex++);
			// CalType
			pCal->perChannelCal[ eCH_ONE ].iCalType = sqlite3_column_int(stmt,  queryIndex++);
			pCal->perChannelCal[ eCH_TWO ].iCalType = sqlite3_column_int(stmt,  queryIndex++);

			perChannelCalSettings = sqlite3_column_int(stmt, queryIndex++);
			memcpy( &pCal->perChannelCal[ eCH_ONE ].settings, &perChannelCalSettings, sizeof( gushort ) );
			perChannelCalSettings = sqlite3_column_int(stmt, queryIndex++);
			memcpy( &pCal->perChannelCal[ eCH_TWO ].settings, &perChannelCalSettings, sizeof( gushort ) );

			settings = sqlite3_column_int(stmt, queryIndex++);
			memcpy( &pCal->settings, &settings, sizeof( gushort ) );

			pGlobal->pCalList = g_list_prepend(pGlobal->pCalList, pCal);
		}
		sqlite3_finalize(stmt);
	}

	pGlobal->pCalList = g_list_sort (pGlobal->pCalList, (GCompareFunc)compareCalItemForSort);
	return OK;
}


/*!     \brief  Free the g_malloced strings in the tHP8753cal structure
 *
 * When we delete a calibration profile we free the allocated memory.
 * When we recall from the database we also free the previously recalled
 * allocated strings
 *
 * \param pCalItem	pointer to tHP8753cal holding the allocated strings
 */
void
freeCalKitIdentifierItem( gpointer pCalItem ) {
	tCalibrationKitIdentifier *pCal = (tCalibrationKitIdentifier *)pCalItem;

	g_free( pCal->sLabel );
	g_free( pCal->sDescription );

	g_free( pCal );
}


/*!     \brief  Comparison routine for sorting the calibration kit identifier structures
 *
 * Determine if the name string in one tCalibrationKitIdentifier is greater, equal to or less than another.
 *
 * \param  pCalItem1	pointer to tCalibrationKitIdentifier holding the first name
 * \param  pCalItem1	pointer tCalibrationKitIdentifier tHP8753cal holding the second name
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareCalKitIdentifierItemForSort( gpointer pCalKitIdenticierItem1, gpointer pCalKitIdenticierItem2 ) {
	if( pCalKitIdenticierItem1 == NULL || pCalKitIdenticierItem2 == NULL )
		return 0;
	else
		return strcmp(((tCalibrationKitIdentifier *)pCalKitIdenticierItem1)->sLabel,
				((tCalibrationKitIdentifier *)pCalKitIdenticierItem2)->sLabel);
}


/*!     \brief  Comparison routine for searching for a calibration structures
 *
 * Determine if the label string in one tCalibrationKitIdentifier is greater, equal to or less than another.
 *
 * \param  pCalKitIdentfierItem    pointer to tCalibrationKitIdentifier holding the first label
 * \param  sLabel                  pointer to label of the calibration kit
 * \return 0                       for equal, +ve for greater than and -ve for less than
 */
gint
compareCalKitIdentifierItem( gpointer pCalKitIdentfierItem, gpointer sLabel ) {
	if( pCalKitIdentfierItem == NULL || sLabel == NULL )
		return 0;
	else
		return strcmp(((tCalibrationKitIdentifier *)pCalKitIdentfierItem)->sLabel, sLabel);
}


/*!     \brief  Get the names of the saved trace profiles
 *
 * Populate a list of available trace profiles
 *
 * \param  pGlobal      pointer to tGlobal structure (containing the pointer to list)
 * \return 				completion status
 */
guint
inventorySavedTraceNames(tGlobal *pGlobal) {
	gchar *zErrMsg = 0;
	g_list_free_full ( g_steal_pointer (&pGlobal->pTraceList), (GDestroyNotify)g_free );
	pGlobal->pTraceList = NULL;

	if (sqlite3_exec(db, "SELECT name FROM HP8753C_TRACEDATA WHERE channel=0;",
			sqlCBtraceNames, &pGlobal->pTraceList, &zErrMsg) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, zErrMsg);
		sqlite3_free(zErrMsg);
		return ERROR;
	}
	pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)strcmp);
	return OK;
}

/*!     \brief  Save the trace profile
 *
 * Save the selected trace data to the database
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sName         trace profile identifier
 * \return 				completion status
 */
gint
saveTraceData(tGlobal *pGlobal, gchar *sName) {

	sqlite3_stmt *stmt = NULL;
	guint perChannelFlags, generalFlags;
	gint queryIndex;

	g_free( pGlobal->sTraceProfile );
	pGlobal->sTraceProfile = g_strdup( sName );
		// Source information
	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO HP8753C_TRACEDATA"
			"  (name, channel, sweepStart, sweepStop, IFbandwidth, "
			"   CWfrequency, sweepType, npoints, points, stimulusPoints, "
			"   format, scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort, "
			"   markers, activeMkr, deltaMkr, mkrType, bandwidth, "
			"   nSegments, segments, title, notes, perChannelFlags, "
			"   generalFlags, time)"
			" VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?)", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}
	for (eChannel channel = 0; channel < eNUM_CH; channel++) {
		queryIndex = 0;
		// name
		if (sqlite3_bind_text(stmt, ++queryIndex, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// channel
		if (sqlite3_bind_int(stmt, ++queryIndex, channel) != SQLITE_OK)
			goto err;
		// sweepStart
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].sweepStart) != SQLITE_OK)
			goto err;
		// sweepStop
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].sweepStop) != SQLITE_OK)
			goto err;
		// IFbandwidth
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].IFbandwidth) != SQLITE_OK)
			goto err;

		// CWfrequency
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].CWfrequency) != SQLITE_OK)
			goto err;
		// sweepType
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].sweepType) != SQLITE_OK)
			goto err;
		// npoints
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].nPoints) != SQLITE_OK)
			goto err;
		// points
		if (sqlite3_bind_blob(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].responsePoints,
				pGlobal->HP8753.channels[channel].nPoints * sizeof(tComplex), SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// stimulusPoints
		if( pGlobal->HP8753.channels[channel].stimulusPoints ) {
			if (sqlite3_bind_blob(stmt, ++queryIndex,
					pGlobal->HP8753.channels[channel].stimulusPoints,
					pGlobal->HP8753.channels[channel].nPoints * sizeof(tComplex), SQLITE_STATIC) != SQLITE_OK)
				goto err;
		} else {
			++queryIndex;
		}

		// format
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].format) != SQLITE_OK)
			goto err;
		//scaleVal
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].scaleVal) != SQLITE_OK)
			goto err;
		// scaleRefPos
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].scaleRefPos) != SQLITE_OK)
			goto err;
		// scaleRefVal
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].scaleRefVal) != SQLITE_OK)
			goto err;
		// sParamOrInputPort
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].measurementType) != SQLITE_OK)
			goto err;

		// markers
		if (sqlite3_bind_blob(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].numberedMarkers,
				MAX_MKRS * sizeof(tMarker), SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// activeMkr
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].activeMarker) != SQLITE_OK)
			goto err;
		// deltaMkr
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].deltaMarker) != SQLITE_OK)
			goto err;
		// mkrType
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].mkrType) != SQLITE_OK)
			goto err;
		// bandwidth
		if (sqlite3_bind_blob(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].bandwidth,
				sizeof(pGlobal->HP8753.channels[channel].bandwidth), SQLITE_STATIC) != SQLITE_OK)
			goto err;

		// nSegments
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].nSegments) != SQLITE_OK)
			goto err;
		// segments
		if (sqlite3_bind_blob(stmt, ++queryIndex,
				pGlobal->HP8753.channels[channel].segments,
				MAX_SEGMENTS * sizeof(tSegment), SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// title
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.sTitle, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// notes
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.sNote, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		memcpy(&perChannelFlags, &pGlobal->HP8753.channels[channel].chFlags, sizeof(gushort));
		memcpy(&generalFlags, &pGlobal->HP8753.flags, sizeof(gushort));
		// perChannelFlags
		if (sqlite3_bind_int(stmt, ++queryIndex, perChannelFlags) != SQLITE_OK)
			goto err;

		// generalFlags
		if (sqlite3_bind_int(stmt, ++queryIndex, generalFlags) != SQLITE_OK)
			goto err;
		// time
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.dateTime, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;

		if (sqlite3_step(stmt) != SQLITE_DONE)
			goto err;

		sqlite3_reset( stmt );
		sqlite3_clear_bindings( stmt );
	}
	sqlite3_finalize(stmt);
	return 0;

err: postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return ERROR;
}

/*!     \brief  Recover the saved trace profile
 *
 * Get the data of the named profile from the database
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sName        name of the profile to recover
 * \return 			   completion status
 */
gint
recoverTraceData(tGlobal *pGlobal, gchar *sName) {
	sqlite3_stmt *stmt = NULL;
	gint nPoints, pointsSize, mkrSize, bandwidthSize, segmentsSize;
	const guchar *points = NULL, *markers = NULL, *bandwidth = NULL, *segments=NULL;
	eChannel channel = eCH_SINGLE;
	const gchar *tText;
	gboolean bTraceRetrieved = FALSE;
	gint queryIndex;
	gushort perChannelFlags, generalFlags;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"   channel, sweepStart, sweepStop, IFbandwidth, CWfrequency, "
			"   sweepType, npoints, points, stimulusPoints, format, "
			"   scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort, markers, "
			"   activeMkr, deltaMkr, mkrType, bandwidth, nSegments, "
			"   segments, title, notes, perChannelFlags, generalFlags, "
			"   time"
			" FROM HP8753C_TRACEDATA WHERE name = (?);", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		sqlite3_bind_text(stmt, 1, sName, strlen(sName), SQLITE_STATIC);
		// We should get one row per channel
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			queryIndex = 0;
			bTraceRetrieved = TRUE;

			channel     = sqlite3_column_int(stmt,     queryIndex++);
			pGlobal->HP8753.channels[channel].sweepStart   = sqlite3_column_double(stmt,  queryIndex++);
			pGlobal->HP8753.channels[channel].sweepStop    = sqlite3_column_double(stmt,  queryIndex++);
			pGlobal->HP8753.channels[channel].IFbandwidth  = sqlite3_column_double(stmt,  queryIndex++);
			pGlobal->HP8753.channels[channel].CWfrequency  = sqlite3_column_double(stmt,  queryIndex++);
			pGlobal->HP8753.channels[channel].sweepType    = sqlite3_column_int(stmt,    queryIndex++);

			nPoints = sqlite3_column_int(stmt, queryIndex++);
			// points
			pointsSize = sqlite3_column_bytes(stmt, queryIndex);
			points = sqlite3_column_blob(stmt, queryIndex++);
			g_free(pGlobal->HP8753.channels[channel].responsePoints);
			if (pointsSize > 0 && nPoints > 0) {
				pGlobal->HP8753.channels[channel].responsePoints = g_memdup2(points, pointsSize);
				pGlobal->HP8753.channels[channel].nPoints = nPoints;
			} else {
				pGlobal->HP8753.channels[channel].nPoints = 0;
				pGlobal->HP8753.channels[channel].responsePoints = NULL;
			}
			// stimulus points
			pointsSize = sqlite3_column_bytes(stmt, queryIndex);
			points = sqlite3_column_blob(stmt, queryIndex++);
			g_free(pGlobal->HP8753.channels[channel].stimulusPoints);
			if (pointsSize > 0 && nPoints > 0) {
				pGlobal->HP8753.channels[channel].stimulusPoints = g_memdup2(points, pointsSize);
			} else {
				pGlobal->HP8753.channels[channel].stimulusPoints = NULL;
			}

			pGlobal->HP8753.channels[channel].format = sqlite3_column_int(stmt, queryIndex++);
			pGlobal->HP8753.channels[channel].scaleVal = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753.channels[channel].scaleRefPos = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753.channels[channel].scaleRefVal =  sqlite3_column_double(stmt, queryIndex++);

			pGlobal->HP8753.channels[channel].measurementType = sqlite3_column_int(stmt, queryIndex++);


			mkrSize = sqlite3_column_bytes(stmt, queryIndex);
			markers = sqlite3_column_blob(stmt, queryIndex++);
			if( mkrSize > 0 )
				memcpy( (guchar*)&pGlobal->HP8753.channels[channel].numberedMarkers, markers, mkrSize);
			else
				memset( pGlobal->HP8753.channels[channel].numberedMarkers, 0, sizeof(pGlobal->HP8753.channels[channel].numberedMarkers));

			pGlobal->HP8753.channels[channel].activeMarker = sqlite3_column_int(stmt,queryIndex++);
			pGlobal->HP8753.channels[channel].deltaMarker = sqlite3_column_int(stmt, queryIndex++);
			pGlobal->HP8753.channels[channel].mkrType = sqlite3_column_int(stmt, queryIndex++);

			bandwidthSize = sqlite3_column_bytes(stmt, queryIndex);
			bandwidth = sqlite3_column_blob(stmt, queryIndex++);
			if (bandwidthSize == sizeof( pGlobal->HP8753.channels[channel].bandwidth ))
				memcpy( (guchar*)&pGlobal->HP8753.channels[channel].bandwidth, bandwidth, bandwidthSize);
			else
				memset( pGlobal->HP8753.channels[channel].bandwidth, 0, sizeof( pGlobal->HP8753.channels[channel].bandwidth ));

			pGlobal->HP8753.channels[channel].nSegments = sqlite3_column_int(stmt, queryIndex++);
			segmentsSize = sqlite3_column_bytes(stmt, queryIndex);
			segments = sqlite3_column_blob(stmt, queryIndex++);
			if (segmentsSize == sizeof( tSegment ) * MAX_SEGMENTS )
				memcpy( (guchar*)&pGlobal->HP8753.channels[channel].segments, segments, segmentsSize);
			else
				memset( pGlobal->HP8753.channels[channel].bandwidth, 0, sizeof( pGlobal->HP8753.channels[channel].bandwidth ));

			if( channel == eCH_ONE ) {
				tText = (const gchar *)sqlite3_column_text(stmt, queryIndex++);
				g_free( pGlobal->HP8753.sTitle );
				pGlobal->HP8753.sTitle = g_strdup( tText );
				tText = (const gchar *)sqlite3_column_text(stmt, queryIndex++);
				g_free( pGlobal->HP8753.sNote );
				pGlobal->HP8753.sNote = g_strdup( tText );
			} else {
				queryIndex +=2;
			}

			perChannelFlags = sqlite3_column_int(stmt, queryIndex++);
			memcpy(&pGlobal->HP8753.channels[channel].chFlags, &perChannelFlags, sizeof(gushort));

			if( channel == eCH_ONE ) {
				generalFlags = sqlite3_column_int(stmt, queryIndex++);
				memcpy(&pGlobal->HP8753.flags, &generalFlags, sizeof(gushort));
				pGlobal->HP8753.dateTime = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			} else {
				queryIndex +=2;
			}
		}
		sqlite3_finalize(stmt);
	}

	return bTraceRetrieved;
}

/*!     \brief  Delete the identified profile
 *
 * Remove either a setup/calibration profile or a trace profile
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sName        name of profile
 * \param bCalibration true if the array to delete is setup/calibration, false for trace
 * \return 			   completion status
 */
guint
deleteDBentry(tGlobal *pGlobal, gchar *sName, tDBtable whichTable) {
	sqlite3_stmt *stmt = NULL;
	gchar *sSQL = NULL;
	GList *listElement = NULL;

	switch( whichTable ) {
	case eDB_CALandSETUP:
		sSQL = "DELETE FROM HP8753C_CALIBRATION WHERE name = (?);";
		listElement = g_list_find_custom( pGlobal->pCalList, sName, (GCompareFunc)compareCalItem );
		if( listElement ) {
			freeCalItem(  listElement->data );
			pGlobal->pCalList = g_list_remove( pGlobal->pCalList, listElement->data );
		}
		break;
	case eDB_TRACE:
		sSQL = "DELETE FROM HP8753C_TRACEDATA WHERE name = (?);";
		listElement = g_list_find_custom( pGlobal->pTraceList, sName, (GCompareFunc)strcmp );
		if( listElement ) {
			g_free( listElement->data );
			pGlobal->pTraceList = g_list_remove( pGlobal->pTraceList, listElement->data );
		}
		break;
	case eDB_CALKIT:
		sSQL = "DELETE FROM CAL_KITS WHERE label = (?);";
		listElement = g_list_find_custom( pGlobal->pCalKitList, sName, (GCompareFunc)compareCalKitIdentifierItem );
		if( listElement ) {
			freeCalKitIdentifierItem(  listElement->data );
			pGlobal->pCalKitList = g_list_remove( pGlobal->pCalKitList, listElement->data );
		}
		break;
	default:
		return 0;
		break;
	}

	if (sqlite3_prepare_v2(db, sSQL, -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		if (sqlite3_bind_text(stmt, 1, sName, STRLENGTH,
				SQLITE_STATIC) != SQLITE_OK)
			goto err;
	}
	if (sqlite3_step(stmt) != SQLITE_DONE)
		goto err;
	sqlite3_finalize(stmt);

	return 0;
err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return ERROR;
}

/*!     \brief  Save the identified setup/calibration profile
 *
 * Save the identified setup/calibration profile
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sName        name of profile
 * \return 			   completion status
 */
gint
saveCalibrationAndSetup(tGlobal *pGlobal, gchar *sName) {

	sqlite3_stmt *stmt = NULL;
	guint perChannelCalSettings, calSettings;
	gint  queryIndex;

	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO HP8753C_CALIBRATION "
			" (name,  channel, learn, sweepStart, sweepStop,"
			"  IFbandwidth, CWfrequency, sweepType, npoints, calType,"
			"  cal01, cal02, cal03, cal04, cal05, "
			"  cal06, cal07, cal08, cal09, cal10, "
			"  cal11, cal12, notes, perChannelCalSettings, calSettings)"
			"  VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?)", -1, &stmt,
			NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	for( eChannel channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		queryIndex = 0;

		// name
		if (sqlite3_bind_text(stmt, ++queryIndex, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// channel
		if (sqlite3_bind_int(stmt, ++queryIndex, channel) != SQLITE_OK)
			goto err;
		//learn
		if( channel == eCH_ONE ) {
			if (sqlite3_bind_blob(stmt, ++queryIndex, pGlobal->HP8753cal.pHP8753C_learn,
					GUINT16_FROM_BE( *(guint16* )(pGlobal->HP8753cal.pHP8753C_learn + 2))
							+ 4, SQLITE_STATIC) != SQLITE_OK)
				goto err;
		} else {
			++queryIndex;
		}
		// sweepStart
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].sweepStart) != SQLITE_OK)
			goto err;
		// sweepStop
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].sweepStop) != SQLITE_OK)
			goto err;

		// IFbandwidth
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].IFbandwidth) != SQLITE_OK)
			goto err;
		// CWfrequency
		if (sqlite3_bind_double(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].CWfrequency) != SQLITE_OK)
			goto err;
		// sweepType
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].sweepType) != SQLITE_OK)
			goto err;
		// npoints
		if (sqlite3_bind_int(stmt, ++queryIndex,
				pGlobal->HP8753cal.perChannelCal[channel].nPoints) != SQLITE_OK)
			goto err;
		// calType
		if (sqlite3_bind_int(stmt, ++queryIndex, pGlobal->HP8753cal.perChannelCal[channel].iCalType) != SQLITE_OK)
			goto err;
		// cal01 to cal12
		for (int i = 0; i < MAX_CAL_ARRAYS; i++) {
			gint length;
			if( i < numOfCalArrays[pGlobal->HP8753cal.perChannelCal[channel].iCalType] )
				length = GUINT16_FROM_BE( *(guint16* )(pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] + 2)) + 4;
			else
				length = 0;
			if (sqlite3_bind_blob(stmt, ++queryIndex, pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i], length,
					SQLITE_STATIC) != SQLITE_OK)
				goto err;
		}
		// notes
		if( channel == eCH_ONE ) {
			if (pGlobal->HP8753cal.sNote)
				if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753cal.sNote, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
					goto err;
		} else {
			++queryIndex;
		}

		memcpy(&perChannelCalSettings, &pGlobal->HP8753cal.perChannelCal[channel].settings, sizeof(gushort));
		memcpy(&calSettings, &pGlobal->HP8753cal.settings, sizeof(gushort));
		// perChannelCalSettings
		if (sqlite3_bind_int(stmt, ++queryIndex, perChannelCalSettings) != SQLITE_OK)
			goto err;
		// calSettings
		if( channel == eCH_ONE ) {
			if (sqlite3_bind_int(stmt, ++queryIndex, calSettings) != SQLITE_OK)
						goto err;
		} else {
			++queryIndex;
		}

		if (sqlite3_step(stmt) != SQLITE_DONE)
			goto err;

		sqlite3_reset( stmt );
		sqlite3_clear_bindings( stmt );
	}
	sqlite3_finalize(stmt);

	GList *calPreviewElement = g_list_find_custom( pGlobal->pCalList, sName, (GCompareFunc)compareCalItem );
	if( calPreviewElement ) {
		freeCalItem( calPreviewElement->data );
		pGlobal->pCalList = g_list_remove( pGlobal->pCalList, calPreviewElement->data );
	}
	tHP8753cal *pCal = g_new0(tHP8753cal, 1);

	pCal->sName = g_strdup( sName );
	pCal->sNote = g_strdup( pGlobal->HP8753cal.sNote );
	for( eChannel channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		pCal->perChannelCal[ channel ].sweepStart = pGlobal->HP8753cal.perChannelCal[ channel ].sweepStart;
		pCal->perChannelCal[ channel ].sweepStop = pGlobal->HP8753cal.perChannelCal[ channel ].sweepStop;
		pCal->perChannelCal[ channel ].IFbandwidth = pGlobal->HP8753cal.perChannelCal[ channel ].IFbandwidth;
		pCal->perChannelCal[ channel ].CWfrequency = pGlobal->HP8753cal.perChannelCal[ channel ].CWfrequency;
		pCal->perChannelCal[ channel ].sweepType = pGlobal->HP8753cal.perChannelCal[ channel ].sweepType;
		pCal->perChannelCal[ channel ].nPoints = pGlobal->HP8753cal.perChannelCal[ channel ].nPoints;
		memcpy( &pCal->perChannelCal[ channel ].settings, &pGlobal->HP8753cal.perChannelCal[ channel ].settings, sizeof( gushort ) );
	}
	memcpy( &pCal->settings, &pGlobal->HP8753cal.settings, sizeof( gushort ) );

	pGlobal->pCalList = g_list_insert_sorted( pGlobal->pCalList, pCal, (GCompareFunc)compareCalItemForSort );

	return 0;

err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return ERROR;
}

/*!     \brief  Recover identified setup/calibration profile
 *
 * Recover the identified setup/calibration profile
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sName        name of profile
 * \return 			   completion status
 */
gint
recoverCalibrationAndSetup(tGlobal *pGlobal, gchar *sName) {
	sqlite3_stmt *stmt = NULL;
	gint length;
	eChannel channel = eCH_SINGLE;
	const guchar *tBlob;
	const gchar *tText;
	gint queryIndex;
	gboolean bFound = FALSE;
	gushort perChannelCalSettings, calSettings;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"  channel, learn, sweepStart, sweepStop, IFbandwidth,"
			"  CWfrequency, sweepType, npoints, calType, cal01,"
			"  cal02, cal03, cal04, cal05, cal06, "
			"  cal07, cal08, cal09, cal10, cal11, "
			"  cal12, notes, perChannelCalSettings, calSettings "
			"  FROM HP8753C_CALIBRATION"
			" WHERE name = (?);", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		sqlite3_bind_text(stmt, 1, sName, strlen(sName), SQLITE_STATIC);
		// Line for ch 1 and ch 2
		// learn, title, notes, flags and options are only taken from ch 1
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			bFound = TRUE;
			queryIndex = 0;

			// Channel
			channel = sqlite3_column_int(stmt, queryIndex++);

			// Learn string
			length  = sqlite3_column_bytes(stmt, queryIndex);
			tBlob   = sqlite3_column_blob(stmt, queryIndex++);
			if( channel == eCH_ONE ) {
				g_free(pGlobal->HP8753cal.pHP8753C_learn);
				pGlobal->HP8753cal.pHP8753C_learn = g_memdup2(tBlob, (gsize) length);
			}

			pGlobal->HP8753cal.perChannelCal[channel].sweepStart   = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753cal.perChannelCal[channel].sweepStop    = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753cal.perChannelCal[channel].IFbandwidth  = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753cal.perChannelCal[channel].CWfrequency  = sqlite3_column_double(stmt, queryIndex++);
			pGlobal->HP8753cal.perChannelCal[channel].sweepType    = sqlite3_column_int(stmt,    queryIndex++);
			pGlobal->HP8753cal.perChannelCal[channel].nPoints      = sqlite3_column_int(stmt,    queryIndex++);

			// calType
			pGlobal->HP8753cal.perChannelCal[channel].iCalType = sqlite3_column_int(stmt, queryIndex++);

			// calArrays
			for (int i = 0; i < MAX_CAL_ARRAYS; i++) {
				length = sqlite3_column_bytes(stmt, queryIndex);
				g_free(pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i]);
				tBlob = sqlite3_column_blob(stmt, queryIndex++);
				if (length > 0) {
					pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] = g_memdup2(tBlob, (gsize) length);
					// We infer size from cal string .. its only used for informational purposes
					if( i==0 && length > 4 ) {
						pGlobal->HP8753cal.perChannelCal[channel].nPoints = GUINT16_FROM_BE( (guint16 *)&tBlob[2] ) / BYTES_PER_CALPOINT;
					}
				} else {
					pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] = NULL;
				}
			}

			tText = (const gchar *)sqlite3_column_text(stmt, queryIndex++);
			if( channel == eCH_ONE ) {
				g_free(pGlobal->HP8753cal.sNote);
				pGlobal->HP8753cal.sNote = g_strdup(tText);
			}

			perChannelCalSettings = sqlite3_column_int(stmt, queryIndex++);
			memcpy( &pGlobal->HP8753cal.perChannelCal[ channel ].settings, &perChannelCalSettings, sizeof( gushort ) );

			if( channel == eCH_ONE ) {
				calSettings = sqlite3_column_int(stmt, queryIndex++);
				memcpy( &pGlobal->HP8753cal.settings, &calSettings, sizeof( gushort ) );
			} else {
				queryIndex++;
			}
		}
		sqlite3_finalize(stmt);
	}

	return bFound;
}

/*!     \brief  Save the program options
 *
 * Save the program options (on shutdown)
 *
 * \param pGlobal      pointer to tGlobal structure
 * \return 			   completion status
 */
gint
saveProgramOptions(tGlobal *pGlobal) {

	sqlite3_stmt *stmt = NULL;
	gushort options;
	GBytes *byPage = NULL;
	GBytes *byPrintSettings = NULL;
	gint queryIndex;

	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO OPTIONS"
			" (ID, flags, GPIBcontrollerName, GPIBdeviceName, GPIBcontrollerCard, "
			"  GPIBdevicePID, GtkPrintSettings, GtkPageSetup, lastDirectory, calProfile, "
			"  traceProfile, learnStringIndexes, product"
			" )"
			" VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}
	queryIndex = 0;
	if (sqlite3_bind_int(stmt, ++queryIndex, 0) != SQLITE_OK)		// always 0 for now
		goto err;

	memcpy( &options, &pGlobal->flags, sizeof( gushort ));
	if (sqlite3_bind_int(stmt, ++queryIndex, options) != SQLITE_OK)
		goto err;
	// No longer using sGPIBcontrollerName
	++queryIndex;
	if( pGlobal->sGPIBdeviceName ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->sGPIBdeviceName, strlen(pGlobal->sGPIBdeviceName), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if (sqlite3_bind_int(stmt, ++queryIndex, pGlobal->GPIBcontrollerIndex) != SQLITE_OK)
		goto err;
	if (sqlite3_bind_int(stmt, ++queryIndex, pGlobal->GPIBdevicePID) != SQLITE_OK)
		goto err;
	if( pGlobal->printSettings ) {
		GVariant *vPrintSettings = gtk_print_settings_to_gvariant( pGlobal->printSettings );
		byPrintSettings = g_variant_get_data_as_bytes( vPrintSettings );
		g_variant_unref( vPrintSettings );
		gsize size;
		gconstpointer pPrintSettingsBytes= g_bytes_get_data( byPrintSettings, &size );
		if (sqlite3_bind_blob(stmt, ++queryIndex, pPrintSettingsBytes, size,
				SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->pageSetup ) {
		GVariant *vPage = gtk_page_setup_to_gvariant( pGlobal->pageSetup );
		byPage = g_variant_get_data_as_bytes( vPage );
		g_variant_unref( vPage );
		gsize size;
		gconstpointer pPageBytes= g_bytes_get_data( byPage, &size );
		if (sqlite3_bind_blob(stmt, ++queryIndex, pPageBytes, size,
				SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->sLastDirectory ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->sLastDirectory, strlen( pGlobal->sLastDirectory ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->sCalProfile ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->sCalProfile, strlen( pGlobal->sCalProfile ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->sTraceProfile ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->sTraceProfile, strlen( pGlobal->sTraceProfile ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->HP8753.analyzedLSindexes.version != 0 ) {
		if (sqlite3_bind_blob(stmt, ++queryIndex, &pGlobal->HP8753.analyzedLSindexes,
								sizeof( tLearnStringIndexes ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}
	if( pGlobal->HP8753.sProduct ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.sProduct, strlen( pGlobal->HP8753.sProduct ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE)
		goto err;
	sqlite3_finalize(stmt);

	g_bytes_unref( byPrintSettings );
	g_bytes_unref( byPage );

	return OK;
err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);

	return ERROR;
}

/*!     \brief  Save the learn string analysis
 *
 * After an analysis is done, we save it to the database
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param pLSanalysis  pointer to the tLearnStringIndexes structure
 * \return 			   completion status
 */
gint
saveLearnStringAnalysis( tGlobal *pGlobal, tLearnStringIndexes *pLSanalysis ) {
	sqlite3_stmt *stmt = NULL;

	if (sqlite3_prepare_v2(db,
			"UPDATE OPTIONS"
			" SET learnStringIndexes = ?;",
			-1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	if (sqlite3_bind_blob(stmt, 1, &pGlobal->HP8753.analyzedLSindexes,
							sizeof( tLearnStringIndexes ), SQLITE_STATIC) != SQLITE_OK)
		goto err;
	if (sqlite3_step(stmt) != SQLITE_DONE)
		goto err;
	sqlite3_finalize(stmt);
	return OK;
err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return ERROR;
}

/*!     \brief  Recover the option settings
 *
 * Recover the option settings upon startup
 *
 * \param pGlobal      pointer to tGlobal structure
 * \return 			   completion status
 */
gint
recoverProgramOptions(tGlobal *pGlobal) {
	sqlite3_stmt *stmt = NULL;
	gint length, size;
	const guchar *tBlob;
	gint queryIndex;
	gushort options;

	if (sqlite3_prepare_v2(db,
			"SELECT flags, GPIBcontrollerName, GPIBdeviceName, "
			"  GPIBcontrollerCard, GPIBdevicePID, "
			"  GtkPrintSettings, GtkPageSetup, lastDirectory, calProfile, traceProfile, learnStringIndexes, product FROM OPTIONS;",
			-1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			queryIndex = 0;
			options = sqlite3_column_int(stmt, queryIndex++);
			memcpy(&pGlobal->flags, &options, sizeof(gushort));
			globalData.flags.bRunning = TRUE;

			setUseGPIBcardNoAndPID(pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_UseGPIB_ID" )), pGlobal->flags.bGPIB_UseCardNoAndPID);
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_SmithSpline" )), pGlobal->flags.bSmithSpline);
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_ShowDateTime" )), pGlobal->flags.bShowDateTime);
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_SmithGBnotRX" )), pGlobal->flags.bAdmitanceSmith);
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_DeltaMarkerAbsolute" )), !pGlobal->flags.bDeltaMarkerZero);
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_UserCalKit" )), pGlobal->flags.bSaveUserKit);

			// No longer using sGPIBcontrollerName
			queryIndex++;

			g_free( pGlobal->sGPIBdeviceName );
			pGlobal->sGPIBdeviceName = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			gtk_entry_set_text( GTK_ENTRY(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Entry_GPIB_HP8753" )), pGlobal->sGPIBdeviceName);

			pGlobal->GPIBcontrollerIndex = sqlite3_column_int(stmt, queryIndex++);
			gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Spin_GPIB_Controller_Card" )), pGlobal->GPIBcontrollerIndex);

			pGlobal->GPIBdevicePID = sqlite3_column_int(stmt, queryIndex++);
			gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Spin_GPIB_HP8753_ID" )), pGlobal->GPIBdevicePID);


			length = sqlite3_column_bytes(stmt, queryIndex);
			if( length > 0 ) {
				tBlob = sqlite3_column_blob(stmt, queryIndex++);
				GBytes *byPrintSettings = g_bytes_new( tBlob, length );
				GVariant *vPrintSettings =
						g_variant_new_from_bytes ( G_VARIANT_TYPE_VARDICT, byPrintSettings, TRUE );
				g_bytes_unref( byPrintSettings );
				if( pGlobal->printSettings != NULL  )
				g_object_unref (pGlobal->printSettings);
				pGlobal->printSettings = gtk_print_settings_new_from_gvariant( vPrintSettings );
			} else {
				queryIndex++;
			}
			length = sqlite3_column_bytes(stmt, queryIndex);
			if( length > 0 ) {
				tBlob = sqlite3_column_blob(stmt, queryIndex++);
				GBytes *byPageSetup = g_bytes_new( tBlob, length );
				GVariant *vPageSetup =
						g_variant_new_from_bytes ( G_VARIANT_TYPE_VARDICT, byPageSetup, TRUE );
				g_bytes_unref( byPageSetup );
				if( pGlobal->pageSetup != NULL )
					g_object_unref (pGlobal->pageSetup);
				pGlobal->pageSetup = gtk_page_setup_new_from_gvariant( vPageSetup );
			} else {
				queryIndex++;
			}
			pGlobal->sLastDirectory = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			pGlobal->sCalProfile = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			pGlobal->sTraceProfile = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );

			size = sqlite3_column_bytes(stmt, queryIndex);
			tBlob = sqlite3_column_blob(stmt, queryIndex++);
			if( size == sizeof( tLearnStringIndexes ))
				 memcpy( &pGlobal->HP8753.analyzedLSindexes, tBlob, sizeof( tLearnStringIndexes ));

			pGlobal->HP8753.sProduct = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );
		}

		sqlite3_finalize(stmt);
	}
	return 0;
}


/*!     \brief  Get the label & description of saved calibration kits
 *
 * Populate a list of available calibration kits
 * including the description.
 *
 * \param  pGlobal      pointer to tGlobal structure (containing the pointer to list)
 * \return 				completion status
 */
gint
inventorySavedCalibrationKits(tGlobal *pGlobal) {

	g_list_free_full ( g_steal_pointer (&pGlobal->pCalKitList), (GDestroyNotify)freeCalItem );
	pGlobal->pCalKitList = NULL;

	tCalibrationKitIdentifier *pCal;
	sqlite3_stmt *stmt = NULL;
	gint queryIndex;

	if (sqlite3_prepare_v2(db,
			"SELECT a.label, a.description "
			" FROM CAL_KITS a;", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			pCal = g_new0(tCalibrationKitIdentifier, 1);
			queryIndex = 0;

			// label
			pCal->sLabel = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			// description
			pCal->sDescription = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );

			pGlobal->pCalKitList = g_list_prepend(pGlobal->pCalKitList, pCal);
		}
		sqlite3_finalize(stmt);
	}

	pGlobal->pCalKitList = g_list_sort (pGlobal->pCalKitList, (GCompareFunc)compareCalKitIdentifierItemForSort);
	return OK;
}

/*!     \brief  Save the calibration kit
 *
 * Save the program options (on shutdown)
 *
 * \param pGlobal      pointer to tGlobal structure
 * \return 			   completion status
 */
gint
saveCalKit(tGlobal *pGlobal) {

	sqlite3_stmt *stmt = NULL;
	tCalibrationKitIdentifier *pCal;
	gint queryIndex;

	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO CAL_KITS"
			" (label, description, standards, classes)"
			" VALUES (?,?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}
	queryIndex = 0;
	if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753calibrationKit.label,
			strlen( pGlobal->HP8753calibrationKit.label ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753calibrationKit.description,
			strlen( pGlobal->HP8753calibrationKit.description ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	if (sqlite3_bind_blob(stmt, ++queryIndex, &pGlobal->HP8753calibrationKit.calibrationStandards,
			sizeof( tHP8753calibrationStandard) * MAX_CAL_STANDARDS, SQLITE_STATIC) != SQLITE_OK)
		goto err;
	if (sqlite3_bind_blob(stmt, ++queryIndex, &pGlobal->HP8753calibrationKit.calibrationClasses,
			sizeof( tHP8753calibrationClass) * MAX_CAL_CLASSES, SQLITE_STATIC) != SQLITE_OK)
		goto err;

	if (sqlite3_step(stmt) != SQLITE_DONE)
		goto err;
	sqlite3_finalize(stmt);

	GList *listElement = g_list_find_custom( pGlobal->pCalKitList,
							pGlobal->HP8753calibrationKit.label, (GCompareFunc)compareCalKitIdentifierItem );
	if( listElement ) {
		g_free(  ((tCalibrationKitIdentifier *)listElement->data)->sDescription );
		((tCalibrationKitIdentifier *)listElement->data)->sDescription = g_strdup( pGlobal->HP8753calibrationKit.description );
	} else {
		pCal = g_new0(tCalibrationKitIdentifier, 1);
		// label
		pCal->sLabel = g_strdup( pGlobal->HP8753calibrationKit.label );
		// description
		pCal->sDescription = g_strdup( pGlobal->HP8753calibrationKit.description );

		pGlobal->pCalKitList = g_list_prepend(pGlobal->pCalKitList, pCal);
		pGlobal->pCalKitList = g_list_sort (pGlobal->pCalKitList, (GCompareFunc)compareCalKitIdentifierItemForSort);
	}
	return OK;
err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);

	return ERROR;
}

/*!     \brief  Recover the calibration kit
 *
 * Recover the calibration kit
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sLabel       calibration kit label
 * \return 			   completion status
 */
gint
recoverCalibrationKit(tGlobal *pGlobal, gchar *sLabel) {
	sqlite3_stmt *stmt = NULL;
	gint length;
	const guchar *tBlob;
	gint queryIndex;
	gboolean bError = FALSE;

	if (sqlite3_prepare_v2(db,
			"SELECT label, description, standards, "
			"  classes FROM CAL_KITS WHERE label = (?);",
			-1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {

		sqlite3_bind_text(stmt, 1, sLabel, strlen(sLabel), SQLITE_STATIC);

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			queryIndex = 0;

			g_strlcpy( pGlobal->HP8753calibrationKit.label, (gchar *)sqlite3_column_text(stmt, queryIndex++), MAX_CALKIT_LABEL_SIZE );
			g_strlcpy( pGlobal->HP8753calibrationKit.description, (gchar *)sqlite3_column_text(stmt, queryIndex++), MAX_CALKIT_LABEL_SIZE );

			length = sqlite3_column_bytes(stmt, queryIndex);
			if( !bError && length == sizeof( tHP8753calibrationStandard ) * MAX_CAL_STANDARDS ) {
				tBlob = sqlite3_column_blob(stmt, queryIndex++);
				memcpy( &pGlobal->HP8753calibrationKit.calibrationStandards, tBlob, sizeof( tHP8753calibrationStandard ) * MAX_CAL_STANDARDS  );
			} else {
				bError = TRUE;
			}

			length = sqlite3_column_bytes(stmt, queryIndex);
			if( !bError && length == sizeof( tHP8753calibrationClass ) * MAX_CAL_CLASSES ) {
				tBlob = sqlite3_column_blob(stmt, queryIndex++);
				memcpy( &pGlobal->HP8753calibrationKit.calibrationClasses, tBlob, sizeof( tHP8753calibrationClass ) * MAX_CAL_CLASSES  );
			} else {
				bError = TRUE;
			}
		}

		sqlite3_finalize(stmt);
	}
	return (bError ? ERROR : 0);
}



/*!     \brief  Close the Sqlite3 database
 *
 * Close the Sqlite3 database prior to ending program
 *
 */
void closeDB(void) {
	sqlite3_close(db);
	sqlite3_shutdown();
}
