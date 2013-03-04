#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>

#include "Minet.h"
#include "tcpstate.h"
#include "constate.h"
#include "packet_queue.h"


using std::cout;
using std::endl;
using std::cerr;
using std::string;

Time subtractTime(Time x, Time y);

void sendSyn(ConnectionToStateMapping<TCPState> & mapping);

void sendSynAck(ConnectionToStateMapping<TCPState> & mapping);

void sendAckNoData(Connection c, unsigned int sequenceNumber, unsigned ackNumber);

bool receiveData(ConnectionToStateMapping<TCPState> & mapping, IPHeader & ipHeader, TCPHeader & tcpHeader, Buffer & payload);

void sendFin(ConnectionToStateMapping<TCPState> & mapping);

void notifySocketOfRead(ConnectionToStateMapping<TCPState> & mapping);



static MinetHandle mux, sock;


int main(int argc, char *argv[]) {


    MinetInit(MINET_TCP_MODULE);

    mux = MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
    sock = MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

    if (MinetIsModuleInConfig(MINET_IP_MUX) && mux==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
    }

    if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
    }

    cerr << "tcp_module handling TCP traffic.\n";

    MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

    MinetEvent event;
    ConnectionList<TCPState> connectionList;
    double minimumTimeout = -1;
    Time timeElapsed;
    gettimeofday(&timeElapsed, NULL);


    unsigned tcphlen;
    Packet p;

    while (MinetGetNextEvent(event, minimumTimeout)==0) {
        // if we received an unexpected type of event, print error
        cerr << "event\n";


         //find how much time has elapsed since the last clocking event
           Time timeSinceLastClock;
           gettimeofday(&timeSinceLastClock, NULL);
           timeElapsed = subtractTime(timeSinceLastClock, timeElapsed);
           cerr << "Time elapsed = " << timeElapsed << "\n";

            //after every event or timeout, update the timeout for each connection
           ConnectionList<TCPState>::iterator mapping = connectionList.begin();
            for (; mapping!=connectionList.end(); mapping++) {
                if (mapping->bTmrActive == true) {
                    cerr << "Updating connection " << mapping->connection << " timeout" << endl;
                    if(mapping->timeout > timeElapsed) {
                        mapping->timeout = subtractTime(mapping->timeout, timeElapsed);
                        cerr << "New timeout " << mapping->timeout << endl;
                    } else {
                        mapping->timeout = Time(0.0);
                        mapping->bTmrActive = false;
                        cerr << "Connection has timed out, turning off timer" << endl;

                        //ALSO HANDLE TIMEOUT SHIT

                        switch(mapping->state.stateOfcnx) {
                            case SYN_RCVD: {
                                    sendSynAck(*mapping);
                                    cerr << "Resending SYN ACK" << endl;
                                }
                                break;
                            case SYN_SENT: {
                                    cerr << "SYN Sent timed out" << endl;
                                    cerr << "Resending syn" << endl;
                                    sendSyn(*mapping);
                                }
                                break;
                            case ESTABLISHED: {

                                }
                                break;
                        }
                    }
                }
            }

            gettimeofday(&timeElapsed, NULL);



        if (event.eventtype==MinetEvent::Timeout) {
            cerr << "timeout\n";
            cerr << "Timeout after" << timeElapsed << " seconds\n";
            //timeout occured
            // find the timeouted connection
            if (connectionList.FindEarliest()!=connectionList.end()) {
                ConnectionList<TCPState>::iterator timeoutConnectionIterator = connectionList.FindEarliest();
            }
            //resend last packet(s)

        }
        cerr << "second if statement.\n";
        if (event.eventtype!=MinetEvent::Dataflow || event.direction!=MinetEvent::IN) {
            MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
            // if we received a valid event from Minet, do processing
        }
        else {   //  Data from the IP layer below  //
            if (event.handle==mux) {
                cerr << "mux!\n";
                MinetReceive(mux,p);
                tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
                cerr << "estimated header len="<<tcphlen<<"\n";
                p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
                IPHeader ipHeader=p.FindHeader(Headers::IPHeader);
                TCPHeader tcpHeader=p.FindHeader(Headers::TCPHeader);
                cerr << "Received packet: " << endl;
                cerr << ipHeader<< endl;
                cerr << tcpHeader << endl;

                cerr  << "Checksum is " << (tcpHeader.IsCorrectChecksum(p) ? "VALID" : "INVALID") << "\n";

                Connection c;
                ipHeader.GetDestIP(c.src);
                ipHeader.GetSourceIP(c.dest);
                ipHeader.GetProtocol(c.protocol);
                tcpHeader.GetDestPort(c.srcport);
                tcpHeader.GetSourcePort(c.destport);
                cerr << "Received from " << c.dest << ":" << c.destport << "\n";
                cerr << "Sent to " << c.src << ":" << c.srcport << "\n";

                ConnectionList<TCPState>::iterator mapping = connectionList.FindMatching(c);
                if(mapping == connectionList.end()) {
                    cerr << "Received packet from a connection not in the list" << endl;
                    //code for testing
/*
                      unsigned int remoteSequenceNumber;
                    unsigned int remoteAckNumber;
                     ConnectionToStateMapping<TCPState> mapping = *matchingConnection;

                    tcpHeader.GetSeqNum(remoteSequenceNumber);
                    tcpHeader.GetAckNum(remoteAckNumber);
                        Packet synAckPacketToSend;
                        IPHeader ipHeader;
                        ipHeader.SetProtocol(IP_PROTO_TCP);
                        ipHeader.SetSourceIP(c.src);
                        ipHeader.SetDestIP(c.dest);
                        ipHeader.SetTotalLength(TCP_HEADER_BASE_LENGTH+IP_HEADER_BASE_LENGTH);
                        synAckPacketToSend.PushFrontHeader(ipHeader);

                        TCPHeader tcpHeader;
                        tcpHeader.SetSourcePort(c.srcport, synAckPacketToSend);
                        tcpHeader.SetDestPort(c.destport, synAckPacketToSend);
                        tcpHeader.SetSeqNum(mapping.state.last_acked, synAckPacketToSend);
                        tcpHeader.SetAckNum(remoteSequenceNumber + 1, synAckPacketToSend);
                        tcpHeader.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, synAckPacketToSend);
                        unsigned char flags;
                        SET_ACK(flags);
                        SET_SYN(flags);
                        tcpHeader.SetFlags(flags, synAckPacketToSend);
                        tcpHeader.SetWinSize(0, synAckPacketToSend);
                        tcpHeader.SetChecksum(0);
                        tcpHeader.SetUrgentPtr(0, synAckPacketToSend);
                        tcpHeader.RecomputeChecksum(synAckPacketToSend);
                        synAckPacketToSend.PushBackHeader(tcpHeader);

                        cerr << endl << "Sending response: " << endl;
                        cerr << "Is checksum correct?" << tcpHeader.IsCorrectChecksum(synAckPacketToSend) << endl;
                        IPHeader foundIPHeader=synAckPacketToSend.FindHeader(Headers::IPHeader);
                        cerr << foundIPHeader << endl;
                        TCPHeader foundTCPHeader=synAckPacketToSend.FindHeader(Headers::TCPHeader);
                        cerr << foundTCPHeader << endl;

                        MinetSend(mux, synAckPacketToSend);


*/
                //end of code for testing



                } else {
                    cerr << "Current connection state: " << mapping->state << "\n";
                    cerr << "Current connection timer remaining: " << mapping->timeout << endl;
                    unsigned char flags;
                    tcpHeader.GetFlags(flags);

                    unsigned int remoteSequenceNumber;
                    unsigned int remoteAckNumber;
                    unsigned short int remoteWindowSize;
                    tcpHeader.GetSeqNum(remoteSequenceNumber);
                    tcpHeader.GetAckNum(remoteAckNumber);
                    tcpHeader.GetWinSize(remoteWindowSize);

                    switch(mapping->state.stateOfcnx) {
                        case LISTEN: {
                            if(IS_SYN(flags)) {


                                cerr << "SYN Received, remote ISN is " << remoteSequenceNumber << endl;

                                mapping->connection = c;
                                mapping->state.last_recvd = remoteSequenceNumber;
                                mapping->state.rwnd = remoteWindowSize;

                                sendSynAck(*mapping);

                                //STATE TRANSITION
                                mapping->state.stateOfcnx = SYN_RCVD;




                                SockRequestResponse notifyApplicationOfAcceptedConnection;
                                notifyApplicationOfAcceptedConnection.type = WRITE;
                                notifyApplicationOfAcceptedConnection.connection = mapping->connection;
                                notifyApplicationOfAcceptedConnection.bytes = 0;
                                MinetSend(sock, notifyApplicationOfAcceptedConnection);


                            } else {
                                cerr << "Connection is in LISTEN state, but the received packet does not contain a SYN" << endl;
                            }
                        }
                            break;
                        case SYN_SENT: {
                                if(IS_SYN(flags) && IS_ACK(flags)) {
                                    cerr << "Received SYN ACK" << endl;
                                    if(remoteAckNumber > mapping->state.last_sent) {
                                        mapping->state.SetLastRecvd(remoteSequenceNumber);
                                        mapping->state.SetLastAcked(remoteAckNumber);
                                        cerr << "Last received sequence was " << remoteSequenceNumber;
                                        sendAckNoData(mapping->connection, mapping->state.last_sent + 1, mapping->state.last_recvd + 1);
                                        cerr << "ISN was properly acked" << endl;
                                        cerr << "Switching to established state" << endl;
                                        mapping->state.stateOfcnx = ESTABLISHED;
                                        mapping->bTmrActive = false;

                                        SockRequestResponse notifyApplicationOfEstablishedConnection;
                                        notifyApplicationOfEstablishedConnection.type = WRITE;
                                        notifyApplicationOfEstablishedConnection.connection = mapping->connection;
                                        notifyApplicationOfEstablishedConnection.bytes = 0;
                                        MinetSend(sock, notifyApplicationOfEstablishedConnection);


                                    } else {
                                        cerr << "Received wrong ack for syn (ISN)" << endl;
                                    }
                                } else {
                                    cerr << "Connection is in SYN_SENT state, expecting syn ack but received something else" << endl;
                                }

                            }
                            break;
                        case SYN_RCVD: {
                            if(IS_ACK(flags)) {
                                if(remoteAckNumber > mapping->state.last_sent) {
                                    //Receive available data
                                   bool sendAck = receiveData(*mapping, ipHeader, tcpHeader, p.GetPayload());
                                   if(sendAck) {
                                       sendAckNoData(mapping->connection, mapping->state.last_sent + 1, mapping->state.last_recvd + 1);
                                   }


                                   //STATE TRANSITION
                                   mapping->state.stateOfcnx = ESTABLISHED;
                                   mapping->bTmrActive = false;
                                   mapping->state.last_acked = remoteAckNumber;
                                } else {
                                    cerr << "Did not receive correct ack number in response to synack" << endl;
                                    sendSynAck(*mapping);
                                }
                            } else if(IS_SYN(flags)) {
                                cerr << "SYNACK was probably lost, resending" << endl;
                                sendSynAck(*mapping);
                            }
                        }
                            break;
                        case ESTABLISHED: {

                                bool sendAck;
                                if(IS_ACK(flags)) {
                                    sendAck = receiveData(*mapping, ipHeader, tcpHeader, p.GetPayload());
                                    mapping->state.last_acked = remoteAckNumber;
                                }

                                if(IS_FIN(flags)) {
                                    cerr << "FIN occured" << endl;
                                    mapping->state.last_recvd += 1;
                                }
                                if((IS_ACK(flags) && sendAck) || IS_FIN(flags)) {
                                    sendAckNoData(mapping->connection, mapping->state.last_sent + 1, mapping->state.last_recvd + 1);
                                }


                                if(IS_FIN(flags)) {
                                    //STATE TRANSITION
                                    mapping->state.stateOfcnx = CLOSE_WAIT;
                                    SockRequestResponse requestApplicationClose;
                                    //requestApplicationClose.type = CLOSE;
                                    requestApplicationClose.type = WRITE;
                                    Buffer emptyBuffer;
                                    requestApplicationClose.connection = mapping->connection;
                                    requestApplicationClose.bytes = 0;
                                    requestApplicationClose.data = emptyBuffer;
                                    MinetSend(sock, requestApplicationClose);
                                }

                                if(!(IS_ACK(flags) || IS_FIN(flags))) {
                                    cerr << "Received packet that is not ack or fin while in established state.  Thats not right" << endl;
                                }


                            }
                            break;
                        case LAST_ACK: {
                                if(remoteAckNumber == mapping->state.last_acked + 1) {
                                    cerr << "Received final ack, connection now closed";
                                    mapping->state.stateOfcnx = CLOSED;
                                    //REMOVE CONNECTION FROM LIKST
                                } else {
                                    cerr << "Received final ack with wrong number";
                                }
                            }
                            break;

                    }


                }


            }
            //  Data from the Sockets layer above  //
            if (event.handle==sock) {

                SockRequestResponse s;
                MinetReceive(sock,s);
                cerr << "Received Socket Request:" << s << endl;
                switch (s.type) {
                    case CONNECT:
                    {
                        cerr << "SockRequestResponse Connect.\n";

                        unsigned int isn;
                        isn = timeElapsed.tv_usec / 4;
                        isn += timeElapsed.tv_sec * 1000000 / 4;

                        cerr << "Generated ISN is " << isn << endl;

                        TCPState statesend(isn, SYN_SENT ,5);
                        ConnectionToStateMapping<TCPState> mappingsend(s.connection, 0, statesend, false);

                        cerr << "Adding new active open connection to connection list" << endl;

                        sendSyn(mappingsend);

                        connectionList.push_back(mappingsend);

                        SockRequestResponse replyToSocket;
                        replyToSocket.type = STATUS;
                        replyToSocket.connection = s.connection;
                        replyToSocket.error = EOK;
                        MinetSend(sock, replyToSocket);
                    }
                        break;

                    case ACCEPT:
                    {
                        cerr << "Socket requests to listen on port " << s.connection.srcport << endl;
                            ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatchingSource(s.connection);
                            if(matchingConnection == connectionList.end()) {
                                cerr << "Current time is " << timeElapsed.tv_sec << "." << timeElapsed.tv_usec << " seconds" << endl;

                                unsigned int isn;
                                isn = timeElapsed.tv_usec / 4;
                                isn += timeElapsed.tv_sec * 1000000 / 4;

                                cerr << "Generated ISN is " << isn << endl;

                                TCPState state(isn, LISTEN, 5);
                                Time time(0.0);
                                bool timerActive = false;
                                ConnectionToStateMapping<TCPState> mapping(s.connection, time, state, timerActive);
                                connectionList.push_back(mapping);
                                cerr << "Adding new passive open connection to connection list" << endl;

                                SockRequestResponse replyToSocket;
                                replyToSocket.type = STATUS;
                                replyToSocket.error = EOK;
                                MinetSend(sock, replyToSocket);


                            } else {
                                cerr << "Connection already exists in connection list" << endl;
                                SockRequestResponse replyToSocket;
                                replyToSocket.type = STATUS;
                                replyToSocket.error = EUNKNOWN;
                                MinetSend(sock, replyToSocket);
                            }
                        }
                        break;
                    case WRITE:
                        {
                            // 1) break the data into chunks
                            // 2) add these packets to a queue
                            // 3) send the maximum number of packets
                            // 4) upon acknowledgement send more packets

                         cerr << "Socket requests to write on port " << s.connection.srcport << "to port" << s.connection.destport << "to destination IP" <<s.connection.dest << endl;
                            ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatchingSource(s.connection);
                            if(matchingConnection != connectionList.end()) {

                                    int localISN;
                                    if (s.data.GetSize()==0) {
                                        localISN = 0;
                                    }
                                    else {
                                        localISN = matchingConnection->state.last_sent;
                                    }

                                    int remoteISN = matchingConnection->state.last_recvd;

                                    //s.data.AddBack(matchingConnection->state.SendBuffer);
                                    PacketQueue packetQueue;

                                    matchingConnection->timeout = Time(1.0);
                                    matchingConnection->bTmrActive = true;

                                    for (int offset = 0; offset < (signed int)s.bytes; offset+= 236) {
                                        Packet dataPacketToSend;
                                        Connection c_data = matchingConnection->connection;
                                        IPHeader ipHeader;
                                        ipHeader.SetProtocol(IP_PROTO_TCP);
                                        ipHeader.SetSourceIP(c_data.src);
                                        ipHeader.SetDestIP(c_data.dest);
                                        ipHeader.SetTotalLength(TCP_HEADER_BASE_LENGTH+IP_HEADER_BASE_LENGTH);
                                        dataPacketToSend.PushFrontHeader(ipHeader);

                                        TCPHeader tcpHeader;
                                        tcpHeader.SetSourcePort(c_data.srcport, dataPacketToSend);
                                        tcpHeader.SetDestPort(c_data.destport, dataPacketToSend);
                                        tcpHeader.SetSeqNum(localISN, dataPacketToSend);
                                        tcpHeader.SetAckNum(remoteISN + 1, dataPacketToSend);
                                        tcpHeader.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, dataPacketToSend);
                                        tcpHeader.SetChecksum(0);
                                        tcpHeader.SetUrgentPtr(0, dataPacketToSend);
                                        tcpHeader.RecomputeChecksum(dataPacketToSend);
                                        dataPacketToSend.PushBackHeader(tcpHeader);

                                        if (s.bytes-offset < 236) {
                                            //dataPacketToSend.payload.GetData(%s.data, (s.bytes-offset),offset);
                                        }
                                        else {
                                        //dataPacketToSend.payload.GetData(&s.data,236,offset);
                                        }
                                        packetQueue.PushPacket(dataPacketToSend);

                                        //generate packet p
                                        //send p if we can
                                        //otherwise add p to the queue
                                        localISN += offset;
                                    }


                            }
                            else {
                                cerr << "Unable to find connection.\n";
                                SockRequestResponse replyToSocket;
                                replyToSocket.type = STATUS;
                                replyToSocket.error = EUNKNOWN;
                                MinetSend(sock, replyToSocket);
                            }



                        }
                        break;
                    case FORWARD:
                        {
                            SockRequestResponse replyToSocket;
                            replyToSocket.type = STATUS;
                            replyToSocket.error = EOK;
                            MinetSend(sock, replyToSocket);

                        }

                        break;
                    case CLOSE:
                        {
                            ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatching(s.connection);
                            if(matchingConnection == connectionList.end()) {
                                cerr << "Tried to close a nonexistent connection" << endl;
                                SockRequestResponse replyToSocket;
                                replyToSocket.type = STATUS;
                                replyToSocket.error = ENOMATCH;
                                MinetSend(sock, replyToSocket);
                            } else {
                                if(matchingConnection->state.stateOfcnx == CLOSE_WAIT) {
                                    cerr << "Application has requested to close connection after being notified that remote host has closed its end of the connection" << endl;
                                    sendFin(*matchingConnection);
                                    matchingConnection->state.stateOfcnx = LAST_ACK;

                                    SockRequestResponse replyToSocket;
                                    replyToSocket.type = STATUS;
                                    replyToSocket.error = EOK;
                                    replyToSocket.connection = matchingConnection->connection;
                                    MinetSend(sock, replyToSocket);

                                } else {
                                    cerr << "Attemping to close connection in unknown state";
                                }
                            }

                        }

                        break;
                    case STATUS:
                        {
                            ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatching(s.connection);


                            if(matchingConnection != connectionList.end()) {
                                unsigned int receiveBufferSize = matchingConnection->state.RecvBuffer.GetSize();
                                unsigned int socketBytesPreviouslyRead = s.bytes;
                                cerr << "Socket actually read " << s.bytes << " bytes out of the receive buffer" << endl;
                                cerr << "Receive buffer is now length " << matchingConnection->state.RecvBuffer.GetSize() << endl;
                                cerr << "Removing first " << s.bytes << " from receive buffer" << endl;

                                matchingConnection->state.RecvBuffer.Erase(0, socketBytesPreviouslyRead);

                                unsigned int remainingBytes = receiveBufferSize - socketBytesPreviouslyRead;
                                if(remainingBytes > 0) {
                                    cerr << matchingConnection->state.RecvBuffer.GetSize() << " bytes remaining for socket to read from read buffer" << endl;
                                    notifySocketOfRead(*matchingConnection);

                                } else {
                                    cerr << "Socket has read all data from receive buffer, which is now empty" << endl;
                                }
                            } else {
                                cerr << "Received a status for a connection not in the connection list" << endl;
                            }
                        }
                        break;
                }
            }



        }

        cerr << "down under\n";

        //find the new smallest Timeout
        ConnectionList<TCPState>::iterator earliestTimeout = connectionList.FindEarliest();
        if (earliestTimeout!=connectionList.end()) {
            cerr << "Minimum timeout found" << endl;
            minimumTimeout = earliestTimeout->timeout.tv_sec + (earliestTimeout->timeout.tv_usec/1000000.0);
        }
        else {
            cerr << "No minimum timeout found, turning off timer" << endl;
            minimumTimeout = -1;
        }
        cerr << "new smallest Timeout = " << minimumTimeout << "\n";

        cerr << "\n\n\n";
    }
    return 0;
}

void notifySocketOfRead(ConnectionToStateMapping<TCPState> & mapping) {
    SockRequestResponse toSocket;
    toSocket.type = WRITE;
    toSocket.connection = mapping.connection;
    toSocket.data = mapping.state.RecvBuffer;
    toSocket.error = EOK;
    cerr << "Sending recieve buffer of size" << mapping.state.RecvBuffer.GetSize() << " bytes to socket" << endl;
    MinetSend(sock, toSocket);
}

void sendSyn(ConnectionToStateMapping<TCPState> & mapping) {
    mapping.timeout = Time(3.0);
    mapping.bTmrActive = true;


    Packet psend;
    IPHeader ipsend;
    ipsend.SetProtocol(IP_PROTO_TCP);
    ipsend.SetSourceIP(mapping.connection.src);
    ipsend.SetDestIP(mapping.connection.dest);
    ipsend.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);
    psend.PushFrontHeader(ipsend);

    TCPHeader tcpsend;
    tcpsend.SetSourcePort(mapping.connection.srcport, psend);
    tcpsend.SetDestPort(mapping.connection.destport, psend);
    tcpsend.SetSeqNum(mapping.state.last_sent,psend);
    tcpsend.SetAckNum(mapping.state.last_recvd + 1, psend);
    tcpsend.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, psend);

    unsigned char sendflags;
    SET_SYN(sendflags);
    tcpsend.SetFlags(sendflags, psend);
    tcpsend.SetWinSize(0,psend);
    tcpsend.SetChecksum(0);
    tcpsend.SetUrgentPtr(0, psend);
    tcpsend.RecomputeChecksum(psend);

    cerr << "Outgoing TCP Header is " << tcpsend << "\n";


    psend.PushBackHeader(tcpsend);

    cerr << "Is checksum correct?" << tcpsend.IsCorrectChecksum(psend) << endl;

    MinetSend(mux,psend);

    cerr << "Setting 3s timeout" << endl;

    cerr << "The timeout is " << mapping.timeout << endl;
}

void sendFin(ConnectionToStateMapping<TCPState> & mapping) {
    cerr << "Sending fin" << endl;

    Packet packetToSend;
    IPHeader ipHeader;
    ipHeader.SetProtocol(IP_PROTO_TCP);
    ipHeader.SetSourceIP(mapping.connection.src);
    ipHeader.SetDestIP(mapping.connection.dest);
    ipHeader.SetTotalLength(TCP_HEADER_BASE_LENGTH+IP_HEADER_BASE_LENGTH);
    packetToSend.PushFrontHeader(ipHeader);

    TCPHeader tcpHeader;
    tcpHeader.SetSourcePort(mapping.connection.srcport, packetToSend);
    tcpHeader.SetDestPort(mapping.connection.destport, packetToSend);
    tcpHeader.SetSeqNum(mapping.state.last_acked, packetToSend);
    tcpHeader.SetAckNum(mapping.state.last_recvd + 1, packetToSend);
    tcpHeader.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, packetToSend);
    unsigned char flags;
    SET_FIN(flags);
    tcpHeader.SetFlags(flags, packetToSend);
    tcpHeader.SetWinSize(100, packetToSend);
    tcpHeader.SetChecksum(0);
    tcpHeader.SetUrgentPtr(0, packetToSend);
    tcpHeader.RecomputeChecksum(packetToSend);
    packetToSend.PushBackHeader(tcpHeader);



    cerr << endl << "Sending response: " << endl;
    cerr << "Is checksum correct?" << tcpHeader.IsCorrectChecksum(packetToSend) << endl;
    IPHeader foundIPHeader=packetToSend.FindHeader(Headers::IPHeader);
    cerr << foundIPHeader << endl;
    TCPHeader foundTCPHeader=packetToSend.FindHeader(Headers::TCPHeader);
    cerr << foundTCPHeader << endl;

    MinetSend(mux, packetToSend);
}

Time subtractTime(Time x, Time y) {
           /* Perform the carry for the later subtraction by updating y. */
       if (x.tv_usec < y.tv_usec) {
         int nsec = (y.tv_usec - x.tv_usec) / 1000000.0 + 1;
         y.tv_usec -= 1000000.0 * nsec;
         y.tv_sec += nsec;
       }
       if (x.tv_usec - y.tv_usec > 1000000.0) {
         int nsec = (x.tv_usec - y.tv_usec) / 1000000.0;
         y.tv_usec += 1000000.0 * nsec;
         y.tv_sec -= nsec;
       }
       Time resultTime;
       resultTime.tv_sec = x.tv_sec - y.tv_sec;
       resultTime.tv_usec = x.tv_usec - y.tv_usec;

       return resultTime;
}

//This does not ack, must call something to ack after calling receive data
bool receiveData(ConnectionToStateMapping<TCPState> & mapping, IPHeader & ipHeader, TCPHeader & tcpHeader, Buffer & payload) {
    unsigned char flags;
    tcpHeader.GetFlags(flags);
    unsigned int remoteSequenceNumber;
    unsigned int remoteAckNumber;
    unsigned short int remoteWindowSize;
    tcpHeader.GetSeqNum(remoteSequenceNumber);
    tcpHeader.GetAckNum(remoteAckNumber);
    tcpHeader.GetWinSize(remoteWindowSize);

    cerr << "Payload: \n" << payload << endl;

    unsigned short totalLength;
    unsigned char tcpHeaderLength;
    unsigned char ipHeaderLength;
    ipHeader.GetTotalLength(totalLength);
    ipHeader.GetHeaderLength(ipHeaderLength);
    tcpHeader.GetHeaderLen(tcpHeaderLength);
    unsigned int payloadSize = totalLength - ipHeaderLength * 4 - tcpHeaderLength * 4;


    mapping.state.last_acked = remoteAckNumber - 1;

    cerr << "Received ACK " << remoteAckNumber << endl;
    cerr << "Last segment received " << mapping.state.last_recvd << endl;
    cerr << "Sequence number " << remoteSequenceNumber << endl;
    cerr << "Payload Size " << payloadSize << endl;
    cerr << "Last sequence in payload" << remoteSequenceNumber + payloadSize - 1 << endl;

    unsigned int firstSequenceNumReceived = remoteSequenceNumber;
    unsigned int lastSequenceNumReceived = remoteSequenceNumber + payloadSize - 1;
    if(firstSequenceNumReceived == mapping.state.last_recvd + 1 && payloadSize == 0) {
        cerr << "Received an ack for the correct last sent sequence number, no data has been sent" << endl;
        cerr << "Do not send an ack response" << endl;
        //mapping.state.last_recvd = firstSequenceNumReceived;
        //No need to update last received since we received 0 data bytes
        return false;
    }
    if(firstSequenceNumReceived <= mapping.state.last_recvd + 1) {
        if(lastSequenceNumReceived >= mapping.state.last_recvd + 1) {
            unsigned int copySize = lastSequenceNumReceived - mapping.state.last_recvd;
            cerr << "Adding " << copySize << " bytes to receive buffer" << endl;
            if(copySize > 0) {
                //append [last_recvd + 1, lastSequenceNumReceived] to receive buffer
                mapping.state.RecvBuffer.AddBack(payload.ExtractBack(copySize));
                cerr << "Receive buffer now has " << mapping.state.RecvBuffer.GetSize() << " bytes" << endl;

                notifySocketOfRead(mapping);
                mapping.state.last_recvd = lastSequenceNumReceived;
                cerr << "Setting last received to " << lastSequenceNumReceived << endl;
                return true;
            } else {
                cerr << "Somehow seqence numbers are valid but copy size is 0, this shouldn't happen.  Programmer error" << endl;
                return false;
            }



        } else {
            //else all received bytes are less than last received byte
            cerr << "All received data are less than last received sequence byte, none is useful" << endl;
            cerr << "Ack for last received + 1 should be sent in order to request the next expected byte" << endl;
            return true;
        }
    } else {
        cerr << "All data we received is beyond what we are looking for. " << endl;
        cerr << "Ack for last received + 1 should be sent in order to request the next expected byte" << endl;
        //Resend ack of last received + 1.  All the data we received is beyond what we are looking for
        return true;
    }
}

void sendAckNoData(Connection c, unsigned int sequenceNumber, unsigned ackNumber) {
    cerr << "Sending an ack with no data" << endl;


    Packet packetToSend;
    IPHeader ipHeader;
    ipHeader.SetProtocol(IP_PROTO_TCP);
    ipHeader.SetSourceIP(c.src);
    ipHeader.SetDestIP(c.dest);
    ipHeader.SetTotalLength(TCP_HEADER_BASE_LENGTH+IP_HEADER_BASE_LENGTH);
    packetToSend.PushFrontHeader(ipHeader);

    TCPHeader tcpHeader;
    tcpHeader.SetSourcePort(c.srcport, packetToSend);
    tcpHeader.SetDestPort(c.destport, packetToSend);
    tcpHeader.SetSeqNum(sequenceNumber, packetToSend);
    tcpHeader.SetAckNum(ackNumber, packetToSend);
    tcpHeader.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, packetToSend);
    unsigned char flags = 0;
    SET_ACK(flags);
    tcpHeader.SetFlags(flags, packetToSend);
    tcpHeader.SetWinSize(100, packetToSend);
    tcpHeader.SetChecksum(0);
    tcpHeader.SetUrgentPtr(0, packetToSend);
    tcpHeader.RecomputeChecksum(packetToSend);
    packetToSend.PushBackHeader(tcpHeader);



    cerr << endl << "Sending response: " << endl;
    cerr << "Is checksum correct?" << tcpHeader.IsCorrectChecksum(packetToSend) << endl;
    IPHeader foundIPHeader=packetToSend.FindHeader(Headers::IPHeader);
    cerr << foundIPHeader << endl;
    TCPHeader foundTCPHeader=packetToSend.FindHeader(Headers::TCPHeader);
    cerr << foundTCPHeader << endl;

    MinetSend(mux, packetToSend);
}

void sendSynAck(ConnectionToStateMapping<TCPState> & mapping) {

    mapping.timeout = Time(3.0);
    mapping.bTmrActive = true;

    Packet synAckPacketToSend;
    IPHeader ipHeader;
    ipHeader.SetProtocol(IP_PROTO_TCP);
    ipHeader.SetSourceIP(mapping.connection.src);
    ipHeader.SetDestIP(mapping.connection.dest);
    ipHeader.SetTotalLength(TCP_HEADER_BASE_LENGTH+IP_HEADER_BASE_LENGTH);
    synAckPacketToSend.PushFrontHeader(ipHeader);

    TCPHeader tcpHeader;
    tcpHeader.SetSourcePort(mapping.connection.srcport, synAckPacketToSend);
    tcpHeader.SetDestPort(mapping.connection.destport, synAckPacketToSend);
    tcpHeader.SetSeqNum(mapping.state.last_sent, synAckPacketToSend);
    tcpHeader.SetAckNum(mapping.state.last_recvd + 1, synAckPacketToSend);
    tcpHeader.SetHeaderLen(TCP_HEADER_BASE_LENGTH / 4, synAckPacketToSend);
    unsigned char flags;
    SET_ACK(flags);
    SET_SYN(flags);
    tcpHeader.SetFlags(flags, synAckPacketToSend);
    tcpHeader.SetWinSize(100, synAckPacketToSend);
    tcpHeader.SetChecksum(0);
    tcpHeader.SetUrgentPtr(0, synAckPacketToSend);
    tcpHeader.RecomputeChecksum(synAckPacketToSend);
    synAckPacketToSend.PushBackHeader(tcpHeader);

    cerr << endl << "Sending response: " << endl;
    cerr << "Is checksum correct?" << tcpHeader.IsCorrectChecksum(synAckPacketToSend) << endl;
    IPHeader foundIPHeader=synAckPacketToSend.FindHeader(Headers::IPHeader);
    cerr << foundIPHeader << endl;
    TCPHeader foundTCPHeader=synAckPacketToSend.FindHeader(Headers::TCPHeader);
    cerr << foundTCPHeader << endl;

    MinetSend(mux, synAckPacketToSend);
}

