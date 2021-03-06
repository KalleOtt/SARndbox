#include "HeightMapStreamServer.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <chrono>

boost::regex rainExpression("-rain\\(([0-9]+),([0-9]+)\\)$"); 

Point processRainMessage(boost::cmatch& matches)
{ 
      // matches[0] contains the whole string 
      // matches[1] contains the x coordinate string
      // matches[2] contains the y coordinate string
      int x = std::atoi(matches[1].first); 
      int y = std::atoi(matches[2].first); 
      int z = 42;
      return Point(double(x), double(y), double(z));
}

HeightMapStreamServer::HeightMapStreamServer() {
    running = false;
    // Initialize Asio Transport
    m_server.init_asio();
    m_server.get_alog().clear_channels(websocketpp::log::alevel::frame_header |
        websocketpp::log::alevel::frame_payload |
        websocketpp::log::alevel::control);
    // Register handler callbacks
    m_server.set_open_handler(bind(&HeightMapStreamServer::on_open,this,::_1));
    m_server.set_close_handler(bind(&HeightMapStreamServer::on_close,this,::_1));
    m_server.set_message_handler(bind(&HeightMapStreamServer::on_message,this,::_1,::_2));
    lastFrameTransmission = 0;
}

void HeightMapStreamServer::run() {
    running = true;
    // listen on specified port
    m_server.listen(9000);

    // Start the server accept loop
    m_server.start_accept();
    std::thread processingThread(bind(&HeightMapStreamServer::process_messages,this));

    std::cout << "starting frameSubscription" << std::endl;
    auto threads = rxcpp::observe_on_event_loop();


    frameSubscription = frameSubject.get_observable()
            .observe_on(threads)
            .subscribe_on(threads)
            .subscribe([this](auto frame) {
                std::lock_guard<std::mutex> guard(m_connection_lock);
                size_t frameSize = frame.getSize(0) * frame.getSize(1) * sizeof(float);
                con_list::iterator it;
                int connectionCounter = 1;
                for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                    m_server.send(*it, frame.getBuffer(), frameSize, websocketpp::frame::opcode::binary);
                }
            });


    using namespace std::chrono;

    #define TIME_UNIT seconds

    rainSubscription = rainPointSubject.get_observable()
            .observe_on(threads)
            .subscribe_on(threads)
            .buffer_with_time(TIME_UNIT(10), TIME_UNIT(1))
            .subscribe([this](std::vector<Point> rainPoints){
                if(rainMaker) {
                    rainMaker->setExternalBlobs(rainPoints);
                }
            });

    // Start the ASIO io_service run loop
    try {
        m_server.run();
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }
    processingThread.join();
}

HeightMapStreamServer::~HeightMapStreamServer() {

}

void HeightMapStreamServer::stop() {
    running = false;
    std::cout << "stop server";
    m_server.stop();
    m_server.stop_listening();
    m_server.stop_perpetual();
}

void HeightMapStreamServer::on_open(connection_hdl hdl) {
    {
        std::lock_guard<std::mutex> guard(m_action_lock);
        //std::cout << "on_open" << std::endl;
        m_actions.push(action(SUBSCRIBE,hdl));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::on_close(connection_hdl hdl) {
    {
        std::lock_guard<std::mutex> guard(m_action_lock);
        //std::cout << "on_close" << std::endl;
        m_actions.push(action(UNSUBSCRIBE,hdl));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::on_message(connection_hdl hdl, server::message_ptr msg) {
    // queue message up for sending by processing thread
    {
        std::lock_guard<std::mutex> guard(m_action_lock);
        //std::cout << "on_message" << std::endl;
        m_actions.push(action(MESSAGE,hdl,msg));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::process_messages() {
    std::cout << "process messages" << std::endl;
    while(running) {
        std::unique_lock<std::mutex> lock(m_action_lock);

        while(m_actions.empty()) {
            m_action_cond.wait(lock);
        }

        action a = m_actions.front();
        m_actions.pop();

        lock.unlock();

        if (a.type == SUBSCRIBE) {
            std::lock_guard<std::mutex> guard(m_connection_lock);
            m_connections.insert(a.hdl);
        } else if (a.type == UNSUBSCRIBE) {
            std::lock_guard<std::mutex> guard(m_connection_lock);
            m_connections.erase(a.hdl);
        } else if (a.type == MESSAGE) {
            std::lock_guard<std::mutex> guard(m_connection_lock);

            con_list::iterator it;
            for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                std::string payload = a.msg.get_payload();
                boost::cmatch matches;
                if(boost::regex_match(payload, matches, rainExpression)) {
                    auto rainPoint = processRainMessage(matches);
                    rainPointSubject.get_subscriber().on_next(rainPoint);
                }
                else {
                    m_server.send(*it,a.msg);
                }

            }
        } else {
            // undefined.
        }
    }
}

void HeightMapStreamServer::addNewFrame(Kinect::FrameBuffer &frame) {

    unsigned long now = 
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    if(now - lastFrameTransmission > 500) {
        frameSubject.get_subscriber().on_next(frame);
        lastFrameTransmission = now;
    }

}