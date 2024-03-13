#pragma once

#include <type_traits>
#include <condition_variable>
#include <mutex>
#include <span>

template<typename T, size_t N>
    requires std::is_default_constructible_v<T>
class AudioVisualizationQueue {
public:
    typedef typename std::vector<T> container;

    void readIntoBlocking(container& buffer) {
        std::unique_lock<std::mutex> lk(mtx);

        cv.wait(lk, [this] { return frontBufferFilled; });
        buffer.swap(frontBuffer);
        frontBufferFilled = false;
    }

    bool tryReadInto(container& buffer) {
        std::unique_lock<std::mutex> lk(mtx, std::try_to_lock);
        
        // Because of short-circuit evaluation, frontBufferFilled shouldn't ever be read unless the lock has been acquired 
        if(!lk.owns_lock() || !this->frontBufferFilled) {
            return false;
        }

        buffer.swap(frontBuffer);
        frontBufferFilled = false;
        return true;
    }

    void swapBuffersBlocking() {
        {
            std::lock_guard<std::mutex> lk(mtx);
            std::swap(backBuffer, frontBuffer);
            frontBufferFilled = true;
        }
        cv.notify_one();
    }

    void writeBlocking(std::span<T> data) {
        std::unique_lock<std::mutex> lk(mtx);
        size_t elementsWritten = 0;
        while(elementsWritten < data.size()) {
            // Write either as much data as the back buffer can hold or the remainder of the data buffer, whichever is less
            size_t elementsAvailableInBackBuffer = backBuffer.end() - (backBuffer.begin() + backBufferWriteIdx);
            size_t numElementsToWrite = std::min<size_t>(elementsAvailableInBackBuffer, data.size() - elementsWritten);
            std::copy_n(data.begin() + elementsWritten, numElementsToWrite, backBuffer.begin() + backBufferWriteIdx);
            elementsWritten += numElementsToWrite;
            backBufferWriteIdx += numElementsToWrite;
            if(backBufferWriteIdx >= backBuffer.size()) {
                backBufferWriteIdx -= backBuffer.size();
                if(backBufferWriteIdx > 0) {
                    std::fprintf(stderr, "Warning: %d more elements written than back buffer can hold\n", (int)backBufferWriteIdx);
                }
                backBufferWriteIdx = 0;
                lk.unlock(); // Temporarily unlock to swap buffers
                swapBuffersBlocking();
                // If there is more data to write, reacquire the lock
                if(elementsWritten < data.size()) {
                    lk.lock();
                }
            }
        }
    } 

    AudioVisualizationQueue() = default;

private:

    container containers[2] = { container(N), container(N) };
    container& frontBuffer = containers[0];
    container& backBuffer = containers[1];
    bool frontBufferFilled = false;
    size_t backBufferWriteIdx = 0;

    mutable std::condition_variable cv;
    mutable std::mutex mtx;
};