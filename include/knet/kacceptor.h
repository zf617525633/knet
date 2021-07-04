#pragma once
#include "kworker.h"
#include "kaddress.h"

namespace knet {

/**
 * 接收器，impl为实际的实现类。
 * 分为不同的实现版本，有unix,windows的两个版本
 */
class acceptor final {
public:
    explicit acceptor(workable& wkr);
    ~acceptor();

    void update();

    bool start(const address& addr);
    void stop();

    bool get_sockaddr(address& addr) const;

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

} // namespace knet
