/*
** server.c -- a stream socket server demo
*/

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <vector>
#include <fstream>
#include <string>



#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 5 // how many pending connections queue will hold
#define MAXDATASIZE 256

using namespace std;



// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int login(int socket_fd);
vector<string> getUsers();

int main(){

    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buffer[MAXDATASIZE];

    //zero out the address space of the strucutre for safty
    memset(&hints, 0, sizeof hints);

    // like client this allows for IPV4 and IPV6
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //use same ip address as return address
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " <<  gai_strerror(rv) << endl;
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        cerr << "server: failed to bind" << endl;
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


    cout << ("server: waiting for connections...") << endl;

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        cout << "server: got connection from " <<  s << endl;

        login(new_fd);
        close(sockfd);
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}

vector<string> getUsers(){
    string line;
    vector<string> users;
    ifstream myfile ("users.txt");
    if(myfile.is_open()){

        while(getline(myfile,line)){
            users.push_back(line);
        }
        myfile.close();
    }

    return users;
}


int login(int socket_fd){
    char username[MAXDATASIZE];
    char password[MAXDATASIZE];
    int numbytes = 0;

    vector<string> users = getUsers();

    //Get username
    if (send(socket_fd, "Enter Username", 20, 0) == -1)
        perror("send");


    bzero(username,40);
    if ((numbytes = recv(socket_fd, username, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    username[numbytes] = '\0';

    //Get password
    numbytes = 0;
    if (send(socket_fd, "Enter Password", 20, 0) == -1)
        perror("send1");


    bzero(password,40);
    if ((numbytes = recv(socket_fd, password, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        return 0;
    }

    password[numbytes] = '\0';


    cout << "Username: " << username << endl;
    cout << "Password: " << password << endl;


    for(auto user : users){
        if(user.find(string(username))!= std::string::npos && user.find(string(password)) != std::string::npos){
            if(send(socket_fd, "Login Success",20,0) == -1)
                perror("send");
            return 1;
        }
    }

    if(send(socket_fd, "Invalid Username or Password",40,0) == -1)
        perror("send");

    return 0;

}


// http://beej.us/guide/bgnet/output/html/multipage/clientserver.html