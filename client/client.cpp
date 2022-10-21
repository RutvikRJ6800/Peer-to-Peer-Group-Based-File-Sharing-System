#include <arpa/inet.h>
#include <errno.h> 
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
#include <sys/types.h>
#include <sys/stat.h>
#include <filesystem>
#include <fcntl.h>
#include <thread>
#define CHUNKSIZE 524288
using namespace std;
using std::ifstream;

// Global Data Structure
pthread_t servingThread[100];
int servingThreadIndex = 0;
pthread_t tid;
int trackerPort = 7000;
string trakerrIP = "127.1.2.1";

string uname = "";
bool isLoggedIn = false;
int port = 6005;
string ip = "127.1.1.1";
int client_fd;
vector<string> currDownloadFilechunkMaps;
map<int, pair<string, long long>> numToIP;
size_t numToIPIndex;

class FileInfo{
    public:
    string filePath, fileName;
    long long fileSize;
    vector<bool> chunkDetails;

    FileInfo(){

    }

    FileInfo(string fileP, string fileN, long long fileS){
        filePath = fileP;
        fileName = fileN;
        fileSize = fileS;

    }

    FileInfo(string fileP, string fileN, long long fileS, vector<bool> chunkDtl){
        filePath = fileP;
        fileName = fileN;
        fileSize = fileS;
        chunkDetails = chunkDtl;
    }
};
// gid$filename , FileInfoObject
unordered_map<string, FileInfo> uploadedFiles;

typedef struct chunkDetails{
    string serverPeerIP;
    string filename;
    long long chunkNum; 
    string destination;
} chunkDetails;


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

vector<string> splitString(string inp, char delim)
{
    vector<string> ans;
    string temp = "";
    for (size_t i = 0; i < inp.size(); i++)
    {
        if (inp[i] == delim)
        {	
			if(temp.size()>0)
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

void printVector(vector<string> res){
    for(size_t i=0; i<res.size(); i++){
        cout<<res[i]<<endl;
    }
}

string getChunkDetails(vector<string> cmd){
    // cmd = get_chunk_details gid fileName
    string gid = cmd[1];
    string fileName = cmd[2];
    
    // check if asked file is preset or not
    if(uploadedFiles.size()>0 && uploadedFiles.find(gid+'$'+fileName) != uploadedFiles.end()){
        // file present at this peer;
        FileInfo f1 = uploadedFiles[gid+'$'+fileName];
        vector<bool> v = f1.chunkDetails;

        string chunkMap = "";
        for(size_t i = 0; i<v.size(); i++){
            if(v[i] == 0)
                chunkMap += '0';
            else
                chunkMap += '1';
        }

        return chunkMap;

        
    }
    else{
        string res = "";
        return res;
    }
}

int sendChunk(vector<string> cmd, int new_socket){
    // cmd == get_chunk gid fileName chunkNumber.
    string gid = cmd[1];
    string fileName = cmd[2];
    long long chunkNum = stoll(cmd[3]);
    
    // check if asked file is preset or not
    if(uploadedFiles.size()>0 && uploadedFiles.find(gid+'$'+fileName) != uploadedFiles.end()){
        // file present at this peer;
        FileInfo f1 = uploadedFiles[gid+'$'+fileName];
        vector<bool> v = f1.chunkDetails;

        if(v.size()<=chunkNum){
            // invalid chunk number;
            // send error msg
            string s = "Error301"; // Invalid chunk Number
            write(new_socket, s.c_str(), s.size());
            return -1;

        }
        else if(v[chunkNum] ==  0){
            // this chunk number is not avalilable
            string s = "Error302"; // Data for this chunk is not avilable.
            write(new_socket, s.c_str(), s.size());
            return -1;
        }
        else{
            // get chunk from the file and send it to user
            string filePath = f1.filePath;

            // int fd = open(filePath.c_str(), O_RDONLY);  
            // char buffer[MaxBufferLength];

            // #########################################################
            std::ifstream fp1(filePath, std::ios::in|std::ios::binary);
            fp1.seekg(chunkNum*CHUNKSIZE, fp1.beg);

            Logger::Info("sending data starting at " + to_string(fp1.tellg()));
            char buffer[CHUNKSIZE] = {0}; 
            int rc = 0;

            fp1.read(buffer, sizeof(buffer));
            int count = fp1.gcount();

            Logger::Info("Data read count: "+to_string(count));
            Logger::Info("Data is: "+string(buffer));

            if ((rc = send(new_socket, buffer, count, 0)) == -1) {
                // write(new_socket, chunkMap.c_str(), chunkMap.size()); here send is used instead of write.
                perror("[-]Error in sending file.");
                exit(1);
            }
            
            Logger::Info("sent till "+to_string(fp1.tellg()));


            fp1.close();
            return 0;

        }

        
    }
    else{
        // no such file present at the peer side.
        string s = "Error303"; // No such file present.
        write(new_socket, s.c_str(), s.size());
        return -1;    


    }
}

void *peerServerServing(void *arg)
{
    int new_socket = *(int *)arg;

    // while(1){

        string msg = "Peer started servicing."+to_string(new_socket);

        Logger::Info(msg.c_str());



        char INPbuffer[524288] = {0};

        int valread = read(new_socket, INPbuffer, 1024);
        printf("%s\n", INPbuffer);

        Logger::Info("################Recieved Msg From PEER################");
        Logger::Info(INPbuffer);

        // check if other peer is asking for chunk or chunk map(chunkDetails of file).

        // it asks for chunkDetails
        vector<string> cmd = splitString(INPbuffer, ' ');

        for(size_t r=0; r<cmd.size(); r++){
            cout<<"idx: "<<r<<"= "<<cmd[r]<<endl;
        }

        if(cmd[0] == "get_chunk_details"){
            // function for sending chunk details (other peer asked).
            string chunkMap = getChunkDetails(cmd);

            write(new_socket, chunkMap.c_str(), chunkMap.size());
            Logger::Info("chunk vector of file: "+cmd[2]+" is: "+chunkMap);

        }

        else if(cmd[0] == "get_chunk"){


            // thread *th = new thread(sendChunk, cmd, new_socket); 
            // thread removed temporaryly
            sendChunk(cmd, new_socket);
        }

        else{
            string s = "Erorr201";
            write(new_socket, s.c_str(), s.size()); //Invalid command reached at Peer server
        }



        pthread_exit(NULL);
        return arg;

        // check other peer asking file is present or not

        // if(uploadedFiles.size()>0 && uploadedFiles.find(string(INPbuffer)) != uploadedFiles.end()){
        //     FileInfo f1 = uploadedFiles[string(INPbuffer)];
        //     string filePath = f1.fileName;

        //     int MaxBufferLength = 512;

        //     int fd = open(uploadedFiles[INPbuffer].filePath.c_str(), O_RDONLY);  
        //     char buffer[MaxBufferLength];

        //     while (1) {
        //         // Read data into buffer.  We may not have enough to fill up buffer, so we
        //         // store how many bytes were actually read in bytes_read.
        //         int bytes_read = read(fd, buffer, sizeof(buffer));
        //         if (bytes_read <= 0){ // We're done reading from the file
        //             cout<<"Nothing read."<<endl;
        //             break;
        //         }
        //         else if (bytes_read < 0) {
        //             cout<<"Error in read."<<endl;
        //             break;
        //             // handle errors
        //         }
        //         else{
        //             write(new_socket, buffer, bytes_read);
        //         }

        //     }
        // }
        // else{
        //     Logger::Warn("No such File Present at peer");    
        // }
        
        // Logger::Info("Reply Msg send to client");


    // }

    
}

void *createServer(void *param)
{

    //    int server_port = *(int *)param; //in future assign ip and port taken via command line argument
    //    int serverPort = 7000;
    //    string serverIP = "127.1.1.1";

    cout<<"[WARN] create server executing..."<<endl;

    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address), opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

    address.sin_family = AF_INET;                    // assign type of ip address ipv4 here
    address.sin_port = htons(port);                  // assign port
    address.sin_addr.s_addr = inet_addr(ip.c_str()); // assign ip address

    // Forcefully attaching socket to the port 5000
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int new_socket;
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
    //close(new_socket);
    // closing the listening socket
    // shutdown(server_fd, SHUT_RDWR);

    // pthread_exit(NULL);
}


int sendCommandToTracker(string cmd){
    //send command to tracker
    Logger::Info("sending command to Tracker");

    char *cmdBuffer = new char[cmd.length() + 1];
    strcpy(cmdBuffer, cmd.c_str());
    if(send(client_fd, cmdBuffer, strlen(cmdBuffer), 0) < 0){
        Logger::Error("Could not send the command.");
        return -1;
    }
    else{
        Logger::Info("successfully sent command to Tracker");
        return 0;
    } 

}


int receiveReplyFromTracker(){

    char responseBuffer[1024] = {0};

    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

        Logger::Error("Couldn't read the response of server / got EOF.");
        Logger::Info("************************************");

        return 0;

    }
    else{

        printf("Tracker Response : => %s\n",responseBuffer);
        Logger::Info(responseBuffer);
        Logger::Info("************************************");
        bzero((char *)&responseBuffer, sizeof(responseBuffer));

    }
    
    return -1;
}


void uploadFile(vector<string> cmd){
    // calculate hash
    // append it at end to send at tracker

    //check file present at given path;
    // calculate it's file size

    // send strig to tracker (cmd fileName gid fileSize)
    string FilePath = cmd[1]; // also contain File name in it.
    string groupId = cmd[2];
    
    int idx=FilePath.find_last_of('/');
    // string ss= FilePath.substr(0,idx);
    // if(ss=="")ss="/";
    string fileName=FilePath.substr(idx+1,FilePath.size()-(idx+1));
    long long fileSize = std::filesystem::file_size(FilePath);

    cout<<"filepath: "<<FilePath<<" fileSize: "<<fileSize<<endl;

    string msg = "";
    msg += "upload_file "+fileName+" "+groupId+" "+uname+" "+to_string(fileSize);

    sendCommandToTracker(msg);

    char responseBuffer[1024] = {0};

    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

        Logger::Error("Couldn't read the response of server / got EOF.");
        Logger::Info("************************************");
    }
    else if(string(responseBuffer) == "File Successfully Uploaded."){

        printf("Tracker Response : => %s\n",responseBuffer);
        Logger::Info(responseBuffer);
        Logger::Info("************************************");

        long long noOfChunks = fileSize / CHUNKSIZE;
        if(fileSize%CHUNKSIZE != 0)noOfChunks++; // new chunk for rest some data;

        vector<bool> chunkDtl(noOfChunks, 1); 

        FileInfo f1 = FileInfo(FilePath, fileName, fileSize, chunkDtl);   // store file info into uploaded file map.
        uploadedFiles[groupId+"$"+fileName] = f1;

        bzero((char *)&responseBuffer, sizeof(responseBuffer));

    }
    

}

int fetchChunkInfoFromPeer(string peerIp, int peerPort, string groupId , string fileName){
    
    Logger::Info("fetching chunk details from peer: "+peerIp);

    int peer_sd;  
    struct sockaddr_in peer_serv_add;

    // creating the socket
    if ((peer_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    peer_serv_add.sin_family = AF_INET;
    peer_serv_add.sin_port = htons(peerPort);
    peer_serv_add.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, peerIp.c_str(), &peer_serv_add.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
    }

    if (connect(peer_sd, (struct sockaddr *)&peer_serv_add, sizeof(peer_serv_add)) < 0)
    {
        printf("\nConnection Failed \n");
        Logger::Error("Failed to establish connection with peer: "+peerIp);     
    }

    Logger::Info("connction established with peer: "+peerIp);

    string command = "get_chunk_details "+groupId+" "+fileName;

    send(peer_sd, command.c_str(), command.size(), 0);
    
    Logger::Info("command: "+command+". sent to peer: "+peerIp);

    char responseBuffer[524288] = {0};

    read(peer_sd, responseBuffer, sizeof(responseBuffer));

    Logger::Info("Received chunk details as: "+string(responseBuffer));

    currDownloadFilechunkMaps.push_back(string(responseBuffer));
    numToIP[numToIPIndex++] = make_pair(peerIp, peerPort);
    

    close(peer_sd); // close peer connection.

    cout<<"chunk map at this time:"<<endl;

    for(size_t i=0; i<currDownloadFilechunkMaps.size(); i++){
        cout<<currDownloadFilechunkMaps[i]<<endl;
    }


    return 0;
    
}

int fetchChunkFromPeer(string peerIp, int peerPort, string groupId, string fileName, int chunkNum){
    
    Logger::Info("fetching chunk num "+to_string(chunkNum)+" from peer: "+peerIp);

    int peer_sd;  
    struct sockaddr_in peer_serv_add;

    // creating the socket
    if ((peer_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    peer_serv_add.sin_family = AF_INET;
    peer_serv_add.sin_port = htons(peerPort);
    peer_serv_add.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, peerIp.c_str(), &peer_serv_add.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
    }

    if (connect(peer_sd, (struct sockaddr *)&peer_serv_add, sizeof(peer_serv_add)) < 0)
    {
        printf("\nConnection Failed \n");
        Logger::Error("Failed to establish connection with peer: "+peerIp);     
    }

    Logger::Info("connction established with peer: "+peerIp);

    string command = "get_chunk "+groupId+" "+fileName+" "+to_string(chunkNum);

    send(peer_sd, command.c_str(), command.size(), 0);
    
    Logger::Info("command: "+command+". sent to peer: "+peerIp);

    char responseBuffer[524288] = {0};

    // read(peer_sd, responseBuffer, sizeof(responseBuffer));

    if(string(responseBuffer) == "Error301"){
        Logger::Info("Invalid chunk Number");
        cout<<"Invalid chunk Number"<<endl;
        return -1;
    }
    else if(string(responseBuffer) == "Error302"){
        Logger::Info("Data for this chunk is not avilable.");
        cout<<"Data for this chunk is not avilable."<<endl;
        return -1;
    }
    else if(string(responseBuffer) == "Error303"){
        Logger::Info("No such file present.");
        cout<<"No such file present."<<endl;
        return -1;
    }
    else{

        // Logger::Info("Received chunk: "+string(responseBuffer));
        

        // close(peer_sd); // close peer connection.

        cout<<"chunk map at this time:"<<endl;

        for(size_t i=0; i<currDownloadFilechunkMaps.size(); i++){
            cout<<currDownloadFilechunkMaps[i]<<endl;
        }

        int n, tot = 0;
        char buffer[CHUNKSIZE];

        string content = "";


        while (tot < CHUNKSIZE) {
            n = read(peer_sd, buffer, CHUNKSIZE-1);
            if (n <= 0){
                break;
            }
            buffer[n] = 0;
            fstream outfile(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
            Logger::Info("Received chunk: "+string(buffer));
            outfile.seekp(chunkNum*CHUNKSIZE+tot, ios::beg);
            outfile.write(buffer, n);
            outfile.close();

            Logger::Info("written at: "+ to_string(chunkNum*CHUNKSIZE + tot));
            Logger::Info("written till: " + to_string(chunkNum*CHUNKSIZE + tot + n-1) +"\n");

            content += buffer;
            tot += n;
            bzero(buffer, CHUNKSIZE);
        }

    }

    close(peer_sd); // close peer connection.

    return 0;
    
}

int downloadFile(vector<string> cmd){
    
    string groupId = cmd[1];
    string fileName = cmd[2];

    string msg = "download_file";
    msg += " "+groupId+" "+fileName+" "+uname;

    sendCommandToTracker(msg);

    char responseBuffer[524288] = {0};

    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

        Logger::Error("Couldn't read the response of server / got EOF.");
        Logger::Info("************************************");
    }
    else if(string(responseBuffer) == "Error101"){

        printf("Tracker Response : => %s\n","Unable to get details about group.");
        Logger::Info("Unable to get details about group.");
        Logger::Info("************************************");
        return -1;

        // FileInfo f1 = FileInfo(FilePath, fileName, fileSize);   // store file info into uploaded file map.
        // uploadedFiles[groupId+"$"+fileName] = f1;

        // bzero((char *)&responseBuffer, sizeof(responseBuffer));

    }    
    else if(string(responseBuffer) == "Error102"){
        printf("Tracker Response : => %s\n","You are not member of the group");
        Logger::Info("You are not member of the group");
        Logger::Info("************************************");
        return -1;
    }
    else if(string(responseBuffer) == "Error103"){
        printf("Tracker Response : => %s\n","File not present in the group");
        Logger::Info("File not present in the group");
        Logger::Info("************************************");
        return -1;
    }
    else{
        // we found the file details
        string fileDtls(responseBuffer);
        printf("Tracker Response : => %s\n",responseBuffer);
        Logger::Info(responseBuffer);

        vector<string> vec = splitString(fileDtls, '$');
        if(vec.size()<=2){ // file present in group but no seeders or leechers available.
            cout<<"No seeders or leercher found. Hence download incomplete."<<endl;
            Logger::Error("No seeders or leercher found.");
            Logger::Error("Download Incomplete");
            return -1;
        }


        long long fileSize = stoll(vec[0]);
        vector<pair<string,int>> ips;

        for(size_t i=1; i<vec.size(); i++){

            string sip = vec[i++];
            int pt = stoi(vec[i]);
            ips.push_back(make_pair(sip, pt));

        }

        // print ip and port
        cout<<"IP and port of Seeders or leechers"<<endl;
        for(size_t i=0; i<ips.size(); i++){

           cout<<"IP: "<<ips[i].first<<" Port: "<<ips[i].second<<endl;

        }

        // ###########################################################################
        vector<thread> getChunkMapthread;

        currDownloadFilechunkMaps.clear();
        numToIP.clear();
        numToIPIndex = 0;

        for(size_t i =0; i<ips.size(); i++){
            getChunkMapthread.push_back(thread(fetchChunkInfoFromPeer, ips[i].first, ips[i].second, groupId , fileName));
        }

        for (std::thread & th : getChunkMapthread){
            // If thread Object is Joinable then Join that thread.
            if (th.joinable()) th.join();
        }

        // print chunkMap;

        cout<<"chunk received... chunks are."<<endl;
        for(size_t i=0; i<currDownloadFilechunkMaps.size(); i++){
            cout<<currDownloadFilechunkMaps[i]<<endl;
        }

        vector<vector<int>> chunkToPeersList(currDownloadFilechunkMaps[0].size());


        for(size_t i=0; i<currDownloadFilechunkMaps[0].size(); i++){

            for(size_t j=0; j<currDownloadFilechunkMaps.size(); j++){
                if(currDownloadFilechunkMaps[j][i] == '1'){
                    chunkToPeersList[i].push_back(j);
                }
            }

        }

        // print chunkToPeer file

        for(size_t i=0; i<chunkToPeersList.size(); i++){
            cout<<"chunk No: ";
            for(size_t j=0; j<chunkToPeersList[i].size(); j++){
                cout<<chunkToPeersList[i][j]<<", ";
            }
            cout<<endl;
        }

        // // get chunk from peer and add it in file

        FILE* fp = fopen(fileName.c_str(), "r+");
        if(fp != 0){
            printf("The file already exists.\n") ;
            fclose(fp);
            return -1;
        }

        vector<thread> getChunkthread;

        for(size_t i=0; i<currDownloadFilechunkMaps[0].size(); i++){

            int peerNo = rand()%chunkToPeersList[i].size();

            // getChunkthread.push_back(thread(fetchChunkFromPeer, numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i));
            fetchChunkFromPeer(numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i);
        }



        Logger::Info("File downloaded successfully.");
        // check if the file is corrupted or not

        // for (std::thread & th : getChunkthread){
        //     // If thread Object is Joinable then Join that thread.
        //     if (th.joinable()) th.join();
        // }


        return 0;
        

        




        // ###########################################################################








        /* string sendMsgToPeer = groupId+'$'+fileName;
        
        Logger::Warn("File which we want to download is sended to peer");
        Logger::Info(sendMsgToPeer.c_str());

        int peer_sock;
        struct sockaddr_in peer_serv_add;

        // creating the socket
        if ((peer_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        peer_serv_add.sin_family = AF_INET;
        peer_serv_add.sin_port = htons(ips[0].second);
        peer_serv_add.sin_addr.s_addr = INADDR_ANY;

        if (inet_pton(AF_INET, ips[0].first.c_str(), &peer_serv_add.sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        if (connect(peer_sock, (struct sockaddr *)&peer_serv_add, sizeof(peer_serv_add)) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }

        int valread;
        // char buffer[1024] = {0};

        send(peer_sock, sendMsgToPeer.c_str(), sizeof(sendMsgToPeer), 0);

        // bzero((char *)&buffer, sizeof(buffer));

        // valread = read(peer_sock, buffer, sizeof(buffer));
        // printf("tracker response : => %s\n", buffer);

        // // close(peer_sock);

        int MaxBufferLength = 524288;
        char buffer[MaxBufferLength];
        int  bytesRead= 1, bytesSent;
        int fd = open(fileName.c_str(),O_CREAT | O_WRONLY,S_IRUSR | S_IWUSR);  

        if(fd == -1)
            perror("couldn't open file");

        while(bytesRead > 0)
        {           
            bytesRead = recv(peer_sock, buffer, MaxBufferLength, 0);

            if(bytesRead == 0)
            {
                break;
            }

            printf("bytes read %d\n", bytesRead);

            printf("receivnig data\n");

            bytesSent = write(fd, buffer, bytesRead);


            printf("bytes written %d\n", bytesSent);

            if(bytesSent < 0)
                perror("Failed to send a message");

        }

        return 0;
 */





    }
    return -1; // unexpected error.

}

int establishConnectionWithTracker(){

    Logger::Info("Establishing connection with tracker");

    int server_fd;
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

    if (connect(client_fd, (struct sockaddr *)&peer_serv_add, sizeof(peer_serv_add)) < 0)
    {
        printf("\nConnection Failed \n");
        Logger::Error("Failed to establish connection with tracker.");     
        return -1;
    }

    Logger::Info("connction established with tracker.");
    return 0;
}

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

    return 0;
}

int main()
{
    // Logger::EnableFileOutput();

    Logger::Info("Client 1 start executing.");
    
    if(establishConnectionWithTracker() != 0 ){
        return -1;
    }

    // make thread for server running paralelly
    pthread_t servingThread;

    pthread_create(&servingThread, NULL, createServer, NULL);
    // createServer(NULL);

    while (1){

        string cmdStr;
        cout<<">>> ";
        getline(cin, cmdStr);

        vector<string> cmd;
        cmd = getCommand(cmdStr);

        if(cmd[0]=="create_user"){

            if(cmd.size() != 3){
                Logger::Error("Wrong arguments in 'create_user'");
                continue;
            }
            else{
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
            }
            

        }
        
        else if(cmd[0] == "login"){
            if(cmd.size()!=3){
                Logger::Error("Wrong arguments in 'login'");
                continue;
            }
            else if(isLoggedIn){
                cout<<"You are already Logged in."<<endl;
                continue;
            }
            else{
                cmdStr += " "+uname+" "+ip+" "+to_string(port);
                if(sendCommandToTracker(cmdStr) == 0){
                    char resBuff[1024];
                    read(client_fd, resBuff, sizeof(resBuff));
                    if(string(resBuff) == "Failed"){
                        Logger::Error("Unsucessful login");
                        cout<<"Invalid ID or Password. Please try Again!!"<<endl;
                    }
                    else {
                        isLoggedIn = true;
                        uname = cmd[1];
                        cout<<"Logged in as: "<<uname<<endl;

                    }
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        else if(cmd[0] == "create_group"){
            if(cmd.size()!=2){
                Logger::Error("Wrong arguments in 'create_group'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }

        else if(cmd[0] == "join_group"){
            if(cmd.size() != 2){
                Logger::Error("Wrong arguments in 'join_group'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        else if(cmd[0] == "leave_group"){
            if(cmd.size() != 2){
                Logger::Error("Wrong arguments in 'leave_group'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        else if(cmd[0] == "list_requests"){
            if(cmd.size() != 2){
                Logger::Error("Wrong arguments in 'list_requests'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    
                    char responseBuffer[1024*512] = {0};

                    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

                        Logger::Error("Couldn't read the response of server / got EOF.");
                        Logger::Info("************************************");

                        return 0;

                    }
                    else{
                        
                        string response(responseBuffer);
                        vector<string> res = splitString(response, '$');
                        printVector(res);



                    }

                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        
        else if(cmd[0] == "accept_request"){
            if(cmd.size() != 3){
                Logger::Error("Wrong arguments in 'accept_request'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        else if(cmd[0] == "list_groups"){
            if(cmd.size() != 1){
                Logger::Error("Wrong arguments in 'list_groups'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }

        else if(cmd[0] == "list_files"){
            if(cmd.size() != 2){
                Logger::Error("Wrong arguments in 'list_files'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                if(sendCommandToTracker(cmdStr) == 0){
                    receiveReplyFromTracker();
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
            // listFiles(cmd[1]);
        }
        else if(cmd[0] == "upload_file"){
            if(cmd.size()!=3){
                Logger::Error("Wrong arguments in 'upload_file'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                uploadFile(cmd);
            }
        }
        else if(cmd[0] == "download_file"){
            if(cmd.size()!=3){
                Logger::Error("Wrong arguments in 'download_file'");
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{
                downloadFile(cmd);
                
            }
        }
        
        else if(cmd[0] == "logout"){
            if(cmd.size()>1){
                Logger::Error("Wrong arguments in 'logout'");
                continue;
            }
            // logout(cmd[1]);
        }
        else if(cmd[0] == "show_downloads"){
            if(cmd.size()>1){
                Logger::Error("Wrong arguments in 'show_downloads'");
                continue;
            }
            // show_downloads(cmd[1]);
        }
        else if(cmd[0] == "stop_share"){
            if(cmd.size()>3){
                Logger::Error("Wrong arguments in 'stop_share'");
                continue;
            }
            // stop_share(cmd[1]);
        }
        else if(cmd[0]== "qq"){
            close(client_fd);
            return 0;
        }
        else{
            Logger::Error("Incorrect command entered..!");
        }

    }
    
    pthread_join(servingThread, NULL);
    return 0;
}
