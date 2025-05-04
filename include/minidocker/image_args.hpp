#ifndef MINIDOCKER_IMAGE_ARGS_H
#define MINIDOCKER_IMAGE_ARGS_H
#include <string>

namespace minidocker
{
	struct ImageArgs
	{
		std::string name;
		std::string tag;
	};
}

#endif