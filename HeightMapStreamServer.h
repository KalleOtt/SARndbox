#include "websocketpp/config/asio_no_tls.hpp"

#include "websocketpp/server.hpp"

#include <iostream>
#include <set>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
// #include "websocketpp/common/thread.hpp"

#include "rxcpp/rx.hpp"
#include <Kinect/FrameBuffer.h>

#include <Geometry/Point.h>
typedef Geometry::Point<double,3> Point;

#include "RainMaker.h"

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
      : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

typedef websocketpp::server<websocketpp::config::asio> server;

class HeightMapStreamServer {
    public:
        HeightMapStreamServer();


        void run();

        void on_open(connection_hdl hdl);

        void on_close(connection_hdl hdl);

        void on_message(connection_hdl hdl, server::message_ptr msg);

        void process_messages();

        void addNewFrame(Kinect::FrameBuffer &frame);

        void stop();
        virtual ~HeightMapStreamServer();
    private:
        bool running;
        typedef std::set<connection_hdl,std::owner_less<connection_hdl> > con_list;

        server m_server;
        con_list m_connections;
        std::queue<action> m_actions;

        mutex m_action_lock;
        mutex m_connection_lock;
        condition_variable m_action_cond;

        rxcpp::rxsub::subject<Kinect::FrameBuffer> frameSubject;
        rxcpp::rxsub::subject<Point> rainPointSubject;
        rxcpp::subscription frameSubscription;
        rxcpp::subscription rainSubscription;

        unsigned long lastFrameTransmission;

        RainMaker * rainMaker;
};