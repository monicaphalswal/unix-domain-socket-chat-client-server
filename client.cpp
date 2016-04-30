#include<stdio.h>
#include<sys/socket.h>
#include<iostream>
#include<stdlib.h>    //strlen
#include<arpa/inet.h>//inet_addr
#include<string.h>
#include<unistd.h> //for close(), write
#include<sys/un.h>
#include<bits/stdc++.h>
#include<poll.h>

#define SOCK_PATH "chat_socket"
#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define BLUE    "\033[34m"      /* Blue */

using namespace std;

int authenticated=0;
string usernameSelf;

void sendString(int fd, const char *message){
    if(write(fd, message, strlen(message)) == -1) {
        perror("send");
        exit(1);
    }
}

void readString(int fd, char message[]){
    memset(message, '\0', 2000);
    int n=read(fd, message, 2000);
    if(n<=0){
        cout<<RED<<"Server is offline."<<RESET<<endl;
        exit(1);
    }
    cout<<GREEN<<message<<RESET<<endl;
}

void authenticateUser(int fd){
    char server_reply[2000];
    string input;
    getline (cin,input);
    istringstream iss(input);
    vector<string> token;
    do{
        string sub;
        iss >> sub;
        token.push_back(sub);
    }while(iss);
    token.pop_back();
    if((token[0]!="register" && token[0]!="login") || token.size()!=3){
        cout<<BLUE<<"Command not found.\nUsage:\nregister <username> <password>\nlogin <username> <password>"<<RESET<<endl;
    }
    else{
        sendString(fd, input.c_str());
        readString(fd, server_reply);
        if(strstr(server_reply, "Successfully logged in.")){
            usernameSelf = token[1];
            authenticated=1;
	    cout<<BLUE<<"USAGE:\nlobbystatus\nsend <receiver_username> <message>\nlogout"<<RESET<<endl;
        }
    }
}

void inputNSend(int fd){
    char server_reply[2000];
    string input;
    getline (cin,input);
    istringstream iss(input);
    vector<string> token;
    do{
        string sub;
        iss >> sub;
        token.push_back(sub);
    }while(iss);
    token.pop_back();
    if(token[0]=="logout"){
        if(token.size()>1)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nlogout"<<RESET<<endl;
        else{
            sendString(fd, input.c_str());
            readString(fd, server_reply);
            exit(1);
        }
    }
    else if(token[0]=="lobbystatus"){
        if(token.size()>1)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nlobbystatus"<<RESET<<endl;
        else{
            sendString(fd, input.c_str());
            readString(fd, server_reply);
        }
    }
    else if(token[0]=="send"){
        if(token.size()<3)
            cout<<BLUE<<"Invalid arguments.\nUsage:\nsend <receiver_username> <message>"<<RESET<<endl;
        else{
            if(token[1]==usernameSelf) cout<<BLUE<<"Sending a message to self? ¯\\_(ツ)_/¯"<<RESET<<endl;
            else sendString(fd, input.c_str());
        }
    }
    else{
        cout<<BLUE<<"Command not found.\nUsage:\nlobbystatus\nsend <receiver_username> <message>\nlogout"<<RESET<<endl;
    }
}

int main(int argc, char *argv[]){
    int sockfd;
    struct sockaddr_un server;
    struct pollfd fdarr[2];
    char *message, server_reply[2000], input[2000];
    message = input;

    signal(SIGTSTP, SIG_IGN);

    //Create socket
    if((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1){
        perror("could not create socket");
        return 0;
    }

    server.sun_family = AF_LOCAL;
    strcpy(server.sun_path,SOCK_PATH);

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(server))<0)
    {
        perror("connect error");
        return 0;
    }

    cout<<BLUE<<"Connected to the server.\nUsage:\nregister <username> <password>\nlogin <username> <password>"<<RESET<<endl;
    fdarr[0].fd=sockfd;
    fdarr[0].events=POLLRDNORM;

    fdarr[1].fd=fileno(stdin);
    fdarr[1].events=POLLRDNORM;

    for ( ; ; ) {
    	int nready=poll(fdarr,2,-1000);
    	if(fdarr[0].revents & (POLLRDNORM|POLLERR)){ // If server socket is readable
            readString(sockfd, server_reply);
		}
    	if(fdarr[1].revents & (POLLRDNORM|POLLERR)){ // If terminal input is readable
            if(!authenticated) authenticateUser(sockfd);
            else inputNSend(sockfd);
        }
	}
    return 0;
}
