#pragma once

#include <portaudio.h>
#ifdef _MSC_VER
#include <pa_asio.h>
#endif

#include "Audaptr.h"

namespace Audaptr
{
	/// @brief Class describing binding of an audio stream to a particular system, device, sample rate, format etc.
	class Binding
	{
	public:
		Binding() = default;

		/// @brief Audio stream binding
		/// @param strSystem Audio system name
		/// @param strDevice Audio device name
		/// @param Type Audio stream type (input, output, duplex, any)
		/// @param DeviceInfo PortAudio descriptor for the device
		/// @param vdSampleRates_Hz Sample rates supported
		/// @param iDeviceIndex Index associated with the device
		Binding(std::string strSystem, std::string strDevice, IOType Type, PaDeviceInfo DeviceInfo, std::vector<double> vdSampleRates_Hz, const int iDeviceIndex);

		bool operator ==(const Binding &Another) const;

		/// @brief The audio system name
		const std::string &SystemName() const;

		/// @brief The audio device name
		const std::string &DeviceName() const;

		/// @brief The device index
		const int DeviceIndex() const;

		const IOType Type() const;

		/// @brief String description of the stream type
		const std::string &TypeName() const;

		/// @brief Vector of supported sample rates
		const std::vector<double> &SampleRates() const;

		/// @brief Maximum number of input channels
		int MaxInputChannels() const;

		/// @brief Maximum number of output channels
		int MaxOutputChannels() const;

		/// @brief Minimum latency [s]
		double MinLatency_s() const;

		/// @brief Maximum latency [s]
		double MaxLatency_s() const;

		/// @brief Supported types
		static const std::vector<std::string> TypeStrings;

		/// @brief PortAudio device information
		PaDeviceInfo DeviceInfo_;

		/// @brief System name
		std::string System_;

		/// @brief Device name
		std::string Device_;

		/// @brief Device type (input, output, duplex, any)
		IOType Type_;

		/// @brief  Suported sample rates
		std::vector<double> SampleRates_Hz_;

		/// @brief Default sample rate
		double DefaultSampleRate_Hz_ = 0.0;

		/// @brief Device index
		int DeviceIndex_ = -1;

		/// @brief Current latency
		double Latency_s_ = 0.0;
	};

}
