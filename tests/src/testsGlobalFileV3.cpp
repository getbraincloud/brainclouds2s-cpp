#include "tests.h"
#include "catch.hpp"
#include <brainclouds2s-globalfilev3.h>
#include <json/json.h>
#include <chrono>
#include <string>
#include <vector>

using namespace BrainCloud;

///////////////////////////////////////////////////////////////////////////////
// GlobalFileV3 integration tests
//
// Tests run sequentially within a single TEST_CASE to share state
// (treeId, fileId, fileVersion) across the create → upload → query →
// mutate → cleanup sequence.
///////////////////////////////////////////////////////////////////////////////

namespace
{
    // Poll runCallbacks until the callback fires or 30s timeout.
    // Returns the JSON result string; empty string on timeout.
    static std::string runGFV3(S2SContextRef pContext,
                               std::function<void(S2SCallback)> call)
    {
        bool done = false;
        std::string result;

        call([&](const std::string &r)
             {
            result = r;
            done = true; });

        auto start = std::chrono::system_clock::now();
        while (!done)
        {
            pContext->runCallbacks(100);
            if (std::chrono::system_clock::now() - start > std::chrono::seconds(30))
                break;
        }
        return result;
    }

    // Parse JSON and return status code; 0 on parse failure
    static int getStatus(const std::string &json)
    {
        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(json, data))
            return 0;
        return data["status"].asInt();
    }
}

TEST_CASE("GlobalFileV3", "[GFV3]")
{
    loadIdsIfNot();

    auto pContext = S2SContext::create(
        BRAINCLOUD_APP_ID,
        BRAINCLOUD_SERVER_NAME,
        BRAINCLOUD_SERVER_SECRET,
        BRAINCLOUD_SERVER_URL,
        false);
    pContext->setLogEnabled(true);

    // Authenticate once for the full sequence
    bool authOk = runAuth(pContext);
    REQUIRE(authOk);

    BrainCloudS2SGlobalFileV3 *gfv3 = pContext->getGlobalFileV3();
    REQUIRE(gfv3 != nullptr);

    std::string gfv3FolderTreeId;
    std::string gfv3FileId;
    int gfv3FileVersion = 1;

    // -----------------------------------------------------------------------
    // Test 01: SysGetGlobalFileList
    // -----------------------------------------------------------------------
    SECTION("01 SysGetGlobalFileList")
    {
        std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                     { gfv3->sysGetGlobalFileList("s2s_test_folder", true, cb); });
        REQUIRE(getStatus(result) == 200);
    }

    // -----------------------------------------------------------------------
    // Test 02–14: Full create → upload → query → mutate → cleanup sequence
    // -----------------------------------------------------------------------
    SECTION("02-14 Full GFV3 sequence")
    {
        // 02: SysLookupFolder
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysLookupFolder("s2s_test_folder", cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 03: SysCreateFolder — captures treeId
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysCreateFolder(
                                               "s2s_test_folder", -1, "s2s_test_folder_2",
                                               "S2S integration test folder", false, cb); });
            REQUIRE(getStatus(result) == 200);

            Json::Value data;
            Json::Reader reader;
            reader.parse(result, data);
            gfv3FolderTreeId = data["data"]["createdTreeId"].asString();
            REQUIRE_FALSE(gfv3FolderTreeId.empty());
        }

        // 04: UploadGlobalFile — captures fileId / version
        {
            std::string fileContent = "Hello from brainCloud S2S C++ file upload test!";
            std::vector<uint8_t> fileData(fileContent.begin(), fileContent.end());

            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->uploadGlobalFile(gfv3FolderTreeId, "s2s_test_file.txt",
                                                                  true, fileData, cb); });
            REQUIRE(getStatus(result) == 200);

            Json::Value data;
            Json::Reader reader;
            reader.parse(result, data);
            const Json::Value &fd = data["data"]["fileDetails"]["fileDetails"];
            gfv3FileId = fd["fileId"].asString();
            gfv3FileVersion = fd["version"].asInt();
            REQUIRE_FALSE(gfv3FileId.empty());
        }

        // 05: SysGetFileInfo
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysGetFileInfo(gfv3FileId, cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 06: SysGetFileInfoSimple
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysGetFileInfoSimple(
                                               "s2s_test_folder/s2s_test_folder_2", "s2s_test_file.txt", cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 07: SysCheckFilenameExists
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysCheckFilenameExists(
                                               "s2s_test_folder/s2s_test_folder_2", "s2s_test_file.txt", cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 08: SysCheckFullpathFilenameExists
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysCheckFullpathFilenameExists(
                                               "s2s_test_folder/s2s_test_folder_2/s2s_test_file.txt", cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 09: SysGetGlobalCDNUrl
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysGetGlobalCDNUrl(gfv3FileId, cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 10: SysCopyGlobalFile
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysCopyGlobalFile(
                                               gfv3FileId, gfv3FileVersion, gfv3FolderTreeId, -1,
                                               "s2s_file_copy.txt", true, cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 11: SysMoveGlobalFile
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysMoveGlobalFile(
                                               gfv3FileId, gfv3FileVersion, gfv3FolderTreeId, -1,
                                               "s2s_file_moved.txt", true, cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 12: SysRenameFolder
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysRenameFolder(gfv3FolderTreeId, -1, "s2s_test_folder_renamed", cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 13: SysDeleteGlobalFiles — cleanup files
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysDeleteGlobalFiles(
                                               gfv3FolderTreeId, "s2s_test_folder/s2s_test_folder_renamed",
                                               -1, true, cb); });
            REQUIRE(getStatus(result) == 200);
        }

        // 14: SysDeleteFolder — cleanup folder
        {
            std::string result = runGFV3(pContext, [&](S2SCallback cb)
                                         { gfv3->sysDeleteFolder(
                                               gfv3FolderTreeId, "s2s_test_folder/s2s_test_folder_renamed",
                                               -1, true, cb); });
            REQUIRE(getStatus(result) == 200);
        }
    }
}
