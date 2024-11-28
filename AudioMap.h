#pragma once

#include <string>
#include <vector>

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

#include "Audaptr.h"
#include "Binding.h"
#include "QuickBuffer.h"

namespace Audaptr
{
	class AudioMap
	{
	public:
		/// @brief Default constructor
		/// @param bMapDevices
		AudioMap(bool bMapDevices = false);

		virtual ~AudioMap();

		/// @brief Selector based on a vector of possible system names
		/// @param vstrSystems The system names
		/// @return A map of possible audio devices, filtered according to the systems specified
		AudioMap System(const std::vector<std::string> &vstrSystems = {}) const;

		/// @brief Selector based on a single system name
		/// @param strSystem The system name
		/// @return A map of possible audio devices, filtered according to the system specified
		AudioMap System(std::string &strSystem) const;

		/// @brief Selector based on a vector of possible device names
		/// @param vstrDevices The device names
		/// @return A map of possible audio devices, filtered according to the devices specified
		AudioMap Device(const std::vector<std::string> &vstrDevices = {}) const;

		/// @brief Selector based on a single device name
		/// @param strDevices The device name
		/// @return A map of possible audio devices, filtered according to the device specified
		AudioMap Device(const std::string &strDevices) const;

		/// @brief Selector based on a vector of desired sample rates
		/// @param vstrSystems The sample rates for filtering
		/// @return A map of possible audio devices, filtered according to the specified sample rates
		AudioMap SampleRate(const std::vector<double> &vdSampleRates_Hz = {}) const;

		/// @brief Selector based on a single sample rate
		/// @param vstrSystems The sample rate for filtering
		/// @return A map of possible audio devices, filtered according to the specified sample rate
		AudioMap SampleRate(const double dSampleRate_Hz) const;

		/// @brief Selector based on a vector of desired stream type
		/// @param vstrSystems The stream type for filtering
		/// @return A map of possible audio devices, filtered according to the specified stream type
		AudioMap Type(const IOType Type) const;

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
		Binding &operator[](int n) { return Bindings_[n]; }

		operator std::vector<Binding>() &
		{
			return Bindings_;
		}

		operator Binding() const { return Bindings_.front(); }

		std::vector<Binding> Bindings_;

		void MapAudioSystem();

	protected:
		static Binding DefaultInputDevice_; //

		static Binding DefaultOutputDevice_; // {"", "", IOType::Input, PaDeviceInfo(), {}, -1}
	};

}
