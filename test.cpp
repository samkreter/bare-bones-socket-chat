#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

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

int main () {
    vector<User> t = test();
    string name("To");
    auto it = find_if(t.begin(),t.end(),[name](User u){
        return (u.username == name);
    });
    if(it == t.end()){
        cout << "big dog";
    }
    cout << (*it).username;

    return 0;
}