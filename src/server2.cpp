/*
** Sam Kreter
** 11/1/16
** version 2
** This is the server side application for a multithreaded socket chat service
**  complie the code and run ./server2  to start chatting
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
#include <sys/poll.h>

//All the needed c++ structures to make my life way better
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <queue>



#define PORT "3491"  // the port users will be connecting to

// number of pending connection will be help, shouldn't ever hit this
#define BACKLOG 5
//max data sent over the socket
#define MAXDATASIZE 256
//Number of empty recived before we know connection was lost
#define RECIEVE_MAX 15
//Max number of clients that will be acccepted
#define MAX_NUM_THREADS 10


// Program error codes, basicly useless for this but thought i'd add them
#define USER_EXISTS -1
#define UP_NOT_IN_BOUNDS -2



using namespace std;


//structre to hold all the current users
using User = struct {
    string username;
    string password;
};

//Struct to hold the currently logged in users
using cUser = struct {
    string username;
    int id;
};


//prototypes for all the functions
int who(vector<cUser>& currUsers,int new_fd);
int newUser(string cmd, int socket_fd);
int login(string cmd, int socket_fd, string* currUser,vector<cUser>& currUsers, int id,bool* msgFlags, vector<string>& msgs);
vector<User> getUsers();
string* getCommand(int socket_fd, struct pollfd* pollData,bool* msgFlag, int id);
void *get_in_addr(struct sockaddr *sa);
void threadFunc(int id,int new_fd, bool* finished, bool* msgFlags, vector<string>& msgs,vector<cUser>& currUsers);
int sendMessage(string cmd, int new_fd,const string& currUser, vector<cUser>& currUsers, bool* msgFlags, vector<string>& msgs);


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



    //session based varibles to pass all the info around
    vector<cUser> currUsers;
    bool finished[MAX_NUM_THREADS];
    bool msgFlags[MAX_NUM_THREADS];
    vector<string> msgs(MAX_NUM_THREADS);
    vector<thread> threads(MAX_NUM_THREADS);
    queue<int> ordering;


    //initialize message queue and thread quit flags
    for(int i=0; i<MAX_NUM_THREADS; i++){
        ordering.push(i);
        finished[i] = false;
        msgFlags[i] = false;
    }

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

    //check if something went wrong
    if (p == NULL)  {
        cerr << "server: failed to bind" << endl;
        exit(1);
    }

    //start listening on the socket
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    //init message
    cout << "My chat room server. Version Two." << endl;




    while(1){

        //if at max clients what to accept until a connection was closed
        //stops people over max number of connections from connecting
        while(ordering.empty()){
            for(int i=0; i<MAX_NUM_THREADS; i++){
                if(finished[i]){
                    finished[i] = false;
                    msgFlags[i] = false;
                    threads.at(i).join();
                    ordering.push(i);
                }
            }
        }

        //get the accepted connection
        sin_size = sizeof their_addr;
        new_fd = 0;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        //check for errors
        if (new_fd == -1) {
            perror("accept");
        }
        else if(new_fd == 0){
            //cout << "testing";
            continue;
        }

        //turn the address from binary to family friendly G rated words
        //I don't use this here but I will definily later, this is really cool :)
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);


        //cout << "server: got connection from " <<  s << endl;

        //get the next threadid to spawn
        int nextIndex = ordering.front();
        ordering.pop();

        //spawn a thread for the new connection made
        threads.at(nextIndex) = thread(threadFunc,nextIndex,new_fd,
            ref(finished),ref(msgFlags),ref(msgs),ref(currUsers));


        //check if any clients have quite, reap the thread and free the index
        // make sure to set the flags back and add the index back to the queue
        for(int i=0; i<MAX_NUM_THREADS; i++){
            if(finished[i]){
                threads.at(i).join();
                finished[i] = false;
                msgFlags[i] = false;
                ordering.push(i);
            }
        }


    }

    //tity up everyting
    close(sockfd);

    return 0;
}




//Main driver function for the thread to deal with all the connections
void threadFunc(int id,int new_fd, bool* finished,
    bool* msgFlags, vector<string>& msgs, vector<cUser>& currUsers){

    //needed flags for user flow
    bool loginFlag = false;
    bool connectionLost = false;
    char buffer[MAXDATASIZE];
    string* cmd;
    string currUser;
    struct pollfd pollData[1];

    //set up the polling data for the sockets function
    pollData[0].fd = new_fd;
    pollData[0].events = POLLIN;


    while(1){
        //don't let anyone past unless they got that login
        while(!loginFlag){

            //Send login notice
            if (send(new_fd, "Use Command Login To Login:", 28, 0) == -1)
                perror("send");

            //get the users command
            cmd = getCommand(new_fd,pollData,msgFlags,id);

            //if the getCommand function returns null it means connection was lost
            if(cmd == NULL){
                cout << "connection with host lost" << endl;
                connectionLost = true;
                break;
            }

            //if they are all logged in set it all up
            if(cmd[0] == string("login") && login(cmd[1],new_fd,&currUser,currUsers,id, msgFlags, msgs)){
                //add the user to the current users list
                loginFlag = true;
            }
            else{
                send(new_fd, "Server: Denied. Please login first.", 40, 0);
            }
        }

        //make sure connection isn't lost during logins
        if(connectionLost){
            delete[] cmd;
            close(new_fd);
            return;
        }
        //start looping through recieving and sending messages
        if(loginFlag){
            string* cmd = getCommand(new_fd,pollData,msgFlags,id);

            //connection to the socket was lost
            if(cmd == NULL){
                cout << "Client connection lost" << endl;
                break;
            }


            //user flow to activate functions for each command
            if(cmd[0] == string("newuser")){
                newUser(cmd[1],new_fd);
            }
            else if(cmd[0] == string("msg")){
                if(send(new_fd,msgs[id].c_str(),MAXDATASIZE-1,0) == -1)
                    perror("send error");
                msgFlags[id] = false;
            }
            else if (cmd[0] == string("who")){
                who(currUsers,new_fd);
            }
            else if (cmd[0] == string("send")){
                sendMessage(cmd[1], new_fd, currUser, currUsers, msgFlags, msgs);
            }
            else if (cmd[0] == string("logout")){
                if (send(new_fd, "You are now loged out", 25, 0) == -1)
                    perror("send");

                loginFlag = false;

                cout << currUser + " logout." << endl;

                //remove the user from the list of current users
                currUsers.erase(remove_if(currUsers.begin(), currUsers.end(),[currUser](cUser u){
                    return (u.username == currUser);
                }));

                //broadcast the user has joined
                for(cUser user : currUsers){
                    msgs[user.id] = string(currUser + " left");
                    msgFlags[user.id] = true;
                }

                break;

            }
            //catch any other invalid input
            else {
                if(send((new_fd),"Invalid Command",20,0) == -1)
                    perror("send");
            }
        }
    }
    //final clean up
    delete[] cmd;
    close(new_fd);
}

//used to send a message with the username of the connected user back to themselves
// param cmd: the string of the input to the command from the user
// param new_fd: socket to be userd
// param currUser: string for the username of the current user
// param currUsers: list of the currently logged in usrs
int sendMessage(string cmd, int new_fd, const string& currUser, vector<cUser>& currUsers,
 bool* msgFlags, vector<string>& msgs){
    //find where the space is int he
    size_t spacePos = cmd.find(" ");
    //pull out the username to send the message to
    string username = cmd.substr(0,spacePos);
    // pull out th message from the command string
    string msg = cmd.substr(spacePos+1);


    //handle the boadcast case
    if(username == string("all")){
        msg = currUser + ": " + msg;

        cout << msg << endl;

        //set all loged in users message flags and add the message to their queue
        for(cUser user : currUsers){
            if(user.username != currUser){
                msgs[user.id] = msg;
                msgFlags[user.id] = true;
            }
        }
        return 1;
    }

    // check if the message is being sent to a logged in user
    auto it = find_if(currUsers.begin(), currUsers.end(),[username](cUser u){
        return (u.username == username);
    });

    //if the user is logged in set the flag and add the message to their queue
    if(it != currUsers.end()){

        cout << currUser + " (to " + username + "): " + msg << endl;

        msg = currUser + ": " + msg;
        msgs[it->id] = msg;
        msgFlags[it->id] = true;

        return 1;
    }

    //send and error if the user is not logged in
    if(send(new_fd, "Invalid UserID to send message", 32, 0) == -1)
        perror("Another send error bro");

    return 0;
}

//add a new user to the users.txt file
int newUser(string cmd, int socket_fd){

    //parse the username and password from the command string
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


// return a list of all loged in users
int who(vector<cUser>& currUsers,int new_fd){
    string names;

    // add all the currently logged in user's names to the string
    for(cUser user : currUsers){
        names += (" " + user.username);
    }

    // make sure its not to long
    if(names.length() > MAXDATASIZE - 1){
        cerr << "We in some trouble with this";
        return 0;
    }

    //send it back to the client
    if (send(new_fd, names.c_str(), MAXDATASIZE - 1, 0) == -1)
        perror("send");

    return 1;

}

//validates the user's username and password for login status
int login(string cmd, int socket_fd, string* currUser,vector<cUser>& currUsers,
    int id, bool* msgFlags, vector<string>& msgs){

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
        auto currUsersIt = find_if(currUsers.begin(),currUsers.end(),[username](cUser u){
            return (u.username == username);
        });

        //check if the user is already logged in
        if(currUsersIt == currUsers.end()){

            if(send(socket_fd, "Login Confirmed",20,0) == -1)
                perror("send");

            //broadcast the user has joined
            for(cUser user : currUsers){
                msgs[user.id] = string(it->username + " has joined");
                msgFlags[user.id] = true;
            }

            cout << it->username + " Login." << endl;

            //add evertyhing to the currently logged in users list
            *currUser = it->username;
            currUsers.push_back(cUser());
            currUsers.back().username = it->username;
            currUsers.back().id = id;

            return 1;
        }
        else{
            if(send(socket_fd, "User is already logged in",30,0) == -1)
                perror("user already logged in");
        }

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
string* getCommand(int socket_fd, struct pollfd* pollData, bool* msgFlags, int id){

    char command[MAXDATASIZE];
    string cmdString;
    int numbytes = 0,rv = -1;
    string* returns = new string[2];


    //keep count of number of empty data recieved,
    // if more than max number was recieved, we know that the client has
    //  been disconnected
    size_t count = 0;

    while(cmdString.length() == 0){

        //check if there was something received on the socket,
        // if nothing received within .5 seconds then check if a message is
        // in the message queue
        rv = poll(pollData, 1, 500);


        if(rv == -1){
            perror("Error wiht polling");
        }
        // check if there was a time out, then check the message queue
        else if(rv == 0){
            if(msgFlags[id]){
                //propogate up that a message from another users has been receieved
                returns[0] = string("msg");
                return returns;
            }
        }
        // something was received on the socket connection
        else{
            //zero out the command just in case
            bzero(command,MAXDATASIZE);

            //get the message
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