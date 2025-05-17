#ifndef MINIDOCKER_CONTAINER_H
#define MINIDOCKER_CONTAINER_H

#include "image.hpp"
#include <sys/types.h>
#include <string>

namespace minidocker
{
	class Container
	{
	private:
		//Image of the container
		Image m_image;
		std::string m_hostname;
		std::string m_container_fs_dir;

		//util functions
		void mapRootUserInContainer(pid_t pid);
		std::string generateHostName();
		static void limitResourceUsageUsingCgroups(pid_t pid, std::string hostname);
		static void setHostNameForContainer(std::string& hostname);
		static void mountProc(const std::string& container_fs_dir);
		static bool isProcStillMounted(const std::string& container_fs_dir);
		static void unmountProc(const std::string& container_fs_dir);
		static void cleanupCgroup(std::string& hostname);
		void prepareContainerFs(const std::string& hostname);
		void fetchMinidockerDefaultFs();
		static std::string resolveExecutablePath(const std::string& command, char** envp);

		static int runDockerCommandInIsolation(void* arg);
		static int runDockerImageInIsolation(void* arg);
	public:
		Container(const Image& image);
		void runDockerCommand();
		Image getImage();
		std::string getHostname();
		std::string getContainerFsDir();
	};
}


#endif