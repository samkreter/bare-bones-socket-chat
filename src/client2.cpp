/*
** Sam Kreter
** 11/1/16
** version 2
** This is the client side to a socket chat application with multiple conncetions
**  complie the code and run ./client2 [IP Address] to start chatting
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
#include <sys/poll.h> //the library that saved me from the blocking recv function

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
    struct pollfd pollData[2];


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
        //connect to that socket
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
    cout << "My chat room server. Version Two." << endl;




    //set everything up for stdin and sockect recv polling
    pollData[0].fd = sockfd;
    pollData[0].events = POLLIN;
    pollData[1].fd = fileno(stdin);
    pollData[1].events = POLLIN;


    while(1){

        //TODO finish polling
        rv = poll(pollData, 2, 500);

        if(rv == -1){
            perror("Polling error");
        }
        //check if there was some data recieved on stdin or the socket
        else if (rv != 0){
            //check if it was received on the socket
            if (pollData[0].revents & POLLIN) {
                bzero(buf,MAXDATASIZE);
                if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }

                //add the nul terminator to stop the string of data
                buf[numbytes] = '\0';

                //if recieved actual data print it otherwise ignore it
                if(numbytes > 0){
                    //print out the message received from the serveer
                    cout << buf << endl << "> ";
                    //the last > gets caught if you don't flush standard out
                    cout.flush();
                }
            }
            else{
                // check if the user is typing on the command line
                string sUserInput;
                getline(cin,sUserInput);

                //copy it to a cstring
                strcpy (userInput, sUserInput.c_str());

                //send the input to the server
                if (send(sockfd, userInput, MAXDATASIZE, 0) == -1)
                    perror("send");
                //check if the user wants to log out
                if(sUserInput == string("logout")){
                    break;
                }
                //use this to fixed a missing > for the output
                else if(sUserInput.find("send") !=  string::npos){
                    cout << "> ";
                    cout.flush();
                }
            }
        }

    }

    //close the socket
    close(sockfd);

    return 0;
}

//http://beej.us/guide/bgnet/output/html/multipage/pollman.html