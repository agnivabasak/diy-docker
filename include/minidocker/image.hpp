#ifndef MINIDOCKER_IMAGE_H
#define MINIDOCKER_IMAGE_H
#include <string>

namespace minidocker
{
	class Image
	{
	private:
		std::string m_docker_command;
		std::string m_type;
	public:
		Image(const std::string& docker_command);
		std::string getDockerCommand() const;
		std::string getImageType() const;
	};
}


#endif