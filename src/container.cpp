#include "../include/minidocker/image.hpp"
#include "../include/minidocker/container.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <climits>

using namespace std;

namespace fs = std::filesystem;
static string cache_dir = "/var/lib/minidocker/layers";
static string tar_dir = "/tmp/minidocker";
static string container_dir = "/var/lib/minidocker/containers";

namespace minidocker
{
	Container::Container(const Image& image) : m_image(image)
	{
		
	}

	Image Container::getImage()
	{
		return m_image;
	}

	std::string Container::getHostname()
	{
		return m_hostname;
	}

	void Container::fetchMinidockerDefaultFs()
	{
		const char* path = std::getenv("MINIDOCKER_DEFAULT_FS");
		if (path) {
			m_container_fs_dir = string(path);
			if (!m_container_fs_dir.empty() && m_container_fs_dir.back() == '/') {
				m_container_fs_dir.pop_back(); // Remove '/' at the end for consistency in case its there
			}
			struct stat sb;
			if (!(stat(m_container_fs_dir.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))) {
				// It's not a valid directory
				throw ContainerRuntimeException("Path stored in MINIDOCKER_DEFAULT_FS is not valid!"
					"\n\tPlease set it to a valid path of a minimal root filesystem (e.g., alpine-minirootfs) before running a single dockerized command!");
			}
		} else {
			throw ContainerRuntimeException("Couldn't find env variable MINIDOCKER_DEFAULT_FS."
				"\n\tPlease set it to a valid path of a minimal root filesystem (e.g., alpine-minirootfs) before running a single dockerized command!");
		}
	}

	string Container::getContainerFsDir()
	{
		return m_container_fs_dir;
	}


	void Container::mapRootUserInContainer(pid_t pid)
	{
		pid_t host_uid = getuid();
		gid_t host_gid = getgid();

		// This is done so the current user executing the command in the cli becomes the root user in the isolated environment
		// Write to /proc/[pid]/setgroups to "deny" group changes
		// Required before gid_map changes, so as to prevent permission escalations by changing groups
		{
			ofstream setgroups("/proc/" + to_string(pid) + "/setgroups");
			if (setgroups) {
				setgroups << "deny";
			} 
			else {
				throw UserMapException("setgroups write failed");
			}
		}

		// Write uid_map
		{
			ofstream uid_map("/proc/" + to_string(pid) + "/uid_map");
			if (uid_map) {
				uid_map << "0 " << host_uid << " 1";
			}
			else {
				throw UserMapException("uid_map write failed");
			}
		}

		// Write gid_map
		{
			ofstream gid_map("/proc/" + std::to_string(pid) + "/gid_map");
			if (gid_map) {
				gid_map << "0 " << host_gid << " 1";
			}
			else {
				throw UserMapException("gid_map write failed");
			}
		}
	}

	string Container::generateHostName()
	{
		//Generate random id for container
		random_device rd;
		mt19937 g(rd());
		uniform_int_distribution<int> dist(0, INT_MAX);
		int containerId = dist(g);

		m_hostname = "minidocker-" + to_string(containerId);
		//Getting hostname for the isolated process
		return m_hostname;
	}

	void Container::limitResourceUsageUsingCgroups(pid_t pid, std::string hostname)
	{
		//Using cgroups to limit resource usage
		//This is static for now, can be made configurable
		struct stat sb;
		if (stat("/sys/fs/cgroup/cgroup.controllers", &sb) == 0 && S_ISREG(sb.st_mode)) {
			//This indicates the os uses cgroup v2

			//Create cgroup
			string createCgroupCommand = "mkdir /sys/fs/cgroup/" + hostname;
			if (system(createCgroupCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't create cgroup!");
			}

			//Enable memory and cpu controllers
			string enableControllersCommand = "echo '+memory +cpu' > /sys/fs/cgroup/cgroup.subtree_control";
			if (system(enableControllersCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't enable controllers!");
			}

			//set memory limit to 256 MB
			string setMemorylimitCommand = "echo 268435456 > /sys/fs/cgroup/" + hostname + "/memory.max";
			if (system(setMemorylimitCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set memory limits!");
			}

			//set cpu usage to 25%
			//set cpu quota to 25000 = 25ms and set period as 100ms
			//essentially saying the process gets to run 25% of the time
			string setCpuLimitCommand = "echo '25000 100000' > /sys/fs/cgroup/" + hostname + "/cpu.max";
			if (system(setCpuLimitCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set CPU limits!");
			}

			//save pid into the cgroup file
			string assignPidToCgroup = "echo " + to_string(pid) + " > /sys/fs/cgroup/" + hostname + "/cgroup.procs";
			if (system(assignPidToCgroup.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't assign process to cgroup!");
			}

		}
		else {
			//This indicates the os uses cgroup v1

			//Create cgroups
			string createCgroupCommand = "mkdir /sys/fs/cgroup/memory/" + hostname;
			if (system(createCgroupCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't create cgroup!");
			}
			createCgroupCommand = "mkdir /sys/fs/cgroup/cpu/" + hostname;
			if (system(createCgroupCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't create cgroup!");
			}

			//set memory limit to 256 MB
			string setMemorylimitCommand = "echo 268435456 > /sys/fs/cgroup/memory/" + hostname + "/memory.limit_in_bytes";
			if (system(setMemorylimitCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set memory limits!");
			}

			//set cpu usage to 25%
			//set cpu quota to 25000 = 25ms and set period as 100ms
			//essentially saying the process gets to run 25% of the time
			string setCpuQuotaCommand = "echo 25000 > /sys/fs/cgroup/cpu/" + hostname + "/cpu.cfs_quota_us";
			if (system(setCpuQuotaCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set CPU limits!");
			}
			string setCpuPeriodCommand = "echo 100000 > /sys/fs/cgroup/cpu/" + hostname + "/cpu.cfs_period_us";
			if (system(setCpuPeriodCommand.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set CPU limits!");
			}

			//save pid into the memory cgroup file
			string assignPidToMemoryCgroup = "echo " + to_string(pid) + " > /sys/fs/cgroup/memory/" + hostname + "/tasks";
			if (system(assignPidToMemoryCgroup.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set CPU limits!");
			}
			//save pid into the cpu cgroup file
			string assignPidToCpuCgroup = "echo " + to_string(pid) + " > /sys/fs/cgroup/cpu/" + hostname + "/tasks";
			if (system(assignPidToCpuCgroup.c_str()) != 0)
			{
				throw CgroupLimitException("Couldn't set CPU limits!");
			}
		}
	}

	void Container::setHostNameForContainer(string& hostname)
	{
		string hostnameCommand = "hostname " + hostname;
		//Getting hostname for the isolated process
		if (system(hostnameCommand.c_str()) != 0)
		{
			throw HostnameException("Couldn't modify hostname of the container!");
		}
	}

	void Container::mountProc(const string& container_fs_dir)
	{
		//if proc folder doesn't exist, create one
		string proc_folder_path = container_fs_dir + "/proc";
		struct stat sb;
		if (!(stat(proc_folder_path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))) {
			// It's not a valid directory, so create one
			fs::create_directories(proc_folder_path);
		}

		//mount the proc to /proc mountpoint
		//proc is a VFS(Virtual File System) with access to information about processes
		//mounting it, gives the container access to default /proc path
		//the kernel takes care of only giving access to info about the processes in the same PID Namespace
		string mountCommand = "mount -t proc proc \""+ proc_folder_path+"\"";
		if (system(mountCommand.c_str()) != 0)
		{
			throw MountException("Couldn't mount proc to successfully isolate process info!");
		}
	}

	bool Container::isProcStillMounted(const string& container_fs_dir)
	{
		ifstream mountinfo(container_fs_dir+"/proc/self/mountinfo");
		string line;

		while (getline(mountinfo, line))
		{
			if (line.find(" /proc ") != std::string::npos)
			{
				return true; // Found /proc as a mountpoint
			}
		}
		return false; // Not found => unmounted
	}

	void Container::unmountProc(const string& container_fs_dir)
	{
		string unmountCommand = "umount \"" + container_fs_dir + "/proc\"";
		if (system(unmountCommand.c_str()) != 0)
		{
			// BusyBox umount might fail to update /etc/mtab - old way of updating /etc/mtab directly,
			// Since we are writing a simple version, there is no need to handle this since this is mostly because of tooling issue of alpine minirootfs
			// but check if /proc is really unmounted - thats what matters
			if (isProcStillMounted(container_fs_dir))
			{
				// Real error
				throw UnmountException("Couldn't unmount /proc successfully and it is still mounted!");
			} else {
				cerr << "Warning: umount reported failure, but /proc is no longer mounted. Continuing safely.\n";
			}
		}
	}

	void Container::cleanupCgroup(string& hostname)
	{
		//removing cgroup files
		struct stat sb;
		if (stat("/sys/fs/cgroup/cgroup.controllers", &sb) == 0 && S_ISREG(sb.st_mode)) {
			//This indicates the os uses cgroup v2

			//move the pids from container cgroup.procs to the host cgroup.procs
			std::ifstream cgroupProcs("/sys/fs/cgroup/" + hostname + "/cgroup.procs");
			std::string pid;
			while (std::getline(cgroupProcs, pid)) {
				std::ofstream rootProcs("/sys/fs/cgroup/cgroup.procs");
				rootProcs << pid;
			}

			//Remove the actual cgroup directory
			string removeCgroup = "rmdir /sys/fs/cgroup/" + hostname;
			if (system(removeCgroup.c_str()) != 0)
			{
				throw CleanupCgroupException("Couldn't remove cgroup!");
			}

		}
		else {
			//This indicates the os uses cgroup v1

			//move the pids from container tasks to the host tasks
			std::ifstream memTasks("/sys/fs/cgroup/memory/" + hostname + "/tasks");
			std::string pid;
			while (std::getline(memTasks, pid)) {
				std::ofstream rootTasks("/sys/fs/cgroup/memory/tasks");
				rootTasks << pid;
			}

			std::ifstream cpuTasks("/sys/fs/cgroup/cpu/" + hostname + "/tasks");
			while (std::getline( cpuTasks, pid)) {
				std::ofstream rootTasks("/sys/fs/cgroup/cpu/tasks");
				rootTasks << pid;
			}

			//Remove the actual cgroup directory
			string removeCgroupMemory = "rmdir /sys/fs/cgroup/memory/" + hostname;
			if (system(removeCgroupMemory.c_str()) != 0)
			{
				throw CleanupCgroupException("Couldn't remove cgroup!");
			}
			string removeCgroupCpu = "rmdir /sys/fs/cgroup/cpu/" + hostname;
			if (system(removeCgroupCpu.c_str()) != 0)
			{
				throw CleanupCgroupException("Couldn't remove cgroup!");
			}
		}
	}

	void Container::prepareContainerFs(const std::string& hostname)
	{
		cout << "Preparing container filesystem...\n";
		fs::create_directories(container_dir);
		string host_container_dir = container_dir + "/" + hostname;
		m_container_fs_dir = host_container_dir;

		fs::create_directories(host_container_dir);
		ImageManifest image_manifest = m_image.getImageManifest();
		for (const ImageLayer& layer : image_manifest.m_image_layers) {
			string digest_clean = layer.m_image_digest.substr(layer.m_image_digest.find(":") + 1); // remove "sha256:"
			string image_layer_dir = cache_dir + "/" + digest_clean;

			if (!fs::exists(image_layer_dir)) {
				throw ContainerRuntimeException("Image Layer doesn't exist! Container FS can't be created successfully!\nAborting...\n\n");
			}

			fs::copy(image_layer_dir, host_container_dir,
				fs::copy_options::recursive |
				fs::copy_options::overwrite_existing |
				fs::copy_options::copy_symlinks);
		}

		cout << "Success\n\n";
	}

	string Container::resolveExecutablePath(const string& command, char** envp) {

		// If command is already an absolute or relative path
		if (command.find('/') != std::string::npos) {
			if (access(command.c_str(), X_OK) == 0) {
				return command;
			}
			else {
				return "";  // Invalid path or not executable
			}
		}

		// 1. Check current working directory
		std::string cwd_exec = "./" + command;
		if (access(cwd_exec.c_str(), X_OK) == 0) {
			return cwd_exec;
		}

		// 2. Find PATH variable from envp
		std::string path_env;
		for (size_t i = 0; envp[i] != nullptr; ++i) {
			std::string env_entry(envp[i]);
			if (env_entry.rfind("PATH=", 0) == 0) {
				path_env = env_entry.substr(5);  // remove "PATH="
				break;
			}
		}

		// 3. Search in each path dir
		if (!path_env.empty()) {
			size_t start = 0, end;
			while ((end = path_env.find(':', start)) != std::string::npos) {
				std::string dir = path_env.substr(start, end - start);
				std::string full_path = dir + "/" + command;
				if (access(full_path.c_str(), X_OK) == 0) {
					return full_path;
				}
				start = end + 1;
			}
			// Last path segment
			std::string last_dir = path_env.substr(start);
			std::string full_path = last_dir + "/" + command;
			if (access(full_path.c_str(), X_OK) == 0) {
				return full_path;
			}
		}

		return "";  // Not found
	}


	int Container::runDockerCommandInIsolation(void* arg)
	{
		try {
			//sleep for a sec, so all the assignments required before processing is done
			usleep(1000000);

			//Isolate the resource and set the limits
			Container* cur_container = static_cast<Container*>(arg);

			string hostname = cur_container->getHostname();
			setHostNameForContainer(hostname);

			string container_fs_dir = cur_container->getContainerFsDir();
			mountProc(container_fs_dir);

			//execute the command

			std::string sCommand = cur_container->getImage().getDockerCommand();

			//chroot is used to set the root directory, this helps isolate the process from the host filesystem
			//currently using an alpine installation to test that chroot works as expected
			string chrootedCommand = "chroot \"" + container_fs_dir + "\" " + sCommand;
			if (system(chrootedCommand.c_str()) != 0)
			{
				throw ContainerRuntimeException("Couldn't isolate the process from the host filesystem!");
			}

			unmountProc(container_fs_dir);

			return 0;

		} catch (ContainerRuntimeException &ex) {
			cerr << "A Container Runtime Exception occured : " + string(ex.what()) + "\n";
			return 1;
		} catch (...) {
			cerr << "An unexpected error occured!\n";
			return 1;
		}
	}

	int Container::runDockerImageInIsolation(void* arg)
	{
		try {
			//sleep for a sec, so all the assignments required before processing is done
			usleep(1000000);

			//Isolate the resource and set the limits
			Container* cur_container = static_cast<Container*>(arg);

			string hostname = cur_container->getHostname();
			setHostNameForContainer(hostname);

			string container_fs_dir = cur_container->getContainerFsDir();
			mountProc(container_fs_dir);

			//execute the command

			//chroot into the container filesystem
			if (chroot(container_fs_dir.c_str()) != 0) {
				throw ContainerRuntimeException("Couldn't isolate the process from the host filesystem!");
			}

			//chdir to root
			if (chdir("/") != 0) {
				throw ContainerRuntimeException("Couldn't isolate the process from the host filesystem!");
			}

			ImageConfig image_config = cur_container->getImage().getImageManifest().m_image_config;
			//chdir to working directory if specified
			string working_dir = image_config.m_working_dir;
			if (!working_dir.empty()) {
				if (chdir(working_dir.c_str()) != 0) {
					throw ContainerRuntimeException("Couldn't isolate the process from the host filesystem!");
				}
			}

			//Prepare argv for execve
			std::vector<char*> argv;

			vector<string> entrypoint = image_config.m_entrypoint;
			for (const std::string& s : entrypoint) {
				argv.push_back(const_cast<char*>(s.c_str()));
			}

			vector<string> cmd = image_config.m_cmd;
			for (const std::string& s : cmd) {
				argv.push_back(const_cast<char*>(s.c_str()));
			}

			argv.push_back(nullptr); // Null-terminated array

			//Prepare env variables for execve
			char** envp = new char* [image_config.m_env.size() + 1]; // +1 for nullptr terminator
			for (size_t i = 0; i < image_config.m_env.size(); ++i) {
				envp[i] = new char[image_config.m_env[i].size() + 1];
				std::strcpy(envp[i], image_config.m_env[i].c_str());
			}
			envp[image_config.m_env.size()] = nullptr;
			// exec the command. execvp doesn't let us add custom environment variables,
			// execve let's us add custom env variables but doesn't resolve the executabels using the PATH variable
			//so we have to manually resolve the path for the executable

			std::string resolved_path = resolveExecutablePath(argv[0], envp);
			if (resolved_path.empty()) {
				throw ContainerRuntimeException("Executable not found for command: "+ string(argv[0]));
			}
			argv[0] = const_cast<char*>(resolved_path.c_str());
			execve(argv[0], argv.data(), envp);

			//execve replaces the current process  with a new one, so the control doesn't return back to parent process
			//It only returns if there is a failure
			throw ContainerRuntimeException("Couldn't isolate the process from the host filesystem!");
		}
		catch (ContainerRuntimeException& ex) {
			cerr << "A Container Runtime Exception occured : " + string(ex.what()) + "\n";
			return 1;
		}
		catch (...) {
			cerr << "An unexpected error occured!\n";
			return 1;
		}
	}


	void Container::runDockerCommand()
	{
		int STACK_SIZE;
		if (m_image.getImageType()=="SINGLE_COMMAND") {

			fetchMinidockerDefaultFs();
			generateHostName();

			STACK_SIZE = 1024*1024; //clone() -> doesn't create stack on its own like fork, we have to create it manually
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

			pid_t pid = clone(runDockerCommandInIsolation, stackTop, CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWUSER | SIGCHLD, this);
			if (pid == -1) {
				throw ContainerRuntimeException("Failed to create isolated process");
			}

			mapRootUserInContainer(pid);
			limitResourceUsageUsingCgroups(pid, m_hostname);

			int status;
			waitpid(pid, &status, 0);
			if (!WIFEXITED(status)) {
				throw ContainerRuntimeException("Couldn't containerize command successfully!");
			}

			//cleanup
			cleanupCgroup(m_hostname);
			delete[] stack;

		} else if (m_image.getImageType()=="DOCKER_IMAGE"){

			string hostname = generateHostName();
			prepareContainerFs(hostname);

			STACK_SIZE = 16*1024 * 1024; //clone() -> doesn't create stack on its own like fork, we have to create it manually
			char* stack = new char[STACK_SIZE];
			char* stackTop = stack + STACK_SIZE;

			pid_t pid = clone(runDockerImageInIsolation, stackTop, CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWUSER | SIGCHLD, this);
			if (pid == -1) {
				throw ContainerRuntimeException("Failed to create isolated process");
			}

			mapRootUserInContainer(pid);
			limitResourceUsageUsingCgroups(pid, m_hostname);

			int status;
			cout << "\nRunning the container... \n\n";
			waitpid(pid, &status, 0);
			if (!WIFEXITED(status)) {
				throw ContainerRuntimeException("Couldn't containerize image successfully!");
			}

			//cleanup
			cleanupCgroup(m_hostname);
			//unmountProc(m_container_fs_dir); -> cant be done outside the runDockerImageInIsolation function as proc is mounted in that mount ns
			// It's also not required as we will be removing the container fs any way and won't reuse a container fs
			delete[] stack;

		} else {
			throw ContainerRuntimeException("Unknown type of image requested to be containerized!");
		}
	}

}