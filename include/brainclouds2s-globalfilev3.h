// Copyright 2026 bitHeads, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace BrainCloud
{
    class S2SContext;
    using S2SCallback = std::function<void(const std::string &)>;

    /**
     * S2S service for brainCloud Global File V3 operations.
     *
     * Access via S2SContext::getGlobalFileV3() after creating a context.
     *
     * File upload is a two-step process:
     *   1. SYS_PREPARE_UPLOAD is sent via the S2S dispatcher and returns an uploadId.
     *   2. The file bytes are POSTed as multipart/form-data to the upload endpoint.
     *
     * The upload callback is dispatched on the next call to S2SContext::runCallbacks()
     * after the upload HTTP request completes.
     */
    class BrainCloudS2SGlobalFileV3
    {
    public:
        BrainCloudS2SGlobalFileV3(S2SContext* s2s);
        ~BrainCloudS2SGlobalFileV3() = default;

        /** Derives the upload URL from the S2S dispatcher URL. Called by S2SContext automatically. */
        void init(const std::string& serverUrl);

        // -----------------------------------------------------------------------
        // File Info / Query
        // -----------------------------------------------------------------------

        /** Returns metadata for a global file identified by its fileId. */
        void sysGetFileInfo(const std::string& fileId, const S2SCallback& callback);

        /** Returns metadata for a global file identified by folder path and filename. */
        void sysGetFileInfoSimple(const std::string& folderPath, const std::string& filename,
            const S2SCallback& callback);

        /** Returns true if a file with the given name exists in the specified folder. */
        void sysCheckFilenameExists(const std::string& folderPath, const std::string& filename,
            const S2SCallback& callback);

        /** Returns true if a file exists at the given full path (e.g. "folder/sub/file.txt"). */
        void sysCheckFullpathFilenameExists(const std::string& fullpathFilename,
            const S2SCallback& callback);

        /** Returns the CDN URL for the global file identified by fileId. */
        void sysGetGlobalCDNUrl(const std::string& fileId, const S2SCallback& callback);

        /**
         * Lists all global files under the given folder path.
         * Pass folderPath="" and recurse=true to list the entire tree.
         */
        void sysGetGlobalFileList(const std::string& folderPath, bool recurse,
            const S2SCallback& callback);

        // -----------------------------------------------------------------------
        // File Management
        // -----------------------------------------------------------------------

        /** Moves a file from personal cloud storage into the global file system. */
        void sysMoveToGlobalFile(const std::string& userProfileId, const std::string& userCloudPath,
            const std::string& userCloudFilename, const std::string& globalTreeId,
            const std::string& globalFilename, bool overwriteIfPresent,
            const S2SCallback& callback);

        /**
         * Copies a global file to another folder, optionally with a new name.
         * Pass version=-1 to copy the latest version. Pass treeVersion=-1 to skip the version check.
         */
        void sysCopyGlobalFile(const std::string& fileId, int version,
            const std::string& newTreeId, int treeVersion,
            const std::string& newFilename, bool overwriteIfPresent,
            const S2SCallback& callback);

        /**
         * Moves a global file to another folder, optionally with a new name.
         * Pass version=-1 to move the latest version. Pass treeVersion=-1 to skip the version check.
         */
        void sysMoveGlobalFile(const std::string& fileId, int version,
            const std::string& newTreeId, int treeVersion,
            const std::string& newFilename, bool overwriteIfPresent,
            const S2SCallback& callback);

        /** Deletes a single global file. Pass version=-1 to delete without a version check. */
        void sysDeleteGlobalFile(const std::string& fileId, int version,
            const std::string& filename, const S2SCallback& callback);

        /**
         * Deletes all global files in the specified folder.
         * Pass treeVersion=-1 to skip the version check. Set recurse=true to include sub-folders.
         */
        void sysDeleteGlobalFiles(const std::string& treeId, const std::string& folderPath,
            int treeVersion, bool recurse, const S2SCallback& callback);

        // -----------------------------------------------------------------------
        // Folder Management
        // -----------------------------------------------------------------------

        /**
         * Creates a new folder at the given path.
         * Pass treeVersion=-1 to skip the version check.
         * Set createInterimDirectories=true to auto-create any missing parent folders.
         */
        void sysCreateFolder(const std::string& folderPath, int treeVersion,
            const std::string& name, const std::string& desc,
            bool createInterimDirectories, const S2SCallback& callback);

        /**
         * Moves a folder to a new path, optionally renaming it.
         * Pass treeVersion=-1 to skip the version check.
         */
        void sysMoveFolder(const std::string& treeId, int treeVersion,
            const std::string& newFolderPath, const std::string& updatedName,
            bool createInterimDirectories, const S2SCallback& callback);

        /**
         * Renames a folder in place.
         * Pass treeVersion=-1 to skip the version check.
         */
        void sysRenameFolder(const std::string& treeId, int treeVersion,
            const std::string& updatedName, const S2SCallback& callback);

        /** Resolves the treeId for a folder given its full path. */
        void sysLookupFolder(const std::string& fullFolderPath, const S2SCallback& callback);

        /**
         * Deletes a folder. Set force=true to also delete any files and sub-folders inside it.
         * Pass treeVersion=-1 to skip the version check.
         */
        void sysDeleteFolder(const std::string& treeId, const std::string& folderPath,
            int treeVersion, bool force, const S2SCallback& callback);

        // -----------------------------------------------------------------------
        // Upload
        // -----------------------------------------------------------------------

        /**
         * Uploads a file to the brainCloud Global File V3 system via S2S.
         *
         * @param treeId              Folder tree ID (use "_root_" for root)
         * @param filename            Name of the file as it will appear in brainCloud
         * @param overwriteIfPresent  Replaces any existing file with the same name when true
         * @param fileData            File content as bytes
         * @param callback            Invoked with the upload result JSON string
         */
        void uploadGlobalFile(const std::string& treeId, const std::string& filename,
            bool overwriteIfPresent, const std::vector<uint8_t>& fileData,
            const S2SCallback& callback);

        // -----------------------------------------------------------------------
        // Lifecycle (called internally by S2SContext)
        // -----------------------------------------------------------------------

        /** Dispatches completed upload callbacks. Called by S2SContext::runCallbacks(). */
        void runCallbacks();

        /** Cancels pending upload callbacks. Called by S2SContext::disconnect(). */
        void disconnect();

    private:
        S2SContext* _s2s;
        std::string _uploadUrl;
        std::atomic<int> _generation;

        struct UploadCompletion
        {
            S2SCallback callback;
            std::string result;
        };

        std::mutex _uploadMutex;
        std::queue<UploadCompletion> _completedUploads;

        void sendFileUpload(const std::string& uploadUrl, const std::string& filename,
            const std::vector<uint8_t>& fileData, const S2SCallback& callback);

        void log(const std::string& message);
    };

} // namespace BrainCloud
