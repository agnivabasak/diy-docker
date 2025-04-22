#include<iostream>
#include<algorithm>
#include<string>
#include "run-command.hpp"

using namespace std;

struct CleanExit {
	//to add a new line when the cli tool exits
	//to make the program cli-use friendly
    ~CleanExit() {
		cerr << endl;
    }
	//This is a hack as cerr is what causes the program to end abruptly
	//TODO: This doesn't work consistently. Make a logger wrapper or use one that does this for both cerr and cout
};

int main(int argc, char* argv[]){
	CleanExit cleanExit;
	//Currently assuming order  - <command> <subCommand> <containerCommand> <containerArgs>
	//first arg is the file name/cli name itself (in this case ./mini-docker)
	try{
		if(argc<3){
			cerr<<"There should be at least three arguments provided to the command line tool\n";
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
				containerArgs.append(" ");
				argInd++;
			}
		}
		transform(subCommand.begin(), subCommand.end(), subCommand.begin(),
                   [](unsigned char c) { return tolower(c); }); //transforming string in-place to lower case characters
		if(subCommand=="run")
		{
			int returnCode = runDockerCommand(containerCommand + containerArgs);
			if (returnCode !=0){
				cerr << "Command execution failed!";
				return 1;
			}
		} else{
			cerr<<"Unrecognized subcommand !";
				return 1;
		}
	} catch(...){
		cerr<<"There was an error while trying to parse the command";
		return 1;
	}
	return 0;
}