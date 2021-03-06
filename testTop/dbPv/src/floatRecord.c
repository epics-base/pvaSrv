/* floatRecord.c */
/* Example record support module */
  
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "dbEvent.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "floatRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table */
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();
 
rset floatrRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double
};
epicsExportAddress(rset,floatrRSET);

static void checkAlarms(floatrRecord *pfloat);
static void monitor(floatrRecord *pfloat);

static long init_record(void *precord,int pass)
{
    return(0);
}

static long process(void *precord)
{
    floatrRecord	*pfloat = (floatrRecord *)precord;

	pfloat->pact = TRUE;

	recGblGetTimeStamp(pfloat);
	/* check for alarms */
	checkAlarms(pfloat);
	/* check event list */
	monitor(pfloat);
	/* process the forward scan link record */
        recGblFwdLink(pfloat);

	pfloat->pact=FALSE;
	return(0);
}

static long get_units(DBADDR *paddr, char *units)
{
    floatrRecord	*pfloat=(floatrRecord *)paddr->precord;

    strncpy(units,pfloat->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    floatrRecord	*pfloat=(floatrRecord *)paddr->precord;

    *precision = pfloat->prec;
    if(paddr->pfield == (void *)&pfloat->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    floatrRecord	*pfloat=(floatrRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == floatrRecordVAL
    || fieldIndex == floatrRecordHIHI
    || fieldIndex == floatrRecordHIGH
    || fieldIndex == floatrRecordLOW
    || fieldIndex == floatrRecordLOLO
    || fieldIndex == floatrRecordHOPR
    || fieldIndex == floatrRecordLOPR) {
        pgd->upper_disp_limit = pfloat->hopr;
        pgd->lower_disp_limit = pfloat->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    floatrRecord	*pfloat=(floatrRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == floatrRecordVAL
    || fieldIndex == floatrRecordHIHI
    || fieldIndex == floatrRecordHIGH
    || fieldIndex == floatrRecordLOW
    || fieldIndex == floatrRecordLOLO) {
	pcd->upper_ctrl_limit = pfloat->hopr;
	pcd->lower_ctrl_limit = pfloat->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    floatrRecord	*pfloat=(floatrRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == floatrRecordVAL) {
	pad->upper_alarm_limit = pfloat->hihi;
	pad->upper_warning_limit = pfloat->high;
	pad->lower_warning_limit = pfloat->low;
	pad->lower_alarm_limit = pfloat->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(floatrRecord *pfloat)
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pfloat->udf == TRUE ){
		recGblSetSevr(pfloat,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pfloat->hihi; lolo = pfloat->lolo; high = pfloat->high; low = pfloat->low;
	hhsv = pfloat->hhsv; llsv = pfloat->llsv; hsv = pfloat->hsv; lsv = pfloat->lsv;
	val = pfloat->val; hyst = pfloat->hyst; lalm = pfloat->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pfloat,HIHI_ALARM,pfloat->hhsv)) pfloat->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pfloat,LOLO_ALARM,pfloat->llsv)) pfloat->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pfloat,HIGH_ALARM,pfloat->hsv)) pfloat->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pfloat,LOW_ALARM,pfloat->lsv)) pfloat->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pfloat->lalm = val;
	return;
}

static void monitor(floatrRecord *pfloat)
{
	unsigned short	monitor_mask;
	double		delta;

        monitor_mask = recGblResetAlarms(pfloat);
	/* check for value change */
	delta = pfloat->mlst - pfloat->val;
	if(delta<0.0) delta = -delta;
	if (delta > pfloat->mdel) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		pfloat->mlst = pfloat->val;
	}

	/* check for archive change */
	delta = pfloat->alst - pfloat->val;
	if(delta<0.0) delta = -delta;
	if (delta > pfloat->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		pfloat->alst = pfloat->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pfloat,&pfloat->val,monitor_mask);
	}
	return;
}
