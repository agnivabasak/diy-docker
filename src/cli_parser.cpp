#include "../include/minidocker/cli_parser.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
#include <string>
#include <algorithm>
#include <utility>

using namespace std;

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

		//In case of Image rather than direct command execution
		ImageArgs imageArgs;
		auto pos = m_container_command.find(':');
		if (pos == string::npos) {
			imageArgs.name = m_container_command;
			imageArgs.tag = "latest";
		} else {
			imageArgs.name = m_container_command.substr(0, pos);
			string tempTag = m_container_command.substr(pos + 1);

			if (tempTag.empty()) {
				tempTag = "latest";
			}

			imageArgs.tag = tempTag;
		}

		m_image_args = imageArgs;
	}

	string CLIParser::getSubCommand() const
	{
		return m_sub_command;
	}

	string CLIParser::getDockerCommand() const
	{
		return m_container_command + m_container_args;
	}

	ImageArgs CLIParser::getDockerImageArgs() const
	{
		return m_image_args;
	}


}