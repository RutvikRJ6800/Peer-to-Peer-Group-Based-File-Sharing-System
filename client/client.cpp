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
#include <fcntl.h>
#include <thread>
#include <openssl/sha.h> 
#define CHUNKSIZE 524288
using namespace std;
using std::ifstream;

// Global Data Structure
pthread_t servingThread[5000];
vector<thread> downloadThread;
int servingThreadIndex = 0;
pthread_t tid;
int trackerPort;
string trakerrIP;

string uname = "";
bool isLoggedIn = false;
int port;
string ip;
int client_fd;


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
unordered_map<string, FileInfo> downloadingFiles;

// gid, filename form
vector<pair<string, string>> downloadCompletedFiles;

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

long long getFileSize(string path){
    
    std::ifstream in(path.c_str(), std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 

}

string calculateHashofchunk(char *schunk, int length1, int shorthashflag){

    unsigned char hash[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH * 2];
    SHA1((unsigned char *)schunk, length1, hash);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf((char *)&(buf[i * 2]), "%02x", hash[i]);

    //cout<<"hash : "<<buf<<endl;
    string ans;
    for (int i = 0; i < 20; i++) {
        ans += buf[i];
    }
    return ans;
}

string calculateFileHash(char *path){
    string fileHash;
    ifstream file1(path, ifstream::binary);

    /* basic sanity check */
    if (!file1)
    {
        cout << "FILE DOES NOT EXITST : " << string(path) << endl;
        return "-1";
    }

    struct stat fstatus;
    stat(path, &fstatus);

    // Logic for deviding file1 into chunks
    long int totalSize = fstatus.st_size;
    long int chunk_size = CHUNKSIZE;

    int total_chunks = totalSize / chunk_size;
    int last_chunk_size = totalSize % chunk_size;

    if (last_chunk_size != 0){
        ++total_chunks;
    }
    else {
        last_chunk_size = chunk_size;
    }

    // loop to getting each chunk
    for (int chunk = 0; chunk < total_chunks; ++chunk)
    {
        int cur_cnk_size;
        if (chunk == total_chunks - 1)
            cur_cnk_size = last_chunk_size;
        else
            cur_cnk_size = chunk_size;

        char *chunk_data = new char[cur_cnk_size];
        file1.read(chunk_data, cur_cnk_size); /* this many bytes is to be read */

        string sh1out = calculateHashofchunk(chunk_data, cur_cnk_size, 1);
        fileHash = fileHash + sh1out;
    }

    return fileHash;
}

void printVector(vector<string> res){
    cout<<"---------------------------------------"<<endl;
    for(size_t i=0; i<res.size(); i++){
        cout<<" ["<<i+1<<"] "<<res[i]<<"."<<endl;
    }
    cout<<"---------------------------------------"<<endl;
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

            std::ifstream fp1(filePath, std::ios::in|std::ios::binary);
            fp1.seekg(chunkNum*CHUNKSIZE, fp1.beg);

            Logger::Info("sending data starting at " + to_string(fp1.tellg()));
            char buffer[CHUNKSIZE] = {0}; 
            int rc = 0;

            fp1.read(buffer, sizeof(buffer));
            int count = fp1.gcount();

            Logger::Info("Data read count: "+to_string(count));
            // Logger::Info("Data is: "+string(buffer));

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

    string msg = "Peer "+ip+':'+to_string(port)+" started servicing."+to_string(new_socket);

    Logger::Info(msg.c_str());



    char INPbuffer[524288] = {0};

    int valread = read(new_socket, INPbuffer, 524288);
    // printf("%s\n", INPbuffer);

    Logger::Info("***************Recieved Msg From PEER***************");
    Logger::Info(INPbuffer);

    // check if other peer is asking for chunk or chunk map(chunkDetails of file).

    // it asks for chunkDetails
    vector<string> cmd = splitString(INPbuffer, ' ');

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
    
}

void *createServer(void *param)
{

    //    int server_port = *(int *)param; //in future assign ip and port taken via command line argument
    //    int serverPort = 7000;
    //    string serverIP = "127.1.1.1";

    cout<<"[INFO] create server executing..."<<endl;

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

    if (listen(server_fd, 5000) < 0)
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

    char responseBuffer[524288] = {0};

    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

        Logger::Error("Couldn't read the response of server / got EOF.");
        Logger::Info("************************************");

        return -1;

    }
    else{

        printf("Tracker Reply: ==> %s\n",responseBuffer);
        Logger::Info(responseBuffer);
        Logger::Info("************************************");
        bzero((char *)&responseBuffer, sizeof(responseBuffer));

    }
    
    return -1;
}


int uploadFile(vector<string> cmd){

    //check file present at given path;
    // calculate it's file size

    // send strig to tracker (cmd fileName gid fileSize)
    string FilePath = cmd[1]; // also contain File name in it.
    string groupId = cmd[2];
    
    int idx=FilePath.find_last_of('/');
    // string ss= FilePath.substr(0,idx);
    // if(ss=="")ss="/";
    string fileName=FilePath.substr(idx+1,FilePath.size()-(idx+1));
    long long fileSize = getFileSize(FilePath);

    if(fileSize == -1) return -1; // no such file present

    cout<<"filepath: "<<FilePath<<" fileSize: "<<fileSize<<endl;

    // calculate hash
    // append it at end to send at tracker
    string hofpath = FilePath;
    char *longhash = new char[hofpath.length() + 1];
    strcpy(longhash, hofpath.c_str());
    string hashOfFile = calculateFileHash(longhash);

    Logger::Info(hashOfFile);

    string msg = "";
    msg += "upload_file "+fileName+" "+groupId+" "+uname+" "+to_string(fileSize)+" "+hashOfFile;

    sendCommandToTracker(msg);

    char responseBuffer[524288] = {0};

    if(read(client_fd, responseBuffer, sizeof(responseBuffer))<=0){

        Logger::Error("Couldn't read the response of server / got EOF.");
        Logger::Info("************************************");
        return -1;
    }
    else if(string(responseBuffer) == "File Successfully Uploaded."){

        Logger::Info(responseBuffer);
        Logger::Info("************************************");

        long long noOfChunks = fileSize / CHUNKSIZE;
        if(fileSize%CHUNKSIZE != 0)noOfChunks++; // new chunk for rest some data;

        vector<bool> chunkDtl(noOfChunks, 1); 

        FileInfo f1 = FileInfo(FilePath, fileName, fileSize, chunkDtl);   // store file info into uploaded file map.
        uploadedFiles[groupId+"$"+fileName] = f1;

        bzero((char *)&responseBuffer, sizeof(responseBuffer));

        return 0;

    }

    return -1;
    

}

int fetchChunkInfoFromPeer(string peerIp, int peerPort, string groupId , string fileName, vector<string> &currDownloadFilechunkMaps, map<int, pair<string, long long>> &numToIP, int i){
    
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

    //Logger::Info("Received chunk details as: "+string(responseBuffer));

    currDownloadFilechunkMaps.push_back(string(responseBuffer));
    numToIP[i] = make_pair(peerIp, peerPort);
    

    close(peer_sd); // close peer connection.

    // cout<<"chunk map at this time:"<<endl;
    Logger::Info("chunk map at this time:");
    for(size_t i=0; i<currDownloadFilechunkMaps.size(); i++){
        // cout<<currDownloadFilechunkMaps[i]<<endl;
        Logger::Info(currDownloadFilechunkMaps[i]);
    }


    return 0;
    
}

int fetchChunkFromPeer(string peerIp, int peerPort, string groupId, string fileName, int chunkNum, string thisChunkHash){
    
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

        int n, tot = 0;
        long long reqDataSize;
        char buffer[CHUNKSIZE];
        char buffForSHA[CHUNKSIZE];
        long long  bFS = 0;

        string content = "";

        long long fileSize = downloadingFiles[groupId+'$'+fileName].fileSize;
        string filePath = downloadingFiles[groupId+'$'+fileName].filePath;


        long long noOfChunks = fileSize / CHUNKSIZE;
        if(fileSize%CHUNKSIZE != 0)noOfChunks++; // new chunk for rest some data;

        if(chunkNum == noOfChunks-1){
            reqDataSize = (fileSize % 524288);
        }
        else{
            reqDataSize = 524288;
        }



        while (tot < reqDataSize) {
            n = read(peer_sd, buffer, CHUNKSIZE-1);
            if (n <= 0){
                break;
            }

            for(long long m=0; m<n; m++){
                buffForSHA[bFS++] = buffer[m];
            }

            buffer[n] = 0;
            fstream outfile(filePath, std::fstream::in | std::fstream::out | std::fstream::binary);
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

    
        string calcThisChunkHash = calculateHashofchunk(buffForSHA, reqDataSize, 1);

        Logger::Info("[SHA1] Original Hash: "+thisChunkHash+" calculated Hash: "+calcThisChunkHash);
        if(thisChunkHash != calcThisChunkHash){
            Logger::Error("SHA1 doesn't match for chunk no. "+to_string(chunkNum));
            return -1;
        }

        if(uploadedFiles.find(groupId+'$'+fileName) == uploadedFiles.end()){
            // cout<<"[DEBUG] First chunk of file recieved."<<endl;
            // first time its uploadeing.
            vector<bool> cm(noOfChunks, 0);
            cm[chunkNum] = 1;
            FileInfo f1 = FileInfo(filePath, fileName, fileSize, cm);
            

            // tell tracker about this good news.
            string goodNews = "i_am_leacher "+groupId+" "+fileName+" "+uname;
            // send command to tracker 
            if(send(client_fd, goodNews.c_str(), goodNews.size(), 0)>0){
                Logger::Info("Become leacher of file: "+fileName);
                uploadedFiles[groupId+'$'+fileName] = f1; // update chunk vector of the file.

            }         
        }
        else{
            uploadedFiles[groupId+'$'+fileName].chunkDetails[chunkNum] = 1; // update chunk vector of the file.
        }

    }

    close(peer_sd); // close peer connection.

    return 0;
    
}

int downloadFile(vector<string> cmd){
    
    string groupId = cmd[1];
    string fileName = cmd[2];
    string filePath = cmd[3]+'/'+fileName;

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
        Logger::Info(responseBuffer);

        vector<string> vec = splitString(fileDtls, '$');
        if(vec.size()<=2){ // file present in group but no seeders or leechers available.
            cout<<"No seeders or leercher found. Hence download incomplete."<<endl;
            Logger::Error("No seeders or leercher found.");
            Logger::Error("Download Incomplete");
            return -1;
        }


        long long fileSize = stoll(vec[0]);
        string shaone = vec[1];

        long long noOfChunks = fileSize / CHUNKSIZE;
        if(fileSize%CHUNKSIZE != 0)noOfChunks++;


        FileInfo f1 = FileInfo(filePath, fileName, fileSize);

        
        vector<pair<string,int>> ips;

        for(size_t i=2; i<vec.size(); i++){

            string sip = vec[i++];
            int pt = stoi(vec[i]);
            ips.push_back(make_pair(sip, pt));

        }

        if(ips.size() == 0){

            cout<<"No seeders available for this file.."<<endl;
            Logger::Error("No seeders available for this file..");

            return -1;
        }
        // print ip and port
        cout<<"***********************************"<<endl;
        cout<<"IP and port of Seeders or leechers"<<endl;
        for(size_t i=0; i<ips.size(); i++){

           cout<<"IP: "<<ips[i].first<<" Port: "<<ips[i].second<<endl;

        }
        
        downloadingFiles[groupId+'$'+fileName] = f1;

        vector<thread> getChunkMapthread;
        vector<string> currDownloadFilechunkMaps;
        map<int, pair<string, long long>> numToIP;
        size_t numToIPIndex;

        currDownloadFilechunkMaps.clear();
        numToIP.clear();
        numToIPIndex = 0;

        for(size_t i =0; i<ips.size(); i++){
            // getChunkMapthread.push_back(thread(fetchChunkInfoFromPeer, ips[i].first, ips[i].second, groupId , fileName)); // dont use thread here
            fetchChunkInfoFromPeer(ips[i].first, ips[i].second, groupId , fileName, currDownloadFilechunkMaps, numToIP, i); // dont use thread here
        }

        // print chunkMap;

        Logger::Info("chunk received... chunks are.");
        for(size_t i=0; i<currDownloadFilechunkMaps.size(); i++){
            Logger::Info(currDownloadFilechunkMaps[i]);
        }

        vector<vector<int>> chunkToPeersList(currDownloadFilechunkMaps[0].size());


        for(size_t i=0; i<currDownloadFilechunkMaps[0].size(); i++){

            for(size_t j=0; j<currDownloadFilechunkMaps.size(); j++){
                if(currDownloadFilechunkMaps[j][i] == '1'){
                    chunkToPeersList[i].push_back(j);
                }
            }

        }

        // // get chunk from peer and add it in file

        int fd = open(fileName.c_str(), O_CREAT | O_WRONLY, S_IRWXU);
        if(fd < 0){
            printf("The file already exists.\n") ;
            close(fd);
            return -1;
        }
        close(fd);

        vector<thread> getChunkthread;
        vector<string> chunkHash;

        for(size_t i=0; i<noOfChunks; i++){
            size_t offst = i*20;
            string chunkSha = "";
            for(size_t j=0; j<20; j++){
                chunkSha += shaone[offst+j];
            }
            chunkHash.push_back(chunkSha);
        }

        int res = 0;

        for(size_t i=0; i<currDownloadFilechunkMaps[0].size(); i++){

            int peerNo = rand()%chunkToPeersList[i].size();
            Logger::Info("downloading chunkNumber: "+to_string(i));
            
            // getChunkthread.push_back(thread(fetchChunkFromPeer, numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i));
            string thisChunkHash = chunkHash[i];
            res += fetchChunkFromPeer(numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i, thisChunkHash);
        }

        if(res<0){

            cout<<"File Downloaded Successfully, but found corrupted."<<endl;
            Logger::Error("File Downloaded Successfully, but found corrupted.");

        }
        else{
            Logger::Info("File downloaded successfully. No corruption found.");
        }
        // for(size_t i=currDownloadFilechunkMaps[0].size()-1; i>=0; i--){

        //     int peerNo = rand()%chunkToPeersList[i].size();
        //     Logger::Info("downloading chunkNumber: "+to_string(i));

        //     // getChunkthread.push_back(thread(fetchChunkFromPeer, numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i));
        //     fetchChunkFromPeer(numToIP[peerNo].first, numToIP[peerNo].second, groupId, fileName, i);
        // }



        // check if the file is corrupted or not

        // for (std::thread & th : getChunkthread){
        //     // If thread Object is Joinable then Join that thread.
        //     if (th.joinable()) th.join();
        // }

        
        
        downloadingFiles.erase(groupId+'$'+fileName); // delete file from downloading list map
        downloadCompletedFiles.push_back(make_pair(groupId, fileName)); // add file into completed map

        return 0;

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

void showDownload(){
    int times = 0;
    cout<<"---------------------------------------"<<endl;
    for (auto i : downloadingFiles){
        vector<string> vec = splitString(i.first, '$');
        times++;
        cout <<times<<". "<<"[D] ["<< vec[0] << "]  " << vec[1] << endl;
    }

    for(size_t i=0; i<downloadCompletedFiles.size(); i++){
        times++;
        cout <<times<<". "<<"[C] ["<< downloadCompletedFiles[i].first << "]  " << downloadCompletedFiles[i].second << endl;
    }

    cout<<"+-+ Total Downloads: "<<times<<" +-+"<<endl;
    cout<<"---------------------------------------"<<endl;
}

int main(int argc, char** argv)
{
    // Logger::EnableFileOutput();

    if(argc != 3){
        cout<<"Wrong number of arguments passed.!"<<endl;
        return -1;
    }

    vector<string> ipp = splitString(argv[1], ':');
    ip = ipp[0], port =stoi(ipp[1]);

    ifstream tinfo;
    tinfo.open(argv[2]);

    string sLine;
    if(!tinfo.eof()){
        tinfo >> sLine;
    }
    else{
        cout<<"Couldn't read data from tracker file."<<endl;
        return -1;
    }

    vector<string> tripp = splitString(sLine, ':');
    trakerrIP = tripp[0], trackerPort =stoi(tripp[1]);

    
    

    Logger::Info("Client "+ip+':'+to_string(port)+" start executing.");
    
    if(establishConnectionWithTracker() != 0 ){
        return -1;
    }

    ofstream log_file("logFile.txt", ios_base::out | ios_base::trunc );

    // make thread for server running paralelly
    pthread_t servingThread;

    pthread_create(&servingThread, NULL, createServer, NULL);
    // createServer(NULL);

    while (1){

        string cmdStr;
        cout<<">>> ";
        getline(cin, cmdStr);

        if(cmdStr == ""){
            cout<<"Enter proper command"<<endl;
            continue;
        }
        vector<string> cmd;
        cmd = getCommand(cmdStr);


        if(cmd[0]=="create_user"){

            if(cmd.size() != 3){
                cout<<"Wrong arguments in 'create_user'"<<endl;
                continue;
            }
            else if(isLoggedIn){
                cout<<"You are already Logged in. Rerun program to create new user."<<endl;
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
                cout<<"Wrong arguments in 'login'"<<endl;
                continue;
            }
            else if(isLoggedIn){
                cout<<"You are already Logged in."<<endl;
                continue;
            }
            else{
                cmdStr += " "+uname+" "+ip+" "+to_string(port);
                if(sendCommandToTracker(cmdStr) == 0){
                    char resBuff[524288];
                    read(client_fd, resBuff, sizeof(resBuff));
                    if(string(resBuff) == "Login Successfull"){
                        isLoggedIn = true;
                        uname = cmd[1];
                        cout<<"Logged in as: "<<uname<<endl;
                    }
                    else {
                        Logger::Error("Unsucessful login");
                        cout<<"Invalid ID or Password or User may already logged in from somewhere else. Please try Again!!"<<endl;

                    }
                }
                else{
                    cout<<"Command Couldn't sent to Tracker."<<endl;
                }
            }
        }
        
        else if(cmd[0] == "create_group"){
            if(cmd.size()!=2){
                cout<<"Wrong arguments in 'create_group'"<<endl;
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
                    cout<<"Command Couldn't sent to Tracker."<<endl;
                }
            }
        }

        else if(cmd[0] == "join_group"){
            if(cmd.size() != 2){
                cout<<"Wrong arguments in 'join_group'"<<endl;
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
                cout<<"Wrong arguments in 'leave_group'"<<endl;
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
                    cout<<"Command Couldn't sent to Tracker."<<endl;
                }
            }
        }
        
        else if(cmd[0] == "list_requests"){
            if(cmd.size() != 2){
                cout<<"Wrong arguments in 'list_requests'"<<endl;
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
                cout<<"Wrong arguments in 'accept_request"<<endl;
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
                cout<<"Wrong arguments in 'list_groups'"<<endl;
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

        else if(cmd[0] == "list_files"){
            if(cmd.size() != 2){
                cout<<"Wrong arguments in 'list_files'"<<endl;
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
                        Logger::Info("************************************");
                    }
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
            // listFiles(cmd[1]);
        }
        else if(cmd[0] == "upload_file"){
            if(cmd.size()!=3){
                cout<<"Wrong arguments in 'upload_file'"<<endl;
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                if(uploadFile(cmd) == 0){
                    cout<<"File Uploaded Successfully"<<endl;
                }
                else{
                    cout<<"Failed to upload File"<<endl;
                }
            }
        }
        else if(cmd[0] == "download_file"){
            if(cmd.size()!=4){
                cout<<"Wrong arguments in 'download_file'"<<endl;
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{

                // downloadThread.push_back(thread(downloadFile, cmd));
                if(downloadFile(cmd) == 0 ){
                    cout<<"File downloaded successfully. No corruption found."<<endl;
                }
                else{
                    cout<<"Failed to download File"<<endl;
                }
                
            }
        }
        
        else if(cmd[0] == "logout"){
            if(cmd.size()!=1){
                cout<<"Wrong arguments in 'logout'"<<endl;
                
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                cmdStr += " "+uname;
                sendCommandToTracker(cmdStr);
                char responseBuffer[524288];

                bzero((char *)&responseBuffer, sizeof(responseBuffer));

                if(read(client_fd, responseBuffer, sizeof(responseBuffer))){


                    if(string(responseBuffer) == "Logout successfull."){
                        isLoggedIn = false;

                        cout<<"User "<<uname<<" Logout Successfully"<<endl;
                        Logger::Info(responseBuffer);
                        Logger::Info("************************************");
                        continue;
                    }
                    else{
                        cout<<"Failed to logout."<<endl;
                        Logger::Info(responseBuffer);
                        Logger::Info("************************************");
                        continue;
                    }
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        
        else if(cmd[0] == "show_downloads"){
            if(cmd.size()!=1){
                cout<<"Wrong arguments in 'show_downloads'"<<endl;
                continue;
            }
            else if(!isLoggedIn){
                cout<<"You are not logged in. Please log in first."<<endl;
                continue;
            }
            else{ 
                showDownload();
            }
        }
        else if(cmd[0] == "stop_share"){
            if(cmd.size()!=3){
                cout<<"Wrong arguments in 'stop_share'"<<endl;
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
                        if(response == "success"){
                            uploadedFiles.erase(cmd[1]+'$'+cmd[2]);
                        }
                        Logger::Info("************************************");
                    }
                }
                else{
                    Logger::Error("Command Couldn't sent to Tracker.");
                }
            }
        }
        else if(cmd[0]== "qq"){
            close(client_fd); // close connection of tracker.
            cout<<"close connection with tracker."<<endl;
            return 0;
            break;
            
        }
        else{
            Logger::Error("Incorrect command entered..!");
        }

    }

    // for (std::thread & th : downloadThread){
    //     // If thread Object is Joinable then Join that thread.
    //     if (th.joinable()) th.join();
    // }
    
    close(client_fd); // close connection of tracker.
    // pthread_join(servingThread, NULL);
    return 0;
}
