/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//       n0    n1   n2   n3
//       |     |    |    |
//       =================
//              LAN
//
// - UDP flows from n0 to n1 and back
// - DropTail queues
// - Tracing of queues and packet receptions to file "udp-echo.tr"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

void * RunSimulation(void *);
void * listenRTI(void *);
void * sendRTI(void *);

int listen_port = 5001;
int send_port = 5000;
struct hostent *RTI_server;
int connFd = -1;

int listenFd, sendFd;
socklen_t len; //store size of the address

struct sockaddr_in svrAdd, clntAdd;
bool loop = false;
double sleepTime = 0.5;

int main(int argc, char *argv[]) {

	Ptr<GridlabdSimulatorImpl> simulatorImpl = new GridlabdSimulatorImpl();
	Simulator::SetImplementation(simulatorImpl);

//if(strcmp(argv[1]))

//
// Users may find it convenient to turn on explicit debugging
// for selected modules; the below lines suggest how to do this
//
#if 1
	LogComponentEnable("UdpEchoExample", LOG_LEVEL_INFO);
	LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
	LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
#endif
//
// Allow the user to override any of the defaults and the above Bind() at
// run-time, via command-line arguments
//
	bool useV6 = false;
	Address serverAddress;

	CommandLine cmd;
	cmd.AddValue("useIpv6", "Use Ipv6", useV6);
	cmd.Parse(argc, argv);
//
// Explicitly create the nodes required by the topology (shown above).
//
	NS_LOG_INFO ("Create nodes.");
	NodeContainer n;
	n.Create(4);

	InternetStackHelper internet;
	internet.Install(n);

	NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
	csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
	csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
	NetDeviceContainer d = csma.Install(n);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
	NS_LOG_INFO ("Assign IP Addresses.");
	if (useV6 == false) {
		Ipv4AddressHelper ipv4;
		ipv4.SetBase("10.1.1.0", "255.255.255.0");
		Ipv4InterfaceContainer i = ipv4.Assign(d);
		serverAddress = Address(i.GetAddress(1));
	} else {
		Ipv6AddressHelper ipv6;
		ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
		Ipv6InterfaceContainer i6 = ipv6.Assign(d);
		serverAddress = Address(i6.GetAddress(1, 1));
	}

	NS_LOG_INFO ("Create Applications.");
//
// Create a UdpEchoServer application on node one.
//
	uint16_t port = 9;  // well-known echo port number
	UdpEchoServerHelper server(port);
	ApplicationContainer apps = server.Install(n.Get(3));
	apps.Start(Seconds(1.0));
	apps.Stop(Seconds(5.0));

//
// Create a UdpEchoClient application to send UDP datagrams from node zero to
// node one.
//
	uint32_t packetSize = 1024;
	uint32_t maxPacketCount = 1000;
	Time interPacketInterval = Seconds(1.);
	UdpEchoClientHelper client(serverAddress, port);
	client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
	client.SetAttribute("Interval", TimeValue(interPacketInterval));
	client.SetAttribute("PacketSize", UintegerValue(packetSize));

	//Simulator::Stop(Seconds(11));

#if 0
//
// Users may find it convenient to initialize echo packets with actual data;
// the below lines suggest how to do this
//
	client.SetFill (apps.Get (0), "Hello World");

	client.SetFill (apps.Get (0), 0xa5, 1024);

	uint8_t fill[] = {0, 1, 2, 3, 4, 5, 6};
	client.SetFill (apps.Get (0), fill, sizeof(fill), 1024);
#endif

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
	csma.EnablePcapAll("udp-echo", false);

	/* socket communication initialization with NS3 begins */

	pthread_t threadA[3];
	int noThread = 0;

	char RTI_serverAddress[100] = "127.0.0.1";
	RTI_server = gethostbyname(RTI_serverAddress);

	if (RTI_server == NULL) {
		printf("Host does not exist");
		return 0;
	}

	//create socket
	listenFd = socket(AF_INET, SOCK_STREAM, 0);

	if (listenFd < 0) {
		printf("Cannot open socket");
		return 0;
	}

	bzero((char*) &svrAdd, sizeof(svrAdd));

	svrAdd.sin_family = AF_INET;
	svrAdd.sin_addr.s_addr = INADDR_ANY;
	svrAdd.sin_port = htons(listen_port);

	//bind socket
	if (bind(listenFd, (struct sockaddr *) &svrAdd, sizeof(svrAdd)) < 0) {
		printf("Cannot bind, listenFd %d", listenFd);
		return 0;
	} else {
		printf("Connection bound, listenFd %d", listenFd);
	}

	listen(listenFd, 5);

	len = sizeof(clntAdd);

	//create client skt
	sendFd = socket(AF_INET, SOCK_STREAM, 0);

	if (sendFd < 0) {
		printf("Cannot open socket");
		return 0;
	}

	bzero((char *) &svrAdd, sizeof(svrAdd));
	svrAdd.sin_family = AF_INET;

	bcopy((char *) RTI_server->h_addr, (char *) &svrAdd.sin_addr.s_addr, RTI_server -> h_length);

	svrAdd.sin_port = htons(send_port);

	int checker = connect(sendFd, (struct sockaddr *) &svrAdd, sizeof(svrAdd));

	if (checker < 0) {
		printf("Cannot connect to GLD!");
		//return 0;
	} else {
		connFd = sendFd;
		printf("Connection to server success ");

		//int64_t maxTime = 0;
		//read(connFd, &maxTime, sizeof(maxTime)) ;
		//int64_t maxGldSimulationTime = ntohl(maxTime);

		int64_t maxGldSimulationTime = 0;
		read(connFd, &maxGldSimulationTime, sizeof(maxGldSimulationTime));
		printf("Max Gld simulation time %lld", maxGldSimulationTime);
		apps = client.Install(n.Get(0));
		apps.Start(Seconds(2.0));
		apps.Stop(Seconds(maxGldSimulationTime));

		apps = client.Install(n.Get(1));
		apps.Start(Seconds(2.0));
		apps.Stop(Seconds(maxGldSimulationTime));
		//pthread_create(&threadA[noThread++], NULL, listenRTI, NULL);
		//pthread_create(&threadA[noThread++], NULL, sendRTI, NULL);

		//noThread+=2;
	}

	/* socket communication initialization with NS3 ends */

	pthread_create(&threadA[noThread++], NULL, RunSimulation, NULL);
	bool convergenceFailure = false;

	while (!loop) {

		bool isFinished = Simulator::IsFinished();
		printf("IsFinished %s\n", isFinished ? "true" : "false");
		if (isFinished == true) {
			printf("Simulation events over");
			//close(connFd);
			loop = true;
			break;
		}

		// receiving part
		printf("reading at NS3 \n");
		char receive[300];
		int error = -1;
		error = read(connFd, receive, 300);
		printf("Printing GLD's message : %s\n", receive);

		if (strcmp(receive, "exit") == 0 || error == -1) {
			//mtx.lock();
			loop = true;
			//mtx.unlock();
			break;
		} else if (strstr(receive, "convergenceFailure") != NULL
				|| error == -1) {
			convergenceFailure = true;
			Simulator::Stop();
			//Simulator::Destroy();
			break;
		}

		// simulator advance
		Simulator::AdvanceTimeGrant();

		// sending part
		printf("Enter stuff: ");
		char send[300] = "sent to GLD";
		//scanf("%s", send);
		char fraudSend[300] = "attack";
		uint64_t currentTime = (uint64_t) Simulator::Now().GetTimeStep();
		uint64_t attackStartTime = (uint64_t) 5 * 1000000000;
		uint64_t attackEndTime = (uint64_t) 6 * 1000000000;
		printf("startTime %llu currentTime %llu endTime %llu\n",
				attackStartTime, currentTime, attackEndTime);
		if (attackStartTime <= currentTime && currentTime <= attackEndTime) {
			printf("Sending %s ", fraudSend);
			write(connFd, fraudSend, strlen(fraudSend));
		} else {
			printf("Sending %s ", send);
			write(connFd, send, strlen(send));
		}

		if (strcmp(send, "exit") == 0) {
			//mtx.lock();
			loop = true;
			//mtx.unlock();
			Simulator::Stop();
			printf("Closing send thread and conn\n");
			close(connFd);
			break;
		}

	}

	if (convergenceFailure) {
		//Simulator::Destroy();
	}

	//Simulator::Destroy();
	Simulator::Stop();
	// close the socket
	close(connFd);

	for (int i = 0; i < noThread; i++) {
		pthread_join(threadA[i], NULL);
	}

}

void * RunSimulation(void *) {
	//
// Now, do the actual simulation.
//
	NS_LOG_INFO ("Run Simulation.");
	Simulator::Run();
	Simulator::Destroy();
	NS_LOG_INFO ("Done.");
	loop = true;

	void * p = NULL;
	return p;
}

/*
 void * listenRTI(void *) {

 //cout << "Thread No: " << pthread_self() << " listening " << endl;
 printf("****************** Thread no. %d listening ************************ \n", 1);

 //int count = 0;


 char test[300];
 //bzero(test, 301);
 char exits[5] = "exit";

 while(!loop )
 {
 sleep(sleepTime);
 bzero(test, 301);

 printf("reading\n");
 read(connFd, test, 300);
 printf("Printing GLD's message : %s\n", test);

 //string tester (test);
 //cout << tester << " count " << ++count << endl;
 //std::cout << tester << endl;

 if(strcmp(test,exits) == 0){
 //mtx.lock();
 loop = true;
 //mtx.unlock();
 break;
 }

 Simulator::AdvanceTimeGrant();

 }
 printf("\nClosing read thread and conn \n" );
 //close(connFd);
 loop = true;

 void * p;
 return p;
 }

 void * sendRTI(void *) {

 printf("Thread NS3: sending ");
 //send stuff to server
 while(!loop)
 {
 sleep(sleepTime);
 char s[300] = "bhadve";
 //cin.clear();
 //cin.ignore(256, '\n');
 printf("Enter stuff: \n");


 printf("Sending \n");
 write(connFd, s, strlen(s));

 if(strcmp(s, "exit") == 0){
 //mtx.lock();
 loop = true;
 //mtx.unlock();
 break;
 }
 }

 printf("\nClosing send thread and conn");
 close(connFd);
 loop = true;

 void * p;
 return p;

 }*/

