#include<cstdlib>
#include<string>
#include "run-command.hpp"

using namespace std;

int runDockerCommand(string command) {
	return system(command.c_str());
}