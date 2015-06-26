/* WOscStreamingClient.cpp - A streaming OSC client using posix threads and sockets
 *
 *  Created on: Apr 21, 2010
 *      Author: uli
 *
 */

#include "WOscConfig.h"

#if OS_IS_LINUX == 1 || OS_IS_MACOSX == 1 || OS_IS_CYGWIN == 1

#include <iostream>
#include <netdb.h>		// gethostbyname
#include <arpa/inet.h>

#include "WOscTcpClient.h"

static bool
resolve(struct in_addr& ia, const char* host)
{
	struct hostent* h = gethostbyname (host);
    if (h == NULL) {
    	return false;
    }

    for (int i = 0; h->h_addr_list[i]; i++) {
        if (h->h_addrtype == AF_INET) {
            /* A records (IPv4) */
            if ((unsigned int) h->h_length > sizeof(ia)) {
            	// Address size mismatch
                continue;
            }
            memcpy (&ia, h->h_addr_list[i], h->h_length);
            return true;
        }
    }
    return false;
}

class WOscStreamingClient: public WOscTcpClient
{
public:
	WOscStreamingClient(bool threading) : WOscTcpClient(threading)
	{

	}
	virtual void HandleNonmatchedMessages(const WOscMessage* msg,
				const WOscNetReturn* networkReturnAddress)
	{
		std::cout<<msg->GetOscAddress()<<":\n"<<std::endl;
		for ( int i = msg->GetNumInts()-1; i >= 0; i-- )
			std::cout<<"int["<<i<<"]: "<<msg->GetInt(i)<<std::endl;
		for ( int i = msg->GetNumStrings()-1; i >= 0; i-- )
			std::cout<<"str["<<i<<"]: "<<msg->GetString(i)<<std::endl;
		for ( int i = msg->GetNumFloats()-1; i >= 0; i-- )
			std::cout<<"flt["<<i<<"]: "<<msg->GetFloat(i)<<std::endl;
	}
};

const char* WOSC_TCP_CLIENT_HELP_STR =
		"\n"
		" WOscTcpClient, Copyright 2004-2010 Uli Franke\n"
		"\n"
		" Invoking \"wosctcpclient\":\n"
		" woscclient <server url> <server port>\n"
		"\n"
		" Commands controlling \"woscclient\":\n"
		"\n"
		" exit           Terminate \"woscclient\".\n"
		"\n"
		" /[msg] [prms]  Send an OSC message \"/msg\" to remote OSC server with\n"
		"                  parameters [params]. [params] can be a space separated\n"
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
	const char* default_serverurl = "localhost";
	short default_serverport = 7554;
	bool threading = false;
	WOscStreamingClient client(threading);

	//
	// Setup OSC address space
	//

	// containers
	WOscContainer* cntRoot = new WOscContainer();
	WOscContainer* cntTest = new WOscContainer(cntRoot, "test");

	// "root" methods
	WOscMethod* msgEcho = new WOscTcpHandlerMethod(cntRoot, &client, "printed", "printed description.");
	msgEcho->AddMethodAlias(cntTest, "echo");

	// add alias of method to same container where the original resides (to test the clean up)
	msgEcho->AddMethodAlias(cntTest, "echo2");

	// add alias of container to same container where the original resides (to test the clean up)
	cntTest->AddContainerAlias(cntRoot, "test2");

	client.SetAddressSpace(cntRoot);

	WOscString addressSpaceStr = cntRoot->GetAddressSpace();
	std::cout<<"Client address space:\n"<<addressSpaceStr.GetBuffer()<<std::flush;

	//
	// Connect to server
	//

	TheNetReturnAddress serverAddress;
	serverAddress.m_addr.sin_family = AF_INET;
	serverAddress.m_addr.sin_port = htons(default_serverport);
	if ( resolve(serverAddress.m_addr.sin_addr, default_serverurl) ) {
		std::cout<<"Resolved \""<<default_serverurl<<"\" successfully."<<std::endl;
	} else {
		std::cout<<"Failed to resolve \""<<default_serverurl<<"\". Defaulting to 127.0.0.1"<<std::endl;
		default_serverurl = "127.0.0.1";
		serverAddress.m_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	}
	std::cout<<"Connecting to \""<<default_serverurl<<"\" ("<<*inet_ntoa(serverAddress.m_addr.sin_addr) <<")."<<std::endl;

	WOscTcpClient::Error clientError = client.Connect(serverAddress);
	if ( clientError != WOscTcpClient::WOS_ERR_NO_ERROR ) {
		std::cout<<"Client connect failed with: "<<WOscTcpClient::GetErrStr(clientError)<<std::endl;
		return -1;
	}

	std::cout<<"Connected. Entering mainloop."<<std::endl;

	//
	// Main loop
	//
	WOscBundle theBundle;
	bool bundleOpen = false;

	while ( true ) {
		char input[1024];
		if ( threading ) {
			std::cin.getline(input,1024,'\n');
		} else {

			fd_set rfds;
			struct timeval tv;
			int retval;

			/* Watch stdin (fd 0) to see when it has input. */
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			FD_SET(client.GetSocketID(), &rfds);

			/* Wait up to five seconds. */
			tv.tv_sec = 0;
			tv.tv_usec = 0;

			retval = select(client.GetSocketID()+1, &rfds, NULL, NULL, NULL/*&tv*/);
			/* Don't rely on the value of tv now! */

			if (retval == -1) {
				perror("select()");
				break;
			} else if (retval) {
				if ( FD_ISSET(0, &rfds) ) {
					std::cin.getline(input,1024,'\n');
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
				} else {
					std::cout<<"No file descriptor for this select."<<std::endl;
					break;
				}
			} else {
				printf("Select timeout.\n");
				continue;
			}

		}

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
		else {
			// exit command
			if ( strcmp("exit",input) == 0 ) {
				break;
			}
			// help command
			else if ( strcmp("help",input) == 0 ) {
//				std::cout << WOSC_CLIENT_HELP_STR ;
			}
			// set local port (slp)
			else if  ( strncmp("slp",input,3) == 0 ) {
				/*
				localport = atoi( &input[4] );
				std::cout << "  Trying to set local port to " << localport << "."<<std::endl ;
				client.NetworkHalt();
				if ( client.NetworkInit( localport ) != WOscClient::WOS_ERR_NO_ERROR ) {
					std::cout << "Exit."<<std::endl ;
					return -1;
				}
				std::cout << "  Port successfully changed and network restarted."<<std::endl ;
				*/
			}
			// set remote port (srp)
			else if  ( strncmp("srp",input,3) == 0 ) {
				/*
				remoteport = atoi( &input[4] );
				std::cout << "  Remote port set to " << remoteport << "."<<std::endl ;
				*/
			}
			// set remote IP (srip)
			else if  ( strncmp("srip",input,4) == 0 ) {
				/*
				strcpy(remoteIP, &input[5]);
				std::cout << "  Remote IP set to " << remoteIP << "."<<std::endl ;
				*/
			}
		}
	}

	clientError = client.Close();
	if ( clientError != WOscTcpClient::WOS_ERR_NO_ERROR ) {
		std::cout<<"Client close failed with: "<<WOscTcpClient::GetErrStr(clientError)<<std::endl;
		return -1;
	}

	return 0;
}

#elif OS_IS_WIN32 == 1
/* currently this example does not support windows - volunteers?
#	include "windows.h"
#	include "winsock2.h"
#	define socklen_t	int
*/
#	warning "WOscStreamingClient not supported on this platform"
int main() { return 0; }
#endif
