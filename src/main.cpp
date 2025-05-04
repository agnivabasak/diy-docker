#include "../include/minidocker/cli_parser.hpp"
#include "../include/minidocker/image_args.hpp"
#include "../include/minidocker/image.hpp"
#include "../include/minidocker/container.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
	try {
		minidocker::CLIParser cliParser(argc,argv);
		if (cliParser.getSubCommand() == "run"){
			minidocker::Image image(cliParser.getDockerCommand());
			minidocker::Container container(image);
			container.runDockerCommand();
		} else if (cliParser.getSubCommand() == "pull") {
			minidocker::ImageArgs imageArgs(cliParser.getDockerImageArgs());
			minidocker::Image image(imageArgs);
			image.fetchManifest();
		} else {
			throw minidocker::CLIParserException("Unrecognized subcommand !\n");
		}
		return 0;
	} catch (minidocker::CLIParserException &ex){
		cerr << "A CLI Parsing Exception occured : \n\t" + string(ex.what()) + "\n";
		return 1;
	} catch (minidocker::ImageException &ex) {
		cerr << "An Image Processing Exception occured : \n\t" + string(ex.what()) + "\n";
		return 1;
	}
	catch (minidocker::ContainerRuntimeException& ex) {
		cerr << "A Container Runtime Exception occured : \n\t" + string(ex.what()) + "\n";
		return 1;
	} catch (...){
		cerr << "An unexpected error occured!\n";
		return 1;
	}
}