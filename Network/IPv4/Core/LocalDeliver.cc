//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


#include <omnetpp.h>
#include "LocalDeliver.h"
#include "IPControlInfo_m.h"


//  Cleanup and rewrite: Andras Varga, 2004

Define_Module(LocalDeliver);


void LocalDeliver::initialize()
{
    fragmentTimeoutTime = strToSimtime(par("fragmentTimeout")); // FIXME why not numeric param?

    for (int i=0; i < FRAGMENT_BUFFER_MAXIMUM; i++)
    {
        fragmentBuf[i].isFree = true;
        fragmentBuf[i].fragmentId = -1;
    }

    fragmentBufSize = 0;
    lastCheckTime = 0;
}


void LocalDeliver::handleMessage(cMessage *msg)
{
    IPDatagram *datagram = check_and_cast<IPDatagram *>(msg);

    // erase timed out fragments in fragmentation buffer; check every 1 second max
    if (simTime() >= lastCheckTime + 1)
    {
        lastCheckTime = simTime();
        eraseTimeoutFragmentsFromBuf();
    }

    // Defragmentation
    // skip Degragmentation if single Fragment Datagram
    if (datagram->fragmentOffset()!=0 || datagram->moreFragments())
    {
        insertInFragmentBuf( datagram );
        if (!datagramComplete(datagram->fragmentId()))
        {
            delete datagram;
            return;
        }

        datagram->setLength( datagram->headerLength()*8 +
                             datagram->encapsulatedMsg()->length() );

        /*
        ev << "\ndefragment\n";
        ev << "\nheader length: " << datagram->headerLength()*8
            << "  encap length: " << datagram->encapsulatedMsg()->length()
            << "  new length: " << datagram->length() << "\n";
        */

        removeFragmentFromBuf(datagram->fragmentId());
    }

    int protocol = datagram->transportProtocol();
    cMessage *packet = decapsulateIP(datagram);

    switch (protocol)
    {
        case IP_PROT_ICMP:
            send(packet, "ICMPOut");
            break;
        case IP_PROT_IGMP:
            send(packet, "multicastOut");
            break;
        case IP_PROT_IP:
            send(packet, "preRoutingOut");
            break;
        case IP_PROT_TCP:
            send(packet, "transportOut",0);
            break;
        case IP_PROT_UDP:
            send(packet, "transportOut",1);
            break;
// BCH Andras: from UTS MPLS model
        case IP_PROT_RSVP:
            ev << "IP send packet to RSVPInterface\n";
            send(packet, "transportOut",3);
            break;
// ECH
        default:
            error("Unknown transport protocol number %d", protocol);
    }
}


//----------------------------------------------------------
// Private functions
//----------------------------------------------------------

cMessage *LocalDeliver::decapsulateIP(IPDatagram *datagram)
{
    cMessage *packet = datagram->decapsulate();

    IPControlInfo *controlInfo = new IPControlInfo();
    controlInfo->setProtocol(datagram->transportProtocol());
    controlInfo->setSrcAddr(datagram->srcAddress());
    controlInfo->setDestAddr(datagram->destAddress());
    controlInfo->setDiffServCodePoint(datagram->diffServCodePoint());
    packet->setControlInfo(controlInfo);
    delete datagram;

    return packet;
}

//----------------------------------------------------------
// Private functions: Fragmentation Buffer management
//----------------------------------------------------------

// erase those fragments from the buffer that have timed out
void LocalDeliver::eraseTimeoutFragmentsFromBuf()
{
    int i;
    simtime_t curTime = simTime();

    for (i=0; i < fragmentBufSize; i++)
    {
        if (!fragmentBuf[i].isFree &&
            curTime > fragmentBuf[i].timeout)
        {
            /* debugging output
            ev << "++++ fragment kicked out: "
                << i << " :: "
                << fragmentBuf[i].fragmentId << " / "
                << fragmentBuf[i].fragmentOffset << " : "
                << fragmentBuf[i].timeout << "\n";
            */
            fragmentBuf[i].isFree = true;
        } // end if
    } // end for
}

void LocalDeliver::insertInFragmentBuf(IPDatagram *d)
{
    int i;
    FragmentationBufferEntry *e;

    for (i=0; i < fragmentBufSize; i++)
    {
        if (fragmentBuf[i].isFree == true)
        {
            break;
        }
    } // end for

    // if no free place found, increase Buffersize to append entry
    if (i == fragmentBufSize)
        fragmentBufSize++;

    e = &fragmentBuf[i];
    e->isFree = false;
    e->fragmentId = d->fragmentId();
    e->fragmentOffset = d->fragmentOffset();
    e->moreFragments = d->moreFragments();
    e->length = d->length()/8 - d->headerLength();
    e->timeout= simTime() + fragmentTimeoutTime;

}

bool LocalDeliver::datagramComplete(int fragmentId)
{
    int nextFragmentOffset = 0; // unit: 8 bytes
    bool newFragmentFound = true;
    int i;

    while(newFragmentFound)
    {
        newFragmentFound = false;
        for (i=0; i < fragmentBufSize; i++)
        {
            if (!fragmentBuf[i].isFree &&
                    fragmentId == fragmentBuf[i].fragmentId &&
                    nextFragmentOffset == fragmentBuf[i].fragmentOffset)
            {
                newFragmentFound = true;
                nextFragmentOffset += fragmentBuf[i].length;
                // Datagram complete if last Fragment found
                if (!fragmentBuf[i].moreFragments)
                {
                    return true;
                }
                // reset i to beginning of buffer
            } // end if
        } // end for
    } // end while

    // when no new Fragment found, Datagram is not complete
    return false;
}

int LocalDeliver::getPayloadSizeFromBuf(int fragmentId)
{
    int i;
    int payload = 0;

    for (i=0; i < fragmentBufSize; i++)
    {
            if (!fragmentBuf[i].isFree &&
            fragmentBuf[i].fragmentId == fragmentId)
        {
            payload += fragmentBuf[i].length;
        } // end if
    } // end for
    return payload;
}

void LocalDeliver::removeFragmentFromBuf(int fragmentId)
{
    int i;

    for (i=0; i < fragmentBufSize; i++)
    {
        if (!fragmentBuf[i].isFree &&
            fragmentBuf[i].fragmentId == fragmentId)
        {
            fragmentBuf[i].fragmentId = -1;
            fragmentBuf[i].isFree = true;
        } // end if
    } // end for
}

