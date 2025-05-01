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

		//util functions
		void mapRootUserInContainer(pid_t pid);
		static std::string generateHostName();
		static void limitResourceUsageUsingCgroups(pid_t pid, std::string hostname);
		static void setHostNameForContainer(std::string& hostname);
		static void mountProc();
		static bool isProcStillMounted();
		static void unmountProc();
		static void cleanupCgroup(std::string& hostname);

		static int runDockerCommandInIsolation(void* arg);

	public:
		Container(const Image& image);
		void runDockerCommand();
	};
}


#endif