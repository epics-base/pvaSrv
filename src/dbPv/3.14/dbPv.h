/**
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution.
 */
/**
 * @author mrk
 */

#ifndef DBPV_H
#define DBPV_H

#include <dbAccess.h>
#include <dbNotify.h>

#include <pv/thread.h>
#include <pv/event.h>
#include <pv/pvAccess.h>

#include "caMonitor.h"
#include "dbPvDebug.h"

namespace epics { namespace pvaSrv { 

class DbUtil;
typedef std::tr1::shared_ptr<DbUtil> DbUtilPtr;

class DbPvProvider;
typedef std::tr1::shared_ptr<DbPvProvider> DbPvProviderPtr;
class DbPv;
typedef std::tr1::shared_ptr<DbPv> DbPvPtr;
class DbPvProcess;
class DbPvGet;
class DbPvPut;
class DbPvMonitor;
class DbPvArray;

typedef struct dbAddr DbAddr;
typedef std::vector<DbAddr> DbAddrArray;

extern DbPvProviderPtr getDbPvProvider();

class DbPvProvider :
    public epics::pvAccess::ChannelProvider,
    public std::tr1::enable_shared_from_this<DbPvProvider>
{
public:
     POINTER_DEFINITIONS(DbPvProvider);
    virtual ~DbPvProvider();
    virtual  std::string getProviderName();
    virtual void destroy() {}
    virtual epics::pvAccess::ChannelFind::shared_pointer channelFind(
        std::string const &channelName,
        epics::pvAccess::ChannelFindRequester::shared_pointer const & channelFindRequester);
    virtual epics::pvAccess::ChannelFind::shared_pointer channelList(
        epics::pvAccess::ChannelListRequester::shared_pointer const & channelListRequester);
    virtual epics::pvAccess::Channel::shared_pointer createChannel(
        std::string const &channelName,
        epics::pvAccess::ChannelRequester::shared_pointer const &channelRequester,
        short priority)
    { return createChannel(channelName,channelRequester,priority,"");}

    virtual epics::pvAccess::Channel::shared_pointer createChannel(
        std::string const &channelName,
        epics::pvAccess::ChannelRequester::shared_pointer  const &channelRequester,
        short priority,
        std::string const &address);
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    DbPvProvider();
    epics::pvAccess::ChannelFind::shared_pointer channelFinder;
    friend DbPvProviderPtr getDbPvProvider();
};

class DbPv :
    public virtual epics::pvAccess::Channel,
    public std::tr1::enable_shared_from_this<DbPv>
{
public:
    POINTER_DEFINITIONS(DbPv);
    DbPv(
        DbPvProviderPtr const & provider,
        epics::pvAccess::ChannelRequester::shared_pointer const & requester,
        std::string const & name,
        std::tr1::shared_ptr<DbAddr> addr
        );
    virtual ~DbPv();
    void init();
    virtual void destroy(){}
    virtual epics::pvAccess::ChannelProvider::shared_pointer getProvider()
       { return provider;}
    virtual std::string getRemoteAddress()
       { return "local";}
    virtual std::string getChannelName()
       { return name; }
    virtual requester_type::shared_pointer getChannelRequester()
       { return requester_type::shared_pointer(requester);}
    virtual void getField(
        epics::pvAccess::GetFieldRequester::shared_pointer const &requester,
        std::string const &subField);
    virtual epics::pvAccess::AccessRights getAccessRights(
        epics::pvData::PVField::shared_pointer const &pvField)
        {throw std::logic_error("Not Implemented");}
    virtual epics::pvAccess::ChannelProcess::shared_pointer createChannelProcess(
        epics::pvAccess::ChannelProcessRequester::shared_pointer const &channelProcessRequester,
        epics::pvData::PVStructurePtr const &pvRequest);
    virtual epics::pvAccess::ChannelGet::shared_pointer createChannelGet(
        epics::pvAccess::ChannelGetRequester::shared_pointer const &channelGetRequester,
        epics::pvData::PVStructurePtr const &pvRequest);
    virtual epics::pvAccess::ChannelPut::shared_pointer createChannelPut(
        epics::pvAccess::ChannelPutRequester::shared_pointer const &channelPutRequester,
        epics::pvData::PVStructurePtr const &pvRequest);
    virtual epics::pvData::Monitor::shared_pointer createMonitor(
        epics::pvData::MonitorRequester::shared_pointer const &monitorRequester,
        epics::pvData::PVStructurePtr const &pvRequest);
    virtual epics::pvAccess::ChannelArray::shared_pointer createChannelArray(
        epics::pvAccess::ChannelArrayRequester::shared_pointer const &channelArrayRequester,
        epics::pvData::PVStructurePtr const &pvRequest);
    virtual void printInfo(std::ostream& out);
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    DbPvProviderPtr  provider;
    requester_type::weak_pointer requester;
    std::string name;
    std::tr1::shared_ptr<DbAddr> dbAddr;
    epics::pvData::FieldConstPtr recordField; 
    epics::pvData::PVStructurePtr pvNullStructure;
    epics::pvData::BitSetPtr emptyBitSet;
    epics::pvData::StructureConstPtr nullStructure;
};

class DbPvProcess :
  public virtual epics::pvAccess::ChannelProcess,
  public std::tr1::enable_shared_from_this<DbPvProcess>
{
public:
    POINTER_DEFINITIONS(DbPvProcess);
    DbPvProcess(
        DbPvPtr const & dbPv,
        epics::pvAccess::ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        DbAddr &dbAddr);
    virtual ~DbPvProcess();
    bool init(epics::pvData::PVStructurePtr const & pvRequest);
    virtual std::string getRequesterName();
    virtual void message(
        std::string const &message,
        epics::pvData::MessageType messageType);
    virtual void destroy();
    virtual std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel()
      {return dbPv;}
    virtual void cancel(){}
    virtual void lastRequest() {}
    virtual void process();
    virtual void lock();
    virtual void unlock();
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    static void notifyCallback(struct putNotify *);
    DbUtilPtr dbUtil;
    DbPvPtr dbPv;
    requester_type::weak_pointer channelProcessRequester;
    DbAddr &dbAddr;
    int propertyMask;
    bool block;
    std::string recordString;
    std::string processString;
    std::string fieldString;
    std::string fieldListString;
    std::string valueString;
    std::tr1::shared_ptr<struct putNotify> pNotify;
    std::tr1::shared_ptr<DbAddr> notifyAddr;
    epics::pvData::Mutex mutex;
    bool beingDestroyed;
};

class DbPvGet :
  public virtual epics::pvAccess::ChannelGet,
  public std::tr1::enable_shared_from_this<DbPvGet>
{
public:
    POINTER_DEFINITIONS(DbPvGet);
    DbPvGet(
        DbPvPtr const & dbPv,
        epics::pvAccess::ChannelGetRequester::shared_pointer const &channelGetRequester,
        DbAddr &dbAddr);
    virtual ~DbPvGet();
    bool init(epics::pvData::PVStructurePtr const & pvRequest);
    virtual std::string getRequesterName();
    virtual void message(
        std::string const &message,
        epics::pvData::MessageType messageType);
    virtual void destroy();
    virtual void get();
    virtual std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel()
      {return dbPv;}
    virtual void cancel(){}
    virtual void lastRequest() {}
    virtual void lock();
    virtual void unlock();
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    static void notifyCallback(struct putNotify *);
    DbUtilPtr dbUtil;
    DbPvPtr dbPv;
    requester_type::weak_pointer channelGetRequester;
    epics::pvData::PVStructurePtr pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
    DbAddr &dbAddr;
    bool process;
    bool block;
    bool firstTime;
    int propertyMask;
    std::tr1::shared_ptr<struct putNotify> pNotify;
    std::tr1::shared_ptr<DbAddr> notifyAddr;
    epics::pvData::Event event;
    epics::pvData::Mutex dataMutex;
    epics::pvData::Mutex mutex;
    bool beingDestroyed;
};

class DbPvPut :
  public virtual epics::pvAccess::ChannelPut,
  public std::tr1::enable_shared_from_this<DbPvPut>
{
public:
    POINTER_DEFINITIONS(DbPvPut);
    DbPvPut(
        DbPvPtr const & dbPv,
        epics::pvAccess::ChannelPutRequester::shared_pointer const &channelPutRequester,
        DbAddr &dbAddr);
    virtual ~DbPvPut();
    bool init(epics::pvData::PVStructurePtr const & pvRequest);
    virtual std::string getRequesterName();
    virtual void message(
        std::string const &message,
        epics::pvData::MessageType messageType);
    virtual void destroy();
    virtual void put(
        epics::pvData::PVStructurePtr const & pvStructure,
        epics::pvData::BitSetPtr const & bitSet);
    virtual std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel()
      {return dbPv;}
    virtual void cancel(){}
    virtual void lastRequest() {}
    virtual void get();
    virtual void lock();
    virtual void unlock();
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    static void notifyCallback(struct putNotify *);
    DbUtilPtr dbUtil;
    DbPvPtr dbPv;
    requester_type::weak_pointer channelPutRequester;
    epics::pvData::PVStructurePtr pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
    DbAddr &dbAddr;
    int propertyMask;
    bool process;
    bool block;
    bool firstTime;
    std::tr1::shared_ptr<struct putNotify> pNotify;
    std::tr1::shared_ptr<DbAddr> notifyAddr;
    epics::pvData::Mutex dataMutex;
    epics::pvData::Mutex mutex;
    bool beingDestroyed;
};

class DbPvMonitor
: public virtual epics::pvData::Monitor,
  public virtual CaMonitorRequester,
  public std::tr1::enable_shared_from_this<DbPvMonitor>
{
public:
    POINTER_DEFINITIONS(DbPvMonitor);
    DbPvMonitor(
        DbPvPtr const & dbPv,
        epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
        DbAddr &dbAddr
    );
    virtual ~DbPvMonitor();
    bool init(epics::pvData::PVStructurePtr const &  pvRequest);
    virtual std::string getRequesterName();
    virtual void message(
        std::string const &message,
        epics::pvData::MessageType messageType);
    virtual void destroy();
    virtual epics::pvData::Status start();
    virtual epics::pvData::Status stop();
    virtual epics::pvData::MonitorElementPtr poll();
    virtual void release(
        epics::pvData::MonitorElementPtr const & monitorElement);
    virtual void exceptionCallback(long status,long op);
    virtual void connectionCallback();
    virtual void accessRightsCallback();
    virtual void eventCallback(const char *);
    virtual void lock();
    virtual void unlock();
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    DbUtilPtr dbUtil;
    epics::pvData::MonitorElementPtr &getFree();
    DbPvPtr dbPv;
    requester_type::weak_pointer  monitorRequester;
    DbAddr &dbAddr;
    epics::pvData::Event event;
    int propertyMask;
    bool firstTime;
    bool gotEvent;
    CaType caType;
    int queueSize;
    std::tr1::shared_ptr<CaMonitor> caMonitor;
    int numberFree;
    int numberUsed;
    int nextGetFree;
    int nextGetUsed;
    int nextReleaseUsed;
    epics::pvData::Mutex mutex;
    bool beingDestroyed;
    bool isStarted;
    epics::pvData::MonitorElementPtrArray elements;
    epics::pvData::MonitorElementPtr currentElement;
    epics::pvData::MonitorElementPtr nullElement;
};

class DbPvArray :
  public epics::pvAccess::ChannelArray,
  public std::tr1::enable_shared_from_this<DbPvArray>
{
public:
    POINTER_DEFINITIONS(DbPvArray);
    DbPvArray(
        DbPvPtr const & dbPv,
        epics::pvAccess::ChannelArrayRequester::shared_pointer const &channelArrayRequester,
        DbAddr &dbAddr);
    virtual ~DbPvArray();
    bool init(epics::pvData::PVStructurePtr const & pvRequest);
    virtual void destroy();
    virtual void putArray(
        epics::pvData::PVArray::shared_pointer const &putArray,
        size_t offset, size_t count, size_t stride);
    virtual void getArray(size_t offset, size_t count, size_t stride);
    virtual void getLength();
    virtual void setLength(size_t length);
    virtual std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel()
      {return dbPv;}
    virtual void cancel(){}
    virtual void lastRequest() {}
    virtual void lock();
    virtual void unlock();
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    DbPvPtr dbPv;
    requester_type::weak_pointer channelArrayRequester;
    epics::pvData::PVScalarArray::shared_pointer pvScalarArray;
    DbAddr &dbAddr;
    epics::pvData::Mutex dataMutex;
    epics::pvData::Mutex mutex;
    bool beingDestroyed;
};

}}

#endif  /* DBPV_H */
