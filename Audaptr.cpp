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

// Called by the PortAudio engine when audio is needed, possibly at interrupt level. Do not block.
int g_pfnInputPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long lFramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
{
	// StatusFlags: paInputUnderflow, paInputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
	IO *pAudioIO = (IO *)pUserData;

	size_t uNumToRead = (size_t)lFramesPerBuffer * pAudioIO->InputParams_.channelCount;

	// If no free space in the input buffer, increment a count to record the overflow; do not block in this callback.
	auto *pfBuffer = pAudioIO->InputBuffer_.WriteReserve(uNumToRead);
	if(!pfBuffer || (StatusFlags & paInputOverflow))
		pAudioIO->InputOverflowCount_++;
	else {
		memcpy(pfBuffer, pInputBuffer, uNumToRead * sizeof(float));
		pAudioIO->InputBuffer_.WriteCommit(uNumToRead);
	}
	if(!pAudioIO->InputBuffer_.IsOpen())
		return paComplete;
	return paContinue;
}

int g_pfnOutputPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long lFramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
{
	// StatusFlags: paOutputUnderflow, paOutputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
	IO *pAudioIO = (IO *)pUserData;
	size_t uNumToWrite = (size_t)lFramesPerBuffer * pAudioIO->OutputParams_.channelCount, uAvailable;

	auto *pfRead = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
	size_t uThisWrite = (uAvailable > uNumToWrite) ? uNumToWrite : uAvailable;
	memcpy(pOutputBuffer, pfRead, uThisWrite * sizeof(float));
	uNumToWrite -= uThisWrite;
	pOutputBuffer = (float *)pOutputBuffer + uThisWrite;
	// This check is necessary because the buffer may segment read transactions.
	if((uNumToWrite > 0) && pAudioIO->OutputBuffer_.IsOpen()) {
		pfRead = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
		uThisWrite = (uAvailable > uNumToWrite) ? uNumToWrite : uAvailable;
		memcpy(pOutputBuffer, pfRead, uThisWrite * sizeof(float));
		uNumToWrite -= uThisWrite;
		pOutputBuffer = (float *)pOutputBuffer + uThisWrite;
		pAudioIO->OutputBuffer_.ReadRelease(uThisWrite);
	}
	// If no free space in the output buffer, increment a count to record the overflow; do not block in this callback.
	if((uNumToWrite > 0) || (StatusFlags & paOutputOverflow))
		pAudioIO->OutputOverflowCount_++;
	if(!pAudioIO->OutputBuffer_.IsOpen())
		return paComplete;
	return paContinue;
}

int g_pfnDuplexPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long lFramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
{
	// TODO: Check StatusFlags: paInputUnderflow, paInputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
	IO *pAudioIO = (IO *)pUserData;

	size_t uNumToWrite = (size_t)lFramesPerBuffer * pAudioIO->InputParams_.channelCount;
	size_t uNumToRead = (size_t)lFramesPerBuffer * pAudioIO->OutputParams_.channelCount, uAvailable;

	auto * pfBuff = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
	size_t uThisWrite = (uAvailable > uNumToWrite) ? uNumToWrite : uAvailable;
	memcpy(pOutputBuffer, pfBuff, uThisWrite * sizeof(float));
	uNumToWrite -= uThisWrite;
	pOutputBuffer = (float *)pOutputBuffer + uThisWrite;
	// This check is necessary because the buffer may segment read transactions.
	if((uNumToWrite > 0) && pAudioIO->OutputBuffer_.IsOpen()) {
		pfBuff = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
		uThisWrite = (uAvailable > uNumToWrite) ? uNumToWrite : uAvailable;
		memcpy(pOutputBuffer, pfBuff, uThisWrite * sizeof(float));
		uNumToWrite -= uThisWrite;
		pOutputBuffer = (float *)pOutputBuffer + uThisWrite;
		pAudioIO->OutputBuffer_.ReadRelease(uThisWrite);
	}
	// If no free space in the output buffer, increment a count to record the overflow; do not block in this callback.
	if((uNumToWrite > 0) || (StatusFlags & paOutputOverflow))
		pAudioIO->OutputOverflowCount_++;

	// If no free space in the input buffer, increment a count to record the overflow; do not block in this callback.
	pfBuff = pAudioIO->InputBuffer_.WriteReserve(uNumToWrite);
	if(!pfBuff || (StatusFlags & paInputOverflow))
		pAudioIO->InputOverflowCount_++;
	else {
		memcpy(pfBuff, pInputBuffer, uNumToWrite * sizeof(float));
		pAudioIO->InputBuffer_.WriteCommit(uNumToWrite);
	}

	if(!pAudioIO->OutputBuffer_.IsOpen() || !pAudioIO->InputBuffer_.IsOpen())
		return paComplete;
	return paContinue;
}

Map &Devices()
{
	static Map ToReturn(true);
	return ToReturn;
}

Map Map::System(const vector<string> & vstrSystems) const
{
	Map ToReturn;
	if(vstrSystems.empty())
		return *this;
	for(auto && TestBinding : Bindings_) {
		for(auto && TestSystem : vstrSystems) {
			if(StringContains(TestBinding.System_, TestSystem))
				ToReturn.Bindings_.emplace_back(TestBinding);
		}
	}
	return ToReturn;
}

Map Map::System(std::string &strSystem) const
{
	const std::vector<std::string> vstrSystems{strSystem};
	return System(vstrSystems);
}

Map Map::Device(const std::vector<std::string> & vstrDevices) const
{
	Map ToReturn;
	if(vstrDevices.empty())
		return *this;
	for(auto && TestBinding : Bindings_) {
		for(auto && TestDevice : vstrDevices) {
			if(StringContains(TestBinding.Device_, TestDevice))
				ToReturn.Bindings_.emplace_back(TestBinding);
		}
	}
	return ToReturn;
}

Map Map::Device(std::string &strDevice) const
{
	const std::vector<std::string> vstrDevices{strDevice};
	return Device(vstrDevices);
}

Map Map::SampleRate(const std::vector<double> & vdSampleRates_Hz) const
{
	Map ToReturn;
	if(vdSampleRates_Hz.empty())
		return *this;
	for(auto && TestSampleRate : vdSampleRates_Hz) {
		for(auto && TestBinding : Bindings_) {
			auto iterSampleRate = std::find(TestBinding.SampleRates_Hz_.begin(), TestBinding.SampleRates_Hz_.end(), TestSampleRate);
			if(iterSampleRate != TestBinding.SampleRates_Hz_.end()) {
				ToReturn.Bindings_.emplace_back(TestBinding);
				continue;
			}
		}
	}
	return ToReturn;
}

Map Map::SampleRate(const double dSampleRate_Hz) const
{
	const std::vector vdSampleRates_Hz{dSampleRate_Hz};
	return SampleRate(vdSampleRates_Hz);
}

Map Map::Type(const IOType Type) const
{
	Map ToReturn;
	for(auto && TestBinding : Bindings_)
		if(TestBinding.Type_ == Type)
			ToReturn.Bindings_.emplace_back(TestBinding);
	return ToReturn;
}

Binding Map::DefaultInput()
{
	return DefaultInputDevice_;
}

Binding Map::DefaultOutput()
{
	return DefaultOutputDevice_;
}

std::vector<std::string> Map::Systems()
{
	std::set<std::string> setResult;
	for(auto &&x : Bindings_)
		setResult.insert(x.System_);
	return std::vector<std::string>(setResult.begin(), setResult.end());
}

std::vector<std::string> Map::Devices()
{
	std::set<std::string> setResult;
	for(auto &&x : Bindings_)
		setResult.insert(x.Device_);
	return std::vector<std::string>(setResult.begin(), setResult.end());
}

std::vector<double> Map::SampleRates()
{
	std::set<double> setResult;
	for(auto &&x : Bindings_)
		for(auto &&SampleRate : x.SampleRates_Hz_)
			setResult.insert(SampleRate);
	return std::vector<double>(setResult.begin(), setResult.end());
}

const std::vector<std::string> Binding::TypeStrings({ "Input", "Output", "Duplex", "Any" });

const std::string & Binding::SystemName() const
{
	return System_;
}

const std::string & Binding::DeviceName() const
{
	return Device_;
}

const int Binding::DeviceIndex() const
{
	return DeviceIndex_;
}

const std::string &Binding::TypeName() const
{
	return TypeStrings[(size_t)Type_];
}

const std::vector<double> & Binding::SampleRates() const
{
	return SampleRates_Hz_;
}

int Binding::MaxChannels()
{
	return NumChannels_;
}

Map::Map(bool bMapDevices)
{
	if(bMapDevices)
		MapAudioSystem();
}

Map::~Map()
{
	/*while(m_iPaInitFlag) {
		m_iPaInitFlag--;
		Pa_Terminate();
	}*/
}

void Map::MapAudioSystem()
{
	PaError iPaErr = Pa_Initialize();
	int iDefaultInputDevice = (int)Pa_GetDefaultInputDevice(), iDefaultOutputDevice = (int)Pa_GetDefaultOutputDevice();
	if(iPaErr)
		throw Exception("Audio API error while initialising: " + PaErrorString(iPaErr));
	for(int iApiId = (int)paInDevelopment; iApiId <= (int)paAudioScienceHPI; iApiId++) {
		if(Pa_GetHostApiInfo(iApiId)) {
			PaHostApiInfo ApiInfo = *Pa_GetHostApiInfo(iApiId);
			int iDeviceIndex = Pa_HostApiDeviceIndexToDeviceIndex(iApiId, 0);
			for(int iDevice = iDeviceIndex; iDevice < iDeviceIndex + ApiInfo.deviceCount; iDevice++) {
				if(Pa_GetDeviceInfo(iDevice)) {
					PaDeviceInfo DeviceInfo = *Pa_GetDeviceInfo(iDevice);
					PaStreamParameters InStreamParams, OutStreamParams;
					InStreamParams.channelCount = DeviceInfo.maxInputChannels;
					InStreamParams.device = iDevice;
					InStreamParams.hostApiSpecificStreamInfo = nullptr;
					InStreamParams.sampleFormat = paFloat32;
					OutStreamParams.channelCount = 0;
					OutStreamParams.device = iDevice;
					OutStreamParams.hostApiSpecificStreamInfo = nullptr;
					OutStreamParams.sampleFormat = paFloat32;
					vector<double> vdSampleRates_Hz;
					if(DeviceInfo.maxInputChannels > 0) {
						for(auto dSampleRate_Hz : StandardSampleRates_Hz) {
							if(Pa_IsFormatSupported(&InStreamParams, nullptr, dSampleRate_Hz) == (PaError)0)
								vdSampleRates_Hz.push_back(dSampleRate_Hz);
						}
						if(!vdSampleRates_Hz.empty()) {
							Bindings_.emplace_back(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Input, DeviceInfo, vdSampleRates_Hz, iDevice);
							if(iDevice == iDefaultInputDevice)
								DefaultInputDevice_ = Binding(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Input, DeviceInfo, {DeviceInfo.defaultSampleRate}, iDevice);
						}
					}
					InStreamParams.channelCount = 0;
					OutStreamParams.channelCount = DeviceInfo.maxOutputChannels;
					vdSampleRates_Hz.clear();
					if(DeviceInfo.maxOutputChannels > 0) {
						for(auto dSampleRate_Hz : StandardSampleRates_Hz) {
							if(Pa_IsFormatSupported(nullptr, &OutStreamParams, dSampleRate_Hz) == (PaError)0)
								vdSampleRates_Hz.push_back(dSampleRate_Hz);
						}
						if(!vdSampleRates_Hz.empty()) {
							Bindings_.emplace_back(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Output, DeviceInfo, vdSampleRates_Hz, iDevice);
							if(iDevice == iDefaultOutputDevice)
								DefaultInputDevice_ = Binding(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Output, DeviceInfo, {DeviceInfo.defaultSampleRate}, iDevice);
						}
					}
					if((DeviceInfo.maxInputChannels > 0) && (DeviceInfo.maxOutputChannels > 0)) {
						InStreamParams.channelCount = DeviceInfo.maxInputChannels;
						vdSampleRates_Hz.clear();
						for(auto dSampleRate_Hz : StandardSampleRates_Hz) {
							if(Pa_IsFormatSupported(nullptr, &OutStreamParams, dSampleRate_Hz) == (PaError)0)
								vdSampleRates_Hz.push_back(dSampleRate_Hz);
						}
						if(!vdSampleRates_Hz.empty())
							Bindings_.emplace_back(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Duplex, DeviceInfo, vdSampleRates_Hz, iDevice);
					}
				}
			}
		}
	}
	Pa_Terminate();
}

Binding::Binding(std::string strSystem, std::string strDevice, IOType Type, PaDeviceInfo DeviceInfo, std::vector<double> vdSampleRates_Hz, const int iDeviceIndex) :
	System_(strSystem), Device_(strDevice), Type_(Type), SampleRates_Hz_(vdSampleRates_Hz), DeviceIndex_(iDeviceIndex)
{
	switch(Type) {
		case IOType::Input:
			LatencyMin_s_ = DeviceInfo.defaultLowInputLatency;
			LatencyMax_s_ = DeviceInfo.defaultHighInputLatency;
			NumChannels_ = DeviceInfo.maxInputChannels;
			break;
		case IOType::Output:
			LatencyMin_s_ = DeviceInfo.defaultLowOutputLatency;
			LatencyMax_s_ = DeviceInfo.defaultHighOutputLatency;
			NumChannels_ = DeviceInfo.maxOutputChannels;
			break;
		case IOType::Duplex:
			LatencyMin_s_ = std::max(DeviceInfo.defaultLowInputLatency, DeviceInfo.defaultLowOutputLatency);
			LatencyMax_s_ = std::max(DeviceInfo.defaultHighInputLatency, DeviceInfo.defaultHighOutputLatency);
			NumChannels_ = std::min(DeviceInfo.maxOutputChannels, DeviceInfo.maxInputChannels);
			break;
	}
	DefaultSampleRate_Hz_ = DeviceInfo.defaultSampleRate;
}

IO::IO(Binding & DeviceToUse) : Binding_(DeviceToUse),
	m_iPaInitFlag(0), pPaStream_(nullptr),
	InputBuffer_(512),
	InputOverflowCount_(0),
	OutputBuffer_(512),
	OutputOverflowCount_(0),
	Status_("Audio device closed")
{
	SampleRate_Hz_ = static_cast<uint32_t>(Binding_.SampleRates_Hz_.front());
}

bool IO::Open(IOType StreamType)
{
	return true;
}

bool IO::OpenInput()
{
	PaError iPaErr;
	unsigned long framesPerBuffer = 0; // allow PortAudio to choose the number of frames per buffer
	iPaErr = Pa_OpenStream(&pPaStream_, &InputParams_, nullptr, SampleRate_Hz_, framesPerBuffer, paClipOff | paDitherOff, g_pfnInputPaCallback, this);
	if(iPaErr != 0) {
		Status_ = "Input: " + string(Pa_GetDeviceInfo(InputParams_.device)->name) + " error: " + g_mapPaError[iPaErr];
		return false;
	}

	// Reset input buffers
	InputBuffer_.Resize(65536);
	InputOverflowCount_ = 0;
	const PaStreamInfo *pStreamInfo = Pa_GetStreamInfo(pPaStream_);
	double dInputLatency = (double)pStreamInfo->inputLatency;
	UpdateStatus();

	InputBuffer_.Open();

	return true;
}

// TODO: Request buffer size and latency implied by Fs
bool IO::OpenOutput(int iNumOutputChannels)
{
	PaError iPaErr;
	if(iNumOutputChannels > Binding_.NumChannels_)
		return false;

	//PaWinMmeDeviceAndChannelCount WinMmeDevCh;
	//WinMmeDevCh.channelCount = iNumOutputChannels;
	//WinMmeDevCh.device = m_iOutputDevice;
	//PaWinMmeStreamInfo WinMmmeStreamInfo;
	//WinMmmeStreamInfo.size = sizeof(PaWinMmeStreamInfo);
	//WinMmmeStreamInfo.bufferCount = 2;
	//WinMmmeStreamInfo.deviceCount
	void *pHostApiSpecificStreamInfo = nullptr;

	OutputParams_ = PaStreamParameters{Binding_.DeviceIndex_, iNumOutputChannels, paFloat32, Binding_.LatencyMin_s_, pHostApiSpecificStreamInfo};

	unsigned long framesPerBuffer = 0; // allow PortAudio to choose the number of frames per buffer
	iPaErr = Pa_OpenStream(&pPaStream_, nullptr, &OutputParams_, SampleRate_Hz_, framesPerBuffer, paNoFlag, g_pfnOutputPaCallback, this); /// paClipOff | paDitherOff
	if(iPaErr != 0)
		return false;

	// Reset input buffers
	OutputBuffer_.Resize(65536);
	OutputOverflowCount_ = 0;
	const PaStreamInfo *pStreamInfo = Pa_GetStreamInfo(pPaStream_);

	OutputBuffer_.Open();
	UpdateStatus();

	return true;
}

bool IO::StartInput(unsigned uBufferLevel)
{
	if(!pPaStream_) {
		Status_ = "Input stream pointer is null.";
		return false;
	}

	PaError iPaErr = Pa_StartStream(pPaStream_);
	if(iPaErr) {
		InputBuffer_.Close();
		Status_ = "Error when attempting to start input stream: " + PaErrorString(iPaErr);
		return false;
	}
	return true;
}

bool IO::Stop()
{
	if(Started()) {
		PaError iPaErr = Pa_StopStream(pPaStream_);
		if(iPaErr)
			throw Exception("PortAudio error when attempting to stop stream: " + PaErrorString(iPaErr));
		InputBuffer_.Close();
	}
	return true;
}

bool IO::Close()
{
	if(pPaStream_) {
		PaError iPaErr = Pa_CloseStream(pPaStream_);
		pPaStream_ = nullptr;
	}
	return false;
}

#ifdef paAsioUseChannelSelectors
void IO::ShowAsioControlPanel(void *hWindow)
{
	if(Binding_.System_ == "ASIO")
		PaError iPaErr = PaAsio_ShowControlPanel(Binding_.DeviceIndex_, hWindow);
}
#endif

void IO::UpdateStatus()
{
	Status_.clear();
	const PaStreamInfo *pStreamInfo = Pa_GetStreamInfo(pPaStream_);
	if((Binding_.Type_ == IOType::Input) || (Binding_.Type_ == IOType::Duplex)) {
		Status_ += "Input: " + string(Pa_GetDeviceInfo(InputParams_.device)->name) + " open: " + to_string_precision(1e-3 * (double)SampleRate_Hz_, 3) + "kHz, latency: " +
			to_string_precision(pStreamInfo->inputLatency, 3) + "s";
		// + ", used: " + to_string(m_vfInputBuffer.UsedSpace())
	}
}

#ifdef paAsioUseChannelSelectors
void IO::SetAsioHostParams(const vector<int> & viInputChannels, const vector<int> & viOutputChannels)
{
	AsioInputChannels_ = viInputChannels;
	AsioInputStreamInfo_.size = sizeof(AsioInputStreamInfo_);
	AsioInputStreamInfo_.hostApiType = paASIO;
	AsioInputStreamInfo_.version = 1;
	AsioInputStreamInfo_.flags = (viInputChannels.empty()) ? 0 : paAsioUseChannelSelectors;
	AsioInputStreamInfo_.channelSelectors = AsioInputChannels_.data();
	pHostInputParams_ = (void *)&AsioInputStreamInfo_;
		
	AsioOutputChannels_ = viInputChannels;
	AsioOutputStreamInfo_.size = sizeof(AsioOutputStreamInfo_);
	AsioOutputStreamInfo_.hostApiType = paASIO;
	AsioOutputStreamInfo_.version = 1;
	AsioOutputStreamInfo_.flags = (viOutputChannels.empty()) ? 0 : paAsioUseChannelSelectors;
	AsioOutputStreamInfo_.channelSelectors = AsioOutputChannels_.data();
	pHostOutputParams_ = (void *)&AsioOutputStreamInfo_;
}
#endif

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
