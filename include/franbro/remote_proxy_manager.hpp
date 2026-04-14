#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "local_resource_registry.hpp"

namespace franbro {

class RemoteProxyManager {
public:
    struct RemoteResourceAdvertisement {
        std::string host;
        std::vector<std::string> publishers;  // topic names
        std::vector<std::string> services;    // service names
        std::vector<std::string> action_servers;  // action names
    };

    RemoteProxyManager();

    // Advertise remote resources available on a connected host
    void advertiseRemoteResources(const RemoteResourceAdvertisement& advertisement);

    // Create local proxy for remote publisher
    void createPublisherProxy(const std::string& remote_topic_name,
                             const std::string& message_type,
                             const std::string& remote_host);

    // Create local proxy for remote service
    void createServiceProxy(const std::string& remote_service_name,
                           const std::string& service_type,
                           const std::string& remote_host);

    // Create local proxy for remote action
    void createActionProxy(const std::string& remote_action_name,
                          const std::string& action_type,
                          const std::string& remote_host);

    // Get list of available remote resources
    std::vector<RemoteResourceAdvertisement> getRemoteResources() const;

    // Cleanup proxies for a disconnected host
    void removeProxiesForHost(const std::string& host);

private:
    std::map<std::string, RemoteResourceAdvertisement> remote_resources_;
    std::map<std::string, void*> proxy_handles_;
};

}  // namespace franbro
