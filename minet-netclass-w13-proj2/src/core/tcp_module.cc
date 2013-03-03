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

void sendSynAck(Connection c, unsigned int remoteISN, unsigned int localISN);

void sendAckNoData(Connection c, unsigned int sequenceNumber, unsigned ackNumber);

void receiveData(ConnectionToStateMapping<TCPState> & mapping, IPHeader & ipHeader, TCPHeader & tcpHeader, Buffer & payload);


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
    double minimumTimeout = 100000;
    Time timeElapsed;
    gettimeofday(&timeElapsed, NULL);


    unsigned tcphlen;
    Packet p;

    while (MinetGetNextEvent(event, minimumTimeout)==0) {
        // if we received an unexpected type of event, print error
        cerr << "event\n";
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

                                sendSynAck(c, remoteSequenceNumber, mapping->state.last_acked);


                                mapping->connection = c;
                                //STATE TRANSITION
                                mapping->state.stateOfcnx = SYN_RCVD;
                                mapping->state.last_sent = mapping->state.last_acked; //==ISN
                                mapping->state.last_recvd = remoteSequenceNumber;
                                mapping->state.rwnd = remoteWindowSize;

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
                        case SYN_RCVD: {
                            if(IS_ACK(flags)) {
                                if(remoteAckNumber > mapping->state.last_sent) {
                                    //Receive available data
                                   receiveData(*mapping, ipHeader, tcpHeader, p.GetPayload());
                                   //STATE TRANSITION
                                   mapping->state.stateOfcnx = ESTABLISHED;
                                } else {
                                    cerr << "Did not receive correct ack number in response to synack" << endl;
                                    sendSynAck(c, mapping->state.last_recvd, mapping->state.last_sent);
                                }
                            } else if(IS_SYN(flags)) {
                                cerr << "SYNACK was probably lost, resending" << endl;
                                sendSynAck(c, mapping->state.last_recvd, mapping->state.last_sent);
                            }
                        }
                        break;
                        case ESTABLISHED: {
                                unsigned int ackNum;
                                if(IS_ACK(flags)) {
                                    receiveData(*mapping, ipHeader, tcpHeader, p.GetPayload());
                                    ackNum = mapping->state.last_recvd + 1;
                                }

                                if(IS_FIN(flags)) {
                                    cerr << "FIN occured" << endl;
                                    ackNum = mapping->state.last_recvd + 1 + 1;



                                }
                                if(IS_ACK(flags) || IS_FIN(flags)) {
                                    sendAckNoData(mapping->connection, mapping->state.last_sent + 1, ackNum);
                                } else {
                                    cerr << "Received unknown packet while in established state, packet was not a fin or ack" << endl;
                                }

                                if(IS_FIN(flags)) {
                                    //STATE TRANSITION
                                    mapping->state.stateOfcnx = CLOSE_WAIT;
                                    SockRequestResponse requestApplicationClose;
                                    //requestApplicationClose.type = CLOSE;
                                    requestApplicationClose.type = WRITE;
                                    requestApplicationClose.connection = mapping->connection;
                                    requestApplicationClose.bytes = 0;
                                    MinetSend(sock, requestApplicationClose);
                                }


                            }
                            break;
                    }


                }

                cerr << "\n\n\n";
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
                        TCPState statesend(0,0,0);
                        Time timesend(1.0);
                        ConnectionToStateMapping<TCPState> mappingsend(s.connection, timesend, statesend, false);
                        connectionList.push_back(mappingsend);

                        Packet psend;
                        IPHeader ipsend;
                        ipsend.SetProtocol(IP_PROTO_TCP);
                        ipsend.SetSourceIP(s.connection.src);
                        ipsend.SetDestIP(s.connection.dest);
                        ipsend.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);
                        psend.PushFrontHeader(ipsend);

                        TCPHeader tcpsend;
                        tcpsend.SetSourcePort(s.connection.srcport, psend);
                        tcpsend.SetDestPort(s.connection.destport, psend);
                        unsigned char sendflags;
                        SET_SYN(sendflags);
                        tcpsend.SetSeqNum(0,psend);
                        tcpsend.SetWinSize(0,psend);
                        tcpsend.SetFlags(sendflags, psend);
                        tcpsend.SetChecksum(tcpsend.ComputeChecksum(psend));
                        cerr << "Outgoing TCP Header is " << tcpsend << "\n";
                        psend.PushBackHeader(tcpsend);
                        MinetSend(mux,psend);
                    }
                        break;

                    case ACCEPT:
                    {
                        cerr << "Socket requests to listen on port " << s.connection.srcport << endl;
                            ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatchingSource(s.connection);
                            if(matchingConnection == connectionList.end()) {
                                int isn = 666;
                                TCPState state(isn, LISTEN, 5);
                                Time time(0.0);
                                bool timerActive = false;
                                ConnectionToStateMapping<TCPState> mapping(s.connection, time, state, timerActive);
                                connectionList.push_back(mapping);
                                cerr << "Adding new connection to connection list";

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
                                cerr << "Application has requested to close connection" << endl;
                            }

                        }

                    break;
                case STATUS:
                    cerr << "SockRequestResponse Status.\n";
                    break;
                default:
                    cerr << "SockRequestResponse Unknown\n";


                        }
                        break;
                }
            }

            cerr << "down under\n";
            //find how much time has elapsed since the last clocking event
           Time timeSinceLastClock;
           gettimeofday(&timeSinceLastClock, NULL);
           timeElapsed = subtractTime(timeSinceLastClock, timeElapsed);
           cerr << "Time elapsed = " << timeElapsed << "\n";

            //after every event or timeout, update the timeout for each connection
           ConnectionList<TCPState>::iterator connectionListIterator = connectionList.begin();
            for (; connectionListIterator!=connectionList.end(); connectionListIterator++) {
                ConnectionToStateMapping<TCPState> updateConnectionToStateMapping = *connectionListIterator;
                if (updateConnectionToStateMapping.bTmrActive == true) {
                    updateConnectionToStateMapping.timeout = subtractTime(updateConnectionToStateMapping.timeout, timeElapsed);
                }
            }

            //find the new smallest Timeout
            ConnectionList<TCPState>::iterator earliestTimeout = connectionList.FindEarliest();
            if (earliestTimeout!=connectionList.end()) {
                minimumTimeout = earliestTimeout->timeout.tv_sec + (earliestTimeout->timeout.tv_usec/1000000.0);
            }
            else {
                minimumTimeout = 100000;
            }
            cerr << "new smallest Timeout = " << minimumTimeout << "\n";

            gettimeofday(&timeElapsed, NULL);

    }
    return 0;
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



void receiveData(ConnectionToStateMapping<TCPState> & mapping, IPHeader & ipHeader, TCPHeader & tcpHeader, Buffer & payload) {
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
    if(firstSequenceNumReceived <= mapping.state.last_recvd + 1) {
        if(lastSequenceNumReceived >= mapping.state.last_recvd + 1) {
            unsigned int copySize = lastSequenceNumReceived - mapping.state.last_recvd;
            cerr << "Adding " << copySize << " bytes to receive buffer" << endl;

            //append [last_recvd + 1, lastSequenceNumReceived] to receive buffer
            mapping.state.last_recvd = lastSequenceNumReceived;
            cerr << "Setting last received to " << lastSequenceNumReceived << endl;

            //sendAckNoData(mapping.connection, mapping.state.last_sent + 1, mapping.state.last_recvd + 1);
        } else {
            //else all received bytes are less than last received byte
            cerr << "All received data are less than last received sequence byte, none is useful" << endl;
            //sendAckNoData(mapping.connection, mapping.state.last_sent + 1, mapping.state.last_recvd + 1);
        }
    } else {
        cerr << "All data we received is beyond what we are looking for.  Re-request last received + 1" << endl;
        //sendAckNoData(mapping.connection, mapping.state.last_sent + 1, mapping.state.last_recvd + 1);
        //Resend ack of last received + 1.  All the data we received is beyond what we are looking for
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
    unsigned char flags;
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

void sendSynAck(Connection c, unsigned int remoteISN, unsigned int localISN) {
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
    tcpHeader.SetSeqNum(localISN, synAckPacketToSend);
    tcpHeader.SetAckNum(remoteISN + 1, synAckPacketToSend);
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

