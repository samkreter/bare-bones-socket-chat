#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <thread>
#include <atomic>



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
    atomic<bool> flag(false);
    thread([&]
    {
        this_thread::sleep_for(chrono::seconds(5));

        if (!flag)
            terminate();
    }).detach();

    string s;
    getline(std::cin, s);
    flag = true;
    cout << s << '\n';

    return 0;
}