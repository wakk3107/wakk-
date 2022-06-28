#include<iostream>
#include"myallocator.h"
#include<vector>
using namespace std;

int main(){
    vector<string,myallocator<string>> wakk;
    for (int i = 0; i < 100;i++){
        wakk.push_back("w123123");

    }
    for(auto i:wakk){
        cout << i << endl;
    }
        return 0;
}