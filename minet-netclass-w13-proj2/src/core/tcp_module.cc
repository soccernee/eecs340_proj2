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


using std::cout;
using std::endl;
using std::cerr;
using std::string;

int main(int argc, char *argv[])
{
  cerr << "howdy, mate\n";

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

  cerr << "tcp_module handling TCP traffic.....\n";

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;
  cerr << "other side of MinetMonitoringEvent\n";

  while (MinetGetNextEvent(event)==0) {
    cerr << "Minet Event.\n";
    // if we received an unexpected type of event, print error
    if (event.eventtype!=MinetEvent::Dataflow
	|| event.direction!=MinetEvent::IN) {
      cerr << "Unknown event ignored.\n";
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    // if we received a valid event from Minet, do processing
    } else {
       cerr << "Known event not ignored.\n";
      //  Data from the IP layer below  //
      if (event.handle==mux) {
        cerr << "Request Received.\n";
        Packet p;
        MinetReceive(mux,p);
        unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
        cerr << "estimated header len="<<tcphlen<<"\n";
        p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
        IPHeader ipl=p.FindHeader(Headers::IPHeader);
        TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

        cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
        cerr << "TCP Header is "<<tcph << " and ";

        cerr  << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");

        Connection c;
        ipl.GetDestIP(c.src);
        ipl.GetSourceIP(c.dest);
        ipl.GetProtocol(c.protocol);
        tcph.GetDestPort(c.srcport);
        tcph.GetSourcePort(c.destport);

        cerr << "\n\nIncoming packet. Return packet should contain:\n";
        cerr << "Source IP is " << c.src << "\n";
        cerr << "Dest IP is " << c.dest << "\n";

        //if connection state = LISTEN (1)
        //if (connectionState == 1) {

         //   if (is_SYN(tcph)) {
         //       TCPHeader tcpsend;

          //  }
        //}

        Packet pssend;
        IPHeader ipsend;
        ipsend.SetProtocol(IP_PROTO_TCP);
        ipsend.SetSourceIP(c.src);
        ipsend.SetDestIP(c.dest);
        ipsend.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);
        p.PushFrontHeader(ipsend);

        TCPHeader tcpsend;
        tcpsend.SetSourcePort(c.srcport,p);
        tcpsend.SetDestPort(c.destport,p);
        tcpsend.SetHeaderLen(TCP_HEADER_BASE_LENGTH,p);
        p.PushBackHeader(tcpsend);
        MinetSend(mux,p);

        }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {
        SockRequestResponse s;
        MinetReceive(sock,s);
        cerr << "Received Socket Request:" << s << endl;
      }
    }
  }
  cerr << "beind while loop\n";
  MinetDeinit();
  return 0;
}
