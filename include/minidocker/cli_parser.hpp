#ifndef MINIDOCKER_CLI_PARSER_H
#define MINIDOCKER_CLI_PARSER_H
#include <string>

namespace minidocker
{
	class CLIParser
	{
	private:
		std::string m_sub_command;
		std::string m_container_command;
		std::string m_container_args;
	public:
		CLIParser(int argc, char* argv[]);
		std::string getDockerCommand() const;
		std::string getSubCommand() const;
	};
}


#endif