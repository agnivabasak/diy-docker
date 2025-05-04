#ifndef MINIDOCKER_CLI_PARSER_H
#define MINIDOCKER_CLI_PARSER_H
#include <string>
#include "image_args.hpp"

namespace minidocker
{
	class CLIParser
	{
	private:
		std::string m_sub_command;
		std::string m_container_command;
		std::string m_container_args;
		ImageArgs m_image_args;
	public:
		CLIParser(int argc, char* argv[]);
		std::string getDockerCommand() const;
		std::string getSubCommand() const;
		ImageArgs getDockerImageArgs() const;
	};
}


#endif