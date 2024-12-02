#pragma once

#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

//#include "AudioMap.h"
#include "QuickBuffer.h"

// PortAudio preprocessor definitions
/*#define PA_USE_ASIO 1
#define PA_USE_DS 1			// (DirectSound)
#define PA_USE_WMME	1		// (MME)
#define PA_USE_WASAPI 1
#define PA_USE_WDMKS 1
#define PA_USE_SKELETON 1*/

#include <portaudio.h>
#ifdef _MSC_VER
#include <pa_asio.h>
#endif

namespace Audaptr {
	
	/// Standard sample rates
	static constexpr std::array<double, 13> StandardSampleRates_Hz = {8000.0, 11025.0, 16000.0, 22050.0, 32000.0, 44100.0, 48000.0,
		88200.0, 96000.0, 176400.0, 192000.0, 352800.0, 384000.0};

	/// @brief Type of audio IO
	enum class IOType {
		Input,
		Output,
		Duplex
	};

	extern const std::vector<std::string> IOTypeNames;

	/// @brief Map of error strings
	extern std::map<int, const std::string> g_mapPaError;

	/// @brief Map of host system names
	extern std::map<int, const std::string> g_mapPaHostSystem;

	/// @brief Determine the descriptive error associated with a PortAudio error string
	/// @param iErrorCode The code number
	/// @return A string describing the error
	std::string PaErrorString(PaError iErrorCode);

	/// @brief Specialisation of runtime_error for exception handling.
	class Exception : public std::runtime_error
	{
	public:
		Exception(const std::string& What) :
			std::runtime_error(What + " (" + std::string(Pa_GetVersionInfo()->versionText) + ")")
		{}
	};

	/// @brief String representation of the PortAudio version
	const std::string PortAudioVersion();

	/// @brief Helper function to determine whether one string is found in another (case-insensitive)
	/// @param strInput The string to be found
	/// @param strSearch The string in which to search
	/// @return true if the string was found, else false
	bool StringContains(const std::string &strInput, const std::string &strSearch);

}
