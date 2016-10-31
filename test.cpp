#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;


vector<string> test(){
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

int main () {
    vector<string> t = test();

    for(int i =0; i < t.size(); i++){
        cout << t.at(i);
    }

    return 0;
}