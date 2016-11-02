#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <thread>
#include <atomic>
#include <sys/poll.h>



using namespace std;





using User = struct {
    string username;
    string password;
};

vector<User> test(){
    string line;
    vector<User> users;
    ifstream myfile ("users.txt");
    if(myfile.is_open()){
        while(getline(myfile,line)){

            int commaPos = line.find(",");

            users.push_back(User());
            users.back().username = line.substr(1,commaPos-1);
            users.back().password = line.substr(commaPos+1,line.length() - commaPos - 2);
        }
        myfile.close();
    }


    return users;
}


int main(){
    struct pollfd pollData[1];

    int rv = -1;
    string name;
    pollData[0].fd = fileno(stdin);
    pollData[0].events = POLLIN;


    while(1){
        rv = poll(pollData, 1, 1000);

        if(rv == 0){
            cout << "nothing yet" << endl;
        }
        else{
            cin >> name;
            cout << "we got " << name << endl;
        }
    }




    return 0;
}