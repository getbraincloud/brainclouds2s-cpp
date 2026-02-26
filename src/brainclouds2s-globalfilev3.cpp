// Copyright 2026 bitHeads, Inc. All Rights Reserved.

#include "brainclouds2s-globalfilev3.h"
#include "brainclouds2s.h"
#include "json/json.h"
#include <curl/curl.h>

#include <sstream>
#include <thread>

namespace BrainCloud
{
    // --------------------------------------------------------------------------
    // Internal helpers
    // --------------------------------------------------------------------------

    static size_t gfv3WriteData(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        std::string* out = static_cast<std::string*>(userdata);
        out->append(ptr, size * nmemb);
        return size * nmemb;
    }

    static std::string buildGFV3Request(const std::string& operation, const Json::Value& data)
    {
        Json::Value request;
        request["service"] = "globalFileV3";
        request["operation"] = operation;
        request["data"] = data;
        Json::FastWriter writer;
        std::string s = writer.write(request);
        // FastWriter appends a newline — strip it
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        return s;
    }

    // --------------------------------------------------------------------------
    // Construction / Init
    // --------------------------------------------------------------------------

    BrainCloudS2SGlobalFileV3::BrainCloudS2SGlobalFileV3(S2SContext* s2s)
        : _s2s(s2s)
        , _generation(0)
    {
    }

    void BrainCloudS2SGlobalFileV3::init(const std::string& serverUrl)
    {
        const std::string needle = "s2sdispatcher";
        size_t pos = serverUrl.find(needle);
        if (pos != std::string::npos)
        {
            _uploadUrl = serverUrl.substr(0, pos) + "s2suploader/globalfile/upload"
                       + serverUrl.substr(pos + needle.size());
        }
        else
        {
            _uploadUrl = "https://api.braincloudservers.com/s2suploader/globalfile/upload";
        }
    }

    // --------------------------------------------------------------------------
    // File Info / Query
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::sysGetFileInfo(
        const std::string& fileId, const S2SCallback& callback)
    {
        Json::Value data;
        data["fileId"] = fileId;
        _s2s->request(buildGFV3Request("SYS_GET_FILE_INFO", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysGetFileInfoSimple(
        const std::string& folderPath, const std::string& filename, const S2SCallback& callback)
    {
        Json::Value data;
        data["folderPath"] = folderPath;
        data["filename"] = filename;
        _s2s->request(buildGFV3Request("SYS_GET_FILE_INFO_SIMPLE", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysCheckFilenameExists(
        const std::string& folderPath, const std::string& filename, const S2SCallback& callback)
    {
        Json::Value data;
        data["folderPath"] = folderPath;
        data["filename"] = filename;
        _s2s->request(buildGFV3Request("SYS_CHECK_FILENAME_EXISTS", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysCheckFullpathFilenameExists(
        const std::string& fullpathFilename, const S2SCallback& callback)
    {
        Json::Value data;
        data["fullPathFilename"] = fullpathFilename;
        _s2s->request(buildGFV3Request("SYS_CHECK_FULLPATH_FILENAME_EXISTS", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysGetGlobalCDNUrl(
        const std::string& fileId, const S2SCallback& callback)
    {
        Json::Value data;
        data["fileId"] = fileId;
        _s2s->request(buildGFV3Request("SYS_GET_GLOBAL_CDN_URL", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysGetGlobalFileList(
        const std::string& folderPath, bool recurse, const S2SCallback& callback)
    {
        Json::Value data;
        data["folderPath"] = folderPath;
        data["recurse"] = recurse;
        _s2s->request(buildGFV3Request("SYS_GET_GLOBAL_FILE_LIST", data), callback);
    }

    // --------------------------------------------------------------------------
    // File Management
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::sysMoveToGlobalFile(
        const std::string& userProfileId, const std::string& userCloudPath,
        const std::string& userCloudFilename, const std::string& globalTreeId,
        const std::string& globalFilename, bool overwriteIfPresent,
        const S2SCallback& callback)
    {
        Json::Value data;
        data["userProfileId"] = userProfileId;
        data["userCloudPath"] = userCloudPath;
        data["userCloudFilename"] = userCloudFilename;
        data["globalTreeId"] = globalTreeId;
        data["globalFilename"] = globalFilename;
        data["overwriteIfPresent"] = overwriteIfPresent;
        _s2s->request(buildGFV3Request("SYS_MOVE_TO_GLOBAL_FILE", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysCopyGlobalFile(
        const std::string& fileId, int version,
        const std::string& newTreeId, int treeVersion,
        const std::string& newFilename, bool overwriteIfPresent,
        const S2SCallback& callback)
    {
        Json::Value data;
        data["fileId"] = fileId;
        data["version"] = version;
        data["newTreeId"] = newTreeId;
        data["treeVersion"] = treeVersion;
        data["newFilename"] = newFilename;
        data["overwriteIfPresent"] = overwriteIfPresent;
        _s2s->request(buildGFV3Request("SYS_COPY_GLOBAL_FILE", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysMoveGlobalFile(
        const std::string& fileId, int version,
        const std::string& newTreeId, int treeVersion,
        const std::string& newFilename, bool overwriteIfPresent,
        const S2SCallback& callback)
    {
        Json::Value data;
        data["fileId"] = fileId;
        data["version"] = version;
        data["newTreeId"] = newTreeId;
        data["treeVersion"] = treeVersion;
        data["newFilename"] = newFilename;
        data["overwriteIfPresent"] = overwriteIfPresent;
        _s2s->request(buildGFV3Request("SYS_MOVE_GLOBAL_FILE", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysDeleteGlobalFile(
        const std::string& fileId, int version,
        const std::string& filename, const S2SCallback& callback)
    {
        Json::Value data;
        data["fileId"] = fileId;
        data["version"] = version;
        data["filename"] = filename;
        _s2s->request(buildGFV3Request("SYS_DELETE_GLOBAL_FILE", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysDeleteGlobalFiles(
        const std::string& treeId, const std::string& folderPath,
        int treeVersion, bool recurse, const S2SCallback& callback)
    {
        Json::Value data;
        data["treeId"] = treeId;
        data["folderPath"] = folderPath;
        data["treeVersion"] = treeVersion;
        data["recurse"] = recurse;
        _s2s->request(buildGFV3Request("SYS_DELETE_GLOBAL_FILES", data), callback);
    }

    // --------------------------------------------------------------------------
    // Folder Management
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::sysCreateFolder(
        const std::string& folderPath, int treeVersion,
        const std::string& name, const std::string& desc,
        bool createInterimDirectories, const S2SCallback& callback)
    {
        Json::Value data;
        data["folderPath"] = folderPath;
        data["treeVersion"] = treeVersion;
        data["name"] = name;
        data["desc"] = desc;
        data["createInterimDirectories"] = createInterimDirectories;
        _s2s->request(buildGFV3Request("SYS_CREATE_FOLDER", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysMoveFolder(
        const std::string& treeId, int treeVersion,
        const std::string& newFolderPath, const std::string& updatedName,
        bool createInterimDirectories, const S2SCallback& callback)
    {
        Json::Value data;
        data["treeId"] = treeId;
        data["treeVersion"] = treeVersion;
        data["newFolderPath"] = newFolderPath;
        data["updatedName"] = updatedName;
        data["createInterimDirectories"] = createInterimDirectories;
        _s2s->request(buildGFV3Request("SYS_MOVE_FOLDER", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysRenameFolder(
        const std::string& treeId, int treeVersion,
        const std::string& updatedName, const S2SCallback& callback)
    {
        Json::Value data;
        data["treeId"] = treeId;
        data["treeVersion"] = treeVersion;
        data["updatedName"] = updatedName;
        _s2s->request(buildGFV3Request("SYS_RENAME_FOLDER", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysLookupFolder(
        const std::string& fullFolderPath, const S2SCallback& callback)
    {
        Json::Value data;
        data["fullFolderPath"] = fullFolderPath;
        _s2s->request(buildGFV3Request("SYS_LOOKUP_FOLDER", data), callback);
    }

    void BrainCloudS2SGlobalFileV3::sysDeleteFolder(
        const std::string& treeId, const std::string& folderPath,
        int treeVersion, bool force, const S2SCallback& callback)
    {
        Json::Value data;
        data["treeId"] = treeId;
        data["folderPath"] = folderPath;
        data["treeVersion"] = treeVersion;
        data["force"] = force;
        _s2s->request(buildGFV3Request("SYS_DELETE_FOLDER", data), callback);
    }

    // --------------------------------------------------------------------------
    // Upload
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::uploadGlobalFile(
        const std::string& treeId, const std::string& filename,
        bool overwriteIfPresent, const std::vector<uint8_t>& fileData,
        const S2SCallback& callback)
    {
        log("[GlobalFileV3] Preparing upload: " + filename +
            " (" + std::to_string(fileData.size()) + " bytes) treeId=" + treeId);

        Json::Value data;
        data["treeId"] = treeId;
        data["filename"] = filename;
        data["overwriteIfPresent"] = overwriteIfPresent;
        data["fileSize"] = (Json::Int)fileData.size();

        std::string uploadUrlCopy = _uploadUrl;
        std::string filenameCopy = filename;
        std::vector<uint8_t> dataCopy = fileData;

        _s2s->request(buildGFV3Request("SYS_PREPARE_UPLOAD", data),
            [this, uploadUrlCopy, filenameCopy, dataCopy, callback](const std::string& prepareResult)
            {
                Json::Value prepareData;
                Json::Reader reader;
                if (!reader.parse(prepareResult, prepareData) ||
                    prepareData["status"].asInt() != 200)
                {
                    log("[GlobalFileV3] SYS_PREPARE_UPLOAD failed: " + prepareResult);
                    if (callback) callback(prepareResult);
                    return;
                }

                const Json::Value& fileDetails = prepareData["data"]["fileDetails"];
                if (!fileDetails.isMember("uploadId") || fileDetails["uploadId"].asString().empty())
                {
                    log("[GlobalFileV3] SYS_PREPARE_UPLOAD missing uploadId: " + prepareResult);
                    if (callback) callback(prepareResult);
                    return;
                }

                std::string uploadId = fileDetails["uploadId"].asString();
                std::string resolvedUrl = uploadUrlCopy + "?gameId=" +
                                          _s2s->getAppId() + "&uploadId=" + uploadId;

                // Use server-supplied uploadUrl if present
                if (fileDetails.isMember("uploadUrl"))
                {
                    std::string serverUrl = fileDetails["uploadUrl"].asString();
                    if (!serverUrl.empty())
                    {
                        if (serverUrl.substr(0, 4) == "http")
                        {
                            resolvedUrl = serverUrl;
                        }
                        else
                        {
                            // Relative path: prefix with scheme + host from uploadUrl
                            size_t schemeEnd = uploadUrlCopy.find("://");
                            if (schemeEnd != std::string::npos)
                            {
                                size_t hostEnd = uploadUrlCopy.find('/', schemeEnd + 3);
                                if (hostEnd != std::string::npos)
                                    resolvedUrl = uploadUrlCopy.substr(0, hostEnd) + serverUrl;
                            }
                        }
                    }
                }

                log("[GlobalFileV3] Uploading to: " + resolvedUrl);
                sendFileUpload(resolvedUrl, filenameCopy, dataCopy, callback);
            });
    }

    void BrainCloudS2SGlobalFileV3::sendFileUpload(
        const std::string& uploadUrl, const std::string& filename,
        const std::vector<uint8_t>& fileData, const S2SCallback& callback)
    {
        int gen = _generation.load();

        std::thread([this, gen, uploadUrl, filename, fileData, callback]()
        {
            CURL* curl = curl_easy_init();
            if (!curl)
            {
                std::unique_lock<std::mutex> lock(_uploadMutex);
                if (_generation.load() == gen)
                    _completedUploads.push({callback,
                        "{\"status\":900,\"status_message\":\"cURL initialization failed\"}"});
                return;
            }

            // Build multipart/form-data body with curl_mime (curl >= 7.56)
            curl_mime* mime = curl_mime_init(curl);
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, "file");
            curl_mime_filename(part, filename.c_str());
            curl_mime_type(part, "application/octet-stream");
            curl_mime_data(part,
                reinterpret_cast<const char*>(fileData.data()),
                fileData.size());

            char curlError[CURL_ERROR_SIZE] = "";
            std::string result;

            curl_easy_setopt(curl, CURLOPT_URL, uploadUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gfv3WriteData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

            CURLcode rc = curl_easy_perform(curl);

            curl_mime_free(mime);
            curl_easy_cleanup(curl);

            std::string response;
            if (rc != CURLE_OK)
            {
                std::string errMsg = (curlError[0] != '\0') ? curlError : curl_easy_strerror(rc);
                response = "{\"status\":900,\"status_message\":\"Upload error: " + errMsg + "\"}";
            }
            else
            {
                response = result;
            }

            log("[GlobalFileV3] Upload complete: " +
                (response.size() > 200 ? response.substr(0, 200) + "..." : response));

            {
                std::unique_lock<std::mutex> lock(_uploadMutex);
                if (_generation.load() == gen)
                    _completedUploads.push({callback, response});
            }
        }).detach();
    }

    // --------------------------------------------------------------------------
    // Lifecycle
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::runCallbacks()
    {
        std::queue<UploadCompletion> pending;
        {
            std::unique_lock<std::mutex> lock(_uploadMutex);
            std::swap(pending, _completedUploads);
        }

        while (!pending.empty())
        {
            auto& completion = pending.front();
            if (completion.callback)
                completion.callback(completion.result);
            pending.pop();
        }
    }

    void BrainCloudS2SGlobalFileV3::disconnect()
    {
        std::unique_lock<std::mutex> lock(_uploadMutex);
        ++_generation;
        while (!_completedUploads.empty())
            _completedUploads.pop();
    }

    // --------------------------------------------------------------------------
    // Private helpers
    // --------------------------------------------------------------------------

    void BrainCloudS2SGlobalFileV3::log(const std::string& message)
    {
        // Mirror the brainclouds2s s2s_log convention
        s2s_log("[S2S] ", message);
    }

} // namespace BrainCloud
