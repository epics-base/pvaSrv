/**
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution.
 */
/**
 * @author mrk
 */
/* Marty Kraimer 2011.03 */
/* This connects to a DB record and presents the data as a PVStructure
 * It provides access to  value, alarm, display, and control.
 */

#include <cstddef>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <memory>

#include <dbAccess.h>
#include <dbEvent.h>
#include <dbNotify.h>
#include <dbCommon.h>

#include <pv/pvIntrospect.h>
#include <pv/pvData.h>
#include <pv/pvAccess.h>

#include "dbPv.h"
#include "dbUtil.h"

using namespace epics::pvData;
using namespace epics::pvAccess;
using std::string;

namespace epics { namespace pvaSrv { 

DbPvPut::DbPvPut(
        DbPvPtr const &dbPv,
        ChannelPutRequester::shared_pointer const &channelPutRequester)
    : dbUtil(DbUtil::getDbUtil()),
      dbPv(dbPv),
      channelPutRequester(channelPutRequester),
      propertyMask(0),
      process(false),
      block(false),
      firstTime(true),
      beingDestroyed(false)
{
    if(DbPvDebug::getLevel()>0) printf("dbPvPut::dbPvPut()\n");
}

DbPvPut::~DbPvPut()
{
    if(DbPvDebug::getLevel()>0) printf("dbPvPut::~dbPvPut()\n");
}

bool DbPvPut::init(PVStructure::shared_pointer const &pvRequest)
{
    requester_type::shared_pointer req(channelPutRequester.lock());
    propertyMask = dbUtil->getProperties(
        req,
        pvRequest,
        dbPv->getDbChannel(),
        true);
    if (propertyMask == dbUtil->noAccessBit) return false;
    if (propertyMask == dbUtil->noModBit) {
        if(req) req->message(
                    "field not allowed to be changed",
                    errorMessage);
        return 0;
    }
    pvStructure = PVStructure::shared_pointer(
        dbUtil->createPVStructure(
            req,
            propertyMask,
            dbPv->getDbChannel(),
            pvRequest));
    if (!pvStructure.get()) return false;
    if (propertyMask & dbUtil->dbPutBit) {
        if (propertyMask & dbUtil->processBit) {
            if(req) req->message(
                        "process determined by dbPutField",
                        errorMessage);
        }
    } else if (propertyMask&dbUtil->processBit) {
        process = true;
        pNotify.reset(new (struct processNotify)());
        struct processNotify *pn = pNotify.get();
        pn->chan = dbPv->getDbChannel();
        pn->requestType = putProcessRequest;
        pn->putCallback  = this->putCallback;
        pn->doneCallback = this->doneCallback;
        pn->usrPvt = this;
        if (propertyMask & dbUtil->blockBit) block = true;
    }
    int numFields = pvStructure->getNumberFields();
    bitSet.reset(new BitSet(numFields));
    if(req) req->channelPutConnect(
       Status::Ok,
       getPtrSelf(),
       pvStructure->getStructure());
    return true;
}

string DbPvPut::getRequesterName() {
    requester_type::shared_pointer req(channelPutRequester.lock());
    return req ? req->getRequesterName() : "<DEAD>";
}

void DbPvPut::message(string const &message,MessageType messageType)
{
    requester_type::shared_pointer req(channelPutRequester.lock());
    if(req) req->message(message,messageType);
}

void DbPvPut::destroy() {
    if(DbPvDebug::getLevel()>0) printf("dbPvPut::destroy beingDestroyed %s\n",
         (beingDestroyed ? "true" : "false"));
    {
        Lock xx(mutex);
        if (beingDestroyed) return;
        beingDestroyed = true;
        if (pNotify) dbNotifyCancel(pNotify.get());
    }
}

void DbPvPut::put(PVStructurePtr const &pvStructure, BitSetPtr const & bitSet)
{
    if (DbPvDebug::getLevel() > 0) printf("dbPvPut::put()\n");

    this->pvStructure = pvStructure;
    this->bitSet = bitSet;

    if (block && process) {
        dbProcessNotify(pNotify.get());
        return;
    }

    requester_type::shared_pointer req(channelPutRequester.lock());

    Lock lock(dataMutex);
    PVFieldPtr pvField = pvStructure.get()->getPVFields()[0];
    if (propertyMask & dbUtil->dbPutBit) {
        status = dbUtil->putField(
                    req,
                    propertyMask,
                    dbPv->getDbChannel(),
                    pvField);
    } else {
        dbScanLock(dbChannelRecord(dbPv->getDbChannel()));
        status = dbUtil->put(
                    req, propertyMask, dbPv->getDbChannel(), pvField);
        if (process) dbProcess(dbChannelRecord(dbPv->getDbChannel()));
        dbScanUnlock(dbChannelRecord(dbPv->getDbChannel()));
    }
    lock.unlock();
    if(req) req->putDone(status, getPtrSelf());
}

int DbPvPut::putCallback(struct processNotify *pn, notifyPutType type)
{
    DbPvPut *pdp = static_cast<DbPvPut *>(pn->usrPvt);

    if (pn->status == notifyCanceled) {
        if (DbPvDebug::getLevel() > 0) printf("dbPvPut::putCallback notifyCanceled\n");
        return 0;
    }
    Lock lock(pdp->dataMutex);
    PVFieldPtr pvField = pdp->pvStructure.get()->getPVFields()[0];
    switch (type) {
    case putDisabledType:
        pn->status = notifyError;
        return 0;
    case putFieldType:
        pdp->status = pdp->dbUtil->putField(
                    pdp->channelPutRequester.lock(),
                    pdp->propertyMask,
                    pdp->dbPv->getDbChannel(),
                    pvField);
        break;
    case putType:
        pdp->status = pdp->dbUtil->put(
                    pdp->channelPutRequester.lock(),
                    pdp->propertyMask,
                    pdp->dbPv->getDbChannel(),
                    pvField);
        break;
    }
    if (!pdp->status.isSuccess())
        pn->status = notifyError;
    return 1;
}

void DbPvPut::doneCallback(struct processNotify *pn)
{
    DbPvPut *pdp = static_cast<DbPvPut *>(pn->usrPvt);

    requester_type::shared_pointer req(pdp->channelPutRequester.lock());
    if(req) req->putDone(
                pdp->status,
                pdp->getPtrSelf());
}

void DbPvPut::get()
{
    if(DbPvDebug::getLevel()>0) printf("dbPvPut::get()\n");
    requester_type::shared_pointer req(channelPutRequester.lock());
    {
        Lock lock(dataMutex);
        dbScanLock(dbChannelRecord(dbPv->getDbChannel()));
        bitSet->clear();
        Status status = dbUtil->get(
                    req,
                    propertyMask,
                    dbPv->getDbChannel(),
                    pvStructure,
                    bitSet,
                    0);
        dbScanUnlock(dbChannelRecord(dbPv->getDbChannel()));
        if(firstTime) {
            firstTime = false;
            bitSet->set(pvStructure->getFieldOffset());
        }
    }
    if(req) req->getDone(
                status,
                getPtrSelf(),
                pvStructure,
                bitSet);
}

void DbPvPut::lock()
{
    dataMutex.lock();
}

void DbPvPut::unlock()
{
    dataMutex.unlock();
}

}}
