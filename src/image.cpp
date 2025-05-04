#include "../include/minidocker/image.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
#include "../include/minidocker/image_args.hpp"
#include <curl/curl.h>
#include <regex>
#include <string>
#include <sys/utsname.h>
#include <utility>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace std;

namespace minidocker
{
	Image::Image(const string& docker_command) : m_docker_command(docker_command), m_type("SINGLE_COMMAND") {}

	Image::Image(const ImageArgs& image_args) : m_type("DOCKER_IMAGE")
	{
		m_image_name = image_args.name;
		m_image_tag = image_args.tag;

	}
	
	string Image::getDockerCommand() const
	{
		return m_docker_command;
	}

	string Image::getImageType() const
	{
		return m_type;
	}

    pair<string, string> Image::getHostArchAndOS() {
        struct utsname buffer;
        if (uname(&buffer) != 0) {
            throw ImageException("Failed to get system architecture and OS.");
        }

        string arch = buffer.machine;  // e.g., "x86_64"
        string os = buffer.sysname;    // e.g., "Linux"

        // Normalize architecture
        if (arch == "x86_64") arch = "amd64";
        else if (arch == "aarch64") arch = "arm64";
        else if (arch.find("arm") != string::npos) arch = "arm";

        // Normalize OS
        std::transform(os.begin(), os.end(), os.begin(), ::tolower); // "Linux" -> "linux"

        return { arch, os };
    }

    size_t Image::writeCallback(void* contents, size_t size, size_t nmemb, string* output) {
        size_t total = size * nmemb;
        output->append((char*)contents, total);
        return total;
    }

    string Image::getToken(const string& auth_url) {
        CURL* curl = curl_easy_init();
        string response;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, auth_url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            smatch match;
            regex token_regex("\"token\"\\s*:\\s*\"([^\"]+)\"");
        	if (regex_search(response, match, token_regex)) {
        		return match[1];
        	} else {
                throw ImageException("Couldn't authenticate to " + auth_url + " !");
        	}
        }
        return "";
    }

    void Image::parseManifest(json manifest_json, const string& image_name, const string& image_tag)
    {
        //parse the manifest that we have now to m_image_manifest
        if (
            //should have relevant media type
            manifest_json.contains("mediaType") &&
            (manifest_json["mediaType"] == "application/vnd.docker.distribution.manifest.v2+json" ||
                manifest_json["mediaType"] == "application/vnd.oci.image.manifest.v1+json") &&
            manifest_json.contains("layers") &&
            manifest_json["layers"].is_array()
            ) {

            ImageManifest image_manifest;
            image_manifest.m_image_version = "unknown";
            image_manifest.m_image_source = "unknown";
            image_manifest.m_image_url = "unknown";

            if (manifest_json.contains("annotations") && manifest_json["annotations"].is_object()) {
                const json& annotations = manifest_json["annotations"];

                if (annotations.contains("org.opencontainers.image.version"))
                    image_manifest.m_image_version = annotations["org.opencontainers.image.version"].get<std::string>();

                if (annotations.contains("org.opencontainers.image.source"))
                    image_manifest.m_image_source = annotations["org.opencontainers.image.source"].get<std::string>();

                if (annotations.contains("org.opencontainers.image.url"))
                    image_manifest.m_image_url = annotations["org.opencontainers.image.url"].get<std::string>();
            }

            const auto& layers = manifest_json["layers"];
            for (const auto& layer : layers) {
                if (!(layer.contains("mediaType") && layer.contains("digest") && layer.contains("size"))) {
                    throw ImageException("Manifest layer missing required fields for image: " + image_name + ":" + image_tag);
                }

                ImageLayer img_layer;
                img_layer.m_media_type = layer["mediaType"].get<string>();
                img_layer.m_image_digest = layer["digest"].get<string>();
                img_layer.m_image_size = to_string(layer["size"].get<int64_t>());

                image_manifest.m_image_layers.push_back(move(img_layer));
            }

            m_image_manifest = image_manifest;
        }
        else {
            //At this point we should have an Image Manifest with layers in them, or else there is something wrong with the manifest received
            throw ImageException("Manifest Parsing Exception for : " + image_name + ":" + image_tag);
        }
    }


    void Image::fetchManifest(string image_name, string image_tag) {
        if (image_name.find('/') == string::npos) {
            image_name = "library/" + image_name;  // Default namespace - for example if we want to pull ubuntu - we need to use library/ubuntu
        }
        string registry_url = "https://registry-1.docker.io/v2/" + image_name + "/manifests/" + image_tag;

        CURL* curl = curl_easy_init();
        string response;

        long http_code = 0;

        if (curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Accept: application/vnd.docker.distribution.manifest.list.v2+json");
            headers = curl_slist_append(headers, "Accept: application/vnd.docker.distribution.manifest.v2+json");

            curl_easy_setopt(curl, CURLOPT_URL, registry_url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            string header_str;
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_str);

            curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

            if (http_code == 401) {

                char* www_authenticate = nullptr;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &www_authenticate);

                smatch match;
                regex realm_regex(R"~(realm="([^"]+)")~");
                regex service_regex(R"~(service="([^"]+)")~");
                regex scope_regex(R"~(scope="([^"]+)")~");

                string realm, service, scope;
                if (regex_search(header_str, match, realm_regex)) realm = match[1];
                if (regex_search(header_str, match, service_regex)) service = match[1];
                if (regex_search(header_str, match, scope_regex)) scope = match[1];

                string token_url = realm + "?service=" + service + "&scope=" + scope;
                string token = getToken(token_url);

                // Retry with Bearer token
                string auth_header = "Authorization: Bearer " + token;
                headers = curl_slist_append(headers, auth_header.c_str());

                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                response.clear();
                curl_easy_perform(curl);
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }

        if (response.empty()){
            throw ImageException("Couldn't get the manifest for " + image_name + ":" + image_tag + " !");
        }

        if (http_code == 401){
            throw ImageException("401 UNAUTHORIZED ERROR - while trying to fetch manifest for " + image_name + ":" + image_tag + " !");
        }

        //In case of multiple platform wise image version available it won't give us an image list, so first we need to select the appropriate version based on our host details
        //We finally need the layers of the image, so if we don't receive it we make a call with the digest of the image matching our architecture
        //Then we will get the Image Manifest with layers of the image to unpack
		json manifest_json = json::parse(response);
        if (manifest_json.contains("mediaType") &&
            (manifest_json["mediaType"] == "application/vnd.docker.distribution.manifest.list.v2+json" ||
                manifest_json["mediaType"] == "application/vnd.oci.image.index.v1+json")) {

            auto [host_arch, host_os] = getHostArchAndOS();

            for (const auto& manifest : manifest_json["manifests"]) {
                if (manifest.contains("platform") &&
                    manifest["platform"]["architecture"] == host_arch &&
                    manifest["platform"]["os"] == host_os) {

                    string digest = manifest["digest"];
                    return fetchManifest(image_name, digest);  // Recursive call with digest
                }
            }

            throw ImageException("No suitable manifest found for " + host_arch + "/" + host_os + " in " + image_name + ":" + image_tag);
        }
        
        parseManifest(manifest_json, image_name, image_tag);
        
    }

    void Image::fetchManifest() {
        return fetchManifest(m_image_name, m_image_tag);
    }


}