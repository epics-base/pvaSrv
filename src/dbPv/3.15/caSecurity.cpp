/**
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution.
 */

#include <osiSock.h>
#include <dbAccess.h>
#include <epicsStdio.h>

#include <db_access_routines.h>
#include <dbChannel.h>

#define epicsExportSharedSymbols
#include <caSecurity.h>

#undef epicsExportSharedSymbols
#include <asDbLib.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvaSrv;


SecuritySession::shared_pointer CAServerSecurityPlugin::createSession(
        osiSockAddr const & /*remoteAddress*/,
        SecurityPluginControl::shared_pointer const & control,
        PVField::shared_pointer const & data)
{
    PVStructure::shared_pointer s = std::tr1::dynamic_pointer_cast<PVStructure>(data);
    if (s)
    {
        PVString::shared_pointer pvUser = s->getSubField<PVString>("user");
        PVString::shared_pointer pvHost = s->getSubField<PVString>("host");
        if (!pvUser)
            throw SecurityException("client must provide a structure with 'user' string field.");
        if (!pvHost)
            throw SecurityException("client must provide a structure with 'host' string field.");

        SecuritySession::shared_pointer ss(
                    new CAServerSecuritySession(shared_from_this(), pvUser->get(), pvHost->get())
                    );

// TODO possible race condition !!!
        control->authenticationCompleted(Status::Ok);
        return ss;
    }
    else
        throw SecurityException("client must provide a structure with 'user' and 'host' string fields.");

}


Status CAServerChannelSecuritySession::m_noAccessStatus(Status::STATUSTYPE_ERROR, "no access");

CAServerChannelSecuritySession::CAServerChannelSecuritySession(std::string const & channelName,
                               const char * user,
                               char * host)
{
    m_dbChannel = dbChannel_create (channelName.c_str());
    if (!m_dbChannel)
        throw NoChannelException();

    long status = asAddClient(
            &m_asClientPvt,
            (ASMEMBERPVT)asDbGetMemberPvt(m_dbChannel),
            asDbGetAsl(m_dbChannel),
            user,
            host);
    if (status != 0 && status != S_asLib_asNotActive)
    {
        dbChannelDelete(m_dbChannel);
        throw SecurityException("no room for security table");
    }
}

CAServerChannelSecuritySession::~CAServerChannelSecuritySession() {
    close();
}

void CAServerChannelSecuritySession::close() {
    if (m_dbChannel)
    {
        dbChannelDelete(m_dbChannel);
        m_dbChannel = 0;
    }
    // multiple calls are OK
    asRemoveClient(&m_asClientPvt);
}
