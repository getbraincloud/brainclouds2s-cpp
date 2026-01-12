#include "S2SReporter.h"

namespace BrainCloud {

	S2SReporter::S2SReporter(Catch::ReporterConfig const& config)
		: Catch::ConsoleReporter(config)
	{}

	void S2SReporter::testCaseStarting(Catch::TestCaseInfo const& testInfo)
	{
		s2s_log(">>> Starting TEST_CASE: ", testInfo.name);
		ConsoleReporter::testCaseStarting(testInfo);
	}

	void S2SReporter::testCaseEnded(Catch::TestCaseStats const& stats)
	{
		if (stats.totals.testCases.failed > 0) {
			s2s_log("<<< Finished TEST_CASE: ", stats.testInfo.name, " [FAILED]");
		}
		else {
			s2s_log("<<< Finished TEST_CASE: ", stats.testInfo.name, " [PASSED]");
		}
		ConsoleReporter::testCaseEnded(stats);
	}

	void S2SReporter::sectionStarting(Catch::SectionInfo const& sectionInfo)
	{
		s2s_log(">>> Entering SECTION: ", sectionInfo.name);
		ConsoleReporter::sectionStarting(sectionInfo);
	}

	void S2SReporter::sectionEnded(Catch::SectionStats const& stats)
	{
		s2s_log("<<< Finished SECTION: ", stats.sectionInfo.name);
		ConsoleReporter::sectionEnded(stats);
	}
	
}

