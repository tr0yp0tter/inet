//
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

#ifndef __TCPALGORITHM_H
#define __TCPALGORITHM_H

#include <omnetpp.h>
#include "TCPConnection.h"
#include "TCPSendQueue.h"
#include "TCPReceiveQueue.h"
#include "TCPSegment_m.h"



/**
 * Abstract base class for TCP algorithms which encapsulate all behaviour
 * during data transfer state: flavour of congestion control, fast
 * retransmit/recovery, selective acknowledgement etc. Subclasses
 * may implement various sets and flavours of the above these algorithms.
 */
class TCPAlgorithm : public cPolymorphic   // FIXME let it be TCPBehaviour? or TCPDataTransfer?
{
  protected:
    TCPConnection *conn; // we belong to this connection
    TCPSendQueue *sendQueue;
    TCPReceiveQueue *receiveQueue;

  public:
    /**
     * Ctor.
     */
    TCPAlgorithm() {conn = NULL; sendQueue = NULL; receiveQueue = NULL;}

    /**
     * Virtual dtor.
     */
    virtual ~TCPAlgorithm() {}

    /**
     * Assign this object to a TCPConnection. SendQueue and receiveQueue of
     * TCPConnection must be set already at this time, because we cache
     * their pointers here.
     */
    void setConnection(TCPConnection *_conn)  {
        conn = _conn;
        sendQueue = conn->getSendQueue();
        receiveQueue = conn->getReceiveQueue();
    }

    /**
     * Create state block (TCB) used by this TCP variant. It is expected
     * that every TCPAlgorithm subclass will have its own state block,
     * subclassed from TCPStateVariables. This factory method should
     * create and return a "blank" state block of the appropriate type.
     */
    virtual TCPStateVariables *createStateVariables() = 0;

    /**
     * Place to process timers specific to this TCPAlgorithm class.
     * TCPConnection will invoke this method on any timer (self-message)
     * it doesn't recognize (that is, any timer other than the 2MSL,
     * CONN-ESTAB and FIN-WAIT-2 timers).
     *
     * Method may also change the event code (by default set to TCP_E_IGNORE)
     * to cause the state transition of TCP FSM.
     */
    virtual void processTimer(cMessage *timer, TCPEventCode& event) = 0;

    /**
     * Called after user sent TCP_C_SEND command to us.
     */
    virtual void sendCommandInvoked() = 0;

    /**
     * Called after we have received data ("text" in RFC 793 lingo).
     * At this point the state variables (rcv_nxt) have already been updated.
     * This method should take care to send ACKs whenever it sees fit.
     */
    virtual void receivedSegmentText() = 0;

    /**
     * Called after we have received an ACK. At this point the state variables
     * (snd_una, snd_wnd) have already been updated.
     */
    virtual void receivedAck() = 0;

    /**
     * Called after we have received an ACK for some data not yet sent.
     * According to RFC 793 this function should send an ACK.
     */
    virtual void receivedAckForDataNotYetSent(uint32 seq) = 0;

};

#endif


