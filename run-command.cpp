#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <string>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <random>
#include <algorithm>
#include <climits>
#include "run-command.hpp"

using namespace std;

random_device rd;
const int STACK_SIZE = 1024 * 1024; //clone() -> doesn't create stack on its own like fork, we have to create it manually

int runDockerCommandInIsolation(void* arg) {
	//Generate random id for container
	mt19937 g(rd());
	uniform_int_distribution<int> dist(0, INT_MAX);
	long long containerId = dist(g);

	//Changing hostname for the isolated process
	string hostnameCommand = "hostname minidocker-" + to_string(containerId);
	system(hostnameCommand.c_str());

	//executing user command
	const char* command = static_cast<const char*>(arg);
	string sCommand(command);
	//chroot is used to set the root directory, this helps isolate the process from the host filesystem
	//currently using an alpine installation to test that chroot works as expected
	string chrootedCommand = "chroot ./alpine " + sCommand;
	return system(chrootedCommand.c_str());
}

int runDockerCommand(string command) {
	char* stack = new char[STACK_SIZE];
	char* stackTop = stack + STACK_SIZE;

	//Creating new UTS namespace allows us to change the hostname/domainname of the process being spawned
	pid_t pid = clone(runDockerCommandInIsolation, stackTop, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*)command.c_str());
	if (pid == -1) {
		perror("Failed to create isolated process");
		return 1;
	}

	int status;
	waitpid(pid, &status, 0);
	int exit_code = 0;
	if (WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
	} else {
		cerr << "There was an unexpected error!";
		return 1;
	}

	delete[] stack;
	return exit_code;
}