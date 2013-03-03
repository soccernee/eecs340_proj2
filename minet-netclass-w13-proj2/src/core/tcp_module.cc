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

Time subtractTime(Time x, Time y);

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

    cerr << "tcp_module handling TCP traffic.\n";

    MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

    MinetEvent event;
    ConnectionList<TCPState> connectionList;
    double minimumTimeout = 100000;
    Time timeElapsed;
    gettimeofday(&timeElapsed, NULL);

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

                    SockRequestResponse replyToSocket;
                    replyToSocket.type=STATUS;
                    replyToSocket.connection = s.connection;
                    replyToSocket.bytes =  0;
                    replyToSocket.error = EOK;
                    MinetSend(sock,replyToSocket);
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
                                replyToSocket.connection = s.connection;
                                replyToSocket.bytes = 0;
                                replyToSocket.error = EOK;

                            } else {
                                cerr << "Connection already exists in connection list" << endl;
                            }

                    }

                    break;
                case STATUS:
                    cerr << "SockRequestResponse Status.\n";
                    break;
                case WRITE:
                    cerr << "SockRequestResponse Status.\n";







                    break;
                default:
                    cerr << "SockRequestResponse Unknown\n";

            }
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



