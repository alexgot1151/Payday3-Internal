#include "Logging.hpp"
#include <iostream>
#include <fstream>
#include <ios>
#include <system_error>
#include <chrono>
#include <Windows.h>


namespace colors {
    inline constexpr auto reset         = "\033[0m";
    inline constexpr auto bold          = "\033[1m";
    inline constexpr auto faint         = "\033[2m";
    inline constexpr auto italic        = "\033[3m";
    inline constexpr auto underline     = "\033[4m";
    inline constexpr auto blink         = "\033[5m";
    inline constexpr auto invisible     = "\033[6m";
    inline constexpr auto strikethrough = "\033[7m";
    // Foreground color.
    inline constexpr auto grey           = "\033[30m";
    inline constexpr auto red            = "\033[31m";
    inline constexpr auto green          = "\033[32m";
    inline constexpr auto yellow         = "\033[33m";
    inline constexpr auto blue           = "\033[34m";
    inline constexpr auto magenta        = "\033[35m";
    inline constexpr auto cyan           = "\033[36m";
    inline constexpr auto white          = "\033[37m";
    inline constexpr auto bright_grey    = "\033[90m";
    inline constexpr auto bright_red     = "\033[91m";
    inline constexpr auto bright_green   = "\033[92m";
    inline constexpr auto bright_yellow  = "\033[93m";
    inline constexpr auto bright_blue    = "\033[94m";
    inline constexpr auto bright_magenta = "\033[95m";
    inline constexpr auto bright_cyan    = "\033[96m";
    inline constexpr auto bright_white   = "\033[97m";
    // Background color
    inline constexpr auto on_grey           = "\033[40m";
    inline constexpr auto on_red            = "\033[41m";
    inline constexpr auto on_green          = "\033[42m";
    inline constexpr auto on_yellow         = "\033[43m";
    inline constexpr auto on_blue           = "\033[44m";
    inline constexpr auto on_magenta        = "\033[45m";
    inline constexpr auto on_cyan           = "\033[46m";
    inline constexpr auto on_white          = "\033[47m";
    inline constexpr auto on_bright_grey    = "\033[100m";
    inline constexpr auto on_bright_red     = "\033[101m";
    inline constexpr auto on_bright_green   = "\033[102m";
    inline constexpr auto on_bright_yellow  = "\033[103m";
    inline constexpr auto on_bright_blue    = "\033[104m";
    inline constexpr auto on_bright_magenta = "\033[105m";
    inline constexpr auto on_bright_cyan    = "\033[106m";
    inline constexpr auto on_bright_white   = "\033[107m";
};

// I hate this.
static std::string ConvertFunctionName(const char* szFunctionName)
{
	std::string sFnName{ szFunctionName };

	size_t iEnd = sFnName.find_last_of('(');
	if (iEnd == std::string::npos) {
		iEnd = sFnName.size();
	}

	int iTemplates = 0;

	bool bDone = false;

	if (iEnd != sFnName.size()) {
		for (size_t i = iEnd - 1; i > 0 && !bDone; --i) {
			const char c = sFnName[i];

			switch (c) {
			case('<'):
				iTemplates--;
				break;
			case('>'):
				iTemplates++;
				break;

			default:
				if (iTemplates == 0) {
					bDone = true;
					iEnd = i;
				}
				break;
			}
		}
	}

	iTemplates = 0;
	bool bHasValidChar = false;
	size_t iStart = 0;

	for (size_t i = iEnd; i > 0 && iStart == 0; --i) {
		const char c = sFnName[i];
		switch (c) {
		case('<'):
			bHasValidChar = false;
			iTemplates--;
			break;
		case('>'):
			bHasValidChar = false;
			iTemplates++;
			break;
		case(' '):
			if (bHasValidChar) {
				iStart = i + 1;
			}
			break;

		case(';'):
		case(':'):
		case('('):
		case(')'):
		case('{'):
		case('}'):
			bHasValidChar = false;
			break;

		default:
			if (iTemplates == 0) {
				bHasValidChar = true;
			}
			break;
		}
	}

	return std::string(sFnName, iStart, iEnd - iStart + 1);
}

static std::string GetLocationString(std::source_location location) 
{
	return std::format("[{}{}{} ({}{}{},{}{}{})]{}{} |",
		colors::green, std::filesystem::path(location.file_name()).filename().string(), colors::white,
		colors::magenta, location.line(), colors::white, 
		colors::magenta, location.column(), colors::white,
		colors::green, ConvertFunctionName(location.function_name()), colors::white);
}

static std::string GetColorlessLocationString(std::source_location location) 
{
	return std::format("{} | {} -> Ln: {} Col: {} |", location.file_name(), ConvertFunctionName(location.function_name()), location.line(), location.column());
}

static void LogError(const std::string& sErrorMessage, std::source_location location = std::source_location::current()) {
	// Error: [$Filename ($Ln,$Col)] $Function | Info: $Message
	std::cout << std::format("{}Error{}: {} {}Info{}: {}\n", colors::red, colors::white, GetLocationString(location), colors::yellow, colors::white, sErrorMessage);
}

static void LogDebug(const std::string& sDebugMessage, std::source_location location = std::source_location::current()) {
	// Debug: [$Filename ($Ln,$Col)] $Function | Info: $Message
	std::cout << std::format("{}Debug{}: {} {}Info{}: {}\n", colors::cyan, colors::white, GetLocationString(location), colors::yellow, colors::white, sDebugMessage);
}

namespace Utils
{
	std::optional<std::filesystem::path> GetLogFilePath(const std::string& sLogFileExtension)
	{
		try {
			// Get the current executable's directory
			char szModulePath[MAX_PATH];
			if (GetModuleFileNameA(nullptr, szModulePath, MAX_PATH) == 0) {
				return std::nullopt;
			}

			std::filesystem::path exePath(szModulePath);
			std::filesystem::path logDir = exePath.parent_path() / "Logs";
			
			// Create logs directory if it doesn't exist
			if (!std::filesystem::exists(logDir)) {
				std::filesystem::create_directories(logDir);
			}

			// Create log filename with timestamp
			auto now = std::chrono::system_clock::now();
			auto time_t = std::chrono::system_clock::to_time_t(now);
			
			std::tm tm;
			localtime_s(&tm, &time_t);
			
			std::string logFileName = std::format("log_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}{}",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, sLogFileExtension);

			return logDir / logFileName;
		}
		catch (...) {
			return std::nullopt;
		}
	}

	void LogHook(const std::string& sHookName, const MH_STATUS eStatus, std::source_location location)
	{
	#ifdef _DEBUG
		if (eStatus == MH_OK) {
			// Hook[$HookName]: [$Filename ($Ln,$Col)] $Function | Success!
			std::cout << std::format("{}Hook[{}]{}: {} {}Success{}!\n", colors::cyan, sHookName, colors::white, GetLocationString(location), colors::green, colors::white);
			return;
		}

		// Hook[$HookName]: [$Filename ($Ln,$Col)] $Function | Failure: $Status
		std::cout << std::format("{}Hook[{}]{}: {} {}Failure{}: {}\n", colors::cyan, sHookName, colors::white, GetLocationString(location), colors::yellow, colors::white, MH_StatusToString(eStatus));
	#endif
	}

	void LogHook(const std::string& sHookName, const std::string& sReason, const std::string& sMessage, std::source_location location)
	{
	#ifdef _DEBUG
		// Hook[$HookName]: [$Filename ($Ln,$Col)] $Function | $Reason: $Message
		std::cout << std::format("{}Hook[{}]{}: {} {}{}{}: {}\n", colors::cyan, sHookName, colors::white, GetLocationString(location), colors::yellow, sReason, colors::white, sMessage);
	#endif
	}

	void LogError(const int iErrorCode, std::source_location location) 
	{
	#ifdef _DEBUG
		::LogError(std::system_category().message(iErrorCode), location);

		std::filesystem::path pathError{};
		{
			auto optPath = Utils::GetLogFilePath("_ERRORS.log");
			if (!optPath) {
				::LogError("Failed to find ERROR log file path", std::source_location::current());
				return;
			}

			pathError = optPath.value();
		}

		std::ofstream fileError(pathError, std::ios::app);
		if (!fileError.is_open() || fileError.fail())
		{
			::LogError("Failed to open ERROR log file for writing", std::source_location::current());
			return;
		}

		fileError << std::format("Error: {} Info: {}\n", GetColorlessLocationString(location), std::system_category().message(iErrorCode));
		fileError.close();
	#endif
	}

	void LogError(const std::string& sErrorMessage, std::source_location location)
	{
	#ifdef _DEBUG
		::LogError(sErrorMessage, location);

		std::filesystem::path pathError{};
		{
			auto optPath = Utils::GetLogFilePath("_ERRORS.log");
			if (!optPath) {
				::LogError("Failed to find ERROR log file path", std::source_location::current());
				return;
			}

			pathError = optPath.value();
		}

		std::ofstream fileError(pathError, std::ios::app);
		if (!fileError.is_open() || fileError.fail())
		{
			::LogError("Failed to open ERROR log file for writing", std::source_location::current());
			return;
		}

		fileError << std::format("Error: {} Info: {}\n", GetColorlessLocationString(location), sErrorMessage);
		fileError.close();
	#endif
	}

	void LogDebug(const std::string& sDebugMessage, std::source_location location)
	{
	#ifdef _DEBUG
		::LogDebug(sDebugMessage, location);

		std::filesystem::path pathDebug{};
		{
			auto optPath = Utils::GetLogFilePath(".log");
			if (!optPath) {
				::LogError("Failed to find DEBUG log file path", std::source_location::current());
				return;
			}

			pathDebug = optPath.value();
		}

		std::ofstream fileDebug(pathDebug, std::ios::app);
		if (!fileDebug.is_open() || fileDebug.fail())
		{
			::LogError("Failed to open DEBUG log file for writing", std::source_location::current());
			return;
		}

		fileDebug << std::format("Debug: {} Info: {}\n", GetColorlessLocationString(location), sDebugMessage);
		fileDebug.close();
	#endif
	}
}