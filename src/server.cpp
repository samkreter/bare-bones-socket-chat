/*
** Sam Kreter
** 11/1/16
** This is the server for a chat application. One client will connect to the
**  server interact over sockets
*/

#include <iostream>
#include <string>
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
#include <algorithm>



#define PORT "3491"  // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold
#define MAXDATASIZE 256
#define RECIEVE_MAX 15 //


// Program error codes, basicly useless for this but thought i'd add them
#define USER_EXISTS -1
#define UP_NOT_IN_BOUNDS -2



using namespace std;


//structre to hold all the current users
using User = struct {
    string username;
    string password;
};


//prototypes for all the functions
int newUser(string cmd, int socket_fd);
int login(string cmd, int socket_fd, string* currUser);
int sendMessage(string cmd, int sock_fd, const string& currUser);
vector<User> getUsers();
string* getCommand(int sock_fd);
void *get_in_addr(struct sockaddr *sa);


int main(){


    //the thousand of varibles needed to run these sockets
    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buffer[MAXDATASIZE];
    string* cmd;
    string currUser;

    //zero out the address space of the strucutre for safty
    memset(&hints, 0, sizeof hints);

    // like client this allows for IPV4 and IPV6
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //use same ip address as return address
    hints.ai_flags = AI_PASSIVE;

    //get the info to create the socket
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

        //bind to the socket address
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

    //start listening on the socket
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


    cout << ("My chat room server. Version One. ") << endl;

    //needed flags for user flow
    bool loginFlag = false;
    bool acceptingNew = true;


    while(1){
        loginFlag = false;
        //accept a new connection
        if(acceptingNew){
            sin_size = sizeof their_addr;
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

            if (new_fd == -1) {
                perror("accept");
            }

            //turn the address from binary to family friendly G rated words
            inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);

            //cout << "server: got connection from " <<  s << endl;
            acceptingNew = false;
        }


        //Send login notice
        if (send(new_fd, "Use Command Login To Login:", 28, 0) == -1)
            perror("send");
        while(1){
            //don't let anyone past unless they got that login
            while(!loginFlag){


                cmd = getCommand(new_fd);

                //if the getCommand function returns null it means connection was lost
                if(cmd == NULL){
                    acceptingNew = true;
                    break;
                }

                //if they are all logged in set it all up
                if(cmd[0] == string("login") && login(cmd[1],new_fd,&currUser)){
                    loginFlag = true;
                }
                else{
                    send(new_fd, "Server: Denied. Please login first.", 40, 0);
                }
            }

            //start looping through recieving and sending messages
            if(loginFlag){
                string* cmd = getCommand(new_fd);

                //connection to the socket was lost
                if(cmd == NULL){
                    acceptingNew = true;
                    break;
                }


                //user flow to activate functions for each command
                if(cmd[0] == string("newuser")){
                    newUser(cmd[1],new_fd);
                }
                else if (cmd[0] == string("send")){
                    sendMessage(cmd[1],new_fd,currUser);
                }
                else if (cmd[0] == string("logout")){
                    if (send(new_fd, "Server: Tom left.", 20, 0) == -1)
                        perror("send");

                    cout << currUser << " logout" << endl;
                    loginFlag = false;
                    acceptingNew = true;
                    break;
                }
                else {
                    if(send((new_fd),"Invalid Command",20,0) == -1)
                        perror("send");
                    continue;
                }
            }
        }
    }

    //tity up everyting
    delete[] cmd;
    close(sockfd);
    close(new_fd);

    return 0;
}

//used to send a message with the username of the connected user back to themselves
int sendMessage(string cmd, int socket_fd, const string& currUser){

    cout << currUser + ": " + cmd << endl;
    if(send(socket_fd,(currUser + ": " + cmd).c_str(), MAXDATASIZE -1, 0) == -1)
        perror("send");

    return 1;
}

//add a new user to the users.txt file
int newUser(string cmd, int socket_fd){

    //splits everything up
    size_t spacePos = cmd.find(" ");
    string username = cmd.substr(0,spacePos);
    string password = cmd.substr(spacePos+1);


    //open the user file
    ofstream usersFile("../users.txt", std::ios_base::app);

    //make sure the file is open
    if(!usersFile.is_open()){
        cerr << "Could not open user.txt file" << endl;
        return -1;
    }

    //get the list of current users
    vector<User> users = getUsers();

    //check that everything is gucci
    if(username.length() < 32 && password.length() > 3 && password.length() < 9){

        //search the currently added users
        auto it = find_if(users.begin(),users.end(),[username](User u){
            return (u.username == username);
        });

        //if it was not found, add the user
        if(it == users.end()){

            //add the new user to the file
            usersFile << endl << "(" << username << ", " << password << ")";

            //send confirmation to client that everythings good
            if(send(socket_fd,"User successfully added!", 30, 0) == -1)
                perror("send");

            //server note
            cout << "User "<< username << " successfully added" << endl;
            return 1;
        }


        //the user already exists
        if(send(socket_fd, "User already Exists",25,0) == -1)
            perror("send");
        cout << "User " << username << " already exists" << endl;
        //return the error
        return USER_EXISTS;

    }

    if(send(socket_fd, "User/password not in bounds",30,0) == -1)
            perror("send");

    cout << "User " << username << "out of bounds" << endl;
    //return the specific error code
    return UP_NOT_IN_BOUNDS;

}


//validates the user's username and password for login status
int login(string cmd, int socket_fd, string* currUser){

    string username;
    string password;
    char command[MAXDATASIZE];
    int numbytes = 0;

    //get the current user's usernames and passwords from the userfile
    vector<User> users = getUsers();

    //parse the command string given to the function
    //if no sername or password this step will catch that and not error out
    size_t spacePos = cmd.find(" ");
    username = cmd.substr(0,spacePos);
    password = cmd.substr(spacePos+1);


    //search through all the users and check for usersname and password
    auto it = find_if(users.begin(),users.end(),[username,password](User u){
        return (u.username == username && u.password == password);
    });

    //if the find function found a match, then add the users name and
    // send confirmation
    if(it != users.end()){
        if(send(socket_fd, ("Server: "+ it->username+" joins").c_str(),MAXDATASIZE - 1,0) == -1)
            perror("send");
        *currUser = it->username;
        cout << *currUser << " login." << endl;
        return 1;

    }

    //otherwise error
    if(send(socket_fd, "Invalid Username or Password",40,0) == -1)
        perror("send");
    cout << "Invalid username or password" << endl;

    return 0;

}

// get sockaddr, IPv4 or IPv6, this is an amazing function I found that works great:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//used to parse input from the users and parsing out the current command
string* getCommand(int socket_fd){

    char command[MAXDATASIZE];
    string cmdString;
    int numbytes = 0;
    string* returns = new string[2];

    //keep count of number of empty data recieved,
    // if more than max number was recieved, we know that the client has
    //  been disconnected
    size_t count = 0;

    while(cmdString.length() == 0){
        //zero out the command just in case
        bzero(command,MAXDATASIZE);

        if ((numbytes = recv(socket_fd, command, MAXDATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        //add null terminator to the received string
        command[numbytes] = '\0';

        //convert to c++ string
        cmdString = string(command);

        //check for the client disconnect
        count++;
        if(count > RECIEVE_MAX)
            return NULL;
    }

    //finish parsing input and return
    size_t spacePos = cmdString.find(" ");
    returns[0] = cmdString.substr(0,spacePos);
    returns[1] = cmdString.substr(spacePos + 1);


    return returns;

}


//helper function to parse username and password from file and return in struct
vector<User> getUsers(){
    string line;
    vector<User> users;
    ifstream myfile ("../users.txt");

    if(myfile.is_open()){
        while(getline(myfile,line)){

            //get pos of comma
            int commaPos = line.find(",");

            users.push_back(User());
            //username if everything before comma and after (
            users.back().username = line.substr(1,commaPos-1);
            //password is evertyhign after the comma and space and before the )
            users.back().password = line.substr(commaPos+2,line.length() - commaPos - 3);
        }
        myfile.close();
    }
    else{
        cerr << "couldn't open file";
        exit(-1);
    }


    return users;
}


// http://beej.us/guide/bgnet/output/html/multipage/clientserver.html