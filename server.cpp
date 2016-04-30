#include<stdio.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<iostream>
#include<stdlib.h>    //strlen
#include<arpa/inet.h>//inet_addr
#include<string.h>
#include<unistd.h> //for close(), write
#include<poll.h>
#include<errno.h>
#include<sqlite3.h>
#include<bits/stdc++.h>
#include<fstream>
#include <ctime>

#define DB_FILE "mydatabase.db"
#define SOCK_PATH "chat_socket"
#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define BLUE    "\033[34m"      /* Blue */

using namespace std;

sqlite3 *db;
int sockfd;
map<string, int> lobby;
map<int, pair<string, int>> fdLookup;
struct pollfd fdarr[100];
static int onlineUsers = 0;
ofstream sfile, cfile;
time_t result = time(nullptr);

void sendString(int fd, const char *message){
    if(write(fd, message, strlen(message)) == -1) {
        perror("send");
        exit(1);
    }
}

string quotesql( const string& s ) {
    return string("'") + s + string("'");
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   return 0;
}

static int getCallback(void *NotUsed, int argc, char **argv, char **azColName){
   string *result = (string *)NotUsed;
   if(argc) *result = argv[1];
   return 0;
}

static int loginCallback(void *NotUsed, int argc, char **argv, char **azColName){
   int *flag = (int *)NotUsed;
   if(!argc) *flag = 0;
   else if(strstr(argv[2], "TRUE")) *flag=-1;
   else *flag =1;
   return 0;
}

int executesql(const string sql, int *dataFlag, int (*callback)(void*, int, char**, char**)){
    int rc;
    char *zErrMsg = 0;
    rc = sqlite3_exec(db, sql.c_str(), callback, dataFlag, &zErrMsg);
    if( rc != SQLITE_OK ){
        sqlite3_free(zErrMsg);
        return 0;
    }
    return 1;
}

int getExecutesql(const string sql, string *dataFlag, int (*callback)(void*, int, char**, char**)){
    int rc;
    char *zErrMsg = 0;
    rc = sqlite3_exec(db, sql.c_str(), callback, dataFlag, &zErrMsg);
    if( rc != SQLITE_OK ){
        sqlite3_free(zErrMsg);
        return rc;
    }
    return rc;
}

string lobbystatus(){
    string response=to_string(onlineUsers)+" user online.\n";
    for(map<string, int>::iterator it=lobby.begin(); it!=lobby.end(); it++)
        response+= it->first + "\t";
    return response;
}

void broadcast(string s){
    for(map<string, int>::iterator it=lobby.begin(); it!=lobby.end(); it++){
        sendString(it->second, s.c_str());
    }
}

void makeLog(int type, string entry){
    string t = asctime(localtime(&result));
    t.pop_back();
    t+=" "+entry+"\n";
    if(!type){
        sfile.open ("system.log", ios::app);
        sfile<<t;
        sfile.close();
    }
    else{
        cfile.open ("client_requests.log", ios::app);
        cfile<<t;
        cfile.close();
    }
}

void parseServerInput(string input){
    istringstream iss(input);
    vector<string> token;
    do{
        string sub;
        iss >> sub;
        token.push_back(sub);
    }while(iss);
    token.pop_back();
    if(token[0]=="broadcast"){
        if(token.size()<2)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nbroadcast <message>"<<RESET<<endl;
        else{
            if(!onlineUsers) cout<<BLUE<<"No online users."<<RESET<<endl;
            else{
                makeLog(0, input);
                string message="ADMIN: ";
                for(int i=1; i<token.size(); i++) message+=token[i]+" ";
                broadcast(message);
                cout<<BLUE<<"Message broadcast done."<<RESET<<endl;
            }
        }
    }
    else if(token[0]=="send"){
        if(token.size()<3)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nsend <receiver_username> <message>"<<RESET<<endl;
        else{
            if(lobby.find(token[1]) != lobby.end()){
                makeLog(0, input);
                string response = "ADMIN: ";
                for(int i=2; i<token.size(); i++) response+=token[i]+" ";
                sendString(lobby[token[1]], response.c_str());
                cout<<BLUE<<"Message sent."<<RESET<<endl;
            }
            else cout<<BLUE<<"User "<<token[1]<<" not found."<<RESET<<endl;
        }
    }
    else if(token[0]=="kick"){
        if(token.size()<2)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nkick <username> [reason_for_the_kick]"<<RESET<<endl;
        else{
            makeLog(0, input);
            if(lobby.find(token[1]) != lobby.end()){
                string response = "ADMIN: You have been kicked out.";
                if(token.size()>2){
                    response +=" [";
                    for(int i=2; i<token.size(); i++) response+=token[i]+" ";
                    response +="]\n";
                }
                sendString(lobby[token[1]], response.c_str());
                int fd = lobby[token[1]];
                lobby.erase(token[1]);
                fdarr[fdLookup[fd].second].fd=-1;
                fdLookup.erase(fd);
                close(fd);
                onlineUsers--;
                response = "ADMIN: kicked "+token[1];
                if(token.size()>2){
                    response +=" [";
                    for(int i=2; i<token.size(); i++) response+=token[i]+" ";
                    response +="].\n";
                }
                response += lobbystatus();
                broadcast(response);
                cout<<BLUE<<"Kicked "<<token[1]<<RESET<<endl;
            }
            else cout<<BLUE<<"User "<<token[1]<<" not found."<<RESET<<endl;
        }
    }
    else if(token[0]=="ban"){
        if(token.size()<2)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nban <username> [reason_for_the_ban]"<<endl;
        else{
            makeLog(0, input);
            string dbResult;
            string sql="SELECT * FROM USER WHERE ID = "+ quotesql(token[1])+";";
            int rc = getExecutesql(sql, &dbResult, getCallback);
            if(dbResult.length()){
                sql= "UPDATE USER SET BANNED='TRUE' WHERE ID="+ quotesql(token[1]) + ";";
                int rc=executesql(sql, NULL, callback);
                cout<<BLUE<<"Banned user "<<token[1]<<RESET<<endl;
                if(lobby.find(token[1]) != lobby.end()){
                    string response = "ADMIN: You have been banned.[";
                    if(token.size()>2){
                        for(int i=2; i<token.size(); i++) response+=token[i]+" ";
                    }
                    response+="]\n";
                    sendString(lobby[token[1]], response.c_str());
                    int fd = lobby[token[1]];
                    lobby.erase(token[1]);
                    fdarr[fdLookup[fd].second].fd=-1;
                    fdLookup.erase(fd);
                    close(fd);
                    onlineUsers--;
                }
                string response = "ADMIN: Banned "+token[1];
                if(token.size()>2){
                    response +=" [";
                    for(int i=2; i<token.size(); i++) response+=token[i]+" ";
                    response +="]";
                }
                response+=".\n";
                response += lobbystatus();
                broadcast(response);
            }
            else cout<<BLUE<<"User "<<token[1]<<" not found."<<RESET<<endl;
        }
    }
    else if(token[0]=="unban"){
        if(token.size()!=2)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nunban <username>"<<RESET<<endl;
        else{
            makeLog(0, input);
            string dbResult;
            string sql="SELECT * FROM USER WHERE ID = "+ quotesql(token[1])+";";
            int rc = getExecutesql(sql, &dbResult, getCallback);
            if(dbResult.length()){
                string sql=
                    "UPDATE USER SET BANNED='FALSE' WHERE ID="+ quotesql(token[1]) + ";";
                if(!executesql(sql, NULL, callback)) cout<<"User "<<token[1]<<" not found.\n";
                else{
                    cout<<BLUE<<"Unbanned user "<<token[1]<<RESET<<endl;
                    string response = "ADMIN: Unbanned "+token[1];
                    response+=".\n";
                    response += lobbystatus();
                    broadcast(response);
                }
            }
            else cout<<BLUE<<"User "<<token[1]<<" not found."<<RESET<<endl;
        }
    }
    else if(token[0]=="logout"){
        if(token.size()!=1)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nlogout"<<RESET<<endl;
        else{
            makeLog(0, input);
            broadcast("Server is offline.");
            cout<<BLUE<<"Successfully logged out."<<RESET<<endl;
            exit(1);
        }
    }
    else{
        cout<<BLUE<<"Command not found.\nUsage:\nbroadcast <message>\nsend <receiver_username> <message>\nkick <username> [reason_for_the_kick]\nban <username> [reason_for_the_ban]\nunban <username>\nlogout"<<RESET<<endl;
    }
}

void parseRequest(int fd, int fdIndex, const char *message){
    istringstream iss(message);
    vector<string> token;
    do{
        string sub;
        iss >> sub;
        token.push_back(sub);
    }while(iss);

    if(token[0]=="logout"){
        string username=fdLookup[fd].first;
        string entry = "[" +username +"] " + message;
        makeLog(1, entry);
        sendString(fd, "Successfully logged out.");
        close(fd);
        fdarr[fdIndex].fd=-1;
        fdLookup.erase(fd);
        lobby.erase(username);
        onlineUsers--;
        string response = lobbystatus();
        broadcast(response);
    }
    else if(token[0]=="lobbystatus"){
        string username=fdLookup[fd].first;
        string entry = "[" +username +"] " + message;
        makeLog(1, entry);
        string response = lobbystatus();
        sendString(fd, response.c_str());
    }
    else if(token[0]=="register"){
        makeLog(1, message);
        string sql=
            "INSERT INTO USER (ID,PASSWORD) VALUES ("
            + quotesql(token[1]) + ","
            + quotesql(token[2]) + ");";
        if(!executesql(sql, NULL, callback)) sendString(fd, "Specified user exits already.");
        else sendString(fd, "Registered successfully. You can login now.");
    }
    else if(token[0]=="login"){
        makeLog(1, message);
        if(lobby.find(token[1]) == lobby.end()){
            string sql=
                "SELECT * FROM USER WHERE ID = "+ quotesql(token[1]) + " AND PASSWORD = " + quotesql(token[2]) + ";";
            int dataFlag=0;
            executesql(sql, &dataFlag, loginCallback);
            if(dataFlag==0) sendString(fd, "Sorry your id and password is not correct.");
            else if(dataFlag==-1) sendString(fd, "You are banned by the server.");
            else{
                lobby[token[1]] = fd;
                fdLookup[fd] = make_pair(token[1], fdIndex);
                onlineUsers++;
                sendString(fd, "Successfully logged in.\n");
                string response = lobbystatus();
                broadcast(response);
            }
        }
        else sendString(fd, "You cannot login multiple times.");
    }
    else{
        string response;
        if(lobby.find(token[1]) == lobby.end()){
            response = "User " + token[1] + " not found.";
            sendString(fd, response.c_str());
        }
        else{
            string username=fdLookup[fd].first;
            for(int i=2; i<token.size()-1; i++) response+=token[i]+" ";
            string entry = "[" +username + " > "+ token[1]+ "] " + response;
            makeLog(1, entry);
            response = username + ": " + response;
            sendString(lobby[token[1]], response.c_str());
        }
    }
}

void createTables(){
    sqlite3 *db;
    char *zErrMsg = 0;
    int  rc;

    //Open database
    rc = sqlite3_open(DB_FILE, &db);
    if(rc){
        string error = "can't open database: "+string(sqlite3_errmsg(db));
        makeLog(0, error);
        exit(0);
    }
    else makeLog(0, "opened database successfully");

    //Create SQL statement
    string sql = "CREATE TABLE USER("
         "ID VARCHAR(15) PRIMARY KEY     NOT NULL,"
         "PASSWORD        VARCHAR(15)    NOT NULL,"
         "BANNED BOOLEAN DEFAULT FALSE );";

    //Execute SQL statements
    rc = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        string error = "SQL error: "+string(zErrMsg);
        makeLog(0, error);
        sqlite3_free(zErrMsg);
    }else{
        makeLog(0, "table created successfully");
    }
    sqlite3_close(db);
}


int main(int argc, char *argv[]){
    int listenfd, connfd;
    struct sockaddr_un server, client;
    char *message, input[2000];
    message = input;

    signal(SIGTSTP, SIG_IGN);

    //Create socket
    if((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1){
        makeLog(0, "could not create socket");
        return 0;
    }

    unlink(SOCK_PATH);
    server.sun_family = AF_LOCAL;
    strcpy(server.sun_path, SOCK_PATH);

    //Bind
    if(bind(sockfd, (struct sockaddr *)&server,SUN_LEN(&server))<0){
        makeLog(0, "bind failed");
        return 0;
    }

    //listen
    listen(sockfd, 10);

    //Check if database exists already
    ifstream file(DB_FILE);
    if(!file) createTables();

    //Open database
    int rc = sqlite3_open(DB_FILE, &db);
    if(rc){
      string error = "can't open database: "+string(sqlite3_errmsg(db));
      makeLog(0, error);
      exit(0);
    }
    else makeLog(0, "opened database successfully");

    fdarr[0].fd=sockfd;
    fdarr[0].events=POLLRDNORM;
    fdarr[1].fd=fileno(stdin);
    fdarr[1].events=POLLRDNORM;
    for(int i=2;i<100;i++) fdarr[i].fd=-1;
    int maxi=0;
    cout<<BLUE<<"Waiting for incoming connections..."<<endl;
    cout<<"Usage:\nbroadcast <message>\nsend <receiver_username> <message>\nkick <username> [reason_for_the_kick]\nban <username> [reason_for_the_ban]\nunban <username>\nlogout"<<RESET<<endl;
    int clilen = sizeof(struct sockaddr_un);

    for(;;){
        int nready=poll(fdarr ,maxi+2,-1000);
        int i;

        /// If listening socket is readable
        if(fdarr[0].revents & POLLRDNORM){
            if((connfd = accept(sockfd, (struct sockaddr *)&client, (socklen_t*)&clilen))<0){
                makeLog(0, "accept failed");
                return 0;
            }
            for(i=2;i<100;i++){
                if(fdarr[i].fd<0){
                    fdarr[i].fd=connfd;
                    break;
                }
            }
            if(i==100)
            {
                makeLog(0, "too many clients");
                sendString(connfd, "Server is busy. Please try later.");
                close(connfd);
            }
            else{
                fdarr[i].events=POLLRDNORM;
                maxi = max(maxi, i);
                if(--nready <= 0) continue;
            }
        }

        /// If terminal input is readable
        if(fdarr[1].revents & POLLRDNORM){
            string input;
            getline (cin,input);
            parseServerInput(input);
	    if(--nready <= 0) continue;
        }

        /// If client sockets are readable
        for(int i=2;i<=maxi;i++)
        {
            if((listenfd=fdarr[i].fd)<0) continue;
            if(fdarr[i].revents & (POLLRDNORM|POLLERR)){
                memset(message, '\0', 2000);
                int n=read(listenfd, message, 2000);
                if(n==0){
                    close(listenfd);
                    fdarr[i].fd=-1;

                    if(fdLookup.find(listenfd)!=fdLookup.end()){
                        string username=fdLookup[listenfd].first;
                        string entry = "[" +username +"] Terminated connection.";
                        makeLog(1, entry);
                        fdLookup.erase(listenfd);
                        lobby.erase(username);
                        onlineUsers--;
                        string response = lobbystatus();
                        broadcast(response);
                    }
                }
                else if(n<0){
                    if(errno==ECONNRESET)
                    {
                        close(listenfd);
                        fdarr[i].fd=-1;
                    }
                    else makeLog(0, "read error");
                }
                else{
                    parseRequest(listenfd, i, message);
                }
                if(--nready<=0) break;
            }
        }
    }
    return 0;
}
