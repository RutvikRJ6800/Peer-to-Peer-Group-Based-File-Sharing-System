#include <stdio.h>
#include <fstream>
#include <string>
using namespace std;
//*********************
//Logger class
//*********************
class Logger{
    public:

    static void Info(string s){
        ofstream log_file("logFile.txt", ios_base::out | ios_base::app );
		string text = "[INFO] "+s;
        log_file << text << endl;
	}

    static void Warn(string s){
        ofstream log_file("logFile.txt", ios_base::out | ios_base::app );
        string text = "[WARN] "+s;
        log_file << text << endl;
    }
    static void Error(string s){
        ofstream log_file("logFile.txt", ios_base::out | ios_base::app );
        string text = "[ERROR] "+s;
        log_file << text << endl;
    }


};
