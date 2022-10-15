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

// uid, [ip, port]
unordered_map<string, vector<string>> isLoggedIn;


class UserInfo{
    public:
    string password, ip, port;

    UserInfo(){

    }
    
    UserInfo(string pswd , string ipAddr, string portNo){
        password = pswd;
        ip = ipAddr;
        port = portNo;
    }
};
// uid , password Object
unordered_map<string, UserInfo> users; 

class GroupInfo{
    public:
    string owner;
    vector<string> members;
    vector<string> files;

    GroupInfo(){

    }

    GroupInfo(string ownerName){
        owner = ownerName;
    }

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

    FileInfo(){

    }

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

// FUNCTION TO LOAD DATA
void loadDATA(){
    UserInfo u1 = UserInfo("jsn", "", "");
    users["jsn"] = u1;
}




// **********************************************
// functions for executing tracker functionality
// **********************************************

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

int createUser(vector<string> cmd){
    if(users.find(cmd[1])!=users.end()){ // user already exist with uid
        return -1;
    }
    else{
        UserInfo u1 = UserInfo(cmd[2], "", "");
        users[cmd[1]] = u1;
    }
    return 0;

}

int loginUser(vector<string> cmd){
    if(users.find(cmd[1])==users.end()){ // user does not exist with given uid
        return -1;
    }
    else if(users[cmd[1]].password == cmd[2]){ // password matched
        users[cmd[1]].ip = cmd[3];
        users[cmd[1]].port = cmd[4];
        // cout<<"set ip as: "<<users[cmd[1]].ip<<" && port as: "<<users[cmd[1]].port;
        
        return 0;

    }
    return -1; // password doesn't match

}

int createGroup(vector<string> cmd){
    if(users.find(cmd[2])==users.end()){ // user does not exist with given uid
        return -1;
    }
    else if(users.find(cmd[2])!=users.end() && users[cmd[2]].ip == ""){ // user is not logged in
        return -1;
    }
    else if(users.find(cmd[2])!= users.end()){ // logged in successfully
        
        if(groups.find(cmd[1]) == groups.end()){ // valid group name

            GroupInfo g1 = GroupInfo(cmd[2]);
            groups[cmd[1]] = g1;
            return 0;

        }
        else{
            return -1; // group name already exist.
        }

    }
    return -1; // unexpected error
}

int joinGroup(vector<string> cmd){
    
    if(groups.find(cmd[1])==groups.end()){ // group does not exist with given uid
        return -1;
    }

    else { // group exist
        
        if(find(groups[cmd[1]].members.begin(), groups[cmd[1]].members.end(), cmd[2]) == groups[cmd[1]].members.end()){
            // request user is not yet joined the group
            pendingRequest[cmd[1]].push_back(cmd[2]); // add user into pending queue.
            // groups[cmd[1]].members.push_back(cmd[2]);
        }
        return 0;

    }
    return -1; // unexpected error
} 

int leaveGroup(vector<string> cmd){

    if(groups.find(cmd[1])==groups.end()){ // group does not exist with given uid
        return -1;
    }

    else { // group exist
        
        if( find(groups[cmd[1]].members.begin(), groups[cmd[1]].members.end(), cmd[2]) != groups[cmd[1]].members.end()){

            groups[cmd[1]].members.erase(find(groups[cmd[1]].members.begin(), groups[cmd[1]].members.end(), cmd[2])); // member of this group
            return 0;

        }
        return -1; // not a member of the group

    }
    return -1; // unexpected error
}

int listRequests(vector<string> cmd){

    string gid = cmd[1];
    string uid = cmd[2];
    if(groups.size() > 0 && groups.find(gid) == groups.end()){
        // no such group not exist
        return -1;
    }
    else if(groups.size() > 0 && groups.find(gid) != groups.end()){
        if(groups[gid].owner != uid){
            // requested user is not the owner of the group
            return -2;
        }
        else{
            return 0;
        }
    }
    return -3;
}

string getPendingRequestsString(vector<string> cmd){

    string gid = cmd[1];
    string uid = cmd[2];

    string res = "";
    
    for(size_t i = 0; i < pendingRequest[gid].size()-1; i++){
        res += pendingRequest[gid][i] + "$";
    }
    res +=  pendingRequest[gid][pendingRequest[gid].size()-1];

    return res;
}

int acceptRequest(vector<string> cmd){

    string gid = cmd[1];
    string pendingUid = cmd[2];
    string uid = cmd[3];
    if(groups.size() > 0 && groups.find(gid) == groups.end()){
        // no such group not exist
        return -1;
    }
    else if(groups.size() > 0 && groups.find(gid) != groups.end()){

        if(groups[gid].owner != uid){
            // requested user is not the owner of the group
            return -2;
        }
        else{
            // requested user is the owner of the group
            if(find(groups[gid].members.begin(), groups[gid].members.end(), pendingUid) == groups[gid].members.end()){
                groups[gid].members.push_back(pendingUid);
                pendingRequest[gid].erase(find(pendingRequest[gid].begin(), pendingRequest[gid].end(), pendingUid)); // delete it from pending list
                return 0;
            }
            else return -3; // uid is already member of the group
        }
    }
    return -4;
    

}

string listGroups(vector<string> cmd){

    string res = "";

    if(groups.size()==0)return res;

    for(auto it = groups.begin(); it != groups.end(); it++){
        res += it->first + "$";
    }
    return res;

}

string listFiles(vector<string> cmd){
    string groupId = cmd[0];

    string res = "";
    if(groups.size() == 0 ){ 
        res = "No group exist.";
        return res ;
    }
    else if(groups.size() > 0 && groups.find(groupId) == groups.end()){
        // no such group exist
        res = "No such group exist.";
        return res ;
    }
    else{

        for(size_t i = 0; i<groups[groupId].files.size(); i++){
            res += groups[groupId].files[i] + "$";
        }

    }
    return res;
}




void * serverserving(void * arg){

    int new_socket = *(int *)arg;

    // put all these  in while loop
    while(1){

        string msg = "Tracker started servicing at socket: "+to_string(new_socket);

        // char *msgBuffer = new char[msg.size()];
        // strcpy(msgBuffer,msg.);
        
        Logger::Info(msg.c_str());

        char buffer[1024] = {0};
        int fd = *(int *)arg;

        int valread = read(fd, buffer, 1024);
        printf("%s\n", buffer);

        Logger::Info("*** Recieved Msg ***");
        Logger::Info(buffer);

        // command decoding
        string cmdStr(buffer);
            // cout<<">>> ";
            // getline(cin, cmdStr);

        vector<string> cmd;
        cmd = getCommand(cmdStr);

        if(cmd[0]=="create_user"){

            Logger::Info("executing 'create_user'...");
            if(createUser(cmd) == 0){
                string replyMsg = "User "+ cmd[1] +" successfuly created.";
                // char *serverreply = new char[replyMsg.length() + 1];
                // strcpy(serverreply, replyMsg.c_str());


                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                
            }else{
                string replyMsg = "User "+ cmd[1] +" Failed to create.";

                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
            }
            Logger::Info("Reply Msg send to client");
            

        }
        
        else if(cmd[0] == "login"){
            Logger::Info("executing 'login'...");
            if(loginUser(cmd) == 0){
                string replyMsg = "User "+ cmd[1] +" successfuly logged in.";

                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
            }else{
                string replyMsg = "User "+ cmd[1] +" Failed to logged in.";

                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
            }
        }
        
        else if(cmd[0] == "create_group"){
            Logger::Info("executing 'create_group'...");
            if(createGroup(cmd) == 0){
                string replyMsg = "Group '"+ cmd[1] +"' successfuly created.";

                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
            }else{
                string replyMsg = "Group '"+ cmd[1] +"' Failed to create.";

                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
            }
        }

        
        else if(cmd[0] == "join_group"){
            string replyMsg;
            Logger::Info("executing 'join_group'...");
            if(joinGroup(cmd) == 0){
                
                replyMsg = "Request successfully sent to join Group '"+ cmd[1] +"'";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
                
            }else{

                replyMsg = "Group '"+ cmd[1] +"' Failed to join. May be No such Group Exist !!";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
                
            }
        }

        else if(cmd[0] == "leave_group"){
            string replyMsg;
            Logger::Info("executing 'leave_group'...");
            if(leaveGroup(cmd) == 0){
                
                replyMsg = "Group '"+ cmd[1] +"' successfuly joined.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
                
            }else{

                replyMsg = "Group '"+ cmd[1] +"' Failed to leave. May be No such Group Exist or You are not member of group !!";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
                
            }
        }
        else if(cmd[0] == "list_requests"){

            string replyMsg;
            Logger::Info("executing 'list_requests'...");
            int status = listRequests(cmd);
            if(status == 0){
                
                replyMsg = getPendingRequestsString(cmd);
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
                
            }
            else if(status == -1){

                replyMsg = "No such Group Exist.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
            }
            else if(status == -2){

                replyMsg = "You are not the owner of the '"+cmd[1]+"' group.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");

            }
            else{

                replyMsg = "Group '"+ cmd[1] +"' Failed to fetch pending request. !!";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
                
            }

        }

        
        else if(cmd[0] == "accept_request"){
            
            string replyMsg;
            Logger::Info("executing 'accept_requests'...");
            int status = acceptRequest(cmd);
            if(status == 0){
                
                replyMsg = "Request of '"+ cmd[2] +"' accepted successfuly to join group.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Info("Reply Msg send to client");
                
            }
            else if(status == -1){

                replyMsg = "No such Group Exist.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
            }
            else if(status == -2){

                replyMsg = "You are not the owner of the '"+cmd[1]+"' group.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");

            }
            else if(status == -3){

                replyMsg = "'"+cmd[2]+"' User is already member of the '"+cmd[1]+"' group.";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");

            }
            else{

                replyMsg = "Failed to accept the request of user '"+cmd[2]+"'. !!";
                send(fd, replyMsg.c_str(), replyMsg.size(), 0);
                Logger::Error("Reply Msg send to client");
                
            }
            // acceptRequest(cmd[1]);
        }
        
        else if(cmd[0] == "list_groups"){
            string replyMsg;
            Logger::Info("executing 'accept_requests'...");
            replyMsg = listGroups(cmd);

            if(replyMsg.size() == 0){
                replyMsg = "No groups present.!!";
            }

            send(fd, replyMsg.c_str(), replyMsg.size(), 0);
            Logger::Error("Reply Msg send to client");

            // listGroups(cmd[1]);
        }
        
        else if(cmd[0] == "list_files"){
            string replyMsg;
            Logger::Info("executing 'list_files'...");
            replyMsg = listFiles(cmd);

            send(fd, replyMsg.c_str(), replyMsg.size(), 0);
            Logger::Error("Reply Msg send to client");
            
        }
        
        else if(cmd[0] == "upload_file"){
            if(cmd.size()>3){
                Logger::Error("Too many arguments in 'upload_file'");
            }
            // uploadFile(cmd[1]);
        }
        
        else if(cmd[0] == "download_file"){
            if(cmd.size()>3){
                Logger::Error("Too many arguments in 'download_file'");
            }
            // download_file(cmd[1]);
        }
        else if(cmd[0] == "logout"){
            if(cmd.size()>1){
                Logger::Error("Too many arguments in 'logout'");
            }
            // logout(cmd[1]);
        }
        else if(cmd[0] == "show_downloads"){
            if(cmd.size()>1){
                Logger::Error("Too many arguments in 'show_downloads'");
            }
            // show_downloads(cmd[1]);
        }
        else if(cmd[0] == "stop_share"){
            if(cmd.size()>3){
                Logger::Error("Too many arguments in 'stop_share'");
            }
            // stop_share(cmd[1]);
        }
        else{
            Logger::Error("Incorrect command entered..!");
        }

    }


    
    
    // string replyMsg = "This is reply Msg from Tracker";
    // char *serverreply = new char[replyMsg.length() + 1];
    // strcpy(serverreply, replyMsg.c_str());
    

    // send(fd, serverreply, strlen(serverreply), 0);
	// Logger::Info("Reply Msg send to client");
    
	// pthread_exit(NULL);
}



void * listening(void* arg){

    Logger::Info("listening executing...");
    int server_fd;
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

    Logger::Info("tracker listening successfuly...");

    while (1)
    {   
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Error in accept connection");
            exit(EXIT_FAILURE);
        }

        Logger::Info("******Connection accepted at tracker side*******");

        // if (pthread_create(&servingThread[servingThreadIndex++], NULL, serverserving, (void *)&new_socket) < 0)
        // {
        //     perror("\ncould not create thread\n");
        // }
        pthread_create(&servingThread[servingThreadIndex++], NULL, serverserving, (void *)&new_socket);

    }
    
}

int main(){
    // port = 7000; set port here from the cmd args
    // ip = "127.1.1.1";
    // string arg1 = "127.1.1.1:7000";

    Logger::EnableFileOutput();
    Logger::Info("Tracker Started Servicing ");

    loadDATA();

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

