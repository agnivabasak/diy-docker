#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <string>
#include <sched.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <climits>
#include "run-command.hpp"

using namespace std;

random_device rd;
const int STACK_SIZE = 1024 * 1024; //clone() -> doesn't create stack on its own like fork, we have to create it manually
uid_t host_uid = getuid();
gid_t host_gid = getgid();

bool isProcStillMounted()
{
    ifstream mountinfo("./alpine/proc/self/mountinfo");
    string line;

    while (getline(mountinfo, line))
    {
		cout<<line<<"\n";
        if (line.find(" /proc ") != std::string::npos)
        {
            return true; // Found /proc as a mountpoint
        }
    }
    return false; // Not found => unmounted
}

int runDockerCommandInIsolation(void* arg) {
	//usleep(1000000);
	//Generate random id for container
	mt19937 g(rd());
	uniform_int_distribution<int> dist(0, INT_MAX);
	long long containerId = dist(g);

	//Getting hostname for the isolated process
	string hostname = "minidocker-" + to_string(containerId);
	pid_t pid = getpid();
	
	//Using cgroups to limit resource usage
	//This is static for now, can be made configurable
	struct stat sb;
    if (stat("/sys/fs/cgroup/cgroup.controllers", &sb) == 0 && S_ISREG(sb.st_mode)) {
        //This indicates the os uses cgroup v2
		
		//Create cgroup
		string createCgroupCommand = "mkdir /sys/fs/cgroup/"+hostname;
		if (system(createCgroupCommand.c_str()) != 0)
		{
			cerr << "Couldn't create cgroup!";
			return 1;
		}

		//Enable memory and cpu controllers
		string enableControllersCommand = "echo '+memory +cpu' > /sys/fs/cgroup/cgroup.subtree_control";
		if (system(enableControllersCommand.c_str()) != 0)
		{
			cerr << "Couldn't enable controllers!";
			return 1;
		}

		//set memory limit to 256 MB
		string setMemorylimitCommand = "echo 268435456 > /sys/fs/cgroup/"+hostname+"/memory.max";
		if (system(setMemorylimitCommand.c_str()) != 0)
		{
			cerr << "Couldn't set memory limits!";
			return 1;
		}

		//set cpu usage to 25%
		//set cpu quota to 25000 = 25ms and set period as 100ms
		//essentially saying the process gets to run 25% of the time
		string setCpuLimitCommand = "echo '25000 100000' > /sys/fs/cgroup/"+hostname+"/cpu.max";
		if (system(setCpuLimitCommand.c_str()) != 0)
		{
			cerr << "Couldn't set CPU limits!";
			return 1;
		}

		//save pid into the cgroup file
		string assignPidToCgroup = "echo " + to_string(pid) + " > /sys/fs/cgroup/"+hostname+"/cgroup.procs";
		if (system(assignPidToCgroup.c_str()) != 0)
		{
			cerr << "Couldn't assign process to cgroup!";
			return 1;
		}

    } else {
        //This indicates the os uses cgroup v1
		
		//Create cgroups
		string createCgroupCommand = "mkdir /sys/fs/cgroup/memory/"+hostname;
		if (system(createCgroupCommand.c_str()) !=0)
		{
			cerr << "Couldn't create cgroup!";
			return 1;
		}
		createCgroupCommand = "mkdir /sys/fs/cgroup/cpu/"+hostname;
		if (system(createCgroupCommand.c_str()) !=0)
		{
			cerr << "Couldn't create cgroup!";
			return 1;
		}
		
		//set memory limit to 256 MB
		string setMemorylimitCommand = "echo 268435456 > /sys/fs/cgroup/memory/"+hostname+"/memory.limit_in_bytes";
		if (system(setMemorylimitCommand.c_str()) !=0)
		{
			cerr << "Couldn't set memory limits!";
			return 1;
		}
		
		//set cpu usage to 25%
		//set cpu quota to 25000 = 25ms and set period as 100ms
		//essentially saying the process gets to run 25% of the time
		string setCpuQuotaCommand = "echo 25000 > /sys/fs/cgroup/cpu/"+hostname+"/cpu.cfs_quota_us";
		if (system(setCpuQuotaCommand.c_str()) !=0)
		{
			cerr << "Couldn't set CPU limits!";
			return 1;
		}
		string setCpuPeriodCommand = "echo 100000 > /sys/fs/cgroup/cpu/"+hostname+"/cpu.cfs_period_us";
		if (system(setCpuPeriodCommand.c_str()) !=0)
		{
			cerr << "Couldn't set CPU limits!";
			return 1;
		}
		
		//save pid into the memory cgroup file
		string assignPidToMemoryCgroup = "echo " + to_string(pid)+ " > /sys/fs/cgroup/memory/"+hostname+"/tasks";
		if (system(assignPidToMemoryCgroup.c_str()) !=0)
		{
			cerr << "Couldn't set CPU limits!";
			return 1;
		}
		//save pid into the cpu cgroup file
		string assignPidToCpuCgroup = "echo " + to_string(pid)+ " > /sys/fs/cgroup/cpu/"+hostname+"/tasks";
		if (system(assignPidToCpuCgroup.c_str()) !=0)
		{
			cerr << "Couldn't set CPU limits!";
			return 1;
		}
    }
	
	string hostnameCommand = "hostname "+hostname;
	//Getting hostname for the isolated process
	if (system(hostnameCommand.c_str()) !=0)
	{
		cerr << "Couldn't modify hostname of the container!";
		return 1;
	}

	//mount the proc to /proc mountpoint
	//proc is a VFS(Virtual File System) with access to information about processes
	//mounting it, gives the container access to default /proc path
	//the kernel takes care of only giving access to info about the processes in the same PID Namespace
	string mountCommand = "mount -t proc proc ./alpine/proc";
	if (system(mountCommand.c_str()) != 0)
	{
		cerr << "Couldn't mount proc to successfully isolate process info!";
		return 1;
	}

	//executing user command
	const char* command = static_cast<const char*>(arg);
	string sCommand(command);
	//chroot is used to set the root directory, this helps isolate the process from the host filesystem
	//currently using an alpine installation to test that chroot works as expected
	string chrootedCommand = "chroot ./alpine "+ sCommand;
	if (system(chrootedCommand.c_str())!=0)
	{
		cerr << "Couldn't isolate the process from the host filesystem!";
		return 1;
	}
	
	string unmountCommand = "umount ./alpine/proc";
	if (system(unmountCommand.c_str()) != 0)
	{
		// BusyBox umount might fail to update /etc/mtab - old way of updating /etc/mtab directly,
		// Since we are writing a simple version, there is no need to handle this since this is mostly because of tooling issue of alpine minirootfs
		// but check if /proc is really unmounted - thats what matters
        if (isProcStillMounted())
        {
            cerr << "Couldn't unmount /proc successfully and it is still mounted!" << endl;
            return 1; // Real error
        }
        else
        {
            cerr << "Warning: umount reported failure, but /proc is no longer mounted. Continuing safely." << endl;
        }
	}
	
	//removing cgroup files
	//TODO: Shift this to after everything to do with the container is done, because currently it fails as tasks or cgroup.procs still has the pid
	if (stat("/sys/fs/cgroup/cgroup.controllers", &sb) == 0 && S_ISREG(sb.st_mode)) {
		//This indicates the os uses cgroup v2
		string removeCgroup = "rmdir /sys/fs/cgroup/" + hostname;
		if (system(removeCgroup.c_str())!=0)
		{
			cerr << "Couldn't remove cgroup!";
			return 1;
		}

	} else {
		//This indicates the os uses cgroup v1
		string removeCgroupMemory = "rmdir /sys/fs/cgroup/memory/" + hostname;
		if (system(removeCgroupMemory.c_str())!=0)
		{
			cerr << "Couldn't remove cgroup!";
			return 1;
		}
		string removeCgroupCpu = "rmdir /sys/fs/cgroup/cpu/" + hostname;
		if (system(removeCgroupCpu.c_str())!=0)
		{
			cerr << "Couldn't remove cgroup!";
			return 1;
		}
	}

	return 0;
}

int runDockerCommand(string command) {
	char* stack = new char[STACK_SIZE];
	char* stackTop = stack + STACK_SIZE;

	/*
	Flags are used for assigning new namespaces to the new process
	CLONE_NEWPID - Creates a new PID Namespace, each PID Namespace starts from 1,2,3...
	CLONE_NEWNS - Creates a new Mount Namespace - doesn't just show a view from the host's /proc when we access it, but from a fresh one
	CLONE_NEWUTS - Creating new UTS Namespace allows us to change the hostname/domainname of the process being spawned
	CLONE_NEWUSER - Creating a new user namespace which lets us run the container rootless. The host user becomes the root inside the container
	In Docker, by default it's the root, and we have to set the user using "USER" in the DockerFile
	*/

	pid_t pid = clone(runDockerCommandInIsolation, stackTop, CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWUSER | SIGCHLD, (void*)command.c_str());
	if (pid == -1) {
		perror("Failed to create isolated process");
		return 1;
	}

	// Write to /proc/[pid]/setgroups to "deny" group changes
	// Required before gid_map changes, so as to prevent permission escalations by changing groups
	{
		ofstream setgroups("/proc/" + to_string(pid) + "/setgroups");
		if (setgroups) {
			setgroups << "deny";
		}
		else {
			perror("setgroups write failed");
			return 1;
		}
	}

	// Write uid_map
	{
		ofstream uid_map("/proc/" + to_string(pid) + "/uid_map");
		if (uid_map) {
			uid_map << "0 " << host_uid << " 1";
		}
		else {
			perror("uid_map write failed");
			return 1;
		}
	}

	// Write gid_map
	{
		ofstream gid_map("/proc/" + std::to_string(pid) + "/gid_map");
		if (gid_map) {
			gid_map << "0 " << host_gid << " 1";
		}
		else {
			perror("gid_map write failed");
			return 1;
		}
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
