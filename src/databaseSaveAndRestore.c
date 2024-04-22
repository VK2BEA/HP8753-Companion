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
sqlCBtraceAbstract(void *ppGList, gint nCols, gchar **columnData, gchar **columnNames) {
	GList **ppList = ppGList;
	tHP8753traceAbstract *pProjectAbstract;

	// project and name
	if( nCols >= 1 ) {
		pProjectAbstract = g_malloc0( sizeof( tHP8753traceAbstract ));

		pProjectAbstract->projectAndName.sProject = g_strdup((gchar *)columnData[0]);
		pProjectAbstract->projectAndName.sName = g_strdup((gchar *)columnData[1]);
		pProjectAbstract->projectAndName.bSelected = (*(gchar *)columnData[2] == '0' ? FALSE:TRUE);
		pProjectAbstract->sTitle = g_strdup((gchar *)columnData[3]);
		pProjectAbstract->sNote = g_strdup((gchar *)columnData[4]);
		pProjectAbstract->sDateTime = g_strdup((gchar *)columnData[5]);

		*ppList = g_list_prepend(*ppList, pProjectAbstract );
	}

	return 0;
}


gchar *sqlCreateTables[] = {
		"CREATE TABLE IF NOT EXISTS HP8753C_CALIBRATION("
		    "project     TEXT,"
			"selected 		INTEGER DEFAULT 0,"
			"name        TEXT	NOT NULL,"
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
		    "PRIMARY KEY (project, name, channel)"
		");",
		"CREATE TABLE IF NOT EXISTS HP8753C_TRACEDATA("
	        "project        TEXT,"
			"selected 		INTEGER DEFAULT 0 NOT NULL,"
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
		    "screenPlot		BLOB,"
			"title			TEXT,"
		    "notes			TEXT,"
			"perChannelFlags    INTEGER,"
			"generalFlags       INTEGER,"
			"time			TEXT,"
			"PRIMARY KEY (project, name, channel)"
		");",
		"CREATE TABLE IF NOT EXISTS CAL_KITS("
			"label           TEXT,"
			"description     TEXT,"
			"standards       BLOB,"
		    "classes         BLOB,"
			"PRIMARY KEY (label)"
		");",
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
		    "project            TEXT,"
            "colors             BLOB,"
            "colorsHPGL         BLOB,"
			"learnStringIndexes BLOB,"
		    "product            TEXT,"
			"PRIMARY KEY (ID)"
		");",
		"PRAGMA auto_vacuum = FULL;"
};


/*!     \brief  Open Sqlite database (or create tables)
 *
 * Open the database and create tables if they do not exist.
 *		\return	ERROR on error or 0
 */
int
openOrCreateDB(void) {
	gchar *zErrMsg = 0;
	gint rc;
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
		for (i = 0; i < sizeof(sqlCreateTables) / sizeof(gchar*); i++) {
			if ((rc = sqlite3_exec(db, sqlCreateTables[i], NULL, 0, &zErrMsg)) != SQLITE_OK) {
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
void
freeCalListItem( gpointer pCalItem ) {
	tHP8753cal *pCal = (tHP8753cal *)pCalItem;

	g_free( pCal->projectAndName.sProject );
	g_free( pCal->projectAndName.sName );
	g_free( pCal->sNote );

	g_free( pCal );
}

/*!     \brief  Free the g_malloced strings in the tHP8753traceAbstract structure
 *
 * Free the allocated project and name strings
 *
 * \param pCalItem	pointer to tHP8753traceAbstract holding the allocated strings
 */
void
freeTraceListItem( gpointer pTraceItem ) {
	tHP8753traceAbstract *pTraceAbstract = (tHP8753traceAbstract *)pTraceItem;

	g_free( pTraceAbstract->projectAndName.sProject );
	g_free( pTraceAbstract->projectAndName.sName );
	g_free( pTraceAbstract->sTitle );
	g_free( pTraceAbstract->sDateTime );
	g_free( pTraceAbstract->sNote );

	g_free( pTraceItem );
}


/*!     \brief  Comparison routine for sorting the calibration structures
 *
 * Determine if the name string in one tHP8753cal is greater, equal to or less than another.
 *
 * \param  pCalItem1	pointer to tHP8753cal holding the first project & name
 * \param  pCalItem1	pointer to tHP8753cal holding the second project & name
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareCalItemsForSort( gpointer pCalItem1, gpointer pCalItem2 ) {
	if( pCalItem1 == NULL || pCalItem2 == NULL )
		return 0;
	else {
		gint projectCmp = g_strcmp0(((tHP8753cal *)pCalItem1)->projectAndName.sProject,
				((tHP8753cal *)pCalItem2)->projectAndName.sProject);
		if( projectCmp == 0 )
			return g_strcmp0(((tHP8753cal *)pCalItem1)->projectAndName.sName, ((tHP8753cal *)pCalItem2)->projectAndName.sName);
		else
			return projectCmp;
	}
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
compareCalItemsForFind( gpointer pCalItem, gpointer pProjectAndName ) {
	if( pCalItem == NULL )
		return 0;
	else {
		int projectCmp = g_strcmp0(((tHP8753cal *)pCalItem)->projectAndName.sProject,
				((tProjectAndName *)pProjectAndName)->sProject);
		if( projectCmp == 0 )
		    return g_strcmp0(((tHP8753cal *)pCalItem)->projectAndName.sName,
		    		((tProjectAndName *)pProjectAndName)->sName);
		else
			return projectCmp;
	}
}

/*!     \brief  Comparison routine for finding a trace project/name in a list of tHP8753traceAbstract objects
 *
 * Determine if the project and name strings are greater, equal to or less than another.
 *
 * \param  pTraceListItem1    pointer to tHP8753traceAbstract holding the first project / name
 * \param  pTraceListItem2    pointer to tProjectAndName holding the project / name to test
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareTraceItemsForFind( gpointer pTraceListItem1, gpointer pTraceListItem2 ) {
	tProjectAndName *pProjectAndName1 = (tProjectAndName *)&((tHP8753traceAbstract *)pTraceListItem1)->projectAndName;
	tProjectAndName *pProjectAndName2 = (tProjectAndName *)pTraceListItem2;
	int prjCmp = g_strcmp0( pProjectAndName1->sProject, pProjectAndName2->sProject );
	if( prjCmp == 0 )
		return g_strcmp0(pProjectAndName1->sName, pProjectAndName2->sName);
	else
		return prjCmp;

}

/*!     \brief  Comparison routine for sorting a trace abstract list
 *
 * Determine if the project and name strings are greater, equal to or less than another.
 *
 * \param  pTraceListItem1    pointer to tHP8753traceAbstract holding the first project / name
 * \param  pTraceListItem2    pointer to tHP8753traceAbstract holding the second project / name to test
 * \return 0 for equal, +ve for greater than and -ve for less than
 */
gint
compareTraceItemsForSort( gpointer pTraceListItem1, gpointer pTraceListItem2 ) {
	tProjectAndName *pProjectAndName1 = (tProjectAndName *)&((tHP8753traceAbstract *)pTraceListItem1)->projectAndName;
	tProjectAndName *pProjectAndName2 = (tProjectAndName *)&((tHP8753traceAbstract *)pTraceListItem2)->projectAndName;
	int prjCmp = g_strcmp0( pProjectAndName1->sProject, pProjectAndName2->sProject );
	if( prjCmp == 0 )
		return g_strcmp0(pProjectAndName1->sName, pProjectAndName2->sName);
	else
		return prjCmp;

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

	g_list_free_full ( g_steal_pointer (&pGlobal->pCalList), (GDestroyNotify)freeCalListItem );
	pGlobal->pCalList = NULL;

	tHP8753cal *pCal;
	sqlite3_stmt *stmt = NULL;
	gint queryIndex;
	gushort perChannelCalSettings, settings;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"   a.project, a.name, a.selected, a.notes,"
			"   a.sweepStart, b.sweepStart, a.sweepStop, b.sweepStop,"
			"   a.IFbandwidth, b.IFbandwidth, a.CWfrequency, b.CWfrequency,"
			"   a.sweepType, b.sweepType, a.npoints, b.npoints, a.CalType, "
			"   b.CalType, a.perChannelCalSettings, b.perChannelCalSettings, a.calSettings "
			" FROM HP8753C_CALIBRATION a LEFT JOIN HP8753C_CALIBRATION b "
			" ON a.project=b.project AND a.name=b.name"
			" WHERE a.channel=0 AND b.channel=1;", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			pCal = g_new0(tHP8753cal, 1);
			queryIndex = 0;
			// project
			pCal->projectAndName.sProject = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			//name
			pCal->projectAndName.sName = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
			//if selected
			pCal->projectAndName.bSelected = (int)sqlite3_column_int(stmt, queryIndex++) == 0 ? FALSE:TRUE;
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

	pGlobal->pCalList = g_list_sort (pGlobal->pCalList, (GCompareFunc)compareCalItemsForSort);

    // find the last selected setup & cal for the project that was last selected
    pGlobal->pCalibrationAbstract = NULL;
    for( GList *l = pGlobal->pCalList; l != NULL; l = l->next ){
        tHP8753cal *pCalibration = (tHP8753cal *)(l->data);
            tProjectAndName *pProjectAndName = &pCalibration->projectAndName;
            if( pProjectAndName->bSelected && g_strcmp0( pProjectAndName->sProject, pGlobal->sProject ) == 0) {
                pGlobal->pCalibrationAbstract = pCalibration;
            }
    }

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
	g_list_free_full ( g_steal_pointer (&pGlobal->pTraceList), (GDestroyNotify)freeTraceListItem );
	pGlobal->pTraceList = NULL;

	if (sqlite3_exec(db, "SELECT project,name,selected,title,notes,time FROM HP8753C_TRACEDATA WHERE channel=0;",
			sqlCBtraceAbstract, &pGlobal->pTraceList, &zErrMsg) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, zErrMsg);
		sqlite3_free(zErrMsg);
		return ERROR;
	}
	pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);

	// find the last selected trace for the project that was last selected
	pGlobal->pTraceAbstract = NULL;
	for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
	    tHP8753traceAbstract *pTraceAbstract = (tHP8753traceAbstract *)(l->data);
	        tProjectAndName *pProjectAndName = &pTraceAbstract->projectAndName;
	        if( pProjectAndName->bSelected && g_strcmp0( pProjectAndName->sProject, pGlobal->sProject ) == 0) {
	            pGlobal->pTraceAbstract = pTraceAbstract;
	        }
	}
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
saveTraceData(tGlobal *pGlobal, gchar *sProject, gchar *sName) {

	sqlite3_stmt *stmt = NULL;
	guint32 perChannelFlags=0;
	guint16 generalFlags=0;
	gint queryIndex;

		// Source information
	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO HP8753C_TRACEDATA"
			"  (project, name, channel, sweepStart, sweepStop, IFbandwidth, "
			"   CWfrequency, sweepType, npoints, points, stimulusPoints, "
			"   format, scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort, "
			"   markers, activeMkr, deltaMkr, mkrType, bandwidth, "
			"   nSegments, segments, screenPlot, title, notes, "
			"   perChannelFlags, generalFlags, time)"
			" VALUES (?,?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	for (eChannel channel = 0; channel < eNUM_CH; channel++) {
		queryIndex = 0;
		// project
		if (sqlite3_bind_text(stmt, ++queryIndex, sProject, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
			goto err;
		}
		// name
		if (sqlite3_bind_text(stmt, ++queryIndex, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
			goto err;
		}
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

        // screenPlot
		if( pGlobal->HP8753.plotHPGL && pGlobal->HP8753.flags.bHPGLdataValid ) {
            if (sqlite3_bind_blob(stmt, ++queryIndex,
                    pGlobal->HP8753.plotHPGL,
                    *(guint *)pGlobal->HP8753.plotHPGL, SQLITE_STATIC) != SQLITE_OK)
                goto err;
		} else {
		    ++queryIndex;
		}
		// title
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.sTitle, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		// notes
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->HP8753.sNote, STRLENGTH, SQLITE_STATIC) != SQLITE_OK)
			goto err;
		memcpy(&perChannelFlags, &pGlobal->HP8753.channels[channel].chFlags, sizeof(guint32));
		memcpy(&generalFlags, &pGlobal->HP8753.flags, sizeof(guint16));
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

err:
	postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
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
recoverTraceData(tGlobal *pGlobal, gchar *sProject, gchar *sName) {
	sqlite3_stmt *stmt = NULL;
	gint nPoints, pointsSize, mkrSize, bandwidthSize, segmentsSize;
	const guchar *points = NULL, *markers = NULL, *bandwidth = NULL, *segments=NULL, *screenPlot = NULL;
	eChannel channel = eCH_SINGLE;
	const gchar *tText;

	gint traceRetrieved = FALSE;
	gint queryIndex;
	guint32 perChannelFlags;
	guint16 generalFlags;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"   channel, sweepStart, sweepStop, IFbandwidth, CWfrequency, "
			"   sweepType, npoints, points, stimulusPoints, format, "
			"   scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort, markers, "
			"   activeMkr, deltaMkr, mkrType, bandwidth, nSegments, "
			"   segments, screenPlot, title, notes, perChannelFlags, generalFlags, "
			"   time"
			" FROM HP8753C_TRACEDATA WHERE project IS (?) AND name = (?);", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	// bind project
	if( sProject == NULL )
		sqlite3_bind_null(stmt, 1);
	else
		sqlite3_bind_text(stmt, 1, sProject, STRLENGTH, SQLITE_STATIC);
	if( sqlite3_errcode( db ) != SQLITE_OK) {
		traceRetrieved = ERROR;
		goto err;
	}
	// bind name
	if (sqlite3_bind_text(stmt, 2, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
		traceRetrieved = ERROR;
		goto err;
	}

	// We should get one row per channel
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		queryIndex = 0;
		traceRetrieved = TRUE;

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
		pGlobal->HP8753.channels[channel].scaleRefVal = sqlite3_column_double(stmt, queryIndex++);

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

		// Screenplot
		screenPlot = sqlite3_column_blob(stmt, queryIndex);
		g_free( pGlobal->HP8753.plotHPGL );
		pGlobal->HP8753.plotHPGL = NULL;
		if( screenPlot != NULL && sqlite3_column_bytes( stmt, queryIndex ) == *(guint *)screenPlot )
		        pGlobal->HP8753.plotHPGL = g_memdup2( screenPlot, *(guint *)screenPlot);
		queryIndex++;

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
		memcpy(&pGlobal->HP8753.channels[channel].chFlags, &perChannelFlags, sizeof(guint32));

		if( channel == eCH_ONE ) {
			generalFlags = sqlite3_column_int(stmt, queryIndex++);
			memcpy(&pGlobal->HP8753.flags, &generalFlags, sizeof(guint16));
			pGlobal->HP8753.dateTime = g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) );
		} else {
			queryIndex +=2;
		}
	}

err:
	if( sqlite3_errcode(db) != SQLITE_DONE) postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return traceRetrieved;
}

/*!     \brief  Delete the identified profile
 *
 * Remove either a setup/calibration profile or a trace profile
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param sProject     name of profile
 * \param sName        name of profile
 * \param bCalibration true if the array to delete is setup/calibration, false for trace
 * \return 			   completion status
 */
guint
deleteDBentry(tGlobal *pGlobal, gchar *sProject, gchar *sName, tDBtable whichTable) {
	sqlite3_stmt *stmt = NULL;
	gchar *sSQL = NULL;
	GList *listElement = NULL;
	tProjectAndName projectAndName = {sProject, sName};

	switch( whichTable ) {
	case eDB_CALandSETUP:
		sSQL = "DELETE FROM HP8753C_CALIBRATION WHERE project IS (?) AND name = (?);";
		break;
	case eDB_TRACE:
		sSQL = "DELETE FROM HP8753C_TRACEDATA WHERE project IS (?) AND name = (?);";
		break;
	case eDB_CALKIT:
		sSQL = "DELETE FROM CAL_KITS WHERE label = (?);";
		// Calkits are not associated with projects
		break;
	default:
		return 0;
		break;
	}

	if (sqlite3_prepare_v2(db, sSQL, -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		goto err;
	}
	if( whichTable == eDB_CALKIT ) {
		if (sqlite3_bind_text(stmt, 1, sName, STRLENGTH,
				SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		// bind project
		if( sProject == NULL )
			sqlite3_bind_null(stmt, 1);
		else
			sqlite3_bind_text(stmt, 1, sProject, STRLENGTH, SQLITE_STATIC);
		if( sqlite3_errcode( db ) != SQLITE_OK)
			goto err;
		// bind name
		if (sqlite3_bind_text(stmt, 2, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
			goto err;
		}
	}

	if (sqlite3_step(stmt) != SQLITE_DONE)
		goto err;

	sqlite3_finalize(stmt);

	// Must do this after the preparation of the SQL command because otherwise the name will be freed
	// and the prep statement will fail
    switch( whichTable ) {
    case eDB_CALandSETUP:
        listElement = g_list_find_custom( pGlobal->pCalList, &projectAndName, (GCompareFunc)compareCalItemsForFind );
        if( listElement ) {
            freeCalListItem(  listElement->data );
            pGlobal->pCalList = g_list_remove( pGlobal->pCalList, listElement->data );
        }
        break;
    case eDB_TRACE:
        listElement = g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareTraceItemsForFind );
        if( listElement ) {
            freeTraceListItem( listElement->data );
            pGlobal->pTraceList = g_list_remove( pGlobal->pTraceList, listElement->data );
        }
        break;
    case eDB_CALKIT:
        listElement = g_list_find_custom( pGlobal->pCalKitList, sName, (GCompareFunc)compareCalKitIdentifierItem );
        if( listElement ) {
            freeCalKitIdentifierItem(  listElement->data );
            pGlobal->pCalKitList = g_list_remove( pGlobal->pCalKitList, listElement->data );
        }
        // Calkits are not associated with projects
        break;
    default:
        break;
    }

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
saveCalibrationAndSetup(tGlobal *pGlobal, gchar *sProject, gchar *sName) {

	sqlite3_stmt *stmt = NULL;
	guint perChannelCalSettings, calSettings;
	gint  queryIndex;

	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO HP8753C_CALIBRATION "
			" (project, name,  channel, learn, sweepStart, sweepStop,"
			"  IFbandwidth, CWfrequency, sweepType, npoints, calType,"
			"  cal01, cal02, cal03, cal04, cal05, "
			"  cal06, cal07, cal08, cal09, cal10, "
			"  cal11, cal12, notes, perChannelCalSettings, calSettings)"
			"  VALUES (?,?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?)", -1, &stmt,
			NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	// project and name

	for( eChannel channel = eCH_ONE; channel < eNUM_CH; channel++ ) {
		queryIndex = 0;

		// bind project
		if( sProject == NULL ) {
			if (sqlite3_bind_null(stmt, ++queryIndex) != SQLITE_OK) {
				goto err;
			}
		} else {
			if (sqlite3_bind_text(stmt, ++queryIndex, sProject, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
				goto err;
			}
		}
		// bind name
		if (sqlite3_bind_text(stmt, ++queryIndex, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
			goto err;
		}
		// channel
		if (sqlite3_bind_int(stmt, ++queryIndex, channel) != SQLITE_OK)
			goto err;
		//learn
		if( channel == eCH_ONE ) {
			if (sqlite3_bind_blob(stmt, ++queryIndex, pGlobal->HP8753cal.pHP8753_learn,
					lengthFORM1data( pGlobal->HP8753cal.pHP8753_learn ), SQLITE_STATIC) != SQLITE_OK)
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
			gint length = 0;
			if( i < numOfCalArrays[pGlobal->HP8753cal.perChannelCal[channel].iCalType] &&
					pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] != NULL )
				length = lengthFORM1data( pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] );
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

	tProjectAndName projectAndName = { sProject, sName, FALSE };
	GList *calPreviewElement = g_list_find_custom( pGlobal->pCalList, &projectAndName, (GCompareFunc)compareCalItemsForFind );
	if( calPreviewElement ) {
		freeCalListItem( calPreviewElement->data );
		pGlobal->pCalList = g_list_remove( pGlobal->pCalList, calPreviewElement->data );
	}
	// Mark all other Calibration profiles as unselected
    for( GList *l = pGlobal->pCalList; l != NULL; l = l->next ){
            tProjectAndName *pProjectAndName = &(((tHP8753cal *)l->data)->projectAndName);
            pProjectAndName->bSelected = FALSE;
    }

	tHP8753cal *pCal = g_new0(tHP8753cal, 1);

	pCal->projectAndName.sProject = g_strdup( sProject );
	pCal->projectAndName.sName = g_strdup( sName );
	pCal->projectAndName.bSelected = TRUE;
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

	pGlobal->pCalList = g_list_insert_sorted( pGlobal->pCalList, pCal, (GCompareFunc)compareCalItemsForSort );
	pGlobal->pCalibrationAbstract = pCal;
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
recoverCalibrationAndSetup(tGlobal *pGlobal, gchar *sProject, gchar *sName) {
	sqlite3_stmt *stmt = NULL;
	gint length;
	eChannel channel = eCH_SINGLE;
	const guchar *tBlob;
	const gchar *tText;
	gint queryIndex;
	gint calRetrieved = FALSE;
	gushort perChannelCalSettings, calSettings;

	if (sqlite3_prepare_v2(db,
			"SELECT "
			"  channel, learn, sweepStart, sweepStop, IFbandwidth,"
			"  CWfrequency, sweepType, npoints, calType, cal01,"
			"  cal02, cal03, cal04, cal05, cal06, "
			"  cal07, cal08, cal09, cal10, cal11, "
			"  cal12, notes, perChannelCalSettings, calSettings "
			"  FROM HP8753C_CALIBRATION"
			" WHERE project IS (?) AND name = (?);", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}

	// bind project
	if( sProject == NULL )
		sqlite3_bind_null(stmt, 1);
	else
		sqlite3_bind_text(stmt, 1, sProject, STRLENGTH, SQLITE_STATIC);
	if( sqlite3_errcode( db ) != SQLITE_OK) {
		calRetrieved = ERROR;
		goto err;
	}
	// bind name
	if (sqlite3_bind_text(stmt, 2, sName, STRLENGTH, SQLITE_STATIC) != SQLITE_OK) {
		calRetrieved = ERROR;
		goto err;
	}

	// Line for ch 1 and ch 2
	// learn, title, notes, flags and options are only taken from ch 1
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		calRetrieved = TRUE;
		queryIndex = 0;

		// Channel
		channel = sqlite3_column_int(stmt, queryIndex++);

		// Learn string
		length  = sqlite3_column_bytes(stmt, queryIndex);
		tBlob   = sqlite3_column_blob(stmt, queryIndex++);
		if( channel == eCH_ONE ) {
			g_free(pGlobal->HP8753cal.pHP8753_learn);
			pGlobal->HP8753cal.pHP8753_learn = g_memdup2(tBlob, (gsize) length);
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
err:
	sqlite3_finalize(stmt);
	return calRetrieved;
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
	gchar *zErrMsg = 0;
	union uOptions {
        struct stOptions {
            guint16 flagsL;
            guint8  flagsU;
            guint8  PDFpaperSize;
        }  __attribute__((packed)) components;
        guint all;
	} options;
	GBytes *byPage = NULL;
	GBytes *byPrintSettings = NULL;
	gint queryIndex;

	if (sqlite3_prepare_v2(db,
			"INSERT OR REPLACE INTO OPTIONS"
			" (ID, flags, GPIBcontrollerName, GPIBdeviceName, GPIBcontrollerCard, "
			"  GPIBdevicePID, GtkPrintSettings, GtkPageSetup, lastDirectory, calProfile, "
			"  traceProfile, project, colors, colorsHPGL, learnStringIndexes, product"
			" )"
			" VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	}
	queryIndex = 0;
	if (sqlite3_bind_int(stmt, ++queryIndex, CURRENT_DB_SCHEMA) != SQLITE_OK)		// always 0 for now
		goto err;

	pGlobal->flags.bHoldLiveMarker = FALSE;
	memcpy( &options.components.flagsL, &pGlobal->flags, sizeof( guint16 ) + sizeof( guint8 ));
	options.components.PDFpaperSize = (guchar)pGlobal->PDFpaperSize;    // top 8 bits of a 64 bit word
	if (sqlite3_bind_int(stmt, ++queryIndex, options.all) != SQLITE_OK)
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
	// saveing the selected calibration and trace profles are deprecated. They are now determied by the selected project
    if (sqlite3_bind_null(stmt, ++queryIndex) ) {
        goto err;
    }
    if (sqlite3_bind_null(stmt, ++queryIndex) ) {
        goto err;
    }

	if( pGlobal->sProject ) {
		if (sqlite3_bind_text(stmt, ++queryIndex, pGlobal->sProject, strlen( pGlobal->sProject ), SQLITE_STATIC) != SQLITE_OK)
			goto err;
	} else {
		++queryIndex;
	}

	// plot colors
    if (sqlite3_bind_blob(stmt, ++queryIndex, &plotElementColors,
                            sizeof( plotElementColors ), SQLITE_STATIC) != SQLITE_OK)
        goto err;

    // HPGL plot colors
    if (sqlite3_bind_blob(stmt, ++queryIndex, &HPGLpens,
                            sizeof( HPGLpens ), SQLITE_STATIC) != SQLITE_OK) {
        goto err;
    }

	if( pGlobal->HP8753.analyzedLSindexes.version != 0 ) {
		if (sqlite3_bind_blob(stmt, ++queryIndex, &pGlobal->HP8753.analyzedLSindexes,
								sizeof( tLearnStringIndexes ), SQLITE_STATIC) != SQLITE_OK) {
			goto err;
		}
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

	// first clear all selections of trace and calibration profiles from the tables
	if ( sqlite3_exec(db,
			"UPDATE HP8753C_CALIBRATION SET selected=0;"
			"UPDATE HP8753C_TRACEDATA SET selected=0;"
			, NULL, 0, &zErrMsg) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, zErrMsg);
		sqlite3_free(zErrMsg);
		return ERROR;
	}
	// set the selected calibration and trace profiles for each project
	for( GList *l = pGlobal->pCalList; l != NULL; l = l->next ){
		tProjectAndName *pProjectAndName = &(((tHP8753cal *)l->data)->projectAndName);
		if( pProjectAndName->bSelected ) {
			if (sqlite3_prepare_v2(db,
					"UPDATE HP8753C_CALIBRATION SET selected=1"
					" WHERE project IS (?) AND name=(?);", -1, &stmt, NULL) != SQLITE_OK) {
				postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
				return ERROR;
			}
			queryIndex = 0;
			// bind project
			if( pProjectAndName->sProject == NULL )
				sqlite3_bind_null(stmt, ++queryIndex);
			else
				sqlite3_bind_text(stmt, ++queryIndex, pProjectAndName->sProject, STRLENGTH, SQLITE_STATIC);
			sqlite3_bind_text(stmt, ++queryIndex, pProjectAndName->sName, STRLENGTH, SQLITE_STATIC);

			if (sqlite3_step(stmt) != SQLITE_DONE)
				goto err;
			sqlite3_finalize(stmt);
		}
	}
	for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
		tProjectAndName *pProjectAndName = &((tHP8753traceAbstract *)(l->data))->projectAndName;
		if( pProjectAndName->bSelected ) {
			if (sqlite3_prepare_v2(db,
					"UPDATE HP8753C_TRACEDATA SET selected=1"
					" WHERE project IS (?) AND name=(?);", -1, &stmt, NULL) != SQLITE_OK) {
				postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
				return ERROR;
			}
			queryIndex = 0;
			// bind project
			if( pProjectAndName->sProject == NULL )
				sqlite3_bind_null(stmt, ++queryIndex);
			else
				sqlite3_bind_text(stmt, ++queryIndex, pProjectAndName->sProject, STRLENGTH, SQLITE_STATIC);
			sqlite3_bind_text(stmt, ++queryIndex, pProjectAndName->sName, STRLENGTH, SQLITE_STATIC);
			if (sqlite3_step(stmt) != SQLITE_DONE)
				goto err;
			sqlite3_finalize(stmt);
		}
	}

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
 * \return 			   completion status (ERROR, TRUE of options recovered or FALSE if no options recovered)
 */
gint
recoverProgramOptions(tGlobal *pGlobal) {
	sqlite3_stmt *stmt = NULL;
	gint length, size;
	const guchar *tBlob;
	gint queryIndex;
	gint schemaVersion = 0;
	gboolean bOptionsRecovered  = FALSE;
    union uOptions {
        struct stOptions {
            guint16 flagsL;
            guint8  flagsU;
            guint8  PDFpaperSize;
        }  __attribute__((packed)) components;
        guint all;
    } options;

	// First update database schema if needed
	// Find out the current schema ID
	if (sqlite3_prepare_v2(db,
			"SELECT ID FROM OPTIONS;", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			// check the database version and update if necessary
			schemaVersion = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);

		// *************** Alert!
		// if the stored database is older than the current schema ... update
		// ***************
		while( schemaVersion < CURRENT_DB_SCHEMA ) {
			switch( schemaVersion ) {
			case 0: // going from version 0 to 1
				if (sqlite3_exec(db,
						"ALTER TABLE HP8753C_CALIBRATION RENAME TO OLD_HP8753C_CALIBRATION;"
						"ALTER TABLE HP8753C_TRACEDATA RENAME TO OLD_HP8753C_TRACEDATA;",
						NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				if (sqlite3_exec(db, sqlCreateTables[0], NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				if (sqlite3_exec(db,
						"INSERT INTO HP8753C_CALIBRATION ( "
						"    project, name, channel, learn, sweepStart, sweepStop, IFbandwidth, CWfrequency, "
						"    sweepType, npoints, calType, "
						"    cal01, cal02, cal03,cal04, cal05, cal06, cal07, cal08, cal09, cal10, cal11, cal12,"
						"    notes, perChannelCalSettings, calSettings )"
						"  SELECT '🚧 default', name, channel, learn, sweepStart, sweepStop, IFbandwidth, CWfrequency,"
						"    sweepType, npoints, calType, "
						"    cal01, cal02, cal03,cal04, cal05, cal06, cal07, cal08, cal09, cal10, cal11, cal12,"
						"    notes, perChannelCalSettings, calSettings "
						"    FROM OLD_HP8753C_CALIBRATION; "
						" DROP TABLE OLD_HP8753C_CALIBRATION;"
						, NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				if (sqlite3_exec(db,sqlCreateTables[1], NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				if (sqlite3_exec(db,
						"INSERT INTO HP8753C_TRACEDATA ( "
						"   project, name, channel, sweepStart, sweepStop, IFbandwidth, "
						"   CWfrequency, sweepType, npoints, points, stimulusPoints,"
						"   format, scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort,"
						"   markers, activeMkr, deltaMkr, mkrType, bandwidth, nSegments,"
						"   segments, title, notes, perChannelFlags, generalFlags, time )"
						" SELECT '🚧 default', name, channel, sweepStart, sweepStop, IFbandwidth, "
						" CWfrequency, sweepType, npoints, points, stimulusPoints,"
						" format, scaleVal, scaleRefPos, scaleRefVal, sParamOrInputPort,"
						" markers, activeMkr, deltaMkr, mkrType, bandwidth, nSegments,"
						" segments, title, notes, perChannelFlags, generalFlags, time "
						"    FROM OLD_HP8753C_TRACEDATA; "
						" DROP TABLE OLD_HP8753C_TRACEDATA;"
						"VACUUM;PRAGMA auto_vacuum = FULL;"
						, NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				if (sqlite3_exec(db,
						"ALTER TABLE Options ADD COLUMN project TEXT DEFAULT '🚧 default';"
						, NULL, NULL, NULL) != SQLITE_OK) {
					postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
					return ERROR;
				}
				schemaVersion++;
				break;
			case 1: // from version 1 to version 2 (when we get there)
                if (sqlite3_exec(db,
                        "ALTER TABLE Options ADD COLUMN colors BLOB; ALTER TABLE Options ADD COLUMN colorsHPGL BLOB;"
                        , NULL, NULL, NULL) != SQLITE_OK) {
                    postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
                    return ERROR;
                }
			    break;
			case 2:
			    break;
			default:
				postMessageToMainLoop(TM_ERROR, (gchar*) "Database schema version error");
				return ERROR;
			}

			// Update the schema so that if we crash, the schema ID will reflect reality
			gchar *sCmd = g_strdup_printf( "UPDATE OPTIONS SET ID = %d;", ++schemaVersion );
			if (sqlite3_exec(db, sCmd, NULL, NULL, NULL) != SQLITE_OK) {
				postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
				g_free( sCmd );
				return ERROR;
			}
			g_free( sCmd );
		}
	}

	if (sqlite3_prepare_v2(db,
			"SELECT flags, GPIBcontrollerName, GPIBdeviceName, "
			"  GPIBcontrollerCard, GPIBdevicePID, "
			"  GtkPrintSettings, GtkPageSetup, lastDirectory, calProfile, traceProfile, project, colors, colorsHPGL, learnStringIndexes, product FROM OPTIONS;",
			-1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			bOptionsRecovered = TRUE;
			queryIndex = 0;
			// check the database version and update if necessary

			options.all = sqlite3_column_int(stmt, queryIndex++);
			// don't overwrite this bit if it has been set with the command line switch
			gint bNoGPIBtimeout = pGlobal->flags.bNoGPIBtimeout;
			memcpy(&pGlobal->flags, &options.components.flagsL, sizeof( guint16 ) + sizeof( guint8 ));
			// The top byte is the PDF paper size
			pGlobal->PDFpaperSize = options.components.PDFpaperSize;
			GtkComboBox     *wComboPDFpaperSize = GTK_COMBO_BOX ( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_CB_PDFpaperSize" ) );
			gtk_combo_box_set_active( wComboPDFpaperSize, pGlobal->PDFpaperSize );

			pGlobal->flags.bRunning = TRUE;
			pGlobal->flags.bNoGPIBtimeout = bNoGPIBtimeout;

			setUseGPIBcardNoAndPID(pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
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

			// deprecated ... recovery of the selected calibration and trace profiles are determined from the project.
			queryIndex++;
			queryIndex++;

			pGlobal->sProject = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );

			// plot colors
            size = sqlite3_column_bytes(stmt, queryIndex);
			tBlob = sqlite3_column_blob(stmt, queryIndex++);
            if( size == sizeof( plotElementColors ))
                 memcpy( &plotElementColors, tBlob, sizeof( plotElementColors ));
			// HPGL plot colors
            size = sqlite3_column_bytes(stmt, queryIndex);
			tBlob = sqlite3_column_blob(stmt, queryIndex++);
            if( size == sizeof( HPGLpens ))
                 memcpy( &HPGLpens, tBlob, sizeof( HPGLpens ));

			size = sqlite3_column_bytes(stmt, queryIndex);
			tBlob = sqlite3_column_blob(stmt, queryIndex++);
			if( size == sizeof( tLearnStringIndexes ))
				 memcpy( &pGlobal->HP8753.analyzedLSindexes, tBlob, sizeof( tLearnStringIndexes ));

			pGlobal->HP8753.sProduct = g_strdup(  (gchar *)sqlite3_column_text(stmt, queryIndex++) );
		}

		sqlite3_finalize(stmt);
	}

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_UseGPIB_ID" )), pGlobal->flags.bGPIB_UseCardNoAndPID);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_SmithSpline" )), pGlobal->flags.bSmithSpline);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_ShowDateTime" )), pGlobal->flags.bShowDateTime);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_SmithGBnotRX" )), pGlobal->flags.bAdmitanceSmith);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_DeltaMarkerAbsolute" )), !pGlobal->flags.bDeltaMarkerZero);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_UserCalKit" )), pGlobal->flags.bSaveUserKit);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_DoNotRetrieveHPGL" )), pGlobal->flags.bDoNotRetrieveHPGLdata);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_ChkBtn_ShowHPlogo" )), pGlobal->flags.bHPlogo);

	return bOptionsRecovered ? TRUE : FALSE;;
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

	g_list_free_full ( g_steal_pointer (&pGlobal->pCalKitList), (GDestroyNotify)freeCalListItem );
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

/*!     \brief  Get the projects
 *
 * Populate a list of available projects
 *
 * \param  pGlobal      pointer to tGlobal structure (containing the pointer to list)
 * \return 				completion status
 */
gint
inventoryProjects(tGlobal *pGlobal) {

	g_list_free_full ( g_steal_pointer (&pGlobal->pProjectList), (GDestroyNotify)g_free );
	pGlobal->pProjectList = NULL;
	sqlite3_stmt *stmt = NULL;
	gint queryIndex;

	if (sqlite3_prepare_v2(db,
			"SELECT DISTINCT project FROM  HP8753C_TRACEDATA"
			" UNION SELECT DISTINCT project FROM HP8753C_CALIBRATION;", -1, &stmt, NULL) != SQLITE_OK) {
		postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
		return ERROR;
	} else {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			queryIndex = 0;
			pGlobal->pProjectList = g_list_prepend(pGlobal->pProjectList, g_strdup( (gchar *)sqlite3_column_text(stmt, queryIndex++) ));
		}
		sqlite3_finalize(stmt);
	}

	pGlobal->pProjectList = g_list_sort (pGlobal->pProjectList, (GCompareFunc)g_strcmp0);
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



/*!     \brief  reanme, move or copy project/cal/trace
 *
 * Rename database items
 *
 * \param pGlobal      pointer to tGlobal structure
 * \param target       enum indication if we are refering to the project, the calibration or the trace
 * \param purpose      enum indication if we are renaming, moving or copying
 * \param sWhat        the project or name to move, copy or the project (if renaming calibration or trace)
 * \param sFrom        the source
 * \param sTo          the new name or project
 * \return             completion status
 */
gint
renameMoveCopyDBitems(tGlobal *pGlobal, tRMCtarget target, tRMCpurpose purpose,
        gchar *sWhat, gchar *sFrom, gchar *sTo) {
    gint rtn = ERROR;
    gchar *sSQL = 0;
    sqlite3_stmt *stmt = NULL;
    gint queryIndex=0;
    gchar *sSQLquery;

    switch( purpose ) {
    case eMove:
        if( target == eCalibrationName )
            sSQLquery = "UPDATE HP8753C_CALIBRATION SET project = (?) "
                        " WHERE project = (?) AND name = (?);";
        else if( target == eTraceName )
            sSQLquery = "UPDATE HP8753C_TRACEDATA SET project = (?) "
                        " WHERE project = (?) AND name = (?);";
        else
            goto err;

        if (sqlite3_prepare_v2(db, sSQLquery, -1, &stmt, NULL) != SQLITE_OK) {
            postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
            goto err;
        }
        if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        if (sqlite3_bind_text(stmt, ++queryIndex, sWhat, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        break;
    case eCopy:
        if( target == eCalibrationName ) {
            sSQLquery =
                    "INSERT INTO HP8753C_CALIBRATION "
                    " ( project, selected, name, channel, learn, sweepStart, sweepStop, "
                    "   IFbandwidth, CWfrequency, sweepType, npoints, calType, "
                    "   cal01, cal02, cal03, cal04, cal05, cal06, "
                    "   caL07, cal08, cal09, cal10, cal11, cal12, "
                    "   notes, perChannelCalSettings, calSettings )"
                    " SELECT (?), 0, name, channel, learn, sweepStart, sweepStop, "
                    "   IFbandwidth, CWfrequency, sweepType, npoints, calType, "
                    "   cal01, cal02, cal03, cal04, cal05, cal06, "
                    "   caL07, cal08, cal09, cal10, cal11, cal12, "
                    "   notes, perChannelCalSettings, calSettings "
                    " FROM HP8753C_CALIBRATION WHERE project = (?) AND name = (?);";
        } else if( target == eTraceName ) {
            sSQLquery =
                    "INSERT INTO HP8753C_TRACEDATA "
                    " ( project, selected, name, channel, sweepStart, sweepStop, "
                    "   IFbandwidth, CWfrequency, sweepType, npoints, points, "
                    "   stimulusPoints, format, scaleVal, scaleRefPos, scaleRefVal, "
                    "   sParamOrInputPort, markers, activeMkr, deltaMkr, mkrType, "
                    "   bandwidth, nSegments, segments, screenPlot, title, notes, "
                    "   perChannelFlags, generalFlags, time ) "
                    " SELECT (?), 0, name, channel, sweepStart, sweepStop, "
                    "   IFbandwidth, CWfrequency, sweepType, npoints, points, "
                    "   stimulusPoints, format, scaleVal, scaleRefPos, scaleRefVal, "
                    "   sParamOrInputPort, markers, activeMkr, deltaMkr, mkrType, "
                    "   bandwidth, nSegments, segments, screenPlot, title, notes, "
                    "   perChannelFlags, generalFlags, time "
                    " FROM HP8753C_TRACEDATA WHERE project = (?) AND name = (?);";
        } else {
            goto err;
        }
        if (sqlite3_prepare_v2(db, sSQLquery, -1, &stmt, NULL) != SQLITE_OK) {
            postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
            goto err;
        }
        if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        if (sqlite3_bind_text(stmt, ++queryIndex, sWhat, -1, SQLITE_STATIC) != SQLITE_OK)
                goto err;
        break;
    case eRename:
        switch( target ) {
        case eProjectName:
            if (sqlite3_prepare_v2(db,
                    "UPDATE HP8753C_CALIBRATION SET project = (?) WHERE project = (?);",
                    -1, &stmt, NULL) != SQLITE_OK) {
                postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
                goto err;
            }
            if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_step(stmt) != SQLITE_DONE) goto err;
            if (sqlite3_finalize(stmt) != SQLITE_OK) goto err;

            if (sqlite3_prepare_v2(db,
                    "UPDATE HP8753C_TRACEDATA SET project = (?) WHERE project = (?);",
                    -1, &stmt, NULL) != SQLITE_OK) {
                postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
                goto err;
            }
            queryIndex = 0;
            if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            break;
        case eCalibrationName:
            if (sqlite3_prepare_v2(db,
                    "UPDATE HP8753C_CALIBRATION SET name = (?) "
                    "   WHERE name = (?) AND project = (?);",
                    -1, &stmt, NULL) != SQLITE_OK) {
                postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
                goto err;
            }
            if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sWhat, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            break;
        case eTraceName:
            if (sqlite3_prepare_v2(db,
                    "UPDATE HP8753C_TRACEDATA SET name = (?) "
                    "   WHERE name = (?) AND project = (?);",
                    -1, &stmt, NULL) != SQLITE_OK) {
                postMessageToMainLoop(TM_ERROR, (gchar*) sqlite3_errmsg(db));
                goto err;
            }
            if (sqlite3_bind_text(stmt, ++queryIndex, sTo, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sFrom, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            if (sqlite3_bind_text(stmt, ++queryIndex, sWhat, -1, SQLITE_STATIC) != SQLITE_OK)
                    goto err;
            break;
        }

        break;
    default:
        goto err;
        break;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        goto err;
    rtn = 0;
err:
    g_free( sSQL );
    sqlite3_finalize(stmt);
    return rtn;
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
