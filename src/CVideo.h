#pragma once
#ifndef GUITARPP_CVIDEO_h
#define GUITARPP_CVIDEO_h
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

class CVideo {
    int fd;
    int sizeimage;

    std::atomic<bool> frameready{};
    std::mutex fmmtx;
    std::unique_ptr<char[]> frameBuffer;
    std::unique_ptr<char[]> tmpBuffer;

    std::atomic<bool> keepDecodeTask{};
    std::future<int> decodeft;

    static int decodeFun(CVideo *ths);

  public:
    int width, height;

    bool openDeviceFile(const char *address);
    const char *getFrame(bool &updated,
                         const std::function<void(const char *ptr, int size,
                                                  bool)> &threadSafeFun);

    CVideo();
    ~CVideo();
};

#endif
