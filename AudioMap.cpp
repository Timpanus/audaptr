#include <set>
#include <string>

#include "AudioMap.h"

using namespace std;

namespace Audaptr
{
	AudioMap::AudioMap(bool bMapDevices)
	{
		if(bMapDevices)
			MapAudioSystem();
	}

	AudioMap::~AudioMap()
	{
		/*while(m_iPaInitFlag) {
			m_iPaInitFlag--;
			Pa_Terminate();
		}*/
	}

	void AudioMap::MapAudioSystem()
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
									DefaultOutputDevice_ = Binding(Pa_GetHostApiInfo(iApiId)->name, DeviceInfo.name, IOType::Output, DeviceInfo, {DeviceInfo.defaultSampleRate}, iDevice);
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

	AudioMap AudioMap::System(const vector<string> &vstrSystems) const
	{
		AudioMap ToReturn;
		if(vstrSystems.empty())
			return *this;
		for(auto &&TestBinding : Bindings_) {
			for(auto &&TestSystem : vstrSystems) {
				if(StringContains(TestBinding.System_, TestSystem))
					ToReturn.Bindings_.emplace_back(TestBinding);
			}
		}
		return ToReturn;
	}

	AudioMap AudioMap::System(std::string &strSystem) const
	{
		const std::vector<std::string> vstrSystems{strSystem};
		return System(vstrSystems);
	}

	AudioMap AudioMap::Device(const std::vector<std::string> &vstrDevices) const
	{
		AudioMap ToReturn;
		if(vstrDevices.empty())
			return *this;
		for(auto &&TestBinding : Bindings_) {
			for(auto &&TestDevice : vstrDevices) {
				if(StringContains(TestBinding.Device_, TestDevice))
					ToReturn.Bindings_.emplace_back(TestBinding);
			}
		}
		return ToReturn;
	}

	AudioMap AudioMap::Device(const std::string &strDevice) const
	{
		const std::vector<std::string> vstrDevices{strDevice};
		auto ReturnMap = Device(vstrDevices);
		return ReturnMap;
	}

	AudioMap AudioMap::SampleRate(const std::vector<double> &vdSampleRates_Hz) const
	{
		AudioMap ToReturn;
		if(vdSampleRates_Hz.empty())
			return *this;
		for(auto &&TestSampleRate : vdSampleRates_Hz) {
			for(auto &&TestBinding : Bindings_) {
				auto iterSampleRate = std::find(TestBinding.SampleRates_Hz_.begin(), TestBinding.SampleRates_Hz_.end(), TestSampleRate);
				if(iterSampleRate != TestBinding.SampleRates_Hz_.end()) {
					if(std::find(ToReturn.Bindings_.begin(), ToReturn.Bindings_.end(), TestBinding) == ToReturn.Bindings_.end()) {
						ToReturn.Bindings_.emplace_back(TestBinding);
						ToReturn.Bindings_.back().SampleRates_Hz_.clear();
					}
					ToReturn.Bindings_.back().SampleRates_Hz_.emplace_back(TestSampleRate);
					continue;
				}
			}
		}
		return ToReturn;
	}

	AudioMap AudioMap::SampleRate(const double dSampleRate_Hz) const
	{
		const std::vector vdSampleRates_Hz{dSampleRate_Hz};
		return SampleRate(vdSampleRates_Hz);
	}

	AudioMap AudioMap::Type(const IOType Type) const
	{
		AudioMap ToReturn;
		for(auto &&TestBinding : Bindings_)
			if(TestBinding.Type_ == Type)
				ToReturn.Bindings_.emplace_back(TestBinding);
		return ToReturn;
	}

	Binding AudioMap::DefaultInput()
	{
		return DefaultInputDevice_;
	}

	Binding AudioMap::DefaultOutput()
	{
		return DefaultOutputDevice_;
	}

	std::vector<std::string> AudioMap::Systems()
	{
		std::set<std::string> setResult;
		for(auto &&x : Bindings_)
			setResult.insert(x.System_);
		return std::vector<std::string>(setResult.begin(), setResult.end());
	}

	std::vector<std::string> AudioMap::Devices()
	{
		std::set<std::string> setResult;
		for(auto &&x : Bindings_)
			setResult.insert(x.Device_);
		return std::vector<std::string>(setResult.begin(), setResult.end());
	}

	std::vector<double> AudioMap::SampleRates()
	{
		std::set<double> setResult;
		for(auto &&x : Bindings_)
			for(auto &&SampleRate : x.SampleRates_Hz_)
				setResult.insert(SampleRate);
		return std::vector<double>(setResult.begin(), setResult.end());
	}

}
