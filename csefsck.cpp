#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <map>
using namespace std;


vector<string> Parse(string value, char delimiter, bool concatenateWhitespace = true){
	if (value[0] == '{'){ value = value.substr(1, value.length() - 2); }
	vector<string> tokens;
	int lastTokenLocation = 0;
	for (int i = 0; i < value.size(); ++i){
		if (value[i] == delimiter){
			if (value[lastTokenLocation] == ' '){ 
				++lastTokenLocation;
				continue;
			}
			string substring = value.substr(lastTokenLocation, i - 1);
			if (concatenateWhitespace){
				for (int j = 0; j < substring.size(); ++j){
					if (substring[j] == ' '){
						substring = substring.substr(0, j - 1) + substring.substr(j + 1);
						break;
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
	ifstream file("fusedata.0");
	if (!file.is_open()){
		return false;
	}
	stringstream stream;
	stream << file.rdbuf();
	string readFile = stream.str();
	vector<string> fields = Parse(readFile, ',');
	for each (string field in fields)
	{
		vector<string> fieldPair = Parse(field, ' ');
		if (fieldPair[0] != "devId:"){ continue; }
		if (fieldPair[1] == "20"){ continue; }
		cout << "The devId field is incorrect." << endl;
		return false;
	}
	file.close();
	return true;
}

bool checkTimes(string fileInfo){
	vector<string> fields = Parse(fileInfo, ',');
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ' ');
		if (fieldPair[0] != "atime:" || fieldPair[0] != "ctime:" || fieldPair[0] != "mtime:"){ continue; }
		if (stoi(fieldPair[1]) < time(0)){ continue; }
		cout << "The " + fieldPair[0] + " is incorrect" << endl;
		return false;
	}
	return true;
}

bool checkLinkCount(string fileInfo){
	vector<string> dictBreak = Parse(fileInfo, '{');
	vector<string> dictFields = Parse(dictBreak[1].substr(0, dictBreak[1].size() - 1), ',');
	int actualCount = dictFields.size();

	vector<string> fields = Parse(fileInfo, ',');
	int givenCount = 0;
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ' ');
		if (fieldPair[0] != "linkcount:"){ continue; }
		givenCount = stoi(fieldPair[1]);
		if (givenCount == actualCount){ return true; }
		else { return false; }
	}
	return false;
}

bool checkIndirect(string fileInfo, int &indirect, vector<string> &locations){
	
	vector<string> locationBreak = Parse(fileInfo, '{');

	vector<string> fields = Parse(fileInfo, ',');
	for each(string field in fields){
		vector<string> fieldPair = Parse(field, ':');
		if (fieldPair[0] != "indirect"){ continue; }
		indirect = stoi(fieldPair[1]);
		if (indirect == 0 && locationBreak.empty() || indirect == 1 && !locationBreak.empty()){ 
			locations = Parse(locationBreak[1].substr(0, locationBreak[1].size() - 1 ), ',');
			return true; 
		}
		else { return false; }
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
			return false;
		}
	}
	return false;
}

bool checkDirectory(string fileInfo, int currentFileNum){
	//TODO: Parse down to the inode dictionary and delimit the : so that 3 fields are contianed in the vector
	//		loop through the entire dictionary until a . and .. are found then return true (two functions?)
	//		Take number from . check if matches current filename and .. open that filename check if current filename exists in it
	vector<string> directoryContainer = Parse(fileInfo, '{');
	vector<string> inodeDirectory = Parse(directoryContainer[1].substr(0, directoryContainer[1].size() - 1), ',');
	for each (string field in inodeDirectory){
		vector<string> fieldValues = Parse(field, ':');
		if (fieldValues[1] != "d"){ continue; }
		if (fieldValues[2] == "."){
			if (stoi(fieldValues[3]) == currentFileNum){ continue; }
			cout << "The current directory listing (d:.) of fusedata." + currentFileNum << " is incorrect." << endl;
		}
		if (fieldValues[2] == ".."){}
	}
}

bool checkChildDirectory(string fileNum, int currentFileNum){
	ifstream file("fusedata." + fileNum);
	if (!file.is_open()){
		return false;
	}
	stringstream stream;
	stream << file.rdbuf();
	string readFile = stream.str();
	file.close();
	vector<string> inodeDictionary = Parse(readFile, '{', false);
	int pos = inodeDictionary[1].find(to_string(currentFileNum));
	if (pos == -1){
		cout << "The parent directory listing (d:..) of fusedata." + currentFileNum << " is incorrect." << endl;
	}
}

bool checkFreeBlock(string fileInfo, int currentFileNum, vector<int> &freeblockList, map<string,string> &fileSysFreeBlocks){
	//TODO: Overwrite without check, open block and check for data
	//		if no data store in a vector,compare against current free block list for distorted information
	if (!fileInfo.empty()){ return true; }
	freeblockList.push_back(currentFileNum);
	for (int i = 1; i < 26; ++i){
		ifstream file("fusedata." + i);
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
	cout << "The freeblock fusedata." + currentFileNum << " is not contained in the Free Block List.";
	return false;
}

bool checkFreeBlockList(map<string, string> &fileSysFreeBlocks){
	for each(pair<string,string> block in fileSysFreeBlocks){
		ifstream file("fusedata." + block.first);
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
}

int main(){
	//TODO: Open file, if empty close it move on, create "global variables" to pass into each function for storage
	//		store file into string, pass string into functions,

	checkDevice();
	vector<int> freeBlockList;
	map<string, string> fileSysFreeBlocks;
	for (int i = 0; i < 10000; ++i){
		ifstream file("fusedata." + i);
		if (!file.is_open()){
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
	}
	checkFreeBlockList(fileSysFreeBlocks);
}