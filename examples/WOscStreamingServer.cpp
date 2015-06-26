/*
 * WOscStreamingServer.cpp - A streaming OSC server using posix threads and sockets
 *
 *  Created on: Oct 22, 2010
 *      Author: uli
 *
 */

#include "WOscConfig.h"

#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1

#include <iostream>
#include <arpa/inet.h>

#include "WOscTcpServer.h"

static void
msgPrint(const WOscMessage* msg)
{
	int nI = msg->GetNumInts(), nS = msg->GetNumStrings(), nF = msg->GetNumFloats();
	std::cout<<msg->GetOscAddress()<<(nI+nS+nF>0?":\n":"\n");
	for ( int i = nI-1; i >= 0; i-- )
		std::cout<<"int["<<i<<"]: "<<msg->GetInt(i)<<std::endl;
	for ( int i = nS-1; i >= 0; i-- )
		std::cout<<"str["<<i<<"]: "<<msg->GetString(i)<<std::endl;
	for ( int i = nF-1; i >= 0; i-- )
		std::cout<<"flt["<<i<<"]: "<<msg->GetFloat(i)<<std::endl;
}

class WOscStreamingHandler;

class WOscTcpHandlerPrintMethod: public WOscTcpHandlerMethod {
public:
	WOscTcpHandlerPrintMethod(WOscContainer* parent, WOscTcpHandler* receiverContext) :
				WOscTcpHandlerMethod(parent, receiverContext, "print", "I will just print to stdout.") {
	}
protected:
	virtual void Method(const WOscMessage *message) {
		msgPrint(message);
	}
};

class WOscTcpHandlerEchoMethod: public WOscTcpHandlerMethod {
public:
	WOscTcpHandlerEchoMethod(WOscContainer* parent, WOscTcpHandler* receiverContext) :
				WOscTcpHandlerMethod(parent, receiverContext, "echo", "I will return the message to its origin.") {
	}
protected:
	virtual void Method(const WOscMessage *message) {
		msgPrint(message);
		WOscMessage msg(message);
		GetContext()->NetworkSend(msg.GetBuffer(), msg.GetBufferLen());
	}
};

class WOscStreamingHandler: public WOscTcpServerHandler
{
public:
	WOscStreamingHandler(WOscTcpServer* server) : WOscTcpServerHandler(server)
	{
		//
		// Setup OSC address space
		//

		// containers
		WOscContainer* cntRoot = new WOscContainer();
		WOscContainer* cntTest = new WOscContainer(cntRoot, "test");

		// "root" methods
		/*WOscMethod* methodEcho =*/ new WOscTcpHandlerEchoMethod(cntRoot, this);
		WOscMethod* methodPrint = new WOscTcpHandlerPrintMethod(cntRoot, this);
		methodPrint->AddMethodAlias(cntTest, "prt");

		// add alias of method to same container where the original resides (to test the clean up)
		methodPrint->AddMethodAlias(cntTest, "prt2");

		// add alias of container to same container where the original resides (to test the clean up)
		cntTest->AddContainerAlias(cntRoot, "test2");

		SetAddressSpace(cntRoot);

		std::cout<<"Handler's address space:\n"<<cntRoot->GetAddressSpace().GetBuffer()<<std::flush;
	}

	virtual void HandleNonmatchedMessages(const WOscMessage* msg,
				const WOscNetReturn* networkReturnAddress)
	{
		std::cout<<"No handler installed for:"<<std::endl;
		msgPrint(msg);
	}
};

class WOscStreamingServer: public WOscTcpServer
{
public:
	virtual WOscTcpServerHandler::Error SetupHandlerForNewConnection(WOscTcpServerHandler** handler, int socket, const TheNetReturnAddress& peer)
	{
		WOscTcpServerHandler* h = new WOscStreamingHandler(this);
		WOscTcpServerHandler::Error err = h->Start(socket, peer);
		if (err != WOscTcpServerHandler::WOS_ERR_NO_ERROR ) {
			WOSC_ERR_PRT("Starting connection handler failed with "<<err);
			delete h;
		} else {
			*handler = h;
		}
		return err;
	}
};

const char* WOSC_TCP_SERVER_HELP_STR =
		"\n"
		" WOscStreamingServer, Copyright 2004-2010 Uli Franke\n"
		"\n"
		" Invoking \"woscstreamingserver\":\n"
		" woscstreamingserver [server url] [server port]\n"
		"\n"
		" Commands controlling \"woscclient\":\n"
		"\n"
		" exit           Terminate \"woscclient\".\n"
		"\n"
		" /[msg] [prms]  Broadcast an OSC message \"/msg\" to all connected OSC clients\n"
		"                  parameters [prms]. [prms] can be a space separated\n"
		"                  list containing integers ( 666 ), floats ( 3.14 ) and\n"
		"                  strings ( hello_world ).\n"
		"\n"
		" [              Start a bundle (currently no nesting). Messages can be\n"
		"                  added to the bundle afterwards as depicted above.\n"
		" ]              Close a bundle and transmit it.\n"
		"\n";

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


int main()
{
//	const char* default_serverurl = "localhost";
	short default_serverport = 7554;
	in_addr_t default_serveraddr = INADDR_ANY;
	WOscStreamingServer server;

	WOscStreamingServer::Error err = server.Bind(default_serverport, default_serveraddr);
	if ( err != WOscStreamingServer::WOS_TCPS_ERR_NO_ERR ) {
		std::cout<<"Server bind failed with: "<<WOscTcpServer::GetErrStr(err)<<std::endl;
		return -1;
	}
//	std::cout<<"Listening at ("<<*inet_ntoa(default_serveraddr) <<")."<<std::endl;
	std::cout<<"Listening at port "<<default_serverport<<"."<<std::endl;

	//
	// Main loop
	//
//	WOscBundle theBundle;
//	bool bundleOpen = false;

	while ( true ) {
		char input[1024];

		fd_set rfds;
		struct timeval tv;
		int retval;

		/* Watch stdin (fd 0) to see when it has input. */
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
//			FD_SET(client.GetSocketID(), &rfds);

		/* Wait up to five seconds. */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL/*&tv*/);
		/* Don't rely on the value of tv now! */

		if (retval == -1) {
			perror("select()");
			break;
		} else if (retval) {
			if ( FD_ISSET(0, &rfds) ) {
				std::cin.getline(input,1024,'\n');
			/*
			} else if ( FD_ISSET(client.GetSocketID(), &rfds) ) {
				WOscTcpClient::Error err = client.NetworkReceive();
				if ( err == WOscTcpClient::WOS_ERR_NO_ERROR )
					continue;
				else if ( err == WOscTcpClient::WOS_ERR_CONNECTION_CLOSED ) {
					std::cout<<"Connection has been closed by peer."<<std::endl;
					break;
				} else {
					std::cout<<"Connection failure on receive."<<std::endl;
					break;
				}
			*/
			} else {
				std::cout<<"No file descriptor for this select."<<std::endl;
				break;
			}
		} else {
			printf("Select timeout.\n");
			continue;
		}

#if 0
		if ( input[0] == '/' ) {
			// message assembly
			try {
				// parse parameters
				char* params[100];
				int nParams = 0;
				char* next = strchr ( input, ' ' );
				while ( next != 0 || nParams >= 100 ) {
					// add parameter to list
					params[nParams] = next + 1;
					// and zero-terminate previous
					*next = 0;
					// try to find new parameter
					next = strchr ( params[nParams++], ' ' );
				}

				// assemble message
				WOscMessage msg(input);
				// add parameters
				for ( int i = 0; i < nParams; i++ ) {
					int parInt = atoi(params[i]);
					float parFlt = atof(params[i]);
					// check if float
					if ( strchr ( params[i], '.' ) ) {
						msg.Add( parFlt );
					} else if ( parInt != 0 ) {
						msg.Add( parInt );
					} else {
						msg.Add( params[i] );
					}
				}
				// we pack message into bundle when open
				if ( bundleOpen ) {
					theBundle.Add(new WOscMessage(msg));
				} else {
					clientError = client.NetworkSend(msg.GetBuffer(), msg.GetBufferLen());
					if ( clientError != WOscTcpClient::WOS_ERR_NO_ERROR ) {
						std::cout<<"Client transmit failed with: "<<WOscTcpClient::GetErrStr(clientError)<<std::endl;
						break;
					}
				}
			} catch(const WOscException& e ) {
				std::cout<<"Exception: " << e.GetErrStr() <<std::endl;
			}
		}
		// open bundle
		else if (input[0] == '[') {
			if ( bundleOpen ) {
				std::cout<<"Bundle already open."<<std::endl;
			} else {
				theBundle.Reset();
				bundleOpen = true;
			}
		}
		// close bundle
		else if (input[0] == ']') {
			if ( bundleOpen ) {
				clientError = client.NetworkSend(theBundle.GetBuffer(), theBundle.GetBufferLen());
				if ( clientError != WOscTcpClient::WOS_ERR_NO_ERROR ) {
					std::cout<<"Client transmit of bundle failed with: "<<WOscTcpClient::GetErrStr(clientError)<<std::endl;
					break;
				}
				bundleOpen = false;
			} else {
				std::cout<<"No bundle to close."<<std::endl;
			}
		}
		// other commands
		else
#endif
		{
			// exit command
			if ( strcmp("exit",input) == 0 ) {
				break;
			}
			// help command
			else if ( strcmp("help",input) == 0 ) {
					std::cout << WOSC_TCP_SERVER_HELP_STR ;
			}
		}
	}
	std::cout<<"Stopping server."<<std::endl;
	err = server.Stop();
	if ( err != WOscTcpServer::WOS_TCPS_ERR_NO_ERR ) {
		std::cout<<"Server stop failed with: "<<WOscTcpServer::GetErrStr(err)<<std::endl;
		return -1;
	}
	std::cout<<"Done."<<std::endl;

	return 0;
}

#elif OS_IS_WIN32 == 1
/* currently this example does not support windows - volunteers?
#	include "windows.h"
#	include "winsock2.h"
#	define socklen_t	int
*/
#	warning "WOscStreamingServer not supported on this platform"
int main() { return 0; }
#endif
