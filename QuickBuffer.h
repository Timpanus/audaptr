#pragma once
#include "FastSemaphore.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>

/// @brief Single Producer, Single Consumer (SPSC) queue with lockfree semantics.
/// WriteReserve / WriteCommit and ReadAcquire / ReadRelease supply zero-copy access to buffer regions.
/// Readers and writers may spin-wait on operations. Optional support for blocking and signalling is provided.
/// A bipartite buffer construction is used to ensure availability of contiguous space.
/// @tparam T The type of element buffered. It must be trivial.
template<typename T> class QuickBuffer {
    static_assert(std::is_trivial<T>::value, "The buffer element type T must be trivial.");

    // 64 bytes is suitable for vectorised AVX-512 operations.
    static constexpr size_t kAlignment = 16;
#if((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    static constexpr size_t kCacheLineSize{std::hardware_destructive_interference_size};
#else
    static constexpr size_t kCacheLineSize{64};
#endif
public:
    explicit QuickBuffer(const size_t Size)
        : Size_(Size), SignalWriter_(false), SignalReader_(false), ReadIdx_(0), WriteIdx_(0), EndIdx_(0)
    {
		Resize(Size);
    }

    ~QuickBuffer()
    {
        if(pBuffer_) {
#ifdef _MSC_VER
            _aligned_free(pBuffer_);
#else
            free(pBuffer_);
#endif
        }
    }

    void Resize(const size_t Size)
	{
		Close();
		if(pBuffer_) {
#ifdef _MSC_VER
			_aligned_free(pBuffer_);
#else
			free(pBuffer_);
#endif
		}
#ifdef _MSC_VER
		pBuffer_ = static_cast<T*>(
			_aligned_malloc((size_t)((Size * sizeof(T)) + (kAlignment - (Size * sizeof(T)) % kAlignment)),
				kAlignment));
#else
		pBuffer_ = static_cast<T*>(
			aligned_alloc(kAlignment,
				(size_t)((Size * sizeof(T)) + (kAlignment - (Size * sizeof(T)) % kAlignment))));
#endif
	}

    /// @brief Open the buffer for reading or writing.
    inline void Open() noexcept
    {
        Open_.store(true, std::memory_order_release);
        ReadIdx_ = 0;
        WriteIdx_ = 0;
        EndIdx_ = 0;
        NotFull_.Post();
        NotEmpty_.Post();
    }

    /// @brief Close the buffer and cancel all waiting reads or writes.
    inline void Close() noexcept
    {
        Open_.store(false, std::memory_order_release);
        ReadIdx_ = 0;
        WriteIdx_ = 0;
        EndIdx_ = 0;
        NotFull_.Post();
        NotEmpty_.Post();
    }

    /// @brief Indicate whether the buffer is open for operation.
    /// @return true if the buffer is open, else false
    inline bool IsOpen() const noexcept { return Open_.load(std::memory_order_relaxed); }

    /// @brief Acquire a contiguous region in the buffer for writing. Block until this space is available.
    /// @param uNumToWrite Required number of items to write
    /// @return Pointer to space acquired for contiguous writing.
    inline T* WaitWrite(const size_t NumToWrite) noexcept
    {
        T* pWrite = WriteReserve(NumToWrite);
        while(!pWrite) {
            // Could not find contiguous free space with required size, so wait until something is available.
            SignalWriter_.store(true, std::memory_order_release);
            NotFull_.Wait();
            if(!Open_)
                return nullptr;
            pWrite = WriteReserve(NumToWrite);
        }
        return pWrite;
    }

    /// @brief Block until a specified number of items are available in the buffer for reading.
    /// @param NumToRead Required number of items to read
    /// @param pDest Destination for the complete block of items requested
    /// @return true if the read succeeded, otherwise false (e.g. if the buffer had been closed)
    inline bool WaitRead(size_t NumToRead, T*& pDest) noexcept
    {
        size_t Available = 0;
        T* pRead = ReadAcquire(Available);
        // Fast path
        if(Available >= NumToRead) {
            // pDest = pRead;
            memcpy(pDest, pRead, NumToRead * sizeof(T));
            ReadRelease(NumToRead);
            return true;
        }
        // Slow path
        while(NumToRead > 0) {
            if(pRead) {
                size_t ThisRead = (Available > NumToRead) ? NumToRead : Available;
                memcpy(pDest, pRead, ThisRead * sizeof(T));
                ReadRelease(ThisRead);
                NumToRead -= ThisRead;
                if(NumToRead == 0)
                    return true;
                pDest += ThisRead;
            }
            else {
                // Could not find free contiguous space with required size, so wait until something is available.
                SignalReader_.store(true, std::memory_order_release);
                NotEmpty_.Wait();
                if(!Open_)
                    return false;
            }
            pRead = ReadAcquire(Available);
        }
        return false;
    }

    /// @brief Block until some number of items are available for reading
    /// @param uAvailable Set to the number of items available for reading, iff the return value is not nullptr
    /// @return Pointer to uAvailable items that may be read, else nullptr
    inline T* WaitReadAcquire(size_t& Available) noexcept
    {
        T* pRead = ReadAcquire(Available);
        while(!pRead) {
            // Could not find free contiguous space with required size; wait until something is available.
            SignalReader_.store(true, std::memory_order_release);
            NotEmpty_.Wait();
            if(!Open_)
                return nullptr;
            pRead = ReadAcquire(Available);
        }
        return pRead;
    }

    /// @brief Acquire a contiguous region in the buffer for writing.
    /// @param uNumToWrite Required number of items to write
    /// @return Pointer to space acquired for contiguous writing; nullptr if free space is insufficient.
    inline T* WriteReserve(const size_t NumToWrite) noexcept
    {
        // Cache write and read indices with necessary memory ordering.
        const size_t w = WriteIdx_.load(std::memory_order_relaxed);
        const size_t r = ReadIdx_.load(std::memory_order_acquire);
        const size_t Free = FreeSpace(w, r);
        const size_t ContigSpace = Size_ - w;
        const size_t ContigFree = std::min(Free, ContigSpace);

        // If there is contiguous space until the end of the buffer, return that.
        if(NumToWrite <= ContigFree)
            return pBuffer_ + w;

        // Else return contiguous space from the beginning of the buffer.
        if(NumToWrite <= Free - ContigFree) {
            WriteWrapped_ = true;
            return pBuffer_;
        }
        return nullptr;
    }

    /// @brief Release items in the buffer, following a write, so that these are available for reading.
    /// @param uNumWritten Number of items written; not necessarily equal to the number initially acquired.
    void WriteCommit(const size_t NumWritten) noexcept
    {
        size_t w = WriteIdx_.load(std::memory_order_relaxed);

        // If the write wrapped set the invalidate index and reset write index
        size_t i;
        if(WriteWrapped_) {
            WriteWrapped_ = false;
            i = w;
            w = 0;
        }
        else
            i = EndIdx_.load(std::memory_order_relaxed);
        w += NumWritten;

        // If we wrote over invalidated parts of the buffer move the invalidate index
        if(w > i)
            i = w;

        // Wrap to 0 if needed
        if(w == Size_)
            w = 0;

        // Store the indices with adequate memory ordering
        EndIdx_.store(i, std::memory_order_relaxed);
        WriteIdx_.store(w, std::memory_order_release);
        if(SignalReader_.exchange(false))
            NotEmpty_.Post();
    }

    /// @brief Request the number of items available for reading.
    /// @param uAvailable The number of items available, if a pointer to available items is returned.
    /// @return nullptr if no items are available for reading; otherwise, a pointer to the available items.
    inline T* ReadAcquire(size_t& Available) noexcept
    {
        // Cache read and write indices with necessary memory ordering.
        const size_t r = ReadIdx_.load(std::memory_order_relaxed);
        const size_t w = WriteIdx_.load(std::memory_order_acquire);

        // When read and write indexes are equal, the buffer is empty.
        if(r == w)
            return nullptr;

        // Simplest case: read index is behind the write index
        if(r < w) {
            Available = w - r;
            return pBuffer_ + r;
        }

        // Read index reached the invalidate index, so make the read wrap.
        const size_t i = EndIdx_.load(std::memory_order_relaxed);
        if(r == i) {
            ReadWrapped_ = true;
            Available = w;
            return pBuffer_;
        }

        // There is some data until the invalidate index.
        Available = i - r;
        return pBuffer_ + r;
    }

    /// @brief Release some number of items after a read operation.
    /// @param uToRelease Number of items to relase; not necessarily equal to the number specified in ReadAcquire.
    void ReadRelease(const size_t ToRelease) noexcept
    {
        // If the read wrapped, record that and set its index to 0.
        size_t r;
        if(ReadWrapped_) {
            ReadWrapped_ = false;
            r = 0;
        }
        else
            r = ReadIdx_.load(std::memory_order_relaxed);

        // Increment the read index and wrap to 0 if needed
        r += ToRelease;
        if(r == Size_)
            r = 0;

        // Store the indexes with adequate memory ordering
        ReadIdx_.store(r, std::memory_order_release);
        if(SignalWriter_.exchange(false))
            NotFull_.Post();
    }

private:
    inline size_t FreeSpace(const size_t w, const size_t r) const noexcept
    {
        return (r > w) ? ((r - w) - (size_t)1) : ((Size_ - (w - r)) - (size_t)1);
    }

    /// @brief Size of the data buffer (must be a power-of-2)
    const size_t Size_{0};

    /// @brief Data buffer
    alignas(kCacheLineSize) T* pBuffer_{nullptr};

    /// @brief Flag indicating whether the buffer is open
    alignas(kCacheLineSize) std::atomic_bool Open_{false};

    /// @brief Read index
    alignas(kCacheLineSize) std::atomic_size_t ReadIdx_{0};

    /// @brief Read wrapped flag, used only in the consumer
    alignas(kCacheLineSize) bool ReadWrapped_{false};

    /// @brief Flag set to indicate that the writer should be signalled
    std::atomic_bool SignalWriter_{false};

    /// @brief Semaphore indicating to the writer that the buffer is no longer full
    FastSemaphore NotFull_;

    /// @brief Write index
    alignas(kCacheLineSize) std::atomic_size_t WriteIdx_;

    /// @brief End (of valid region) index
    alignas(kCacheLineSize) std::atomic_size_t EndIdx_;

    /// @brief Write wrapped flag, used only in the producer
    alignas(kCacheLineSize) bool WriteWrapped_{false};

    /// @brief Flag set to indicate that the reader should be signalled
    std::atomic_bool SignalReader_;

    /// @brief Semaphore indicating to the reader that the buffer is no longer empty
    FastSemaphore NotEmpty_;
};
