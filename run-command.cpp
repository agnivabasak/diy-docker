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
	if (system(hostnameCommand.c_str()) !=0)
	{
		cerr << "Couldn't modify hostname of the container!";
		return 1;
	}

	//mount the proc to /proc mountpoint
	//proc is a VFS(Virtual File System) with access to information about processes
	//mounting it, gives the container access to default /proc path
	//the kernel takes care of only giving access to info about the processes in the same PID Namespace
	/*string mountCommand = "mount -t proc proc /proc";
	if (system(mountCommand.c_str()) != 0)
	{
		cerr << "Couldn't mount proc to successfully isolate process info!";
		return 1;
	}*/

	//executing user command
	const char* command = static_cast<const char*>(arg);
	string sCommand(command);
	//chroot is used to set the root directory, this helps isolate the process from the host filesystem
	//currently using an alpine installation to test that chroot works as expected
	//have to mount after chroot because or else the mount exists outside the alpine directory which becomes the root
	string chrootedCommand = "chroot ./alpine /bin/sh -c 'mount -t proc proc /proc && " + sCommand + "'";
	if (system(chrootedCommand.c_str())!=0)
	{
		cerr << "Couldn't isolate the process from the host filesystem!";
		return 1;
	}

	string unmountCommand = "umount /proc";
	if (system(unmountCommand.c_str()) != 0)
	{
		cerr << "Couldn't unmount /proc successfully!";
		return 1;
	}

	return 0;
}

int runDockerCommand(string command) {
	char* stack = new char[STACK_SIZE];
	char* stackTop = stack + STACK_SIZE;

	//Flags are used for assigning new namespaces to the new process
	//CLONE_NEWPID - Creates a new PID Namespace, each PID Namespace starts from 1,2,3...
	//CLONE_NEWNS - Creates a new Mount Namespace - doesnt just show a view from the host's /proc when we access it, but from a fresh one
	//CLONE_NEWUTS - Creating new UTS Namespace allows us to change the hostname/domainname of the process being spawned
	pid_t pid = clone(runDockerCommandInIsolation, stackTop, CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | SIGCHLD, (void*)command.c_str());
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