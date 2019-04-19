#include <ctime>
#include <iostream>
#include <atomic>
#include <sstream>


#include <signal.h>
#include <string.h>  // for strsignal

#include <simple_responder.hpp>


namespace {

    // TODO: this is ghetto as hell, but this is only for a demo!
    ResponseSource g_source;


    int callback_minimal(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
    {
        int m;
        unsigned char buf[128];

        if (reason == LWS_CALLBACK_ESTABLISHED) {
            std::cout << "callback established" << std::endl;
        } else if (reason == LWS_CALLBACK_RECEIVE) {
            std::string to_send(g_source());
            memcpy(reinterpret_cast<char*>(buf), to_send.c_str(), to_send.size());
            m = lws_write(wsi, buf, to_send.size(), LWS_WRITE_TEXT);
            if (m < to_send.size()) {
    			std::cerr << "AH NUTS it BROOOOKE " << std::endl;
                std::cout << m << " vs " << to_send.size() << std::endl;
    		}
        } else {
            std::cerr << __LINE__ << ": I dunno what happened! code " << reason << std::endl;
        }
        return 0;
    }

        // List all acceptible protocols
        lws_protocols protocols[] = {
        	{"accel_server", callback_minimal, 128, 128, 0, NULL, 0},
            {NULL, NULL, 0, 0} /* terminator */
        };
    }



SimpleResponder::SimpleResponder(int port, ResponseSource source)
    : m_port(port)
    , m_source(source) // TODO: this should really be null-checked
{
    // TODO: Force port to be 0-20000, error somehow on bad number
    lws_context_creation_info info;
    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = m_port;
	info.protocols = protocols;
	info.vhost_name = "localhost";
	info.ws_ping_pong_interval = 10;

    m_context = lws_create_context(&info);
	if (!m_context) {
		std::cerr << __func__ << "lws init failed" << std::endl;
	}

    // TODO: This is SO BUSTED but hey it's just a demo!
    g_source = m_source;
}


int SimpleResponder::Respond(std::string response, int timeout_ms)
{
    // Not implemented yet!
    return -1;
}


int SimpleResponder::Respond(int timeout_ms)
{
    if (!IsReady()) {
        return -1;
    }
    return lws_service(m_context, timeout_ms);
}


SimpleResponder::~SimpleResponder()
{
    lws_context_destroy(m_context);
}

int SimpleResponder::ChangeResponseSource(ResponseSource source)
{
    if (!source) {
        std::cerr << __func__ << ": response source cannot be null!" << std::endl;
        return -1;
    }
    m_source = source;

    // TODO: This is SO BUSTED but hey it's just a demo!
    g_source = m_source;

    return 0;
}


bool SimpleResponder::IsReady()
{
    return (m_context != nullptr);
}