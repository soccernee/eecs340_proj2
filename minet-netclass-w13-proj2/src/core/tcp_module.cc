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


using std::cout;
using std::endl;
using std::cerr;
using std::string;

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

    MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

    MinetEvent event;
    ConnectionList<TCPState> connectionList;

    while (MinetGetNextEvent(event)==0) {
        // if we received an unexpected type of event, print error
        if (event.eventtype!=MinetEvent::Dataflow || event.direction!=MinetEvent::IN) {
            MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
            // if we received a valid event from Minet, do processing
        } else {
            //  Data from the IP layer below  //
            if (event.handle==mux) {
                Packet p;
                MinetReceive(mux,p);
                unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
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


                //if connection state = LISTEN (1)
                //if (connectionState == 1) {

                //   if (is_SYN(tcph)) {
                //       TCPHeader tcpsend;

                //  }
                //}
                cerr << "\n\n\n";
            }
            //  Data from the Sockets layer above  //
            if (event.handle==sock) {
                SockRequestResponse s;
                MinetReceive(sock,s);
                cerr << "Received Socket Request:" << s << endl;
                switch(s.type) {
                    case CONNECT:
                        {
                            //ACTIVE OPEN
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
                        {

                        }
                        break;
                }
                //Get destination
                //If destination is not in the connection list, add it to the connection list
            }
        }
    }
    return 0;
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
