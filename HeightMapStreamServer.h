#include "rxcpp/rx.hpp"
#include <Kinect/FrameBuffer.h>

using namespace rxcpp;
using namespace rxcpp::subjects;
using namespace rxcpp::operators;
using namespace rxcpp::util;

class HeightMapStreamServer {
    public:
        static subject<Kinect::FrameBuffer> FrambufferSubject;
        static void foo();

    private:


};