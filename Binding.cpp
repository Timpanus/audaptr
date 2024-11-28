#include "Binding.h"

namespace Audaptr
{
	Binding::Binding(std::string strSystem, std::string strDevice, IOType Type, PaDeviceInfo DeviceInfo, std::vector<double> vdSampleRates_Hz, const int iDeviceIndex) :
		System_(strSystem), Device_(strDevice), Type_(Type), SampleRates_Hz_(vdSampleRates_Hz), DeviceIndex_(iDeviceIndex)
	{
		DeviceInfo_ = DeviceInfo;
		DefaultSampleRate_Hz_ = DeviceInfo.defaultSampleRate;
	}

	bool Binding::operator==(const Binding &Another) const
	{
		return (System_ == Another.System_) && (Device_ == Another.Device_) && (Type_ == Another.Type_) &&
			(SampleRates_Hz_ == Another.SampleRates_Hz_) && (DefaultSampleRate_Hz_ == Another.DefaultSampleRate_Hz_) &&
			(DeviceIndex_ == Another.DeviceIndex_) && (Latency_s_ == Another.Latency_s_);
	}

	const std::string &Binding::SystemName() const
	{
		return System_;
	}

	const std::string &Binding::DeviceName() const
	{
		return Device_;
	}

	const int Binding::DeviceIndex() const
	{
		return DeviceIndex_;
	}

	const IOType Binding::Type() const
	{
		return Type_;
	}

	const std::string &Binding::TypeName() const
	{
		return TypeStrings[(size_t)Type_];
	}

	const std::vector<double> &Binding::SampleRates() const
	{
		return SampleRates_Hz_;
	}

	int Binding::MaxInputChannels() const
	{
		return DeviceInfo_.maxInputChannels;
	}

	int Binding::MaxOutputChannels() const
	{
		return DeviceInfo_.maxOutputChannels;
	}

	double Binding::MinLatency_s() const
	{
		switch(Type_) {
		case IOType::Input:
			return DeviceInfo_.defaultLowInputLatency;
		case IOType::Output:
			return DeviceInfo_.defaultLowOutputLatency;
		case IOType::Duplex:
			return std::max(DeviceInfo_.defaultLowInputLatency, DeviceInfo_.defaultLowOutputLatency);
		}
		return std::max(DeviceInfo_.defaultLowInputLatency, DeviceInfo_.defaultLowOutputLatency);
	}

	double Binding::MaxLatency_s() const
	{
		switch(Type_) {
		case IOType::Input:
			return DeviceInfo_.defaultHighInputLatency;
		case IOType::Output:
			return DeviceInfo_.defaultHighOutputLatency;
		case IOType::Duplex:
			return std::min(DeviceInfo_.defaultHighInputLatency, DeviceInfo_.defaultHighOutputLatency);
		}
		return std::min(DeviceInfo_.defaultHighInputLatency, DeviceInfo_.defaultHighOutputLatency);
	}

}
