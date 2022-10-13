#include <arpa/inet.h>
#include <pthread.h>
#include <bits/stdc++.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include "logger.h"
#define SIZE 6000
using namespace std;

// Global Data Structure
pthread_t servingThread[10];
int servingThreadIndex = 0;
pthread_t tid;
int trackerPort = 7000;
string trakerrIP = "127.1.2.1";
int port = 6000;
string ip = "127.1.1.1";

/*##############################################
split command taken as inp with space separate
 ##############################################*/
vector<string> getCommand(string inp)
{
    vector<string> ans;
    string temp = "";
    bool flag = false;
    for (size_t i = 0; i < inp.size(); i++)
    {
        if (inp[i] == '"' && !flag)
        {
            flag = true;
        }
        else if (inp[i] == '"' && flag)
        {
            ans.push_back(temp);
            i++;
            temp = "";
            flag = false;
        }
        else if (inp[i] == ' ' && flag)
        {
            temp += inp[i];
        }
        else if (inp[i] == ' ' && !flag)
        {
            ans.push_back(temp);
            temp = "";
        }
        else
        {
            temp += inp[i];
        }
    }
    if (temp.size() > 0)
    {
        ans.push_back(temp);
    }

    return ans;
}

bool sendFileToPeerClient(string fileName, int socketfd)
{

    printf("Send file called...\n");

    std::ifstream fp1(fileName);

    char buffer[SIZE] = {0};

    fp1.read(buffer, sizeof(buffer));

    // write(STDOUT_FILENO,buffer,sizeof(buffer));

    send(socketfd, buffer, sizeof(buffer), 0);

    fp1.close();

    return true;
}

bool recieveFileFromPeerServer(string fileName, int socketfd)
{

    ofstream myfile(fileName);

    int n;

    char buffer[SIZE] = {0};

    n = read(socketfd, buffer, SIZE);

    myfile.write(buffer, n);

    myfile.close();

    return true;
}

void *peerServerServing(void *arg)
{

    Logger::Info("Peer started servicing.");

    char buffer[1024] = {0};
    int fd = *(int *)arg;

    int valread = read(fd, buffer, 1024);
    printf("%s\n", buffer);

    Logger::Info("*** Recieved Msg ***");
    Logger::Info(buffer);

    string replyMsg = "This is reply Msg from Peer";
    char *serverreply = new char[replyMsg.length() + 1];
    strcpy(serverreply, replyMsg.c_str());

    send(fd, serverreply, strlen(serverreply), 0);
    Logger::Info("Reply Msg send to client");

    pthread_exit(NULL);
    return arg;
}

void *createServer(void *param)
{

    //    int server_port = *(int *)param; //in future assign ip and port taken via command line argument
    //    int serverPort = 7000;
    //    string serverIP = "127.1.1.1";

    cout<<"[WARN] create server executing...";

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;                    // assign type of ip address ipv4 here
    address.sin_port = htons(port);                  // assign port
    address.sin_addr.s_addr = inet_addr(ip.c_str()); // assign ip address

    // Forcefully attaching socket to the port 5000
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0)
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

        Logger::Info("******Connection accepted at peer side*******");

        if (pthread_create(&servingThread[servingThreadIndex++], NULL, peerServerServing, (void *)&new_socket) < 0)
        {
            perror("\ncould not create thread\n");
        }
    }

    // closing the connected socket
    close(new_socket);
    // closing the listening socket
    shutdown(server_fd, SHUT_RDWR);

    // pthread_exit(NULL);
}

/* int createUser(string uid, string password){

}

int loginUser(string uid, string password){

}

int createGroup(string groupID){

}

int joinGroup(string groupID){

}

int leaveGroup(string groupID){

}

int listRequests(string groupID){

}
 */

int sendMsg()
{
    Logger::Info("sendMsg start executing...");

    int client_fd, server_fd;
    struct sockaddr_in peer_serv_add;

    // creating the socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    peer_serv_add.sin_family = AF_INET;
    peer_serv_add.sin_port = htons(trackerPort);
    peer_serv_add.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, trakerrIP.c_str(), &peer_serv_add.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((server_fd = connect(client_fd, (struct sockaddr *)&peer_serv_add, sizeof(peer_serv_add))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    int valread;
    char buffer[1024] = {0};

    char str[1024] = "Hello msg from RJ";

    send(client_fd, str, sizeof(str), 0);

    bzero((char *)&buffer, sizeof(buffer));

    valread = read(client_fd, buffer, sizeof(buffer));
    printf("tracker response : => %s\n", buffer);

    close(server_fd);
}

int main()
{
    Logger::EnableFileOutput();

    Logger::Info("Client 1 start executing.");

    // make thread for server running paralelly
    pthread_t servingThread;

    pthread_create(&servingThread, NULL, createServer, NULL);
    // createServer(NULL);


        int i;
    while(1){
        cin>>i;
        if(i==1){
    sendMsg();
        }
        else{
            break;
        }
    }




    /*
        while (1){

            string cmdStr;
            cin>>cmdStr;

            vector<string> cmd;
            cmd = getCommand(cmdStr);

            if(cmd[0]=="create_user"){

                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'create_user'");
                    continue;
                }
                createUser(cmd[1], cmd[2]);

            }
            else if(cmd[0] == "login"){
                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'login'");
                    continue;
                }
                //loginUser(cmd[1], cmd[2]);
            }
            else if(cmd[0] == "create_group"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'create_group'");
                    continue;
                }
                createGroup(cmd[1]);
            }
            else if(cmd[0] == "join_group"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'join_group'");
                    continue;
                }
                joinGroup(cmd[1]);
            }
            else if(cmd[0] == "join_group"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'join_group'");
                    continue;
                }
                joinGroup(cmd[1]);
            }
            else if(cmd[0] == "leave_group"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'leave_group'");
                    continue;
                }
                leaveGroup(cmd[1]);
            }
            else if(cmd[0] == "list_requests"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'list_requests'");
                    continue;
                }
                listRequests(cmd[1]);
            }
            else if(cmd[0] == "accept_request"){
                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'accept_request'");
                    continue;
                }
                // acceptRequest(cmd[1]);
            }
            else if(cmd[0] == "list_groups"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'list_groups'");
                    continue;
                }
                // listGroups(cmd[1]);
            }
            else if(cmd[0] == "list_requests"){
                if(cmd.size()>1){
                    Logger::Error("Too many arguments in 'list_requests'");
                    continue;
                }
                listRequests(cmd[1]);
            }
            else if(cmd[0] == "list_files"){
                if(cmd.size()>2){
                    Logger::Error("Too many arguments in 'list_files'");
                    continue;
                }
                // listFiles(cmd[1]);
            }
            else if(cmd[0] == "upload_file"){
                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'upload_file'");
                    continue;
                }
                // uploadFile(cmd[1]);
            }
            else if(cmd[0] == "download_file"){
                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'download_file'");
                    continue;
                }
                // download_file(cmd[1]);
            }
            else if(cmd[0] == "logout"){
                if(cmd.size()>1){
                    Logger::Error("Too many arguments in 'logout'");
                    continue;
                }
                // logout(cmd[1]);
            }
            else if(cmd[0] == "show_downloads"){
                if(cmd.size()>1){
                    Logger::Error("Too many arguments in 'show_downloads'");
                    continue;
                }
                // show_downloads(cmd[1]);
            }
            else if(cmd[0] == "stop_share"){
                if(cmd.size()>3){
                    Logger::Error("Too many arguments in 'stop_share'");
                    continue;
                }
                // stop_share(cmd[1]);
            }
            else{
                Logger::Error("Too many arguments in 'stop_share'");
            }

        }
     */
    pthread_join(servingThread, NULL);
    return 0;
}
