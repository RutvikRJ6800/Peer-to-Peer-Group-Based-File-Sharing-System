#include <arpa/inet.h>
#include <pthread.h>
#include<bits/stdc++.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include<fstream>
#include "logger.h"
#define SIZE 6000
using namespace std;

// global variables
pthread_t servingThread[10];
int servingThreadIndex=0;
int port = 7000;
string ip = "127.1.2.1";

class UserInfo{
    public:
    string password, ip, port;
    
    UserInfo(string pswd, string ipAddr, string portNo){
        password = pswd;
        ip = ipAddr;
        port = portNo;
    }
};
// uid , UserInfo Object
unordered_map<string, UserInfo> users; 

class GroupInfo{
    public:
    string owner;
    vector<string> members;
    vector<string> files;

    GroupInfo(string ownerName, vector<string> membersList, vector<string> filesList){
        owner = ownerName;
        members = membersList;
        files = filesList;
    }
};
// gid , GroupInfo Object
unordered_map<string, GroupInfo> groups;

class FileInfo{
    public:
    long long fileSize;
    string sha1;
    vector<string> senders;

    FileInfo(long long file_size, string sha, vector<string> sendersList){
        fileSize = file_size;
        sha1 = sha;
        senders = sendersList;
    }
};
// gid$filename , FileInfoObject
unordered_map<string, FileInfo> files;

// gid , uid1,uid2...
unordered_map<string, vector<string>> pendingRequest;



// **********************************************
// functions for executing tracker functionality
// **********************************************


void * serverserving(void * arg){
    
    Logger::Info("Server started servicing.");

    char buffer[1024] = {0};
    int fd = *(int *)arg;

    int valread = read(fd, buffer, 1024);
	printf("%s\n", buffer);

    Logger::Info("*** Recieved Msg ***");
    Logger::Info(buffer);


    
    string replyMsg = "This is reply Msg from Tracker";
    char *serverreply = new char[replyMsg.length() + 1];
    strcpy(serverreply, replyMsg.c_str());
    

    send(fd, serverreply, strlen(serverreply), 0);
	Logger::Info("Reply Msg send to client");
    
	pthread_exit(NULL);
}



void * listening(void* arg){

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Error in accept connection");
            exit(EXIT_FAILURE);
        }

        Logger::Info("******Connection accepted at tracker side*******");

        if (pthread_create(&servingThread[servingThreadIndex++], NULL, serverserving, (void *)&new_socket) < 0)
        {
            perror("\ncould not create thread\n");
        }

    }
    
}

int main(){
    // port = 7000; set port here from the cmd args
    // ip = "127.1.1.1";
    // string arg1 = "127.1.1.1:7000";

    Logger::EnableFileOutput();
    Logger::Info("Tracker Started Servicing ");

    pthread_t servingThread;

    pthread_create(&servingThread, NULL, listening, NULL);

    while(1){
        string inp;
        cin>>inp;

        if(inp=="quit"){
            // copy in memory data structure into local files
            break;
        }
    }

    pthread_join(servingThread,NULL);


    return 0;
}

