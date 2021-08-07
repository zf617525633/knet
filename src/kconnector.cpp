#include "../include/knet/kconnector.h"
#include "internal/kinternal.h"

namespace knet {

connector::connector(workable& wkr)
    : _wkr(wkr)
{
}

connector::~connector() = default;

bool connector::connect(const address& addr)
{
    //创建socket
    //连接器由客户端创建
    auto rs = create_rawsocket(addr.get_rawfamily(), false);
    if (INVALID_RAWSOCKET == rs)
        return false;

    const auto sa = addr.as_ptr<sockaddr>();
    const auto salen = addr.get_socklen();
    //连接到服务端
    if (RAWSOCKET_ERROR == ::connect(rs, sa, salen)) {
        kdebug("connect() failed");
        close_rawsocket(rs);
        return false;
    }

#ifndef _WIN32
    if (!set_rawsocket_nonblock(rs)) {
        kdebug("set_rawsocket_nonblock() failed");
        close_rawsocket(rs);
        return false;
    }
#endif
    //连接成功就加入到poll中监控，poll在不同的系统中具体实现不同
    _wkr.add_work(rs);
    return true;
}

} // namespace knet
