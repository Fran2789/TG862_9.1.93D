/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *          Marek Senderski <msend@o2.pl>
 * changes: Michal Kowalczuk <michal@kowalczuk.eu>
 *
 * released under GNU GPL v2 only licence
 *
 */

#include <cmath>
#include "SmartPtr.h"
#include "OptEmpty.h" // rapid-commit option
#include "SrvMsgReply.h"
#include "SrvMsg.h"
#include "OptOptionRequest.h"
#include "OptStatusCode.h"
#include "SrvOptIAAddress.h"
#include "SrvOptIA_NA.h"
#include "SrvOptIA_PD.h"
#include "SrvOptTA.h"
#include "SrvOptFQDN.h"
#include "AddrClient.h"
#include "AddrIA.h"
#include "AddrAddr.h"
#include "IfaceMgr.h"
#include "Logger.h"
// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
#include "OptVendorClass.h"
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
#include <unistd.h> // ARRIS ADD

using namespace std;

/**
 * this constructor is used to create REPLY message as a response for CONFIRM message
 *
 * @param confirm
 */
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgConfirm> confirm)
    :TSrvMsg(confirm->getIface(),confirm->getAddr(), REPLY_MSG,
             confirm->getTransID())
{
    getORO( (Ptr*)confirm );
    copyClientID((Ptr*)confirm );
    copyRelayInfo((Ptr*)confirm);
    copyAAASPI((Ptr*)confirm);
    copyRemoteID((Ptr*)confirm);
    
    if (!handleConfirmOptions( confirm->getOptLst() )) {
        IsDone = true;
        return;
    }

    appendMandatoryOptions(ORO);
    appendAuthenticationOption(ClientDUID);

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    SendLeaseInfoToNotifyLog( (Ptr*)confirm , CONFIRM_MSG );  // ARRIS ADD
#endif //defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)


    this->MRT_ = 31;
    IsDone = false;
    this->send();
}


bool TSrvMsgReply::handleConfirmOptions(TOptList & options) {

    SPtr<TSrvCfgIface> cfgIface = SrvCfgMgr().getIfaceByID(Iface);
    if (!cfgIface) {
        Log(Crit) << "Msg received through not configured interface. "
            "Somebody call an exorcist!" << LogEnd;
        IsDone = true;
        return ADDRSTATUS_UNKNOWN;
    }

    EAddrStatus onLink = ADDRSTATUS_YES;
    int checkCnt = 0;

//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    bool isGetDeviceIdentity = false;
    bool isGetDslForumString = false;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

    TOptList::iterator opt = options.begin();
    while ( (opt!=options.end()) && (onLink==ADDRSTATUS_YES) ) {

        switch ( (*opt)->getOptType()) {
        case OPTION_IA_NA: {
            /* ARRIS MOD BEGIN */
            /* We need to check if the interface least time has changed.
             * if so, return NOBINDING in an OptIA_NA
             * if not, proceed as normal, looking at the individual IAs
             * if we can't find the DUID, return NOT_ON_LINK to force new dhcp session
             */
            SPtr<TAddrClient> ptrClient = SrvAddrMgr().getClient(ClientDUID);            
            if ( ptrClient ) {
                /* Get the configured T1 */
                SPtr<TSrvOptIA_NA> ptrIA_NA = (Ptr*) (*opt);
                SPtr<TAddrIA> ptrIA = ptrClient->getIA(ptrIA_NA->getIAID());
                SPtr<TSrvCfgIface> iface = SrvCfgMgr().getIfaceByID(Iface);
// UNIHAN MOD START for PROD00207122
                if ( ptrIA!=NULL && iface!=NULL && (ptrIA->getT1()) != (iface->getT1(0))) {
// UNIHAN MOD END for PROD00207122
                    Log(Warning) << "The Client T1 is different from Cfg T1" << LogEnd;
                    Options.push_back( new TSrvOptIA_NA(ptrIA_NA->getIAID(), 0, 0, STATUSCODE_NOBINDING,
                                      "Lease Time Is Changed.",this) );
                    return true;
                } else {
                    SPtr<TSrvOptIA_NA> ia = (Ptr*) (*opt);

                    // now we check whether this IA exists in Server Address database or not.
                    SPtr<TOpt> opt;
                    ia->firstOption();
                    while ((opt = ia->getOption()) && (onLink == ADDRSTATUS_YES) ) {
                        if (opt->getOptType() != OPTION_IAADDR){
                            continue;
                        }

                        SPtr<TSrvOptIAAddress> optAddr = (Ptr*) opt;

                        /*SERCOMM MODIFY START*/
                        /*Fix for CDRouter dhcpv6_server_31*/
                        SPtr<TAddrClient> cli = SrvAddrMgr().getClient( optAddr->getAddr() );
                        if ( !ptrIA && !cli ) {
                            Log(Warning) << "Both IAID and Address are invalid, return NotOnLink" << LogEnd;
                            onLink = ADDRSTATUS_NO; 
                            checkCnt = 1; // with replay showing NotOnLink 
                        } else {
                            onLink = cfgIface->confirmAddress(IATYPE_IA, optAddr->getAddr());
                            checkCnt++;
                        }
                        /*SERCOMM MODIFY END*/

                    }
                }
            } else {
                /* We don't know about this DUID 
                 * reject the confirm by marking IA_NA as offlink 
                 * set checkCnt to indicate we have an IA_NA to nak
                 */
                onLink = ADDRSTATUS_NO;
                checkCnt = 1;
            }
            /* ARRIS MOD END */
            break;
        }
        case OPTION_IA_TA: {
            SPtr<TSrvOptTA> ta = (Ptr*) (*opt);

            SPtr<TOpt> opt;
            ta->firstOption();
            while (opt = ta->getOption() && (onLink == ADDRSTATUS_YES)) {
                if (opt->getOptType() != OPTION_IAADDR)
                    continue;

                SPtr<TSrvOptIAAddress> optAddr = (Ptr*) opt;
                onLink = cfgIface->confirmAddress(IATYPE_TA, optAddr->getAddr());
                checkCnt++;
            }
            break;
        }
        case OPTION_IA_PD: {
            SPtr<TSrvOptIA_PD> ta = (Ptr*) (*opt);

            SPtr<TOpt> opt;
            ta->firstOption();
            while (opt = ta->getOption() && (onLink == ADDRSTATUS_YES)) {
                if (opt->getOptType() != OPTION_IAPREFIX)
                    continue;

                SPtr<TSrvOptIAPrefix> optPrefix = (Ptr*) opt;
                onLink = cfgIface->confirmAddress(IATYPE_PD, optPrefix->getPrefix());
                checkCnt++;
            }
            break;
        }

// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
        case OPTION_VENDOR_CLASS: {
            // Change to Dibbler0.8.4 new support TOptVendorClass.
            SPtr<TOptVendorClass> v = (Ptr*) (*opt);
            isGetDslForumString = v->isGet_Broadband_DslForumString();
            break;
        }
        case OPTION_VENDOR_OPTS: {
            SPtr<TOptVendorSpecInfo> v = (Ptr*) (*opt);
            isGetDeviceIdentity = v->isGet_Broadband_DeviceIdentity();
            break;
        }
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
        default: {
            handleDefaultOption( *opt);
            break;
        }
        }
    ++opt;    
    }

    if (!checkCnt) {
        Log(Info) << "No addresses or prefixes in CONFIRM. Not sending reply." << LogEnd;
        return false;
    }

// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
/* For COMCAST, doesn't support ACS discovery. */
#ifdef CONFIG_VENDOR_COMCAST
    appendBroadBandVendorSpec(isGetDeviceIdentity, false);
#else
    appendBroadBandVendorSpec(isGetDeviceIdentity, isGetDslForumString);
#endif
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
    
    switch (onLink) {
    case ADDRSTATUS_YES: {
        SPtr <TOptStatusCode> ptrCode =
            new TOptStatusCode(STATUSCODE_SUCCESS,
                               "Your addresses are correct for this link! Yay!",
                               this);
        Options.push_back( (Ptr*) ptrCode);
        return true;
    }

    case ADDRSTATUS_NO: {
        SPtr <TOptStatusCode> ptrCode =
            new TOptStatusCode(STATUSCODE_NOTONLINK,
                               "Sorry, those addresses are not valid for this link.",
                               this);
        Options.push_back( (Ptr*) ptrCode );
        return true;
    }
        
    default:
    case ADDRSTATUS_UNKNOWN: {
        Log(Info) << "Address/prefix being confirmed is outside of defined class,"
                  << " but there is no subnet defined, so can't answer authoratively."
                  << " Will not send answer." << LogEnd;
        return false;
    }
    }

    // should never get here
    return false;
}


/*
 * this constructor is used to create REPLY message as a response for DECLINE message
 *
 * @param decline
 */
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgDecline> decline)
    :TSrvMsg(decline->getIface(),decline->getAddr(), REPLY_MSG, decline->getTransID())
{
    getORO( (Ptr*)decline );
    copyClientID( (Ptr*)decline );
    copyRelayInfo( (Ptr*)decline );
    copyAAASPI( (Ptr*)decline );
    copyRemoteID( (Ptr*)decline );

    SPtr<TOpt> ptrOpt;

    SPtr<TAddrClient> ptrClient = SrvAddrMgr().getClient(ClientDUID);
    if (!ptrClient) {
        Log(Warning) << "Received DECLINE from unknown client, DUID=" << *ClientDUID << ". Ignored." << LogEnd;
        IsDone = true;
        return;
    }

    SPtr<TDUID> declinedDUID = new TDUID("X",1);
    SPtr<TAddrClient> declinedClient = SrvAddrMgr().getClient( declinedDUID );
    if (!declinedClient) {
        declinedClient = new TAddrClient( declinedDUID );
        SrvAddrMgr().addClient(declinedClient);
    }

/* Sercomm Add for PROD00219451 */
        int isMulticast=0;
        isMulticast = decline->getAddr()->getMultiPacketFlag();
/* End Sercomm Add for PROD00219451 */

    decline->firstOption();
    while (ptrOpt = decline->getOption() )
    {
        switch (ptrOpt->getOptType())
        {
        case OPTION_IA_NA:
        {

        /* Sercomm Add for PROD00219451 */
         if(isMulticast!=1) /* UniCast Packet */
                break;
        /* End Sercomm Add for PROD00219451 */

            SPtr<TSrvOptIA_NA> ptrIA_NA = (Ptr*) ptrOpt;
            SPtr<TAddrIA> ptrIA = ptrClient->getIA(ptrIA_NA->getIAID());
            if (!ptrIA)
            {
                Options.push_back( new TSrvOptIA_NA(ptrIA_NA->getIAID(), 0, 0, STATUSCODE_NOBINDING,
                                             "No such IA is bound.",this) );
                continue;
            }

            // create empty IA
            SPtr<TSrvOptIA_NA> replyIA_NA = new TSrvOptIA_NA(ptrIA_NA->getIAID(), 0, 0, this);
            int AddrsDeclinedCnt = 0;

            // IA found in DB, now move each addr in DB to "declined-client" and
            // ignore those, which are not present id DB
            SPtr<TOpt> subOpt;
            SPtr<TSrvOptIAAddress> addr;
            ptrOpt->firstOption();
            while( subOpt = ptrOpt->getOption() ) {
                if (subOpt->getOptType()==OPTION_IAADDR)
                {
                    addr = (Ptr*) subOpt;

                    // remove declined address from client
                    if (SrvAddrMgr().delClntAddr(ptrClient->getDUID(), ptrIA_NA->getIAID(),
                                                 addr->getAddr(), false)) {

                        // add this address to DECLINED dummy client
                        SrvAddrMgr().addClntAddr(declinedDUID,
                                                 new TIPv6Addr("::", true), // client-address
                                                 decline->getIface(),
                                                 0, 0, 0, // IAID
                                                 addr->getAddr(), // declined address
                                                 DECLINED_TIMEOUT, DECLINED_TIMEOUT,
                                                 false);

                        // set pref/valid lifetimes to 0
                        addr->setValid(0);
                        addr->setPref(0);
                        // ... and finally append it in reply
                        replyIA_NA->addOption( subOpt );

                        AddrsDeclinedCnt++;
                    }
                };
            }
            Options.push_back((Ptr*)replyIA_NA);
            char buf[10];
            sprintf(buf,"%d",AddrsDeclinedCnt);
            string tmp = buf;
            SPtr<TOptStatusCode> optStatusCode =
                new TOptStatusCode(STATUSCODE_SUCCESS, tmp + " addrs declined.", this);
            replyIA_NA->addOption( (Ptr*) optStatusCode );
            break;
        };
        case OPTION_IA_TA:
            Log(Info) << "TA address declined. Oh well. Since it's temporary, let's ignore it entirely." << LogEnd;
            break;
        default:
            handleDefaultOption(ptrOpt);
            break;
        }
    }

    appendMandatoryOptions(ORO);

    appendAuthenticationOption(ClientDUID);

    /*SERCOMM ADD START*/
    /*Fix CDRouter dhcpv6_server_80*/
    if(isMulticast != 1) /* UniCast Packet */
        Options.push_back(new TOptStatusCode(STATUSCODE_USEMULTICAST, "UseMulticast for Decline Reply ",this));
    else
    	Options.push_back(new TOptStatusCode(STATUSCODE_SUCCESS, "Decline Success",this));
    /*SERCOMM ADD END*/

    IsDone = false;
    MRT_ = 31;
    this->send();
}

/**
 * this constructor is used to create REPLY message as a response for REBIND message
 *
 * @param rebind
 */
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgRebind> rebind)
    :TSrvMsg(rebind->getIface(),rebind->getAddr(), REPLY_MSG, rebind->getTransID())
{
    getORO( (Ptr*)rebind );
    copyClientID( (Ptr*)rebind );
    copyRelayInfo( (Ptr*)rebind );
    copyAAASPI( (Ptr*)rebind );
    copyRemoteID( (Ptr*)rebind );

    unsigned long addrCount=0;
    SPtr<TOpt> ptrOpt;

    rebind->firstOption();
    while (ptrOpt = rebind->getOption() )
    {
        switch (ptrOpt->getOptType())
        {
        case OPTION_IA_NA:
          {
            SPtr<TSrvOptIA_NA> optIA_NA;
            optIA_NA = new TSrvOptIA_NA((Ptr*)ptrOpt,
                                        rebind->getAddr(), ClientDUID,
                                        rebind->getIface(), addrCount, REBIND_MSG,
                                        this);
            if (optIA_NA->getStatusCode() != STATUSCODE_NOBINDING )
              Options.push_back((Ptr*)optIA_NA);
            else {
                this->IsDone = true;
                Log(Notice) << "REBIND received with unknown addresses and "
                            << "was silently discarded." << LogEnd;
              return;
            }
            break;
          }
        case OPTION_IA_PD: {
            SPtr<TSrvOptIA_PD> pd;
            pd = new TSrvOptIA_PD( (Ptr*)rebind, (Ptr*) ptrOpt, this);
            Options.push_back((Ptr*)pd);
            break;
        }

        case OPTION_IAADDR:
        case OPTION_RAPID_COMMIT:
        case OPTION_STATUS_CODE:
        case OPTION_PREFERENCE:
        case OPTION_UNICAST:
            Log(Warning) << "Invalid option (" <<ptrOpt->getOptType() << ") received." << LogEnd;
            break;
        default:
            handleDefaultOption(ptrOpt);
            break;
        }
    }

    appendMandatoryOptions(ORO);
    appendRequestedOptions(ClientDUID, rebind->getAddr(),rebind->getIface(), ORO);
    appendAuthenticationOption(ClientDUID);

    IsDone = false;
    MRT_ = 0;
    this->send();

}

/**
 * this constructor is used to create REPLY message as a response for RELEASE message
 *
 * @param release
 */
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgRelease> release)
    :TSrvMsg(release->getIface(),release->getAddr(), REPLY_MSG,
             release->getTransID())
{
    getORO( (Ptr*) release );
    copyClientID( (Ptr*) release );
    copyRelayInfo((Ptr*)release);
    copyAAASPI((Ptr*)release);
    copyRemoteID((Ptr*)release);

    /// @todo When the server receives a Release message via unicast from a client
    /// to which the server has not sent a unicast option, the server
    /// discards the Release message and responds with a Reply message
    /// containing a Status Code option with value UseMulticast, a Server
    /// Identifier option containing the server's DUID, the Client Identifier
    /// option from the client message, and no other options.

    //ARRIS REMOVE Begin
    // for notify script
    //TNotifyScriptParams* notifyParams = new TNotifyScriptParams();
    //ARRIS REMOVE End

    SPtr<TOpt> opt, subOpt;

    SPtr<TAddrClient> client = SrvAddrMgr().getClient(ClientDUID);
    if (!client) {
        Log(Warning) << "Received RELEASE from unknown client DUID=" << ClientDUID->getPlain() << LogEnd;
        IsDone = true;
        return;
    }

    SPtr<TSrvCfgIface> ptrIface = SrvCfgMgr().getIfaceByID( this->Iface );
    if (!ptrIface) {
        Log(Crit) << "Msg received through not configured interface. "
            "Somebody call an exorcist!" << LogEnd;
        IsDone = true;
        return;
    }

/* Sercomm Add for PROD00219451 */
        int isMulticast=0;
        isMulticast = release->getAddr()->getMultiPacketFlag();
/* End Sercomm Add for PROD00219451 */

    //ARRIS Add Begin: Memory leak fix: move notifyParams to here to avoid memory 
    //leak when receive RELEASE from unkown client.
    // for notify script
    TNotifyScriptParams* notifyParams = new TNotifyScriptParams();
    //ARRIS Add End

    appendMandatoryOptions(ORO);
    appendAuthenticationOption(ClientDUID);

    release->firstOption();
    while(opt=release->getOption()) {
        switch (opt->getOptType()) {
        case OPTION_IA_NA: {
            SPtr<TSrvOptIA_NA> clntIA = (Ptr*) opt;
            SPtr<TSrvOptIAAddress> addr;
            bool anyDeleted=false;

            // does this client has IA? (iaid check)
            SPtr<TAddrIA> ptrIA = client->getIA(clntIA->getIAID() );
            if (!ptrIA) {
                Log(Warning) << "No such IA (iaid=" << clntIA->getIAID() << ") found for client:" << ClientDUID->getPlain() << LogEnd;
                Options.push_back( new TSrvOptIA_NA(clntIA->getIAID(), 0, 0, STATUSCODE_NOBINDING,"No such IA is bound.",this) );
                continue;
            }

            // if there was DNS Update performed, execute deleting Update
            SPtr<TFQDN> fqdn = ptrIA->getFQDN();
            if (fqdn) {
                delFQDN(ptrIface, ptrIA, fqdn);
            }

            // let's verify each address
            clntIA->firstOption();
            while(subOpt=clntIA->getOption()) {
                if (subOpt->getOptType()!=OPTION_IAADDR)
                    continue;
                addr = (Ptr*) subOpt;
                if (SrvAddrMgr().delClntAddr(ClientDUID, clntIA->getIAID(), addr->getAddr(), false) ) {
                    notifyParams->addAddr(addr->getAddr(), 0, 0, "SRV");

                    SrvCfgMgr().delClntAddr(this->Iface,addr->getAddr());
                    anyDeleted=true;
                } else {
                    Log(Warning) << "No such binding found: client=" << ClientDUID->getPlain() << ", IA (iaid="
                                 << clntIA->getIAID() << "), addr="<< addr->getAddr()->getPlain() << LogEnd;
                };
            };

            // send result to the client
            if (!anyDeleted)
            {
                SPtr<TSrvOptIA_NA> ansIA(new TSrvOptIA_NA(clntIA->getIAID(), clntIA->getT1(),clntIA->getT2(),this));
                Options.push_back((Ptr*)ansIA);
                ansIA->addOption(new TOptStatusCode(STATUSCODE_NOBINDING, "Not every address had binding.",this));
            };
            break;
        }
        case OPTION_IA_TA: {
            SPtr<TSrvOptTA> ta = (Ptr*) opt;
            bool anyDeleted = false;
            SPtr<TAddrIA> ptrIA = client->getTA(ta->getIAID() );
            if (!ptrIA) {
                Log(Warning) << "No such TA (iaid=" << ta->getIAID() << ") found for client:" << ClientDUID->getPlain() << LogEnd;
                Options.push_back( new TSrvOptTA(ta->getIAID(), STATUSCODE_NOBINDING, "No such IA is bound.", this) );
                continue;
            }

            // let's verify each address
            ta->firstOption();
            while ( subOpt = (Ptr*) ta->getOption() ) {
                if (subOpt->getOptType()!=OPTION_IAADDR)
                    continue;
                SPtr<TSrvOptIAAddress> addr = (Ptr*) subOpt;
                if (SrvAddrMgr().delTAAddr(ClientDUID, ta->getIAID(), addr->getAddr(), false) ) {
                    notifyParams->addAddr(addr->getAddr(), 0, 0 , "");

                    SrvCfgMgr().delTAAddr(this->Iface);
                    anyDeleted=true;
                } else {
                    Log(Warning) << "No such binding found: client=" << ClientDUID->getPlain() << ", TA (iaid="
                             << ta->getIAID() << "), addr="<< addr->getAddr()->getPlain() << LogEnd;
                };
            }

            // send results to the client
            if (!anyDeleted)
            {
                SPtr<TSrvOptTA> answerTA = new TSrvOptTA(ta->getIAID(), STATUSCODE_NOBINDING, "Not every address had binding.", this);
                Options.push_back((Ptr*)answerTA);
            };
            break;
        }

        case OPTION_IA_PD: {
            SPtr<TSrvOptIA_PD> pd = (Ptr*) opt;
            SPtr<TSrvOptIAPrefix> prefix;
            bool anyDeleted=false;

            // does this client has PD? (iaid check)
            SPtr<TAddrIA> ptrPD = client->getPD( pd->getIAID() );
            if (!ptrPD) {
                Log(Warning) << "No such PD (iaid=" << pd->getIAID() << ") found for client:" << ClientDUID->getPlain() << LogEnd;
                Options.push_back( new TSrvOptIA_PD(pd->getIAID(), 0, 0, STATUSCODE_NOBINDING,"No such PD is bound.",this) );
                continue;
            }

            // let's verify each address
            pd->firstOption();
            while(subOpt=pd->getOption()) {
                if (subOpt->getOptType()!=OPTION_IAPREFIX)
                    continue;
                prefix = (Ptr*) subOpt;
                if (SrvAddrMgr().delPrefix(ClientDUID, pd->getIAID(), prefix->getPrefix(), false) ) {
                    notifyParams->addPrefix(prefix->getPrefix(), prefix->getPrefixLength(), 0, 0);
                    SrvCfgMgr().decrPrefixCount(Iface, prefix->getPrefix());
                    anyDeleted=true;
                } else {
                    Log(Warning) << "PD: No such binding found: client=" << ClientDUID->getPlain() << ", PD (iaid="
                                 << pd->getIAID() << "), addr="<< prefix->getPrefix()->getPlain() << LogEnd;
                };
            };

            // send result to the client
            if (!anyDeleted)
            {
                Options.push_back(new TSrvOptIA_PD(pd->getIAID(), 0u, 0u, 
                                                   STATUSCODE_NOBINDING, "Not every address had binding.", this));
            };
            break;
        }
        default:
            handleDefaultOption(opt);
            break;
        }; // switch(...)
    } // while

/* Sercomm Modify for PROD00219451 */
    if(isMulticast == 1) /* MultiCast Packet */
        Options.push_back(new TOptStatusCode(STATUSCODE_SUCCESS,
                                         "All IAs in RELEASE message were processed.",this));
    else
        Options.push_back(new TOptStatusCode(STATUSCODE_USEMULTICAST,
                                         "UseMulticast for Release Reply ",this));
/* End Sercomm Modify for PROD00219451 */

    NotifyScripts = notifyParams;
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    SendReleaseToNotifyLog(release); // UNIHAN ADD, PROD00217666
#endif //defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    IsDone = false;
    MRT_ = 46;
    this->send();
}

// used as RENEW reply
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgRenew> renew)
    :TSrvMsg(renew->getIface(),renew->getAddr(), REPLY_MSG, renew->getTransID())
{
    getORO( (Ptr*)renew );
    copyClientID( (Ptr*)renew );
    copyRelayInfo((Ptr*)renew);
    copyAAASPI((Ptr*)renew);
    copyRemoteID((Ptr*)renew);

    // uncomment this to test REBIND
    //IsDone = true;
    //return;
    // uncomment this to test REBIND

    unsigned long addrCount=0;
    SPtr<TOpt> ptrOpt;
//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    bool isGetDeviceIdentity = false;
    bool isGetDslForumString = false;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

/* Sercomm Add for PROD00219451 */
        int isMulticast=0;
        isMulticast = renew->getAddr()->getMultiPacketFlag();
/* End Sercomm Add for PROD00219451 */

    renew->firstOption();
    while (ptrOpt = renew->getOption() )
    {
        switch (ptrOpt->getOptType())
        {
        case OPTION_IA_NA: {
            SPtr<TSrvOptIA_NA> optIA_NA;
            optIA_NA = new TSrvOptIA_NA((Ptr*)ptrOpt,
                                        renew->getAddr(), ClientDUID,
                                        renew->getIface(), addrCount, RENEW_MSG, this);
            Options.push_back((Ptr*)optIA_NA);
            break;
        }
        case OPTION_IA_PD: {
            SPtr<TSrvOptIA_PD> optPD;
            optPD = new TSrvOptIA_PD((Ptr*) renew, (Ptr*)ptrOpt, this);
            Options.push_back( (Ptr*) optPD);
            break;
        }
        case OPTION_IA_TA:
            Log(Warning) << "TA option present. Temporary addreses cannot be renewed." << LogEnd;
            break;
// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
        case OPTION_VENDOR_CLASS: {
            // Change to Dibbler0.8.4 new support TOptVendorClass.
            SPtr<TOptVendorClass> optVendorClass = (Ptr*)ptrOpt;
            isGetDslForumString = optVendorClass->isGet_Broadband_DslForumString();
            break;
        }
        case OPTION_VENDOR_OPTS: {
            SPtr<TOptVendorSpecInfo> optVendorOpts = (Ptr*)ptrOpt;
            isGetDeviceIdentity = optVendorOpts->isGet_Broadband_DeviceIdentity();
            break;
        }
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
        //Invalid options in RENEW message
        case OPTION_RELAY_MSG :
        case OPTION_INTERFACE_ID :
        case OPTION_RECONF_MSG:
        case OPTION_IAADDR:
        case OPTION_PREFERENCE:
        case OPTION_RAPID_COMMIT:
        case OPTION_UNICAST:
        case OPTION_STATUS_CODE:
            Log(Warning) << "Invalid option "<<ptrOpt->getOptType()<<" received." << LogEnd;
            break;
        default:
            handleDefaultOption(ptrOpt);
            // do nothing with remaining options
            break;
        }
    }

    appendMandatoryOptions(ORO);
    appendRequestedOptions(ClientDUID,renew->getAddr(),renew->getIface(), ORO);
    appendAuthenticationOption(ClientDUID);
// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
/* For COMCAST, doesn't support ACS discovery. */
#ifdef CONFIG_VENDOR_COMCAST
    appendBroadBandVendorSpec(isGetDeviceIdentity, false);
#else
    appendBroadBandVendorSpec(isGetDeviceIdentity, isGetDslForumString);
#endif
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD


#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    SendLeaseInfoToNotifyLog( (Ptr*)renew , RENEW_MSG );
#endif //defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)

/* Sercomm Add for PROD00219451 */
    if(isMulticast != 1) /* UniCast Packet */
        Options.push_back(new TOptStatusCode(STATUSCODE_USEMULTICAST, "UseMulticast for Renew Reply ",this));
/* End Sercomm Add for PROD00219451 */

    /* ARRIS MOD -- memory leak fix */
    // if (pkt) delete []pkt;
    IsDone = false;
    MRT_ = 0;
    this->send();
}

/**
 * this constructor is used to construct REPLY for REQUEST message
 *
 * @param request
 */
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgRequest> request)
    :TSrvMsg(request->getIface(), request->getAddr(), REPLY_MSG, request->getTransID())
{
    getORO( (Ptr*)request );
    copyClientID( (Ptr*)request );
    copyRelayInfo( (Ptr*)request );
    copyAAASPI( (Ptr*)request );
    copyRemoteID( (Ptr*)request );

    processOptions((Ptr*)request, false); // be verbose

/* Sercomm Add for PROD00219451 */
        int isMulticast=0;
        isMulticast = request->getAddr()->getMultiPacketFlag();
/* End Sercomm Add for PROD00219451 */

    appendMandatoryOptions(ORO);
    appendRequestedOptions(ClientDUID, PeerAddr, Iface, ORO);
    appendAuthenticationOption(ClientDUID);

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    SendLeaseInfoToNotifyLog( (Ptr*)request , REQUEST_MSG ); // ARRIS ADD
#endif //defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    
    SPtr<TOpt> reconfAccept = request->getOption(OPTION_RECONF_ACCEPT);
    if (reconfAccept) {
        appendReconfigureKey();
    }

/* Sercomm Add for PROD00219451 */
    if(isMulticast != 1) /* UniCast Packet */
        Options.push_back(new TOptStatusCode(STATUSCODE_USEMULTICAST, "UseMulticast for Request Reply ",this));
/* End Sercomm Add for PROD00219451 */

    IsDone = false;
    MRT_ = 330;
    send();
}

/// @brief ctor used for generating REPLY message as SOLICIT response (in rapid-commit mode)
///
/// @param solicit client's message
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgSolicit> solicit)
    :TSrvMsg(solicit->getIface(), solicit->getAddr(), REPLY_MSG, solicit->getTransID())
{
    getORO( (Ptr*)solicit );
    copyClientID( (Ptr*)solicit );
    copyRelayInfo( (Ptr*)solicit );
    copyAAASPI( (Ptr*)solicit );
    copyRemoteID( (Ptr*)solicit );

    processOptions((Ptr*) solicit, false);

    // append RAPID-COMMIT option
    Options.push_back(new TOptEmpty(OPTION_RAPID_COMMIT, this));

    appendMandatoryOptions(ORO);
    appendRequestedOptions(ClientDUID, PeerAddr, Iface, ORO);
    appendAuthenticationOption(ClientDUID);

    IsDone = false;
    SPtr<TIPv6Addr> ptrAddr;
    MRT_ = 330;
    this->send();
}

// INFORMATION-REQUEST answer
TSrvMsgReply::TSrvMsgReply(SPtr<TSrvMsgInfRequest> infRequest)
    :TSrvMsg(infRequest->getIface(),
             infRequest->getAddr(),REPLY_MSG,infRequest->getTransID())
{
// ARRIS ADD PROD00201283
	#ifdef INCLUDE_ARRIS_GW_TR69
	bool isGetDeviceIdentity = false;
	bool isGetDslForumString = false;
#endif // INCLUDE_ARRIS_GW_TR69
// END ARRIS ADD

    getORO( (Ptr*)infRequest );
    copyClientID( (Ptr*)infRequest );
    copyRelayInfo((Ptr*)infRequest);
    copyAAASPI((Ptr*)infRequest);
    copyRemoteID((Ptr*)infRequest);

    Log(Debug) << "Received INF-REQUEST requesting " << showRequestedOptions(ORO) << "." << LogEnd;

    infRequest->firstOption();
    SPtr<TOpt> ptrOpt;
    while (ptrOpt = infRequest->getOption() )
    {
        switch (ptrOpt->getOptType())
        {

            case OPTION_RELAY_MSG   :
            case OPTION_SERVERID    :
            case OPTION_INTERFACE_ID:
            case OPTION_STATUS_CODE :
            case OPTION_IAADDR      :
            case OPTION_PREFERENCE  :
            case OPTION_UNICAST     :
            case OPTION_RECONF_MSG  :
            case OPTION_IA_NA       :
            case OPTION_IA_TA       :
                Log(Warning) << "Invalid option " << ptrOpt->getOptType() <<" received." << LogEnd;
                break;
// ARRIS ADD PROD00201283
			#ifdef INCLUDE_ARRIS_GW_TR69
            case OPTION_VENDOR_OPTS: {
				SPtr<TOptVendorSpecInfo> v = (Ptr*) ptrOpt;
				isGetDeviceIdentity = v->isGet_Broadband_DeviceIdentity();
	            appendVendorSpec(ClientDUID, Iface, v->getVendor(), ORO);
	            break;
			}
            case OPTION_VENDOR_CLASS: {
                // Change to Dibbler0.8.4 new support TOptVendorClass.
                SPtr<TOptVendorClass> v = (Ptr*) ptrOpt;
	            isGetDslForumString = v->isGet_Broadband_DslForumString();
	            break;
	        }
#endif // INCLUDE_ARRIS_GW_TR69
// END ARRIS ADD
            default:
                handleDefaultOption(ptrOpt);
            break;
        }
    }

    appendMandatoryOptions(ORO);
    if ( !appendRequestedOptions(ClientDUID, infRequest->getAddr(),infRequest->getIface(), ORO) ) {
        Log(Warning) << "No options to answer in INF-REQUEST, so REPLY will not be send." << LogEnd;
        IsDone=true;
        return;
    }

    appendAuthenticationOption(ClientDUID);

// ARRIS ADD PROD00201283
// UNIHAN MODIFY
#ifdef INCLUDE_ARRIS_GW_TR69
/* For COMCAST, doesn't support ACS discovery. */
#ifdef CONFIG_VENDOR_COMCAST
    appendBroadBandVendorSpec(isGetDeviceIdentity, false);
#else
    appendBroadBandVendorSpec(isGetDeviceIdentity, isGetDslForumString);
#endif
#endif // INCLUDE_ARRIS_GW_TR69
// UNIHAN MODIFY END
// END ARRIS ADD



    IsDone = false;
    MRT_ = 330;
    send();
}

void TSrvMsgReply::doDuties() {
    IsDone = true;
}

unsigned long TSrvMsgReply::getTimeout() {
    unsigned long diff = now() - FirstTimeStamp_;
    if (diff>SERVER_REPLY_CACHE_TIMEOUT)
        return 0;
    return SERVER_REPLY_CACHE_TIMEOUT-diff;
}

bool TSrvMsgReply::check() {
    /* not used on the server side */
    /* we generate REPLY messages, so they are *GOOD*. */
    return false;
}

TSrvMsgReply::~TSrvMsgReply()
{

}

string TSrvMsgReply::getName() const {
    return "REPLY";
}
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
#include <sys/file.h>
#include <errno.h>

#define MAX_IPV6_LENGTH        (64)
#define MAX_FILE_STRING_LENGTH (1024)
#define MAX_TEMP_STRING_LENGTH (256)
#define MAX_CLIENT_OPTION_FILENAME_LENGTH (128)
#define MAX_OUI_LENGTH         (6)
#define MAX_SER_NUM_LENGTH     (64)
#define MAX_PROD_CLASS_LENGTH  (64)
#define HAS_LINK_ADDR       (0x0001)
#define HAS_GLOB_IA         (0x0002)
#define HAS_FQDN            (0x0004)
#define SEND_MSG            ( HAS_LINK_ADDR | HAS_GLOB_IA )
#define DIBBLER_NOTIFI_LOG_PATH  "/var/lib/dibbler/server-notify.log"

void TSrvMsgReply::SendLeaseInfoToNotifyLog(SPtr<TMsg> request , int msg_type )
{
    SPtr<TSrvCfgIface> ptrIface = SrvCfgMgr().getIfaceByID( Iface );
    
    FILE *fp=NULL;
    char file_string[MAX_FILE_STRING_LENGTH] = {'\0'};
    char temp_string[MAX_TEMP_STRING_LENGTH] = {'\0'};
    char link_ipstr[MAX_IPV6_LENGTH] = {'\0'};
    char global_ipstr[MAX_IPV6_LENGTH] = {'\0'};
    char opt_file_name[MAX_CLIENT_OPTION_FILENAME_LENGTH] = {'\0'};
    unsigned int flags = 0;
    unsigned int leaseTime = 0;
    int flock_status=0;

    fp = fopen(DIBBLER_NOTIFI_LOG_PATH, "w+");
    if (!fp)
    {
        Log(Warning) << "Failed to open for writting "<< DIBBLER_NOTIFI_LOG_PATH << LogEnd;
        return;
    }

    /* Lock, ignore interrupting signals */
    while ((flock_status = flock(fileno(fp), LOCK_EX)) == EINTR);
    if (flock_status != 0)
    {
        Log(Warning) << "Cannot get lock on file:" << DIBBLER_NOTIFI_LOG_PATH << "not writing" << LogEnd;
        fclose(fp);
        return;
    }
    
    snprintf(temp_string, MAX_TEMP_STRING_LENGTH,"event LEASE\n");
    strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);

 /* Build the message - need local link address, IPv6 address, Lease time */
    inet_ntop(AF_INET6, getAddr()->getAddr(), link_ipstr, MAX_IPV6_LENGTH);
    flags |= HAS_LINK_ADDR;
    if (msg_type == REQUEST_MSG || msg_type == CONFIRM_MSG)
    {
        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(opt_file_name, MAX_CLIENT_OPTION_FILENAME_LENGTH, "%s%s%s", DHCPV6_OPTION_FILE_PATH, DHCPV6_OPTION_FILE, link_ipstr);
        if (access(opt_file_name, F_OK) == 0)
        {
            snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "opt_var_file %s\n", opt_file_name);
        }
        else
        {
            snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "opt_var_file \n");
        }
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
    }

    TOptList::iterator opt = Options.begin();
    while (opt != Options.end())
    {
        switch ((*opt)->getOptType())
        {
            /* From the IA_NA, we get the first IA suboption for the IPv6 ADDRESS */
            case OPTION_IA_NA:
            {
                SPtr<TSrvOptIA_NA> ia = (Ptr *)(*opt);
                SPtr<TOpt> subOpt;

                ia->firstOption();
                while (subOpt = ia->getOption())
                {
                    if (subOpt->getOptType() != OPTION_IAADDR)
                    {
                        continue;
                    }

                    SPtr<TSrvOptIAAddress> optAddr = (Ptr *)subOpt;
                    memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
                    inet_ntop(AF_INET6, optAddr->getAddr()->getAddr(), global_ipstr, MAX_IPV6_LENGTH);
                    snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "globAddr %s\n", global_ipstr);
                    strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
                    Log(Warning) << "SendLeaseInfoToNotifyLog Found Global Address:" << global_ipstr << LogEnd;

                    memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
                    snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "linkAddr %s\n", link_ipstr);
                    strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
                    Log(Warning) << "SendLeaseInfoToNotifyLog Found Link Address:" << link_ipstr << LogEnd;

                    flags |= HAS_GLOB_IA;
                    /* just need the first address - exit while */
                    break;
                }
                break;
            }

            /* From the FQDN, we can get the 'hostname' */
            case OPTION_FQDN:
            {
                SPtr<TSrvOptFQDN> ptrFQDN = (Ptr *)(*opt);
                memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
                snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "fqdn %s\n", ptrFQDN->getFQDN().c_str());
                strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
                Log(Warning) << "SendLeaseInfoToNotifyLog Found FQDN:" << temp_string << LogEnd;
                flags |= HAS_FQDN;
                break;
            }

            default:
            break;
        }
        opt++;
    }
    SPtr<TSrvOptFQDN> ptrFQDN = (Ptr *)(request->getOption(OPTION_FQDN));
    if (ptrFQDN)
    {
        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "fqdn %s\n", ptrFQDN->getFQDN().c_str());
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
    }

#ifdef INCLUDE_ARRIS_GW_TR69
    SPtr<TOptVendorSpecInfo> ptrVendorSpec = (Ptr *)(request->getOption(OPTION_VENDOR_OPTS));
    if (ptrVendorSpec)
    {
        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_OUI_LENGTH + 5, "oui %s\n", ptrVendorSpec->get_Broadband_DeviceManufacturerOUI().c_str());
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);

        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_SER_NUM_LENGTH + 15, "serial_number %s\n",ptrVendorSpec->get_Broadband_DeviceSerialNumber().c_str());
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);

        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_PROD_CLASS_LENGTH + 15, "product_class %s\n",ptrVendorSpec->get_Broadband_DeviceProductClass().c_str());
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
    }
#endif // INCLUDE_ARRIS_GW_TR69

    SPtr<TAddrClient> ptrClient = SrvAddrMgr().getClient(ClientDUID);
    if (ptrClient)
    {
        leaseTime = ptrClient->getLastTimestamp() + ptrClient->getValidTimeout();
        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_TEMP_STRING_LENGTH,"lease_time %d\n", leaseTime);
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
        Log(Notice) << "SendLeaseInfoToNotifyLog getLastTimestamp:" << ptrClient->getLastTimestamp() << ", getValidTimeout:" << ptrClient->getValidTimeout() << LogEnd;

    }

    if ((flags & SEND_MSG) == SEND_MSG)
    {
        fprintf(fp, "%s", file_string);
        Log(Notice) << "SendLeaseInfoToNotifyLog send OK " << LogEnd;
    } 
    else
    {
        Log(Warning) << "SendLeaseInfoToNotifyLog Not enough info for send" << LogEnd;
    }

    fclose(fp);

}

void TSrvMsgReply::SendReleaseToNotifyLog(SPtr<TSrvMsgRelease> release)
{
    SPtr<TSrvCfgIface> ptrIface = SrvCfgMgr().getIfaceByID( Iface );
    FILE *fp=NULL;
    char file_string[MAX_FILE_STRING_LENGTH] = {'\0'};
    char temp_string[MAX_TEMP_STRING_LENGTH] = {'\0'};
    unsigned int flags = 0;
    int flock_status=0;

    fp = fopen(DIBBLER_NOTIFI_LOG_PATH, "w+");
    if (!fp)
    {
        //LOG_GWDB_WARN("Failed to open %s for writting %s", DIBBLER_NOTIFI_LOG_PATH);
        return;
    }

    /* Lock, ignore interrupting signals */
    while ((flock_status = flock(fileno(fp), LOCK_EX)) == EINTR);
    if (flock_status != 0)
    {
        Log(Warning) << "SendReleaseToNotifyLog Cannot get lock on file:" << DIBBLER_NOTIFI_LOG_PATH << "not writing" << LogEnd;
        fclose(fp);
        return;
    }

    snprintf(temp_string, MAX_TEMP_STRING_LENGTH,"event RELEASE\n");
    strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
    if (ptrIface)
    {
        /* Build the message */
        memset(temp_string, '\0', MAX_TEMP_STRING_LENGTH);
        snprintf(temp_string, MAX_TEMP_STRING_LENGTH, "linkAddr %s\n", getAddr()->getAddr());
        strncat(file_string,temp_string,MAX_TEMP_STRING_LENGTH);
        flags |= HAS_LINK_ADDR;
    }
    fclose(fp);
}
#endif //defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
