#include "kacceptor_unix.h"

namespace knet {

acceptor::impl::impl(workable& wkr)
    : _wkr(wkr)
{
}

acceptor::impl::~impl()
{
    kassert(INVALID_RAWSOCKET == _rs);
}

void acceptor::impl::update()
{
    _plr->poll();
}

/***
 * 绑定 监听，设置监听参数
 * @param addr
 * @return
 */
bool acceptor::impl::start(const address& addr)
{
    if (INVALID_RAWSOCKET != _rs)
        return false;

    _family = addr.get_rawfamily();
    _rs = create_rawsocket(_family, true);
    if (INVALID_RAWSOCKET == _rs)
        return false;

    int on = 1;
    if (!set_rawsocket_opt(_rs, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
        kdebug("set_rawsocket_opt(SO_REUSEADDR) failed!");
        close_rawsocket(_rs);
        return false;
    }

    //建立一个指向poller的智能指针。
    _plr.reset(new poller(*this));

    const auto sa = addr.as_ptr<sockaddr>();
    const auto salen = addr.get_socklen();
    //绑定，监听，并加入到监听中
    if (RAWSOCKET_ERROR == ::bind(_rs, sa, salen)
        || RAWSOCKET_ERROR == ::listen(_rs, SOMAXCONN)
        || !_plr->add(_rs, this)) {
        close_rawsocket(_rs);
        return false;
    }

    return true;
}

void acceptor::impl::stop()
{
    close_rawsocket(_rs);
    _plr.reset();
}

bool acceptor::impl::on_pollevent(void* key, void* evt)
{
#ifdef __linux__
    while (true) {
        auto rs = ::accept4(_rs, nullptr, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (INVALID_RAWSOCKET == rs) {
            if (EINTR == errno)
                continue;

            if (EAGAIN != errno)
                kdebug("accept4() failed!");
            break;
        }

        _wkr.add_work(rs);
    }
#else
    while (true) {
        //接受一个客户端套接字
        auto rs = ::accept(_rs, nullptr, 0);
        if (INVALID_RAWSOCKET == rs) {
            //当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误
            if (EINTR == errno)
                continue;
            //EAGAIN 提示你的应用程序现在没有数据可读请稍后再试。
            if (EAGAIN != errno)
                kdebug("accept() failed!");
            break;
        }

        if (!set_rawsocket_nonblock(rs)) {
            kdebug("set_rawsocket_nonblock() failed!");
            close_rawsocket(rs);
            continue;
        }

        if (!set_rawsocket_cloexec(rs)) {
            kdebug("set_rawsocket_cloexec() failed!");
            close_rawsocket(rs);
            continue;
        }

        _wkr.add_work(rs);
    }
#endif
    return true;
} // namespace knet

} // namespace knet
