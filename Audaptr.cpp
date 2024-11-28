#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <utility>

#include "Audaptr.h"
#include "AudIO.h"
#include "AudioMap.h"
#include "QuickBuffer.h"

using namespace std;

namespace Audaptr {

/*#ifndef _MSC_VER
#include <strings.h>
#define _stricmp strcasecmp
#endif*/

string PaErrorString(PaError iErrorCode)
{
	auto iterError = g_mapPaError.find(iErrorCode);
	if(iterError == g_mapPaError.end())
		return "Unknown Audio API error";
	return iterError->second;
}

const std::string PortAudioVersion()
{
	static const std::string strPaVersion(Pa_GetVersionInfo()->versionText);
	return strPaVersion;
}

Binding AudioMap::DefaultInputDevice_{"", "", IOType::Input, PaDeviceInfo(), {}, -1};

Binding AudioMap::DefaultOutputDevice_{"", "", IOType::Output, PaDeviceInfo(), {}, -1};

const std::vector<std::string> Binding::TypeStrings({ "Input", "Output", "Duplex", "Any" });

bool StringContains(const string & strInput, const string & strSearch)
{
	size_t pos = 0;
	string strInputLower = strInput;
	string strSearchLower = strSearch;
    // Convert complete given String to lower case
    transform(strInputLower.begin(), strInputLower.end(), strInputLower.begin(), ::tolower);
    // Convert complete given Sub String to lower case
    transform(strSearchLower.begin(), strSearchLower.end(), strSearchLower.begin(), ::tolower);
    // Find sub string in given string
    return strInputLower.find(strSearchLower, pos) != string::npos;
}

map<int, const string> g_mapPaHostSystem = {
    {0,		"In Development" },
	{1,		"DirectSound" },
	{2,		"MME" },
	{3,		"ASIO" },
	{4,		"SoundManager" },
	{5,		"CoreAudio" },
	{7,		"OSS" },
	{8,		"ALSA" },
	{9,		"AL" },
	{10,	"BeOS" },
	{11,	"WDMKS" },
	{12,	"JACK" },
	{13,	"WASAPI" },
	{14,	"AudioScienceHPI"}
};

map<int, const string> g_mapPaError = {
	{0,			"NoError" },
	{-10000,	"NotInitialized" },
	{-9999,		"UnanticipatedHostError" },
	{-9998,		"InvalidChannelCount" },
	{-9997,		"InvalidSampleRate" },
	{-9996,		"InvalidInputDevice" },
	{-9995,		"InvalidFlag" },
	{-9994,		"SampleFormatNotSupported" },
	{-9993,		"BadIOInputDeviceCombination" },
	{-9992,		"InsufficientMemory" },
	{-9991,		"BufferTooBig" },
	{-9990,		"BufferTooSmall" },
	{-9989,		"NullCallback" },
	{-9988,		"BadStreamPtr" },
	{-9987,		"TimedOut" },
	{-9986,		"InternalError" },
	{-9985,		"InputDeviceUnavailable" },
	{-9984,		"IncompatibleHostApiSpecificStreamInfo" },
	{-9983,		"StreamIsStopped" },
	{-9982,		"StreamIsNotStopped" },
	{-9981,		"InputOverflowed" },
	{-9980,		"OutputUnderflowed" },
	{-9979,		"HostApiNotFound" },
	{-9978,		"InvalidHostApi" },
	{-9977,		"CanNotReadFromACallbackStream" },
	{-9976,		"CanNotWriteToACallbackStream" },
	{-9975,		"CanNotReadFromAnOutputOnlyStream" },
	{-9974,		"CanNotWriteToAnInputOnlyStream" },
	{-9973,		"IncompatibleStreamHostApi" },
	{-9972,		"BadBufferPtr"}
};

}
