/*
** Sam Kreter
** 11/1/16
** This is the client side to a socket chat application
**  complie the code and run ./client [IP Address] to start chatting
*/
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3491" // the port client will be connecting to

#define MAXDATASIZE 256 // max number of bytes we can get at once

using namespace std;

// get sockaddr, IPv4 or IPv6:
// Really nice function I found that looks really smart to other people
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]){
    //lots of declaration with the socket code
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char userInput[MAXDATASIZE];
    bool logout = false;

    if (argc != 2) {
        cerr << "usage: client hostname" << endl;
        exit(1);
    }

    memset(&hints, 0, sizeof hints);

    //Accpets both IPV4 and IPV6
    hints.ai_family = AF_UNSPEC;
    //Using the stream socket type
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " <<  gai_strerror(rv) << endl;
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        cerr << "client: failed to connect" << endl;
        return 2;
    }

    //Convert the binary address to string, Thanks to http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    // for the awsome function with all the bit operators, you da best
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    //cout << "client: connecting to " <<  s << endl;

    freeaddrinfo(servinfo); // all done with this structure
    cout << "My chat room client. Version One." << endl;

   while(1){
        //wait to recieve from the server
        bzero(buf,MAXDATASIZE);
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        //add the nul terminator to stop the string of data
        buf[numbytes] = '\0';
        if(numbytes > 0){
            //print out the message received from the serveer
            cout << "> " <<  buf << endl << "> ";
        }

        if(logout){
            break;
        }

        //get the users input everytime something is received from the server
        string sUserInput;
        getline(cin,sUserInput);

        //copy it to a cstring
        strcpy (userInput, sUserInput.c_str());

        //send the input to the server
        if (send(sockfd, userInput, MAXDATASIZE, 0) == -1)
            perror("send");

        if(sUserInput == string("logout")){
            logout = true;
        }
    }

    //close the socket
    close(sockfd);

    return 0;
}