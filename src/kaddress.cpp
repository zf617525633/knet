#include "../include/knet/kaddress.h"
#include "internal/kinternal.h"
#include <cstring>

namespace {

union saun {
    void* v;
    sockaddr_in* sa;
    sockaddr_in6* sa6;
    sockaddr_storage* ss;

    saun(void* v_)
        : v(v_)
    {
    }
};

union csaun {
    const void* v;
    const sockaddr_in* sa;
    const sockaddr_in6* sa6;

    csaun(const void* v_)
        : v(v_)
    {
    }
};

} // namespace

namespace knet {

bool address::resolve_all(const std::string& node_name, const std::string& service_name,
    std::vector<address>& addrs)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_UNSPEC;

    const char* nn = node_name.c_str();
    if (node_name.empty()) {
        nn = nullptr;
        hints.ai_flags = AI_PASSIVE;
    }

    struct addrinfo* result = nullptr;
    const auto ret = ::getaddrinfo(nn, service_name.c_str(), &hints, &result);
    if (0 != ret || nullptr == result)
        return false;

    for (auto rp = result; nullptr != rp; rp = rp->ai_next) {
        //创建一个空的address并插入到队列的尾部
        addrs.emplace_back();

        auto& addr = addrs.back();
        ::memcpy(addr._addr, rp->ai_addr, rp->ai_addrlen);
    }

    ::freeaddrinfo(result);

    return true;
}

/**
 * 转化IP地址为address类型
 * @param node_name  ip地址
 * @param service_name 端口号
 * @param fa IPV4 or IPV6
 * @param addr 解析完成的地址
 * @return true解析成功，false解析失败。
 */
bool address::resolve_one(const std::string& node_name, const std::string& service_name,
    family_t fa, address& addr)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    switch (fa) {
    case family_t::Unknown:
        hints.ai_family = AF_UNSPEC;
        break;
    case family_t::Ipv4:
        hints.ai_family = AF_INET;
        break;
    case family_t::Ipv6:
        hints.ai_family = AF_INET6;
        break;
    default:
        return false;
    }

    //如果节点地址（ip或者域名）为空，则算是绑定模式bind()可以获取ip
    const char* nn = node_name.c_str();
    if (node_name.empty()) {
        nn = nullptr;
        hints.ai_flags = AI_PASSIVE;
    }

    struct addrinfo* result = nullptr;
    const auto ret = ::getaddrinfo(nn, service_name.c_str(), &hints, &result);
    if (0 != ret || nullptr == result)
        return false;

    ::memcpy(addr._addr, result->ai_addr, result->ai_addrlen);
    ::freeaddrinfo(result);

    return true;
}

bool address::pton(family_t fa, const std::string& addr, uint16_t port)
{
    memset(&_addr, 0, sizeof(_addr));
    saun tmp(_addr);

    if (family_t::Ipv4 == fa) {
        auto& addr4 = *tmp.sa;
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);
        auto sa = reinterpret_cast<void*>(&addr4.sin_addr);

        return 1 == ::inet_pton(addr4.sin_family, addr.c_str(), sa);
    } else if (family_t::Ipv6 == fa) {
        auto& addr6 = *tmp.sa6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port);
        auto sa = reinterpret_cast<void*>(&addr6.sin6_addr);

        return 1 == ::inet_pton(addr6.sin6_family, addr.c_str(), sa);
    } else {
        auto& ad = *tmp.ss;
        ad.ss_family = AF_UNSPEC;
    }

    return false;
}

bool address::ntop(std::string& addr, uint16_t& port) const
{
    addr.clear();
    port = 0;
    csaun tmp(_addr);

    const auto fa = get_family();
    if (family_t::Ipv4 == fa) {
        char buf[INET_ADDRSTRLEN] = {};
        const auto& addr4 = *tmp.sa;
        auto csa = static_cast<const void*>(&addr4.sin_addr);
        auto sa = const_cast<void*>(csa);

        if (nullptr == ::inet_ntop(AF_INET, sa, buf, INET_ADDRSTRLEN))
            return false;

        addr = buf;
        port = ntohs(addr4.sin_port);
    } else if (family_t::Ipv6 == fa) {
        char buf[INET6_ADDRSTRLEN] = {};
        const auto& addr6 = *tmp.sa6;
        auto csa = static_cast<const void*>(&addr6.sin6_addr);
        auto sa = const_cast<void*>(csa);

        if (nullptr == ::inet_ntop(AF_INET6, sa, buf, INET6_ADDRSTRLEN))
            return false;

        addr = buf;
        port = ntohs(addr6.sin6_port);
    } else {
        return false;
    }

    return true;
}

int address::get_rawfamily() const
{
    const auto addr = as_ptr<sockaddr_storage>();
    return addr->ss_family;
}

family_t address::get_family() const
{
    switch (get_rawfamily()) {
    case AF_INET:
        return family_t::Ipv4;
    case AF_INET6:
        return family_t::Ipv6;
    default:
        break;
    }
    return family_t::Unknown;
}

int address::get_socklen() const
{
    switch (get_family()) {
    case family_t::Ipv4:
        return sizeof(sockaddr_in);
    case family_t::Ipv6:
        return sizeof(sockaddr_in6);
    default:
        break;
    }
    static_assert(sizeof(_addr) >= sizeof(sockaddr_storage), "");
    return sizeof(_addr);
}

std::string address::to_string() const
{
    std::string addr;
    uint16_t port;
    if (!ntop(addr, port))
        return "ntop() failed!";
    return addr + ":" + std::to_string(port);
}

} // namespace knet
