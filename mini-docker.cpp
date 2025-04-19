#include<iostream>
#include<cstdlib>
#include<algorithm>
#include<string>

using namespace std;

int main(int argc, char* argv[]){
	//Currently assuming order  - <command> <subCommand> <containerCommand> <containerArgs>
	//first arg is the file name/cli name itself (in this case ./mini-docker)
	try{
		if(argc<3){
			cerr<<"There should be atleast three arguments provided to the command line tool\n";
			cerr<<"Format : <command> <subCommand> <containerCommand> <containerArgs>\n";
			return 1;
		}
		string subCommand = argv[1];
		string containerCommand = argv[2];
		string containerArgs = " ";
		if(argc>=3){
			int argInd = 3;
			while(argInd<argc){
				containerArgs += argv[argInd];
				argInd++;
			}
		}
		transform(subCommand.begin(), subCommand.end(), subCommand.begin(),
                   [](unsigned char c) { return tolower(c); }); //transforming string in-place to lower case characters
		if(subCommand=="run")
		{
			int returnCode =system((containerCommand+containerArgs).c_str());
			if (returnCode !=0){
				cerr << "Command execution failed!\n";
				return 1;
			}
		} else{
			cerr<<"Unrecognized subcommand !\n";
				return 1;
		}
	} catch(...){
		cerr<<"There was an error while trying to parse the command\n";
		return 1;
	}
	return 0;
}