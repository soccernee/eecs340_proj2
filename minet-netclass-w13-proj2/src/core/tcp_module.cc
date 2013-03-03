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

int main(int argc, char *argv[]) {
    MinetHandle mux, sock;

    MinetInit(MINET_TCP_MODULE);

    mux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
    sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

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

                ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatching(c);
                if(matchingConnection == connectionList.end()) {
                    cerr << "Received packet from a connection not in the list" << endl;


                } else {
                    ConnectionToStateMapping<TCPState> mapping = *matchingConnection;
                    cerr << "Current connection state: " << mapping.state << "\n";

                    unsigned char flags;
                    tcpHeader.GetFlags(flags);

                    unsigned int remoteSequenceNumber;
                    unsigned int remoteAckNumber;

                    tcpHeader.GetSeqNum(remoteSequenceNumber);
                    tcpHeader.GetAckNum(remoteAckNumber);


                    if(IS_SYN(flags)) {
                        cerr << "SYN Received, remote ISN is " << remoteSequenceNumber << endl;
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
    /*
                                SockRequestResponse replyToSocket;
                                replyToSocket.type = STATUS;
                                replyToSocket.connection = s.connection;
                                replyToSocket.bytes = 0;
                                replyToSocket.error = EOK;*/

                            } else {
                                cerr << "Connection already exists in connection list" << endl;
                            }
                        }
                        break;
                    case WRITE:
                        break;
                    case FORWARD:
                        break;
                    case CLOSE:
                        break;
                    case STATUS:
                        break;
                }
                //Get destination
                //If destination is not in the connection list, add it to the connection list
            }
        }
    }
    return 0;
}
