#include "CVideo.h"
#include <cstdio>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <iostream>
#include <algorithm>

unsigned int wxlToPos(int width, int x, int line, int numbytes = 1)
{
    return (width * line + x) * numbytes;
}


unsigned int grgbbytes(char *grgb)
{

}

void fillRGB(char *rgb, int pos, int r, int g, int b)
{
    rgb[pos + 0] = r;
    rgb[pos + 1] = g;
    rgb[pos + 2] = b;
}

void grgb2rgb24(char *grgb, char *rgb, int pixels, int width, int height)
{
    for (int i = 0, ir = 0; i < width; i += 2, ir += 2)
    {
        for (int j = 0, jr = 0; j < height; j += 2, jr += 2)
        {
            int g = (unsigned char)grgb[wxlToPos(width, i, j)];
            int r = (unsigned char)grgb[wxlToPos(width, i + 1, j)];
            int b = (unsigned char)grgb[wxlToPos(width, i, j + 1)];

            int pos = wxlToPos(width, ir, jr, 3);
            int pos2 = wxlToPos(width, ir, jr + 1, 3);
            
            fillRGB(rgb, pos, r, g, b);
            fillRGB(rgb, pos + 3, r, g, b);
            
            fillRGB(rgb, pos2, r, g, b);
            fillRGB(rgb, pos2 + 3, r, g, b);
        }
    }
}

const char *CVideo::getFrame(bool &updated, std::function<void(const char *ptr, int size, bool)> threadSafeFun)
{
    updated = false;
    if (!frameready)
    {
        std::lock_guard<std::mutex> lck(fmmtx);
        threadSafeFun(frameBuffer.get(), sizeimage, false);
        return frameBuffer.get();
    }

    std::lock_guard<std::mutex> lck(fmmtx);
    frameready = false;
    updated = true;

    threadSafeFun(frameBuffer.get(), sizeimage, true);
    return frameBuffer.get();
}

int CVideo::decodeFun(CVideo *ths)
{
    std::unique_ptr<char[]> buff(std::make_unique<char[]>(ths->sizeimage));
    while (ths->keepDecodeTask)
    {
        if (!ths->frameready)
        {
            ssize_t resiz = read(ths->fd, buff.get(), ths->sizeimage);
            //std::cout << "resiz " << resiz << std::endl;
            if (!ths->keepDecodeTask)
            {
                return 0;
            }

            grgb2rgb24(buff.get(), ths->tmpBuffer.get(), resiz, ths->width, ths->height);

            {
                std::lock_guard<std::mutex> lck(ths->fmmtx);

                std::reverse_copy(ths->tmpBuffer.get(), ths->tmpBuffer.get() + resiz * 3, ths->frameBuffer.get());
                //memcpy(ths->frameBuffer.get(), ths->tmpBuffer.get(), resiz);
            }

            ths->frameready = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

bool CVideo::openDeviceFile(const char *address)
{
    struct v4l2_capability video_cap;
    struct v4l2_window     video_win;
    struct v4l2_format      videofmt;
    //struct v4l2_picture   video_pic;

    videofmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if((fd = open(address, O_RDONLY)) == -1){
        perror("cam_info: Can't open device");
        return false;
    }

    if(ioctl(fd, VIDIOC_QUERYCAP, &video_cap) == -1)
    {
        perror("cam_info: Can't get capabilities");
        return false;
    }
    else {
        printf("Name:\t\t '%s'\n", video_cap.driver);
        //printf("Minimum size:\t%d x %d\n", video_cap.minwidth, video_cap.minheight);
        //printf("Maximum size:\t%d x %d\n", video_cap.maxwidth, video_cap.maxheight);
    }

    if(ioctl(fd, VIDIOC_G_FMT, &videofmt) == -1)
        perror("cam_info: Can't get format information");
    else
        printf("Current size:\t%d  %d   %d   %x\n", videofmt.fmt.pix.sizeimage, videofmt.fmt.pix.height, videofmt.fmt.pix.width, videofmt.fmt.pix.pixelformat);

    height = videofmt.fmt.pix.height;
    width = videofmt.fmt.pix.width;
    sizeimage = videofmt.fmt.pix.sizeimage * 3;

    frameBuffer = std::make_unique<char[]>(512 * 512 * 3);
    tmpBuffer = std::make_unique<char[]>(512 * 512 * 3);

    std::fill(frameBuffer.get(), frameBuffer.get() + videofmt.fmt.pix.sizeimage * 3, '\0');
    std::fill(tmpBuffer.get(), tmpBuffer.get() + videofmt.fmt.pix.sizeimage * 3, '\0');
    
    decodeft = std::async(std::launch::async, decodeFun, this);
    
    return true;
}

CVideo::CVideo()
{
    fd = 0;
    sizeimage = 0;
    width = height = 0;
    frameready = false;
    keepDecodeTask = true;
}

CVideo::~CVideo()
{
    keepDecodeTask = false;
    
    if (fd)
    {
        close(fd);
    }

    if (decodeft.valid())
    {
        decodeft.wait();
    }
}
