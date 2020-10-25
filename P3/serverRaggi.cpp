//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
// For reasons beyond our understanding, this server works. The original doesn't. We have yet to know why.  Please discuss.
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
#define GROUP_ID "P3_GROUP_85"
#define KEEP_ALIVE_TIMEOUT 5
#define SELECT_TIMEOUT 5
#define BACKLOG  5        // Allowed length of queue of waiting connections

//simple node class with data as type std::string
struct Node
{
    std::string data;
    struct Node *next;
};

//Implementation of linked list
//last in last out
class linked_list{
    private:
        Node *head,*tail;
        int size;
    public:
        linked_list(){
            head = NULL;
            tail = NULL;
            size = 0;
            
        }

        void push(std::string message){
            Node *tmp = new Node;
            tmp->data = message;

            if(head == NULL){
                head = tmp;
                head->next = NULL;
            }
            else{
                tmp->next = head;
                head = tmp;
            }
            size +=1;
            return;
        }

        std::string pop(){
            if(head != NULL){
                std::cout<< "head not null" <<std::endl;
                std::string retValue = head->data;
                head = head->next;
                size -=1;
                return retValue;   
            }
            return "NULL";
        }
        int getSize(){
            return size;
        }
};

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
  public:
    int sock;                      // socket of client connection
    std::string groupId;           // Limit length of name of client's user
    std::string port;
    std::string ip;
    linked_list messages;
    time_t alive;
    bool isServer;
    bool remove = 0;
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

struct commandStruct
{
    int socket;
    bool removed;
    Client* client;
};

//lists the servers connected to the server
std::string listServers(){
    std::string retString;
    for(auto const& isServer : clients){  
        if(isServer.second->isServer == 1 ){ //only add data if client is of a external server, not a local client
            retString += isServer.second->groupId;
            retString += ",";
            retString += isServer.second->ip;
            retString += ",";
            retString += isServer.second->port;
            retString += ";";
        }
    }
    return retString;
}

//keep alive message handler for clients
void keepAlive(Client client){
    std::string msg = "keep alive: ";
    msg += std::to_string(client.messages.getSize());
    msg += " messages are waiting";
    //push and pop work
    //   client->messages.push("message1");
    //   client->messages.push("message2");
    //   client->messages.pop();
    send(client.sock, msg.c_str(), msg.length(), 0);
}

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

//get connected servers
std::string connected(std::string PORT){
    std::string retString = "";
    retString = "*CONNNECTED,";
    retString += GROUP_ID;
    retString += ",";
    retString += "46.182.189.139,"; //change to skel
    retString +=  PORT;
    retString += ";";
    std::string extra = listServers(); //add list of servers connected to the server
    retString += extra;
    retString += "#";
    return retString;
}

void addInfoToClient(int sock, std::string id, std::string ip, std::string port){
    for(auto const& server : clients)
     {  
      
        if(server.second->isServer == 1 && server.second->sock == sock){ //only add data if client is of a external server, not a local client
            server.second->groupId = id;
            server.second->ip = ip;
            server.second->port = port;
            time_t alive;
            // double seconds;
            time(&alive);
            server.second->alive = alive; 
            // std::cout << server.second->groupId<< std::endl;
        }
     }
}

// If first item in list starts with *: continue
// If last item in list ends with #: return true
// If either one fails: Return false
bool commandValidation(std::vector<std::string>& wordList){
    std::string firstWord = wordList[0];
    if(firstWord.length() == 0){
        return false;
    }
    if(firstWord[0] == '*'){
        std::string lastWord = wordList[wordList.size()-1];
        if(lastWord.length() <= 2){
            return false;
        }
        char lastLetter = lastWord[lastWord.size()-2];
        if(lastLetter == '#'){
            // std::cout << "I'll accept your offering, mortal." << std::endl;
            std::string newFirstWord = firstWord.erase(0,1);
            std::cout << "Modified first word: " << newFirstWord << std::endl;
            std::string newLastWord = lastWord.substr(0, lastWord.size()-2);
            std::cout << "Modified last word: " << newLastWord << std::endl;
            // Now we replace these words
            wordList[0] = newFirstWord;
            wordList[wordList.size()-1] = newLastWord;
            return true;
        }
        else{
            // std::cout << "This is a Christian server, no non-# strings allowed." << std::endl;
            return false;
        }
    }
    else
    {
        // std::cout << "What the fuck did you just send me, get that shit outta here." << std::endl;
        return false;
    }
    
}

Client* removeInfoFromClient(std::string ip, std::string port) {
    for(auto const& IsServer: clients)
    {
        Client *client = IsServer.second;
        if(client->isServer == 1 && client->ip == ip && client->port == port){
            client->remove = 1;
            
            return client;
        }
    }
    return NULL;
}

void createNewConnection(fd_set &openSockets, int& maxfds, std::string groupId, std::string ip, std::string port){
    std::stringstream intPort(port); 
    int portToInt = 0; 
    intPort >> portToInt;
    struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   int nwrite;                               // No. bytes written to server
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt
   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

   if(getaddrinfo("127.0.0.1", "4002", &hints, &svr) != 0)
   {
       perror("getaddrinfo failed: ");
       exit(0);
   }
    struct hostent *server;
    server = gethostbyname("127.0.0.1");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = 4002;
        
    int clientSock = open_socket(4002);
    // serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(connect(clientSock, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0){
       // EINPROGRESS means that the connection is still being setup. Typically this
       // only occurs with non-blocking sockets. (The serverSocket above is explicitly
       // not in non-blocking mode, so this check here is just an example of how to
       // handle this properly.)
       if(errno != EINPROGRESS){
         printf("Failed to open socket to server: %s\n", ip);
         perror("Connect failed: ");
         exit(0);
       }
    }
}

// Process command from client on the server
// Returns a type commandstruct
commandStruct clientCommand(int clientSocket, fd_set &openSockets, int &maxfds, 
                  char *buffer, std::string PORT, bool isServer) {
    std::vector<std::string> tokens;
    std::vector<std::string> tokensNoCommas;
    std::string token;
    std::string tokenNoComma;
    commandStruct retStruct;
    retStruct.removed = 0;
    retStruct.socket = 0;
    retStruct.client = NULL;
    std::stringstream stream(buffer);
    std::stringstream stream1(buffer);
    

    while(std::getline(stream, token, ',')){
        std::cout <<"this is the token: "<< token << std::endl;
        std::cout <<"this is the tokensize: "<< token.size() << std::endl;
        tokens.push_back(token);
    }

    while(stream1 >> tokenNoComma)
        tokensNoCommas.push_back(tokenNoComma);

    bool isValid = commandValidation(tokens);
    // std::cout << isValid << std::endl;
    if(!isValid){
        // std::cout << "OI I'M VERY STRICT ON WHAT I GET! * IN FRONT AND # IN BACK!" << buffer << std::endl;
        std::string error_msg = "Due to our subpar C++ programming skills, our server is very strict in what it accepts. Please put * in front and # in back of your command.";
        send(clientSocket, error_msg.c_str(), error_msg.length(), 0);
    }


    //LISTSERVERS
    if((tokensNoCommas[0].compare("LISTSERVERS") == 0))
    {   
        std::cout << "made it" << std::endl;
        std::string msg = listServers();
        std::cout<< "listservers msg: " << msg <<std::endl;
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
       
    //Quearyservers, <from_group_id>
    else if((tokens[0].compare("QUERYSERVERS") == 0) && (tokens.size() == 2))
    {
        std::string msg = connected(PORT);
        std::cout<< msg;
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    //CONNECTED
    else if((tokens[0].compare("CONNECTED") == 0))
    {
        std::string id = tokens[1];
        std::string ip = tokens[2];
        std::string port = tokens[3];
        addInfoToClient(clientSocket ,id, ip, port);
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
        std::string ip = tokens[1];
        std::string port = tokens[2];
        Client* removedSock = removeInfoFromClient(ip, port);
        if(removedSock == NULL) {
            std::cout << "This IP address/port combination was not found. Please try again." << std::endl;
        }
        else{
            retStruct.removed = 1;
            retStruct.client = removedSock;
        }
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
    // GET_MSG,<GROUP_ID>
    // returns 1 message if 1 or more messages are waiting
    else if((tokens[0].compare("GET_MSG") == 0) && tokens.size() == 2)
    {
        std::string msg;
        bool found = 0;
        for(auto const& pair : clients)
        {
            Client *client = pair.second;
            if(client->groupId == tokens[1]){  
                found = 1;
                if(client->messages.getSize() > 0){
                    msg = client->messages.pop();
                    send(clientSocket, msg.c_str(), msg.length(), 0);
                }
                else{
                    send(clientSocket, "No messages for you. No, we don't feel bad for you.", sizeof("No messages for you. No, we don't feel bad for you."), 0);
                }
            }
        }

        if(found == 0){
            send(clientSocket, "GroupID not found", sizeof("GroupID not found"), 0);
        }
    }

    //add msg to a list of messages waiting for a certain client
    //if the client is not connected to our server -> save message in datastructure and keep it there until the client connects to our server
    else if(tokens[0].compare("SEND_MSG") == 0 && tokens.size() >=4)
    {
        bool found = 0;
        std::string sent_msg = "";
        sent_msg += "Message from: ";
        sent_msg += tokens[2];
        sent_msg += " , Message Contents: ";
        sent_msg += tokens[3];
        for(auto const& pair : clients)
        {
            Client *client = pair.second;
            if(client->groupId == tokens[1]){
                found = 1;
                client->messages.push(sent_msg);
                std::string msg = "Message successfully sent. Hopefully. We don't really know for sure.";
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
        }

        if(found == 0){
            send(clientSocket, "GroupID not found", sizeof("GroupID not found"), 0);
        }
    }

    //connect to other server
    else if(tokens[0].compare("CONNECTTO") == 0 && tokens.size() >=4)
    {   
        std::string groupId;
        std::string ip; 
        std::string port;
        groupId = tokens[1]; 
        ip = tokens[2];
        port = tokens[3];
        std::cout <<"connectto: "<< groupId << ip<< port<< std::endl;
        createNewConnection(openSockets, maxfds, groupId, ip, port);
    }
    else
    {
        std::cout << "Unknown command from client:" << buffer << std::endl;
        if(isValid){
            std::string msg = "Command not recognized. Our tiny cutesy little server only understands the base commands in the assignment, so please be patient with it. Harassment will only scare the server.";
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }
    }
   return retStruct;  
}

int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    int localClientSock;            // Socket for the connection from the server to the local client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients
    std::string PORT;               // Port of the server

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to
    PORT = argv[1];
    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

    // setup our special local client, with a hidden port
    localClientSock = open_socket(PRESERVED_PORT);    

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }

    if(listen(localClientSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %d\n", PRESERVED_PORT);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        FD_SET(localClientSock, &openSockets);
        maxfds = listenSock;
        if(localClientSock > listenSock){
            maxfds = localClientSock;
        }
    }

    finished = false;
    while(!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));
        // struct timeval timeout = {KEEP_ALIVE_TIMEOUT, 0}; //this is a set timeout, check file descriptors after x, seconds
        for(auto const& pair : clients){
            Client *client = pair.second;
            time_t now;
            time(&now);
            double diff = difftime(client->alive, now);
            // std::cout <<"NOW: " <<now << ", " <<"BEFORE: "<< client->alive << std::endl;
            // std::cout <<diff<< std::endl;
            if(client->alive > 0){
                // std::cout << "alive >0" << std::endl;
                if(diff >= KEEP_ALIVE_TIMEOUT){
                    Client currClient = *client;
                    keepAlive(currClient);
                }
            }
        }
        
        struct timeval timeout = {SELECT_TIMEOUT, 0};
        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, &timeout);
        if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if(FD_ISSET(listenSock, &readSockets))
            {
               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);
               // std::cout << listenSock;
               // printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock);

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock);
               clients[clientSock]->isServer = true;
               // std::cout << "not so special client connected to server: " << clients[clientSock]->isServer<<"\n";
               std::string msg = "*QUERYSERVERS,";
               msg += GROUP_ID;
               msg += "#";
               send(clientSock, msg.c_str(), msg.length(), 0);
               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Server connected: %d\n", clientSock);
            }

            // THIS PART IS FOR THE SPECIAL LOCAL CLIENT
            if(FD_ISSET(localClientSock, &readSockets))
            {
               clientSock = accept(localClientSock, (struct sockaddr *)&client,
                                   &clientLen);
                std::cout << localClientSock ;
               printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock) ;

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock);
               clients[clientSock]->isServer = false;
               // std::cout << "special client connected is server: " << clients[clientSock]->isServer<<"\n";

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
                          commandStruct retValue = clientCommand(client->sock, openSockets, maxfds, buffer, PORT.c_str(), client->isServer);
                        // removedManually = clientCommand(client->sock, &openSockets, &maxfds, buffer, PORT.c_str(), client->isServer);
                          if(retValue.removed == 1){
                            disconnectedClients.push_back(retValue.client);
                            closeClient(retValue.client->sock, &openSockets, &maxfds);
                          }
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
