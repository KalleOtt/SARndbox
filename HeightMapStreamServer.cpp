#include "HeightMapStreamServer.h"
#include "rxcpp/rx.hpp"

subject<Kinect::FrameBuffer> HeightMapStreamServer::FrambufferSubject;

void HeightMapStreamServer::foo() {
    int i = 0;
    auto sub = HeightMapStreamServer::FrambufferSubject
        .get_observable()
        .subscribe([&](auto frameBuffer){
            i++;
            char* frame = frameBuffer.getBuffer();
        });
}