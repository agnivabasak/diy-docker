#ifndef MINIDOCKER_IMAGE_H
#define MINIDOCKER_IMAGE_H
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#include "image_args.hpp"

namespace minidocker
{
	struct ImageLayer
	{
		std::string m_media_type;
		std::string m_image_digest;
		std::string m_image_size;
	};

	struct ImageManifest
	{
		std::string m_image_version;
		std::string m_image_source;
		std::string m_image_url;
		std::vector<ImageLayer>  m_image_layers;
	};

	class Image
	{
	private:
		std::string m_docker_command;
		std::string m_type;
		std::string m_image_name;
		std::string m_image_tag;
		std::string m_bearer_token;
		ImageManifest m_image_manifest;

		//util functions
		std::pair<std::string, std::string> getHostArchAndOS();
		static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
		static size_t writeFileCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
		std::string getToken(const std::string& auth_url);
		void updateTokenIfUnauthorized(const std::string& header_str);
		void parseManifest(nlohmann::json manifest_json, const std::string& image_name, const std::string& image_tag);
		void fetchManifest();
		void fetchManifest(std::string image_name, std::string image_tag);
		void downloadImageLayer(const std::string& blob_url, const std::string& image_tar_path);
		static void extractImageLayer(const std::string& image_tar_path, const std::string& image_layer_dir);
		void processImageLayers();
	public:
		Image(const std::string& docker_command);
		Image(const ImageArgs& image_args);
		std::string getDockerCommand() const;
		std::string getImageType() const;
		void pull();
	};
}


#endif