#pragma once
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <json/json.h> // jsoncpp (or use your favorite json lib)

#define CATCH_CONFIG_EXTERNAL_INTERFACES
#include <catch.hpp>
#include <brainclouds2s.h>

namespace BrainCloud {

    struct SectionResult {
        std::string name;
        std::string start_time;
        std::string end_time;
        std::string duration;
        std::string status;
        std::string line;
        std::string file;
        int passed;
        int failed;
        int total;
    };

    struct TestResult {
        std::string name;
        std::string start_time;
        std::string end_time;
        std::string status;
        std::vector<std::string> output;
        std::vector<std::string> errors;
        std::vector<SectionResult> sections;
    };


    class S2SReporter : public Catch::ConsoleReporter {
    public:
        S2SReporter(const Catch::ReporterConfig& config);

        void testCaseStarting(Catch::TestCaseInfo const& testInfo) override;
        void testCaseEnded(Catch::TestCaseStats const& stats) override;
        void sectionStarting(Catch::SectionInfo const& sectionInfo) override;
        void sectionEnded(Catch::SectionStats const& stats) override;
        bool assertionEnded(Catch::AssertionStats const& assertionStats) override;

        Catch::ReporterPreferences getPreferences() const override{
            Catch::ReporterPreferences prefs;
            prefs.shouldRedirectStdOut = true;
            prefs.shouldReportAllAssertions = true;
            return prefs;
        }

    private:

        std::vector<std::string> splitOutputLines(const std::string& text);
        //Catch::ReporterConfig const& m_config;
        std::vector<TestResult> m_tests;                       // ordered list of tests
        std::unordered_map<std::string, size_t> m_indexByName; // name -> index into m_tests
        std::string m_outputFile = "test_results.json";
        std::string m_runStartTime;

        std::string currentTimeStringUtc() const;
        void saveResults() const;
    };

    CATCH_REGISTER_REPORTER("s2s", S2SReporter)
}
