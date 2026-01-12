#include "S2SReporter.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace BrainCloud {

	static std::string toIsoUtc(std::time_t t) {
		std::tm tm;
#if defined(_WIN32)
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
		return oss.str();
	}


	S2SReporter::S2SReporter(const Catch::ReporterConfig& config)
		: Catch::ConsoleReporter(config)
	{
		s2s_log("Collecting data on all tests to initialize json data file");

		const char* buildNumber = getenv("BUILD_NUMBER");
		if (buildNumber == NULL) {
			buildNumber = "0";
		}
		m_outputFile = "test_results_";
		m_outputFile += buildNumber;
		m_outputFile += ".json";

		// record run start time
		m_runStartTime = currentTimeStringUtc();

		// Get tests in the order they will run:
		auto all = Catch::getAllTestCasesSorted(*config.fullConfig());
		m_tests.reserve(all.size());
		for (const auto& handle : all) {
			auto const& info = handle.getTestCaseInfo();
			TestResult t;
			t.name = info.name;       // test case name
			t.sections.clear();
			t.output.clear();
			t.errors.clear();
			t.start_time = "";
			t.end_time = "";
			t.status = "pending";     // initial state
			m_indexByName[t.name] = m_tests.size();
			m_tests.push_back(std::move(t));
		}

		// write initial JSON with all tests pending
		saveResults();
	}

	void S2SReporter::testCaseStarting(Catch::TestCaseInfo const& testInfo)
	{
		s2s_log(">>> Starting TEST_CASE: ", testInfo.name);
		
		
		auto it = m_indexByName.find(testInfo.name);
		if (it == m_indexByName.end()) return;
		TestResult& t = m_tests[it->second];
		t.status = "running";
		t.start_time = currentTimeStringUtc();

		ConsoleReporter::testCaseStarting(testInfo);
		saveResults();
	}

	void S2SReporter::testCaseEnded(Catch::TestCaseStats const& stats)
	{
		if (stats.totals.testCases.failed > 0) {
			s2s_log("<<< Finished TEST_CASE: ", stats.testInfo.name, " [FAILED]");
			s2s_log("<<< error log: ", stats.stdErr);
		}
		else {
			s2s_log("<<< Finished TEST_CASE: ", stats.testInfo.name, " [PASSED] ");
			s2s_log("<<< output log: ", stats.stdOut);
		}
		

		auto it = m_indexByName.find(stats.testInfo.name);
		if (it == m_indexByName.end()) return;
		TestResult& t = m_tests[it->second];
		t.end_time = currentTimeStringUtc();
		t.status = (stats.totals.assertions.failed ? "fail" : "success");

		if (!stats.stdOut.empty()) {
			auto splitLines = splitOutputLines(stats.stdOut);
			for (auto l : splitLines) {
				t.output.push_back(l);
			}
		}
		// stats.stdErr and stats.stdOut are strings in many Catch versions:
		if (!stats.stdErr.empty()) t.errors.push_back(stats.stdErr);

		ConsoleReporter::testCaseEnded(stats);
		saveResults();
	}

	void S2SReporter::sectionStarting(Catch::SectionInfo const& sectionInfo)
	{
		s2s_log(">>> Entering SECTION: ", sectionInfo.name);
		
		
		// Find the currently running test and set section name
		for (auto& t : m_tests) {
			if (t.status == "running") {
				SectionResult section;
				section.name = sectionInfo.name;
				section.line = sectionInfo.lineInfo.line;
				section.file = sectionInfo.lineInfo.file;
				section.start_time = currentTimeStringUtc();
				section.status = "running";
				bool sectionExists = false;
				for (auto& s : t.sections) {
					if (s.name == sectionInfo.name) {
						sectionExists = true;
						break;
					}
				}

				if(!sectionExists) t.sections.push_back(section);
				
				break;
			}
		}

		ConsoleReporter::sectionStarting(sectionInfo);
		saveResults();
	}

	void S2SReporter::sectionEnded(Catch::SectionStats const& stats)
	{
		s2s_log("<<< Finished SECTION: ", stats.sectionInfo.name);
		

		for (auto& t : m_tests) {
			if (t.status == "running") {
				for (auto& s : t.sections) {
					if (s.name == stats.sectionInfo.name) {
						s.end_time = currentTimeStringUtc();
						s.status = stats.assertions.allPassed() ? "success" : "fail";
						s.duration = std::to_string(stats.durationInSeconds);

						s.passed = stats.assertions.passed;
						s.failed = stats.assertions.failed;
						s.total = stats.assertions.total();

					}
				}
				break;
			}
		}
		ConsoleReporter::sectionEnded(stats);
		saveResults();
	}

	bool S2SReporter::assertionEnded(Catch::AssertionStats const& assertionStats)
	{
		const auto result = assertionStats.assertionResult;
		for (auto& t : m_tests) {
			if (t.status == "running") {
				if (result.isOk()) {
					if (!assertionStats.infoMessages.empty()) {
						for (auto im : assertionStats.infoMessages) {
							t.output.push_back(im.message + " " + std::to_string(im.lineInfo.line));
						}
					}
				}
				else {
					if (!assertionStats.infoMessages.empty()) {
						for (auto im : assertionStats.infoMessages) {
							t.errors.push_back(im.message + " " + std::to_string(im.lineInfo.line));
						}
					}

					std::ostringstream ss;
					ss << "[FAILED] " << result.getExpression();

					if (result.hasExpandedExpression())
						ss << " | " << result.getExpandedExpression();

					ss << " (" << result.getSourceInfo().file << ":" << result.getSourceInfo().line << ")";

					// Optionally add any info/capture messages
					for (auto const& msg : assertionStats.infoMessages) {
						ss << "\n  info: " << msg.message;
					}

					t.errors.push_back(ss.str());
				}

				if (assertionStats.assertionResult.hasMessage()) {
					t.output.push_back(assertionStats.assertionResult.getMessage());
				}
				break;
			}
		}

		ConsoleReporter::assertionEnded(assertionStats);

		saveResults();
		return result.isOk();
	}

	std::vector<std::string> S2SReporter::splitOutputLines(const std::string& text)
	{
		std::vector<std::string> lines;
		std::string current;
		bool lastWasBackslash = false;

		for (char c : text)
		{
			if (c == '\n' && !lastWasBackslash)
			{
				if (!current.empty()) {
					lines.push_back(current);
					current.clear();
				}
			}
			else
			{
				current += c;
			}

			lastWasBackslash = (c == '\\');
		}

		if (!current.empty()) {
			lines.push_back(current);
		}

		return lines;
	}

	std::string S2SReporter::currentTimeStringUtc() const
	{
		return toIsoUtc(std::time(nullptr));
	}

	void S2SReporter::saveResults() const
	{
		Json::Value root;
		root["run_start_time"] = m_runStartTime;
		Json::Value tests(Json::arrayValue);
		for (auto const& t : m_tests) {
			Json::Value o;
			o["name"] = t.name;
			o["start_time"] = t.start_time;
			o["end_time"] = t.end_time;
			o["status"] = t.status;

			Json::Value sections(Json::arrayValue);
			for (auto s : t.sections) {
				Json::Value section;
				section["name"] = s.name;
				section["start_time"] = s.start_time;
				section["end_time"] = s.end_time;
				section["duration"] = s.duration;
				section["status"] = s.status;

				sections.append(section);
			}
			o["sections"] = sections;
			for (auto const& e : t.errors) o["errors"].append(e);
			for (auto const& l : t.output) o["output"].append(l);
			tests.append(o);
		}
		root["tests"] = tests;

		std::ofstream ofs(m_outputFile.c_str(), std::ios::trunc);
		if (ofs.is_open()) {
			Json::StreamWriterBuilder w;
			w["indentation"] = "  ";
			ofs << Json::writeString(w, root);
		}
	}
	
}

