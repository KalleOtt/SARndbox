#include "HeightMapStreamServer.h"

HeightMapStreamServer::HeightMapStreamServer() {
    running = false;
    // Initialize Asio Transport
    m_server.init_asio();

    // Register handler callbacks
    m_server.set_open_handler(bind(&HeightMapStreamServer::on_open,this,::_1));
    m_server.set_close_handler(bind(&HeightMapStreamServer::on_close,this,::_1));
    m_server.set_message_handler(bind(&HeightMapStreamServer::on_message,this,::_1,::_2));
}

void HeightMapStreamServer::run() {
    running = true;
    // listen on specified port
    m_server.listen(9000);

    // Start the server accept loop
    m_server.start_accept();

        std::cout << "starting frameSubscription" << std::endl;
    auto threads = rxcpp::observe_on_event_loop();
    frameSubscription = frameSubject.get_observable()
            .observe_on(threads)
            .subscribe_on(threads)
            .subscribe([this](auto frame) {
                std::cout << "got frame send it over websocket" << std::endl;
                lock_guard<mutex> guard(m_connection_lock);
                size_t frameSize = frame.getSize(0) * frame.getSize(1) * sizeof(float);
                con_list::iterator it;
                int connectionCounter = 1;
                for (it = m_connections.begin(); it != m_connections.end(); ++it) {

                    std::cout << "sending frame to connection " << connectionCounter << std::endl;
                    m_server.send(*it, frame.getBuffer(), frameSize, websocketpp::frame::opcode::binary);
                }
            });

    // Start the ASIO io_service run loop
    try {
        m_server.run();
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }


}

HeightMapStreamServer::~HeightMapStreamServer() {
    std::cout << "stop server";
    running = false;
    m_server.stop();
    m_server.stop_listening();
    m_server.stop_perpetual();

    stop();
}

void HeightMapStreamServer::stop() {

}

void HeightMapStreamServer::on_open(connection_hdl hdl) {
    {
        lock_guard<mutex> guard(m_action_lock);
        //std::cout << "on_open" << std::endl;
        m_actions.push(action(SUBSCRIBE,hdl));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::on_close(connection_hdl hdl) {
    {
        lock_guard<mutex> guard(m_action_lock);
        //std::cout << "on_close" << std::endl;
        m_actions.push(action(UNSUBSCRIBE,hdl));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::on_message(connection_hdl hdl, server::message_ptr msg) {
    // queue message up for sending by processing thread
    {
        lock_guard<mutex> guard(m_action_lock);
        //std::cout << "on_message" << std::endl;
        m_actions.push(action(MESSAGE,hdl,msg));
    }
    m_action_cond.notify_one();
}

void HeightMapStreamServer::process_messages() {
    while(running) {
        unique_lock<mutex> lock(m_action_lock);

        while(m_actions.empty()) {
            m_action_cond.wait(lock);
        }

        action a = m_actions.front();
        m_actions.pop();

        lock.unlock();

        if (a.type == SUBSCRIBE) {
            lock_guard<mutex> guard(m_connection_lock);
            m_connections.insert(a.hdl);
        } else if (a.type == UNSUBSCRIBE) {
            lock_guard<mutex> guard(m_connection_lock);
            m_connections.erase(a.hdl);
        } else if (a.type == MESSAGE) {
            lock_guard<mutex> guard(m_connection_lock);

            con_list::iterator it;
            for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                m_server.send(*it,a.msg);
            }
        } else {
            // undefined.
        }
    }
}

void HeightMapStreamServer::addNewFrame(Kinect::FrameBuffer frame) {
    frameSubject.get_subscriber().on_next(frame);
}