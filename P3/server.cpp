//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif
#define PRESERVED_PORT  4001
#define GROUP_ID "P3_GROUP_ 85"
#define BACKLOG  5        // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
  public:
    int sock;              // socket of client connection
    std::string groupId;           // Limit length of name of client's user
    std::string port;
    std::string ip;
    bool isServer;
    Client(int socket) : sock(socket){} 

    ~Client(){}            // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients; // Lookup table for per Client information

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      return(sock);
   }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{

     printf("Client closed connection: %d\n", clientSocket);

     // If this client's socket is maxfds then the next lowest
     // one has to be determined. Socket fd's can be reused by the Kernel,
     // so there aren't any nice ways to do this.

     close(clientSocket);      

     if(*maxfds == clientSocket)
     {
        for(auto const& p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
     }

     // And remove from the list of open sockets.

     FD_CLR(clientSocket, openSockets);

}

//GET CONNECTED SERVERS
std::string CONNECTED(std::string PORT){
    std::string retString = "";
    retString = "CONNECTED,";
    retString += GROUP_ID;
    retString += ",";
    retString += "46.182.189.139,"; //change to skel
    retString +=  PORT;
    retString += ";";
    for(auto const& isServer : clients)
     {  
        if(isServer.second->isServer == 1){ //only add data if client is of a external server, not a local client
            retString += "data";
        }
     }
    return retString;
}

// Process command from client on the server
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, 
                  char *buffer, std::string PORT) 
{
  std::vector<std::string> tokens;
  std::string token;

  // Split command from client into tokens for parsing
  std::stringstream stream(buffer);

  while(stream >> token)
      tokens.push_back(token);
  
  //Quearyservers, <from_group_id>
  if((tokens[0].compare("QUERYSERVERS") == 0) && (tokens.size() == 2))
  {
     //CONNECTED(self.groupid, self.ip, self.port)
     std::string msg = CONNECTED(PORT);
     std::cout<< msg;
    //  send(clientSocket, msg.c_str(), msg.length()-1, 0);
  }

  else if((tokens[0].compare("CONNECTED") == 0))
  {
     //CONNECTED()
    //  std::cout<<"made it to CONNECTED";
    std::string msgb;
    // msgb = "connected response hereeeeeeeeeee!";
    msgb = CONNECTED(PORT);
    send(clientSocket, msgb.c_str(), msgb.length(), 0);
  }

  else if((tokens[0].compare("CONNECT") == 0) && (tokens.size() == 2))
  {
     clients[clientSocket]->groupId = tokens[1];
  }
  else if(tokens[0].compare("LEAVE") == 0)
  {
      // Close the socket, and leave the socket handling
      // code to deal with tidying up clients etc. when
      // select() detects the OS has torn down the connection.
 
      closeClient(clientSocket, openSockets, maxfds);
  }
  else if(tokens[0].compare("WHO") == 0)
  {
     std::cout << "Who is logged on" << std::endl;
     std::string msg;

     for(auto const& names : clients)
     {
        msg += names.second->groupId + ",";

     }
     // Reducing the msg length by 1 loses the excess "," - which
     // granted is totally cheating.
     send(clientSocket, msg.c_str(), msg.length()-1, 0);

  }
  // This is slightly fragile, since it's relying on the order
  // of evaluation of the if statement.
  else if((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0))
  {
      std::string msg;
      for(auto i = tokens.begin()+2;i != tokens.end();i++) 
      {
          msg += *i + " ";
      }

      for(auto const& pair : clients)
      {
          send(pair.second->sock, msg.c_str(), msg.length(),0);
      }
  }
  else if(tokens[0].compare("MSG") == 0)
  {
      for(auto const& pair : clients)
      {
          if(pair.second->groupId.compare(tokens[1]) == 0)
          {
              std::string msg;
              for(auto i = tokens.begin()+2;i != tokens.end();i++) 
              {
                  msg += *i + " ";
              }
              send(pair.second->sock, msg.c_str(), msg.length(),0);
          }
      }
  }
  else
  {
      std::cout << "Unknown command from client:" << buffer << std::endl;
  }
     
}

int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    int listenSock1;                // Socket for the connection from the server to the local client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients
    std::string PORT;

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to
    PORT = argv[1];
    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

    listenSock1 = open_socket(PRESERVED_PORT);    

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }

    if(listen(listenSock1, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", PRESERVED_PORT);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        std::cout<< "this is the listenSock" << listenSock;
        FD_SET(listenSock, &openSockets);
        FD_SET(listenSock1, &openSockets);
        maxfds = listenSock;
        if(listenSock1 > listenSock){
            maxfds = listenSock1;
        }
    }

    finished = false;

    while(!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            // if(FD_ISSET(listenSock, &readSockets) || FD_ISSET(listenSock1
    // , &readSockets))
            if(FD_ISSET(listenSock, &readSockets))
            {
               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);
                std::cout << listenSock ;
               printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock);

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock);
               clients[clientSock]->isServer = true;
               std::cout << "not so special client connected is server: " << clients[clientSock]->isServer<<"\n";
               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
               std::string msg = "QUERYSERVERS,";
               msg += + GROUP_ID;
               send(clientSock, msg.c_str(), msg.length(), 0);
               
            }
            // THIS PART IS THE SPECIAL LOCAL CLIENT FOR US
            if(FD_ISSET(listenSock1, &readSockets))
            {
               clientSock = accept(listenSock1, (struct sockaddr *)&client,
                                   &clientLen);
                std::cout << listenSock1 ;
               printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock) ;

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock);
               clients[clientSock]->isServer = false;
               std::cout << "special client connected is server: " << clients[clientSock]->isServer<<"\n"; 

               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;  
            while(n-- > 0)
            {
               for(auto const& pair : clients)
               {
                  Client *client = pair.second;

                  if(FD_ISSET(client->sock, &readSockets))
                  {
                      // recv() == 0 means client has closed connection
                      if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                      {
                          disconnectedClients.push_back(client);
                          closeClient(client->sock, &openSockets, &maxfds);

                      }
                      // We don't check for -1 (nothing received) because select()
                      // only triggers if there is something on the socket for us.
                      else
                      {
                          std::cout << buffer << std::endl;
                          clientCommand(client->sock, &openSockets, &maxfds, buffer, PORT.c_str());
                      }
                  }
               }
               // Remove client from the clients list
               for(auto const& c : disconnectedClients)
                  clients.erase(c->sock);
            }
        }
    }
}
