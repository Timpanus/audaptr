#pragma once

#include "Audaptr.h"
#include "Binding.h"

namespace Audaptr
{
	class AudIO
	{
	public:
		static PaAsioStreamInfo DefaultAsioStreamInfo;

		/// @brief Default constructor
		AudIO();

		/// @brief Constructor, given a device to bind to
		/// @param DeviceToUse Device to which the AudIO instance will be bound
		AudIO(const Binding& DeviceToUse);

		/// @brief Bind to a device and specify necessary parameters for the I/O
		/// @param DeviceToUse Device to which the I/O device will be bound
		/// @param Latency_s Latency [seconds]
		/// @param NumInputChannels Number of input channels required
		/// @param NumOutputChannels Number of output channels
		/// @return true if the binding was successful (exceptions may be thrown)
		bool Bind(const Binding& DeviceToUse, double Latency_s, const int NumInputChannels, const int NumOutputChannels);

		/// Open the audio device for use in input/output full-duplex mode
		bool Open();

		/// Start audio I/O processing
		bool Start();

		/// Stop audio I/O processing
		bool Stop();

		/// Close the audio device
		bool Close();

		/// @brief Flag indicating whether the stream has been started
		/// @return true if the stream has been started
		bool Started()
		{
			return (bool)(Pa_IsStreamStopped(pPaStream_) == 0);
		}

		/// @brief Obtain the status of the AudIO device
		/// @return Status of the AudIO device, represented as a string
		const std::string Status()
		{
			return Status_;
		}

		/// @brief Obtain the currently bound sample rate
		/// @return The sample rate [hertz]
		double SampleRate_Hz() const;

		///
		/*bool AsioEnabled()
		{
			return ((Pa_GetHostApiInfo(Pa_GetDeviceInfo(InputParams_.device)->hostApi)->type == paASIO) ||
					(Pa_GetHostApiInfo(OutputParams_.device)->type == paASIO));
		}*/

		/// @brief Reset the host parameters
		void ResetHostParams()
		{
			pHostParams_ = nullptr;
		}

		/// @brief Access the AudIO device input buffer
		/// @return The buffer associated with input samples
		QuickBuffer<float> & InBuffer();

		/// @brief Acces the AudIO device output buffer
		/// @return The buffer associated with output samples
		QuickBuffer<float>& OutBuffer();

#ifdef paAsioUseChannelSelectors
		/// @param[in] hWindow Handle to main application window, where hwnd is of type HWND (cast to void*)
		void ShowAsioControlPanel(void* hWindow);

		void SetAsioHostParams(const std::vector<int>& viInputChannels, const std::vector<int>& viOutputChannels = {});

		void SetWinMmeStreamParams();

		protected:

		std::vector<int> AsioInputChannels_;

		std::vector<int> AsioOutputChannels_;

		PaAsioStreamInfo AsioInputStreamInfo_{
			sizeof(PaAsioStreamInfo),
			paASIO,
			0,
			0,
			nullptr};

		PaAsioStreamInfo AsioOutputStreamInfo_{
			sizeof(PaAsioStreamInfo),
			paASIO,
			0,
			0,
			nullptr};

#endif

		/// @brief Input stream parameters
		PaStreamParameters InputParams_{
			(PaDeviceIndex)0,
			0,
			paFloat32,
			(PaTime)0.0,
			nullptr};

		/// @brief Output stream parameters
		PaStreamParameters OutputParams_{
			(PaDeviceIndex)0,
			0,
			paFloat32,
			(PaTime)0.0,
			nullptr};

		/// @brief Host-specific API input stream parameters
		void* pHostParams_ = nullptr;

		/// Circular threadsafe input buffer
		QuickBuffer<float> InputBuffer_;

		/// Circular threadsafe output buffer
		QuickBuffer<float> OutputBuffer_;

		/// Flag indicating the count of input buffer overflows that have occurred
		int InputOverflowCount_;

		/// Flag indicating the count of input buffer overflows that have occurred
		int OutputOverflowCount_;

		double SampleRate_Hz_ = -1.0;

	protected:
		friend static int InputPaCallback(const void* pInputBuffer, void* pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags StatusFlags, void* pUserData);

		friend static int OutputPaCallback(const void* pInputBuffer, void* pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags StatusFlags, void* pUserData);

		friend static int DuplexPaCallback(const void* pInputBuffer, void* pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags StatusFlags, void* pUserData);


		template<typename T>
		std::string to_string_precision(const T a_value, const int n = 6)
		{
			std::ostringstream out;
			out.precision(n);
			out << std::fixed << a_value;
			return std::move(out).str();
		}

		Binding Binding_;

		/// Flag incremented upon a successful call to Pa_Initialize
		int PaInitFlag_ = 0;

		/// Pointer to a PortAudio stream
		PaStream* pPaStream_ = nullptr;

		///////

		/// Update the status string associated with the device
		void UpdateStatus();

		/// Last known status of the audio API
		std::string Status_;
	};


}
