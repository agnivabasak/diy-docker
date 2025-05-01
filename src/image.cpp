#include "../include/minidocker/image.hpp"
#include <string>

namespace minidocker
{
	Image::Image(const std::string& docker_command) : m_docker_command(docker_command), m_type("SINGLE_COMMAND") {}
	
	std::string Image::getDockerCommand() const
	{
		return m_docker_command;
	}

	std::string Image::getImageType() const
	{
		return m_type;
	}


}