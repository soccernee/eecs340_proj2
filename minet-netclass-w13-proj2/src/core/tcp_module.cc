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
                IPHeader ipl=p.FindHeader(Headers::IPHeader);
                TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

                cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
                cerr << "TCP Header is "<<tcph << " and ";

                cerr  << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID") << "\n";

                cerr << "\nReceived TCP Packet\n";
                Connection c;
                ipl.GetDestIP(c.src);
                ipl.GetSourceIP(c.dest);
                ipl.GetProtocol(c.protocol);
                tcph.GetDestPort(c.srcport);
                tcph.GetSourcePort(c.destport);
                cerr << "Received from " << c.dest << ":" << c.destport << "\n";
                cerr << "Sent to " << c.src << ":" << c.srcport << "\n";

                ConnectionList<TCPState>::iterator matchingConnection = connectionList.FindMatching(c);
                if(matchingConnection == connectionList.end()) {
                    cerr << "Current connection state: CLOSED\n";
                    cerr << "This is a new connection\n";
                    TCPState state(666, CLOSED, 5);
                    Time time(1.0);
                    bool timerActive = false;
                    ConnectionToStateMapping<TCPState> mapping(c, time, state, timerActive);
                    connectionList.push_back(mapping);
                } else {
                    ConnectionToStateMapping<TCPState> mapping = *matchingConnection;
                    cerr << "Current connection state: " << mapping.state << "\n";
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
            }
        }
    }
    return 0;
}
