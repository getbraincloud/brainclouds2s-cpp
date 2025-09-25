#pragma once
#include <string>

#define CATCH_CONFIG_EXTERNAL_INTERFACES
#include <catch.hpp>
#include <brainclouds2s.h>

namespace BrainCloud {
    class S2SReporter : public Catch::ConsoleReporter {
    public:
        explicit S2SReporter(Catch::ReporterConfig const& config);

        void testCaseStarting(Catch::TestCaseInfo const& testInfo) override;
        void testCaseEnded(Catch::TestCaseStats const& stats) override;
        void sectionStarting(Catch::SectionInfo const& sectionInfo) override;
        void sectionEnded(Catch::SectionStats const& stats) override;
    };

    CATCH_REGISTER_REPORTER("s2s", S2SReporter)
}
