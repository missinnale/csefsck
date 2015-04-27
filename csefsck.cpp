#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <map>
using namespace std;

string FILELOCATION = "C:/filesystem/fusedata.";

vector<string> Parse(string value, char delimiter, bool concatenateWhitespace = true){
	if (value[0] == '{'){ value = value.substr(1, value.length() - 2); }
	vector<string> tokens;
	int lastTokenLocation = 0;
	for (int i = 0; i < value.size(); ++i){
		if (value[i] == delimiter || i == value.size() - 1){
			if (value[lastTokenLocation] == ' '){ 
				++lastTokenLocation;
			}
			string substring;
			if (i == value.size() - 1){
				substring = value.substr(lastTokenLocation);
			}
			else{
				substring = value.substr(lastTokenLocation, i - lastTokenLocation);
			}
			if (concatenateWhitespace){
				for (int j = 0; j < substring.size(); ++j){
					if (substring[j] == ' '){
						substring = substring.substr(0, j) + substring.substr(j + 1);
					}
				}
			}
			tokens.push_back(substring);
			lastTokenLocation = i + 1;
		}
	}
	return tokens;
}

bool checkDevice(){
	fstream file(FILELOCATION + "0");
	if (!file.is_open()){
		return false;
	}
	stringstream stream;
	stream << file.rdbuf();
	string readFile = stream.str();
	vector<string> fields = Parse(readFile, ',');
	for each (string field in fields)
	{
		vector<string> fieldPair = Parse(field, ':', false);
		if (fieldPair[0] != "devId"){ continue; }
		if (fieldPair[1] == "20"){ return true; }
		cout << "The devId field is incorrect." << endl;
		return false;
	}
	file.close();
	return false;
}

bool checkTimes(string fileInfo){
	vector<string> fields = Parse(fileInfo, ',');
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ':', false);
		if (fieldPair[0] != "atime" && fieldPair[0] != "ctime" && fieldPair[0] != "mtime" && fieldPair[0] != "creationTime"){ continue; }
		if (stoi(fieldPair[1]) < time(0)){ continue; }
		cout << "The " + fieldPair[0] + " is incorrect" << endl;
		return false;
	}
	return true;
}

bool checkLinkCount(string fileInfo){
	vector<string> dictBreak = Parse(fileInfo, '{', false);
	if (dictBreak.size() <= 1){ return true; }
	vector<string> dictFields = Parse(dictBreak[1].substr(0, dictBreak[1].size() - 1), ',');
	int actualCount = dictFields.size();

	vector<string> fields = Parse(fileInfo, ',');
	int givenCount = 0;
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ':');
		if (fieldPair[0] != "linkcount"){ continue; }
		givenCount = stoi(fieldPair[1]);
		if (givenCount == actualCount){ return true; }
		else { 
			cout << "The linkcount of " + to_string(givenCount) + " is incorrect, it should be " + to_string(actualCount) << endl;
			return false; 
		}
	}
	cout << "could not find a linkcount field in the file" << endl;
	return false;
}

bool checkIndirect(string fileInfo, int &indirect, vector<string> &locations){
	
	vector<string> locationBreak = Parse(fileInfo, '{', false);

	vector<string> fields = Parse(fileInfo, ',');
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ':');
		if (fieldPair[0] != "indirect"){ continue; }
		string num = fieldPair[1].substr(0, fieldPair[1].find("location"));
		indirect = stoi(num);
		if (indirect == 0 && locationBreak.size() <= 1){
			return true;
		}
		else if(indirect == 1 && locationBreak.size() > 1){ 
			locations = Parse(locationBreak[1].substr(0, locationBreak[1].size() - 1 ), ',');
			return true; 
		}
		else {
			cout << "The indirect of " + to_string(indirect) + " stored on the file does not match up with the location(s)" << endl;
			return false; 
		}
	}
	return false;
}

bool checkSize(string fileInfo, int &currentIndirect, vector<string> &currentLocations){
	vector<string> fields = Parse(fileInfo, ',');
	int size = 0;
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ':');
		if (fieldPair[0] != "size"){ continue; }
		size = stoi(fieldPair[1]);
		if (size <= 0){ return false; }
		if (currentIndirect == 0 && size <= 4096){
			return true;
		}
		else if (currentIndirect != 0 && size < 4096 * currentLocations.size() && size > 4096 * (currentLocations.size() - 1)){
			return true;
		}
		else{
			cout << "The size of " + to_string(size) + " is listed incorrectly based on the location(s) and the indirect" << endl;
			return false;
		}
	}
	return false;
}

bool checkChildDirectory(string fileNum, int currentFileNum){
	ifstream file(FILELOCATION + fileNum);
	if (!file.is_open()){
		return false;
	}
	stringstream stream;
	stream << file.rdbuf();
	string readFile = stream.str();
	file.close();
	vector<string> inodeDictionary = Parse(readFile, '{', false);
	if (inodeDictionary.size() <= 1){
		cout << "The parent directory is not a directory" << endl;
		return false;
	}
	int pos = inodeDictionary[1].find(to_string(currentFileNum));
	if (pos == -1){
		cout << "The parent directory listing (d:..) of fusedata." + to_string(currentFileNum ) + " is incorrect." << endl;
		return false;
	}
	return true;
}

bool checkDirectory(string fileInfo, int currentFileNum){
	//TODO: Parse down to the inode dictionary and delimit the : so that 3 fields are contianed in the vector
	//		loop through the entire dictionary until a . and .. are found then return true (two functions?)
	//		Take number from . check if matches current filename and .. open that filename check if current filename exists in it
	vector<string> directoryContainer = Parse(fileInfo, '{');
	if (directoryContainer.size() <= 1){ return true; }
	vector<string> inodeDirectory = Parse(directoryContainer[1].substr(0, directoryContainer[1].size() - 1), ',');
	for each (string field in inodeDirectory){
		vector<string> fieldValues = Parse(field, ':');
		if (fieldValues[0] != "d"){ continue; }
		if (fieldValues[1] == "."){
			if (stoi(fieldValues[2]) == currentFileNum){ continue; }
			cout << "The current directory listing (d:.) of fusedata." + to_string(currentFileNum ) + " is incorrect." << endl;
		}
		if (fieldValues[1] == ".."){
			return checkChildDirectory(fieldValues[2], currentFileNum);
		}
	}
	return false;
}

bool checkFreeBlock(string fileInfo, int currentFileNum, vector<int> &freeblockList, map<string,string> &fileSysFreeBlocks){
	//TODO: Overwrite without check, open block and check for data
	//		if no data store in a vector,compare against current free block list for distorted information
	if (!fileInfo.empty()){ return true; }
	freeblockList.push_back(currentFileNum);
	for (int i = 1; i < 26; ++i){
		ifstream file(FILELOCATION + to_string(i));
		if (!file.is_open()){
			continue;
		}
		stringstream stream;
		stream << file.rdbuf();
		string readFile = stream.str();
		file.close();

		if (!readFile.empty()){ continue; }

		vector<string> freeBlocks = Parse(readFile, ',');
		string currentFreeBlock = to_string(currentFileNum);
		for each(string block in freeBlocks){
			fileSysFreeBlocks[block] = block;
			if (block != currentFreeBlock){ continue; }
			return true;
		}
	}
	cout << "The freeblock fusedata." + to_string(currentFileNum) + " is not contained in the Free Block List.";
	return false;
}

bool checkFreeBlockList(map<string, string> &fileSysFreeBlocks){
	for each(pair<string,string> block in fileSysFreeBlocks){
		ifstream file(FILELOCATION + block.first);
		if (!file.is_open()){
			return false;
		}
		stringstream stream;
		stream << file.rdbuf();
		string readFile = stream.str();
		file.close();

		if (!readFile.empty()){
			cout << "The filesystem contains a file (fusedata." + block.first + ") in the free block list that is not empty" << endl;
		}
	}
	return true;
}

int main(){
	//TODO: Open file, if empty close it move on, create "global variables" to pass into each function for storage
	//		store file into string, pass string into functions,

	checkDevice();
	vector<int> freeBlockList;
	map<string, string> fileSysFreeBlocks;
	for (int i = 25; i < 31; ++i){
		cout << "working on file fusedata." + to_string(i) << endl;
		ifstream file(FILELOCATION + to_string(i));
		if (!file.is_open()){
			cout << "could not open file" << endl;
			return false;
		}
		stringstream stream;
		stream << file.rdbuf();
		string readFile = stream.str();
		file.close();

		int indirect;
		vector<string> locations;

		checkTimes(readFile);
		checkFreeBlock(readFile, i, freeBlockList, fileSysFreeBlocks);
		checkIndirect(readFile, indirect, locations);
		checkSize(readFile, indirect, locations);
		checkLinkCount(readFile);
		checkDirectory(readFile, i);
		cout << endl;
	}
	checkFreeBlockList(fileSysFreeBlocks);
	string input;
	cin >> input;
}