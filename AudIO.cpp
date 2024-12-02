#include "AudIO.h"

#include "Audaptr.h"

using namespace std;

namespace Audaptr
{
	// Called by the PortAudio engine when audio is needed, possibly at interrupt level. Do not block.
	int InputPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
	{
		// StatusFlags: paInputUnderflow, paInputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
		AudIO *pAudioIO = reinterpret_cast<AudIO *>(pUserData);

		size_t uNumToRead = (size_t)FramesPerBuffer * pAudioIO->InputParams_.channelCount;

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

	int OutputPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
	{
		// StatusFlags: paOutputUnderflow, paOutputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
		AudIO *pAudioIO = reinterpret_cast<AudIO *>(pUserData);
		size_t uNumToWrite = (size_t)FramesPerBuffer * pAudioIO->OutputParams_.channelCount, uAvailable;

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

	int DuplexPaCallback(const void *pInputBuffer, void *pOutputBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo *pTimeInfo, PaStreamCallbackFlags StatusFlags, void *pUserData)
	{
		// TODO: Check StatusFlags: paInputUnderflow, paInputOverflow, paOutputUnderflow, paOutputOverflow, paPrimingOutput
		AudIO *pAudioIO = reinterpret_cast<AudIO *>(pUserData);

		size_t uNumToWrite = (size_t)FramesPerBuffer * pAudioIO->InputParams_.channelCount;

		// Read from the output buffer and write to the device
		size_t uNumToRead = (size_t)FramesPerBuffer * pAudioIO->OutputParams_.channelCount, uAvailable;
		auto *pfBuff = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
		if(pfBuff) {
			size_t uThisRead = (uAvailable > uNumToRead) ? uNumToRead : uAvailable;
			memcpy(pOutputBuffer, pfBuff, uThisRead * sizeof(float));
			uNumToRead -= uThisRead;
			pOutputBuffer = (float *)pOutputBuffer + uThisRead;
			pAudioIO->OutputBuffer_.ReadRelease(uThisRead);
			// This check is necessary because the buffer may segment read transactions.
			if((uNumToRead > 0) && pAudioIO->OutputBuffer_.IsOpen()) {
				pfBuff = pAudioIO->OutputBuffer_.ReadAcquire(uAvailable);
				uThisRead = (uAvailable > uNumToRead) ? uNumToRead : uAvailable;
				memcpy(pOutputBuffer, pfBuff, uThisRead * sizeof(float));
				uNumToRead -= uThisRead;
				pOutputBuffer = (float *)pOutputBuffer + uThisRead;
				pAudioIO->OutputBuffer_.ReadRelease(uThisRead);
			}
		}
		// If no free space in the output buffer, increment a count to record the overflow; do not block in this callback.
		if((uNumToRead > 0) || (StatusFlags & paOutputOverflow))
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


	AudIO::AudIO() :
		PaInitFlag_(0), pPaStream_(nullptr),
		InputBuffer_(65536),
		InputOverflowCount_(0),
		OutputBuffer_(65536),
		OutputOverflowCount_(0),
		Status_("Audio device closed")
	{
	}

	AudIO::AudIO(const Binding &DeviceToUse) :
		Binding_(DeviceToUse),
		PaInitFlag_(0), pPaStream_(nullptr),
		InputBuffer_(65536),
		InputOverflowCount_(0),
		OutputBuffer_(65536),
		OutputOverflowCount_(0),
		Status_("Audio device closed")
	{
		SampleRate_Hz_ = Binding_.SampleRates_Hz_.front();
	}

	bool AudIO::Bind(const Binding &ToBind, double Latency_s, const int NumInputChannels, const int NumOutputChannels)
	{
		// TODO: Set ASIO host parameters
		Binding_ = ToBind;
		SampleRate_Hz_ = static_cast<uint32_t>(Binding_.SampleRates_Hz_.front());
		if(Latency_s < ToBind.MinLatency_s())
			throw Exception("Latency requested is lower than the minimum possible");
		if(Latency_s > ToBind.MaxLatency_s())
			throw Exception("Latency requested is higher than the maximum possible");
		switch(ToBind.Type_) {
		case Audaptr::IOType::Input:
			if(NumInputChannels <= 0)
				throw Exception("Number of input channels should be greater than zero for input");
			if(NumInputChannels > ToBind.MaxInputChannels())
				throw Exception("Number of input channels exceeds the maximum possible");
			InputParams_ = PaStreamParameters{Binding_.DeviceIndex_, NumInputChannels, paFloat32, Latency_s, pHostParams_};
			break;
		case Audaptr::IOType::Output:
			if(NumOutputChannels <= 0)
				throw Exception("Number of output channels should be greater than zero for output");
			if(NumInputChannels > ToBind.MaxOutputChannels())
				throw Exception("Number of input channels exceeds the maximum possible");
			OutputParams_ = PaStreamParameters{Binding_.DeviceIndex_, NumOutputChannels, paFloat32, Latency_s, pHostParams_};
			break;
		case Audaptr::IOType::Duplex:
			if(NumInputChannels <= 0)
				throw Exception("Number of input channels should be greater than zero for duplex operation");
			if(NumInputChannels > ToBind.MaxInputChannels())
				throw Exception("Number of input channels exceeds the maximum possible");
			if(NumOutputChannels <= 0)
				throw Exception("Number of output channels should be greater than zero for duplex operation");
			if(NumInputChannels > ToBind.MaxOutputChannels())
				throw Exception("Number of input channels exceeds the maximum possible");
			InputParams_ = PaStreamParameters{Binding_.DeviceIndex_, NumInputChannels, paFloat32, Latency_s, pHostParams_};
			OutputParams_ = PaStreamParameters{Binding_.DeviceIndex_, NumOutputChannels, paFloat32, Latency_s, pHostParams_};
		}
		PaInitFlag_ = 0;
		pPaStream_ = nullptr;
		InputBuffer_.Close();
		OutputBuffer_.Close();
		return true;
	}

	bool AudIO::Open()
	{
		PaError iPaErr;
		unsigned long FramesPerBuffer = 0; // allow PortAudio to choose the number of frames per buffer
		iPaErr = Pa_Initialize();
		if(iPaErr != 0) {
			Status_ = Binding_ .TypeName() + ": " + string(Binding_.DeviceName()) + " error: " + g_mapPaError[iPaErr];
			return false;
		}
		else
			PaInitFlag_++;
		PaStreamParameters *pInputParams = nullptr, *pOutputParams = nullptr;
		auto PaCallback = InputPaCallback;
		switch(Binding_.Type()) {
		case IOType::Input:
			pInputParams = &InputParams_;
			InputBuffer_.Open();
			break;
		case IOType::Output:
			pOutputParams = &OutputParams_;
			PaCallback = OutputPaCallback;
			OutputBuffer_.Open();
			break;
		case IOType::Duplex:
			pInputParams = &InputParams_;
			pOutputParams = &OutputParams_;
			PaCallback = DuplexPaCallback;
			InputBuffer_.Open();
			OutputBuffer_.Open();
			break;
		}
		iPaErr = Pa_OpenStream(&pPaStream_, pInputParams, pOutputParams, SampleRate_Hz_, FramesPerBuffer, paClipOff | paDitherOff, PaCallback, this);
		if(iPaErr != 0) {
			Status_ = Binding_.TypeName() + ": " + string(Pa_GetDeviceInfo(InputParams_.device)->name) + " error: " + g_mapPaError[iPaErr];
			return false;
		}

		// Reset input buffers
		OutputOverflowCount_ = 0;
		InputOverflowCount_ = 0;
		const PaStreamInfo *pStreamInfo = Pa_GetStreamInfo(pPaStream_);
		double dInputLatency = (double)pStreamInfo->inputLatency;
		switch(Binding_.Type()) {
		case IOType::Input:
			Latency_s_ = (double)pStreamInfo->inputLatency;
			break;
		case IOType::Output:
			Latency_s_ = (double)pStreamInfo->outputLatency;
			break;
		case IOType::Duplex:
			Latency_s_ = (double)pStreamInfo->inputLatency + (double)pStreamInfo->outputLatency;
			break;
		}
		UpdateStatus();
		return true;
	}

	bool AudIO::Start()
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

	bool AudIO::Stop()
	{
		if(Started()) {
			PaError iPaErr = Pa_StopStream(pPaStream_);
			if(iPaErr)
				throw Exception("PortAudio error when attempting to stop stream: " + PaErrorString(iPaErr));
			InputBuffer_.Close();
		}
		return true;
	}

	bool AudIO::Close()
	{
		if(pPaStream_) {
			PaError iPaErr = Pa_CloseStream(pPaStream_);
			pPaStream_ = nullptr;
		}
		if(PaInitFlag_ > 0) {
			Pa_Terminate();
			PaInitFlag_--;
		}
		Status_ = "Audio device closed";
		return false;
	}

	double AudIO::SampleRate_Hz() const
	{
		return SampleRate_Hz_;
	}

#ifdef paAsioUseChannelSelectors
	void AudIO::ShowAsioControlPanel(void *hWindow)
	{
		if(Binding_.System_ == "ASIO")
			PaError iPaErr = PaAsio_ShowControlPanel(Binding_.DeviceIndex_, hWindow);
	}
#endif

	double AudIO::Latency_s()
	{
		return Latency_s_;
	}

	void AudIO::UpdateStatus()
	{
		Status_.clear();
		const PaStreamInfo *pStreamInfo = Pa_GetStreamInfo(pPaStream_);
		if((Binding_.Type_ == IOType::Input) || (Binding_.Type_ == IOType::Duplex)) {
			Status_ += "Input: " + string(Pa_GetDeviceInfo(InputParams_.device)->name) + " open: " + to_string_precision(1e-3 * (double)SampleRate_Hz_, 3) + "kHz, latency: " +
				to_string_precision(1e3 * Latency_s_, 4) + "ms, Input overflows: " + to_string(InputOverflowCount_) + ", Output overflows: " + to_string(OutputOverflowCount_);
		}
	}

#ifdef paAsioUseChannelSelectors
	void AudIO::SetAsioHostParams(const vector<int> &viInputChannels, const vector<int> &viOutputChannels)
	{
		AsioInputChannels_ = viInputChannels;
		AsioInputStreamInfo_.size = sizeof(AsioInputStreamInfo_);
		AsioInputStreamInfo_.hostApiType = paASIO;
		AsioInputStreamInfo_.version = 1;
		AsioInputStreamInfo_.flags = (viInputChannels.empty()) ? 0 : paAsioUseChannelSelectors;
		AsioInputStreamInfo_.channelSelectors = AsioInputChannels_.data();
		pHostParams_ = (void *)&AsioInputStreamInfo_;

		AsioOutputChannels_ = viInputChannels;
		AsioOutputStreamInfo_.size = sizeof(AsioOutputStreamInfo_);
		AsioOutputStreamInfo_.hostApiType = paASIO;
		AsioOutputStreamInfo_.version = 1;
		AsioOutputStreamInfo_.flags = (viOutputChannels.empty()) ? 0 : paAsioUseChannelSelectors;
		AsioOutputStreamInfo_.channelSelectors = AsioOutputChannels_.data();
		pHostParams_ = (void *)&AsioOutputStreamInfo_;
	}
#endif

	void AudIO::SetWinMmeStreamParams()
	{
		/*PaWinMmeDeviceAndChannelCount WinMmeDevCh;
		WinMmeDevCh.channelCount = iNumOutputChannels;
		WinMmeDevCh.device = m_iOutputDevice;
		PaWinMmeStreamInfo WinMmmeStreamInfo;
		WinMmmeStreamInfo.size = sizeof(PaWinMmeStreamInfo);
		WinMmmeStreamInfo.bufferCount = 2;
		WinMmmeStreamInfo.deviceCount
		pHostParams_ = (void *)&WinMmmeStreamInfo;*/
	}

}
