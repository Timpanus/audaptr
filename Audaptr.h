#pragma once
#ifndef __Audaptr_h
#define __Audaptr_h
#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

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

/// @brief Type of audio IO
enum class IOType {
	Input,
	Output,
	Duplex,
	Any
};

/// @brief String representation of the PortAudio version
const std::string PortAudioVersion();

/// @brief Helper function to determine whether one string is found in another (case-insensitive)
/// @param strInput The string to be found
/// @param strSearch The string in which to search
/// @return true if the string was found, else false
bool StringContains(const std::string &strInput, const std::string &strSearch);

/// @brief Class describing binding of an audio stream to a particular system, device, sample rate, format etc.
class Binding {
public:
	/// @brief Audio stream binding
	/// @param strSystem Audio system name
	/// @param strDevice Audio device name
	/// @param Type Audio stream type (input, output, duplex, any)
	/// @param DeviceInfo PortAudio descriptor for the device
	/// @param vdSampleRates_Hz Sample rates supported
	/// @param iDeviceIndex Index associated with the device
	Binding(std::string strSystem, std::string strDevice, IOType Type, PaDeviceInfo DeviceInfo, std::vector<double> vdSampleRates_Hz, const int iDeviceIndex);

	/// @brief The audio system name
	const std::string & SystemName() const;

	/// @brief The audio device name
	const std::string & DeviceName() const;

	/// @brief The device index
	const int DeviceIndex() const;

	/// @brief String description of the stream type
	const std::string &TypeName() const;

	/// @brief Vector of supported sample rates
	const std::vector<double> & SampleRates() const;

	/// @brief Maximum number of channels for the particular type
	int MaxChannels();
	
	/// @brief Supported types
	static const std::vector<std::string> TypeStrings;

	/// @brief System name
	std::string System_;

	/// @brief Device name
	std::string Device_;

	/// @brief Device type (input, output, duplex, any)
	IOType Type_;

	/// @brief Minimum latency
	double LatencyMin_s_ = 0.0;

	/// @brief Maximum latency
	double LatencyMax_s_ = 0.0;

	/// @brief  Suported sample rates
	std::vector<double> SampleRates_Hz_;

	/// @brief Default sample rate
	double DefaultSampleRate_Hz_ = 0.0;

	/// @brief Device index
	int DeviceIndex_ = -1;

	/// @brief Number of chanels in use
	int NumChannels_ = -1;

	/// @brief Sample format
	PaSampleFormat SampleFormat_;

	/// @brief Current latency
	double Latency_s_ = 0.0;
};

class Map {
public:
	/// @brief Selector based on a vector of possible system names
	/// @param vstrSystems The system names
	/// @return A map of possible audio devices, filtered according to the systems specified
	Map System(const std::vector<std::string> & vstrSystems = {}) const;

	/// @brief Selector based on a single system name
	/// @param strSystem The system name
	/// @return A map of possible audio devices, filtered according to the system specified
	Map System(std::string &strSystem) const;

	/// @brief Selector based on a vector of possible device names
	/// @param vstrDevices The device names
	/// @return A map of possible audio devices, filtered according to the devices specified
	Map Device(const std::vector<std::string> & vstrDevices = {}) const;

	/// @brief Selector based on a single device name
	/// @param strDevices The device name
	/// @return A map of possible audio devices, filtered according to the device specified
	Map Device(std::string &strDevices) const;

	/// @brief Selector based on a vector of desired sample rates
	/// @param vstrSystems The sample rates for filtering
	/// @return A map of possible audio devices, filtered according to the specified sample rates
	Map SampleRate(const std::vector<double> & vdSampleRates_Hz = {}) const;

	/// @brief Selector based on a single sample rate
	/// @param vstrSystems The sample rate for filtering
	/// @return A map of possible audio devices, filtered according to the specified sample rate
	Map SampleRate(const double dSampleRate_Hz) const;

	/// @brief Selector based on a vector of desired stream type
	/// @param vstrSystems The stream type for filtering
	/// @return A map of possible audio devices, filtered according to the specified stream type
	Map Type(const IOType Type) const;

	/// @brief A binding associated with the default input device
	Binding DefaultInput();

	/// @brief  A binding associated with the default output device
	Binding DefaultOutput();

	/// @brief A vector of system names in the audio map
	std::vector<std::string> Systems();

	/// @brief A vector of device names in the map 
	std::vector<std::string> Devices();

	/// @brief A vector of sample rates in the map
	std::vector<double> SampleRates();
	
	/// @brief Various helper functions for iterating through the audio map
	/// @return 
	auto begin() { return Bindings_.begin(); }
	auto end() { return Bindings_.end(); }
	auto cbegin() const { return Bindings_.begin(); }
	auto cend() const { return Bindings_.end(); }
	auto begin() const { return Bindings_.begin(); }
	auto end() const { return Bindings_.end(); }
	auto empty() const { return Bindings_.empty(); }

	auto front() { return Bindings_.front(); }
	auto back() { return Bindings_.back(); }
	auto size() const { return Bindings_.size(); }
	Binding & operator[](int n) { return Bindings_[n]; }

	operator std::vector<Binding>() &
	{
		return Bindings_;
	}
	
	operator Binding() const { return Bindings_.front(); }
	
	std::vector<Binding> Bindings_;

	/// @brief Default constructor
	/// @param bMapDevices 
	Map(bool bMapDevices = false);

	virtual ~Map();
		
	void MapAudioSystem();
	
protected:
	Binding DefaultInputDevice_{"", "", IOType::Input, PaDeviceInfo(), {}, -1};

	Binding DefaultOutputDevice_{"", "", IOType::Input, PaDeviceInfo(), {}, -1};
};

Map &Devices();

class IO
{
	IO(Binding & DeviceToUse);

	bool Open(IOType StreamType);

	/// Open the audio device for use in input/output full-duplex mode
	bool OpenDuplex(int iNumInputChannels = 1, int iNumOutputChannels = 0);

	/// Open the input audio device
	bool OpenInput();

	/// Open the output audio device
	bool OpenOutput(int iNumOutputChannels = 1);

	/// Close the audio device
	bool Close();

	/// Start audio I/O processing
	bool StartInput(unsigned uBufferLevel);

	/// Stop audio I/O processing
	bool Stop();

	bool Started()
	{
		return (bool)(Pa_IsStreamStopped(pPaStream_) == 0);
	}

	const std::string Status()
	{
		return Status_;
	}

	///

	/*bool AsioEnabled()
	{
		return ((Pa_GetHostApiInfo(Pa_GetDeviceInfo(InputParams_.device)->hostApi)->type == paASIO) ||
				(Pa_GetHostApiInfo(OutputParams_.device)->type == paASIO));
	}*/
	
	void ResetHostParams()
	{
		pHostInputParams_ = nullptr;
		pHostOutputParams_ = nullptr;
	}

#ifdef paAsioUseChannelSelectors
	/// @param[in] hWindow Handle to main application window, where hwnd is of type HWND (cast to void*)
	void ShowAsioControlPanel(void * hWindow);
	
	void SetAsioHostParams(const std::vector<int> & viInputChannels, const std::vector<int> & viOutputChannels = {});
	
	std::vector<int> AsioInputChannels_;
	
	std::vector<int> AsioOutputChannels_;
	
	PaAsioStreamInfo AsioInputStreamInfo_;
	
	PaAsioStreamInfo AsioOutputStreamInfo_;
#endif

public:
	
	/// @brief Input stream parameters
	PaStreamParameters InputParams_;
	
	/// @brief Output stream parameters
	PaStreamParameters OutputParams_;
	
	/// @brief Host-specific API input stream parameters
	void * pHostInputParams_ = nullptr;
	
	/// @brief Host-specific API input stream parameters
	void * pHostOutputParams_ = nullptr;

	/// Circular threadsafe input buffer
	QuickBuffer<float> InputBuffer_;

	/// Circular threadsafe output buffer
	QuickBuffer<float> OutputBuffer_;

	/// Flag indicating the count of input buffer overflows that have occurred
	int InputOverflowCount_;

	/// Flag indicating the count of input buffer overflows that have occurred
	int OutputOverflowCount_;
	
	uint32_t SampleRate_Hz_;

protected:
	template<typename T>
	std::string to_string_precision(const T a_value, const int n = 6)
	{
		std::ostringstream out;
		out.precision(n);
		out << std::fixed << a_value;
		return std::move(out).str();
	}

	Binding & Binding_;

	/// Flag incremented upon a successful call to Pa_Initialize
	int m_iPaInitFlag = 0;

	/// Pointer to a PortAudio stream
	PaStream * pPaStream_ = nullptr;
	
	///////

	/// Update the status string associated with the device
	void UpdateStatus();

	/// Last known status of the audio API
	std::string Status_;
};

}

#endif
