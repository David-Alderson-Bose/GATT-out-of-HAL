#include <string>
#include <functional>
#include <libwebsockets.h>

using ResponseSource = std::function<std::string()>;

class SimpleResponder {
    public:
        SimpleResponder(int port, ResponseSource source);
        ~SimpleResponder();
        bool IsReady();
        int Respond(std::string response, int timeout_ms=0); // manual response
        int Respond(int timeout_ms=0); // Respond from source
        int ChangeResponseSource(ResponseSource source);

    private:
        int m_port;
        lws_context* m_context;
        ResponseSource m_source;
};

