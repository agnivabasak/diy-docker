#include "../include/minidocker/image.hpp"
#include "../include/minidocker/custom_specific_exceptions.hpp"
#include "../include/minidocker/image_args.hpp"
#include <curl/curl.h>
#include <regex>
#include <string>
#include <sys/utsname.h>
#include <utility>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace std;

namespace fs = std::filesystem;
static string cache_dir = "/var/lib/minidocker/layers";
static string tar_dir = "/tmp/minidocker";
static string container_dir = "/var/lib/minidocker/containers";

//TODO: make sure files created in case of error is deleted like .tar and folder for image layer
namespace minidocker
{
	Image::Image(const string& docker_command) : m_docker_command(docker_command), m_type("SINGLE_COMMAND") {}

	Image::Image(const ImageArgs& image_args) : m_type("DOCKER_IMAGE")
	{
		m_image_name = image_args.name;
		m_image_tag = image_args.tag;

        if (m_image_name.find('/') == string::npos) {
            m_image_name = "library/" + m_image_name;  // Default namespace - for example if we want to pull ubuntu - we need to use library/ubuntu
        }
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
        transform(os.begin(), os.end(), os.begin(), ::tolower); // "Linux" -> "linux"

        return { arch, os };
    }

    size_t Image::writeCallback(void* contents, size_t size, size_t nmemb, string* output)
	{
        size_t total = size * nmemb;
        output->append((char*)contents, total);
        return total;
    }

    size_t Image::writeFileCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        std::ofstream* out = static_cast<std::ofstream*>(userdata);
        if (!out || !out->is_open()) return 0;
        out->write(ptr, size * nmemb);
        return size * nmemb;
    }

    string Image::getToken(const string& auth_url)
	{
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

    void Image::updateTokenIfUnauthorized(const string& header_str)
	{
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
        m_bearer_token = token;
	}

    void Image::parseConfigDetails(json config_json)
    {
        string image_name = m_image_name;
        if (image_name.find('/') == string::npos) {
            image_name = "library/" + image_name;  // Default namespace - for example if we want to pull ubuntu - we need to use library/ubuntu
        }

        //parse the config json to get config details
        if (config_json.contains("config") && config_json["config"].is_object())
        {
            const auto& parsed_config_json = config_json["config"];
            ImageConfig image_config;

            //Entrypoint
            if (parsed_config_json.contains("Entrypoint") && !parsed_config_json["Entrypoint"].is_null() && parsed_config_json["Entrypoint"].is_array()) {
                image_config.m_entrypoint = "";
                const auto& entrypoint_args = parsed_config_json["Entrypoint"];
                for (auto& entrypoint_arg : entrypoint_args)
                {
                    image_config.m_entrypoint.append(" " + string(entrypoint_arg));
                }
            } else {
                image_config.m_entrypoint = "";
            }

            //Cmd
            if (parsed_config_json.contains("Cmd") && !parsed_config_json["Cmd"].is_null() && parsed_config_json["Cmd"].is_array()) {
                image_config.m_cmd = "";
                const auto& cmd_args = parsed_config_json["Cmd"];
                for (auto& cmd_arg: cmd_args)
                {
                    image_config.m_cmd.append(" " + string(cmd_arg));
                }
            } else {
                image_config.m_cmd = "";
            }

            //Env
            if (parsed_config_json.contains("Env") && !parsed_config_json["Env"].is_null() && parsed_config_json["Env"].is_array()) {
                image_config.m_env = vector<string>();
                const auto& env_vars = parsed_config_json["Env"];
                for (auto& env_var: env_vars)
                {
                    image_config.m_env.push_back(string(env_var));
                }
            } else {
                image_config.m_env = vector<string>();
            }

            //WorkingDir
            if (parsed_config_json.contains("WorkingDir") && !parsed_config_json["WorkingDir"].is_null() && parsed_config_json["WorkingDir"].is_string()) {
                image_config.m_working_dir = parsed_config_json["WorkingDir"];
            }
            else {
                image_config.m_working_dir = "";
            }

            m_image_manifest.m_image_config = image_config;
        }
        else {
            throw ImageConfigException("Config detail missing for image: " + image_name + ":" + m_image_tag);
        }
    }


    void Image::fetchConfigDetails(json manifest_json)
    {
        cout << "\nFetching Config Details for : " << m_image_name << ":" << m_image_tag << "...\n";

        string image_name = m_image_name;
        if (image_name.find('/') == string::npos) {
            image_name = "library/" + image_name;  // Default namespace - for example if we want to pull ubuntu - we need to use library/ubuntu
        }

        //parse the manifest to get config details
        if (manifest_json.contains("config") && manifest_json["config"].is_object())
        {
            const auto& configJson = manifest_json["config"];
            if (configJson.contains("digest") && configJson["digest"].is_string())
            {
                string digest = configJson["digest"];
                string registry_url = "https://registry-1.docker.io/v2/" + image_name + "/blobs/" + digest;

                CURL* curl = curl_easy_init();
                string response;

                long http_code = 0;

                if (curl) {
                    struct curl_slist* headers = nullptr;
                    headers = curl_slist_append(headers, "Accept: application/vnd.oci.image.config.v1+json");
                    headers = curl_slist_append(headers, "Accept: application/vnd.docker.container.image.v1+json");
                    string auth_header;
                    if (!m_bearer_token.empty())
                    {
                        auth_header = "Authorization: Bearer " + m_bearer_token;
                        headers = curl_slist_append(headers, auth_header.c_str());
                    }

                    curl_easy_setopt(curl, CURLOPT_URL, registry_url.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

                    string header_str;
                    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeCallback);
                    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_str);

                    //blob fetching can respond with 307 Redirect responses
                    //this is to handle redirect
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L); // limit to 5 redirects
                    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
                    //Also avoid curl writing http errors into the response
                    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

                    curl_easy_perform(curl);
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

                    if (http_code == 401) {

                        //Update token as unauthorized error
                        updateTokenIfUnauthorized(header_str);

                        // Retry with Bearer token
                        auth_header = "Authorization: Bearer " + m_bearer_token;
                        headers = curl_slist_append(headers, auth_header.c_str());

                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                        response.clear();
                        curl_easy_perform(curl);
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                    }

                    curl_easy_cleanup(curl);
                    curl_slist_free_all(headers);

                }
                else {
                    throw ImageConfigException("Couldn't initialize curl to get config of image!");
                }

                if (response.empty()) {
                    throw ImageConfigException("Couldn't get the config for " + image_name + ":" + m_image_tag + " !");
                }

                if (http_code == 401) {
                    throw ImageConfigException("401 UNAUTHORIZED ERROR - while trying to fetch config for " + image_name + ":" + m_image_tag + " !");
                }

                json config_json = json::parse(response);
                parseConfigDetails(config_json);
            }
            else {
                throw ImageConfigException("Config detail missing digest for image: " + image_name + ":" + m_image_tag);
            }
        }
        else {
            throw ImageConfigException("Config detail missing for image: " + image_name + ":" + m_image_tag);
        }

        cout << "Success\n\n";
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
                    image_manifest.m_image_version = annotations["org.opencontainers.image.version"].get<string>();

                if (annotations.contains("org.opencontainers.image.source"))
                    image_manifest.m_image_source = annotations["org.opencontainers.image.source"].get<string>();

                if (annotations.contains("org.opencontainers.image.url"))
                    image_manifest.m_image_url = annotations["org.opencontainers.image.url"].get<string>();
            }

            const auto& layers = manifest_json["layers"];
            for (const auto& layer : layers) {
                if (!(layer.contains("mediaType") && layer.contains("digest") && layer.contains("size"))) {
                    throw ImageManifestException("Manifest layer missing required fields for image: " + image_name + ":" + image_tag);
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
            throw ImageManifestException("Manifest Parsing Exception for : " + image_name + ":" + image_tag);
        }
    }

    void Image::fetchManifest(string image_name, string image_tag)
	{
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
            string auth_header;
            if (!m_bearer_token.empty())
            {
	            auth_header = "Authorization: Bearer " + m_bearer_token;
				headers = curl_slist_append(headers, auth_header.c_str());
            }
        	

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

                //Update token as unauthorized error
                updateTokenIfUnauthorized(header_str);

                // Retry with Bearer token
                auth_header = "Authorization: Bearer " + m_bearer_token;
                headers = curl_slist_append(headers, auth_header.c_str());

                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                response.clear();
                curl_easy_perform(curl);
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

        } else {
            throw ImageManifestException("Couldn't initialize curl to get manifest of image!");
        }

        if (response.empty()){
            throw ImageManifestException("Couldn't get the manifest for " + image_name + ":" + image_tag + " !");
        }

        if (http_code == 401){
            throw ImageManifestException("401 UNAUTHORIZED ERROR - while trying to fetch manifest for " + image_name + ":" + image_tag + " !");
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

            throw ImageManifestException("No suitable manifest found for " + host_arch + "/" + host_os + " in " + image_name + ":" + image_tag);
        }

        parseManifest(manifest_json, image_name, image_tag);
        //Now that we have the actual Image Manifest, We need to get the config details
		fetchConfigDetails(manifest_json);

        cout << "Success\n\n";
    }

    void Image::fetchManifest()
	{
        cout << "Fetching Image Manifest for : " << m_image_name << ":" << m_image_tag << "...\n";
        return fetchManifest(m_image_name, m_image_tag);
    }

    void Image::downloadImageLayer(const string& blob_url, const string& image_tar_path) {
        // Skip download if tarball already exists
        if (fs::exists(image_tar_path)) {
            cout << "Tarball already exists. Skipping download.\n";
            return;
        }

        CURL* curl = curl_easy_init();
        if (!curl) throw ImageTarballException("Couldn't initialize curl to download tarball of layer!");

        ofstream ofs(image_tar_path, ios::binary);
        if (!ofs) throw ImageTarballException("Failed to open tarball file!");

        struct curl_slist* headers = nullptr;
        string auth_header;
        if (!m_bearer_token.empty()) {
            auth_header = "Authorization: Bearer " + m_bearer_token;
            headers = curl_slist_append(headers, auth_header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_URL, blob_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);

        string header_str;
        long http_code = 0;
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_str);

        //blob fetching can respond with 307 Redirect responses
        //this is to handle redirect
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L); // limit to 5 redirects
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
        //Also avoid curl writing http errors into the tar file
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 401) {
            //Update token as unauthorized error
            updateTokenIfUnauthorized(header_str);
            // Retry with Bearer token
            auth_header = "Authorization: Bearer " + m_bearer_token;
            headers = curl_slist_append(headers, auth_header.c_str());

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        
        ofs.close();
        if (http_code == 401) {
            throw ImageTarballException("401 UNAUTHORIZED ERROR - while trying to download tarball of image layer!");
        }

        if (res != CURLE_OK || http_code != 200) {
            throw ImageTarballException("Failed to download layer!");
        }

        cout << "Downloaded tarball of Image Layer\n";
    }

    void Image::extractImageLayer(const string& image_tar_path, const string& image_layer_dir) {

        if (fs::exists(image_layer_dir)) {
            cout << "Image Layer already extracted. Skipping.\n";
            return;
        }

        fs::create_directories(image_layer_dir);
        string cmd = "tar -xf " + image_tar_path + " -C " + image_layer_dir;
        if (system(cmd.c_str()) != 0) {
            throw ImageExtractionException("Failed to extract tarball for Image Layer!");
        }
        cout << "Extracted Image Layer\n";
    }

    void Image::processImageLayers() {
        cout << "Processing each image layer...\n";
        fs::create_directories(tar_dir);
        fs::create_directories(cache_dir);

        for (const ImageLayer& layer : m_image_manifest.m_image_layers) {
            cout << "\nProcessing Image Layer : " << layer.m_image_digest <<"\n";
            string blob_url = "https://registry-1.docker.io/v2/" + m_image_name + "/blobs/" + layer.m_image_digest;

            string digest_clean = layer.m_image_digest.substr(layer.m_image_digest.find(":") + 1); // remove "sha256:"
            string image_layer_dir = cache_dir + "/"+ digest_clean;
            string image_tar_path = tar_dir + "/" + digest_clean + ".tar";
            if (fs::exists(image_layer_dir)) {
                cout << "Image Layer already extracted. Skipping.\n";
            } else if (fs::exists(image_tar_path)) {
	            cout << "Tarball already exists. Skipping download.\n";
                extractImageLayer(image_tar_path, image_layer_dir);
            } else {
                downloadImageLayer(blob_url,image_tar_path);
                extractImageLayer(image_tar_path, image_layer_dir);
            }
        }
        cout << "Success\n\n";
    }

    void Image::pull()
    {
        fetchManifest();
        processImageLayers();
    }

    ImageManifest Image::getImageManifest() const
	{
        return m_image_manifest;
	}

}