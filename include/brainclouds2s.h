/*
    2019 @ bitheads inc.
    Author: David St-Louis
*/

#ifndef BRAINCLOUDS2S_H_INCLUDED
#define BRAINCLOUDS2S_H_INCLUDED

#include <functional>
#include <memory>
#include <string>

static const std::string DEFAULT_S2S_URL = 
    "https://sharedprod.braincloudservers.com/s2sdispatcher";

class S2SContext;

using S2SCallback = std::function<void(const std::string&)>;
using S2SContextRef = std::shared_ptr<S2SContext>;

class S2SContext
{
public:
    /*
     * Create a new S2S context
     * @param appId Application ID
     * @param serverName Server name
     * @param serverSecret Server secret key
     * @param url The server url to send the request to.
     * @return A new S2S context, or nullptr if something bad happened.
     */
    static S2SContextRef create(const std::string& appId,
                                const std::string& serverName,
                                const std::string& serverSecret,
                                const std::string& url = DEFAULT_S2S_URL);

    virtual ~S2SContext() {}

    /*
     * Set wether S2S messages and errors are logged to the console
     * @param enabled Will log if true. Default false
     */
    virtual void setLogEnabled(bool enabled) = 0;

    /*
     * Send an S2S request.
     * @param json Content to be sent
     * @param callback Callback function
     */
    virtual void request(
        const std::string& json,
        const S2SCallback& callback) = 0;

    /*
     * Update requests and perform callbacks on the calling thread.
     * @param timeoutMS Time to block on the call in milliseconds. 
     *                  Pass 0 to return immediately.
     */
    virtual void runCallbacks(uint64_t timeoutMS = 0) = 0;

protected:
    S2SContext() {}
};

#endif /* BRAINCLOUDS2S_H_INCLUDED */
