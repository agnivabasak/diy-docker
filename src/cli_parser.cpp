#include "../include/minidocker/cli_parser.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
#include <string>
#include <algorithm>

namespace minidocker
{
	CLIParser::CLIParser(int argc, char* argv[])
	{
		//Currently assuming order  - <command> <subCommand> <containerCommand> <containerArgs>
		//first arg is the file name/cli name itself (in this case ./mini-docker)
		if (argc < 3) {
			throw CLIParserException(
				"There should be at least three arguments provided to the command line tool\n"
				"Format : <command> <subCommand> <containerCommand> <containerArgs>\n"
			);
		}
		m_sub_command = argv[1];
		m_container_command = argv[2];
		m_container_args = " ";
		if (argc >= 3) {
			int argInd = 3;
			while (argInd < argc) {
				m_container_args += argv[argInd];
				m_container_args.append(" ");
				argInd++;
			}
		}

		transform(m_sub_command.begin(), m_sub_command.end(), m_sub_command.begin(),
			[](unsigned char c) { return tolower(c); }); //transforming string in-place to lower case characters
	}

	std::string CLIParser::getSubCommand() const
	{
		return m_sub_command;
	}

	std::string CLIParser::getDockerCommand() const
	{
		return m_container_command + m_container_args;
	}

}