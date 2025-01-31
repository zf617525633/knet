#include "ksocket_unix.h"
#include "../internal/kinternal.h"

namespace knet {

struct socket::impl::sockbuf {
    size_t used_size = 0;
    char chunk[SOCKET_RWBUF_SIZE - sizeof(used_size)] = {};

    char* unused_ptr()
    {
        return chunk + used_size;
    }
    size_t unused_size() const
    {
        return sizeof(chunk) - used_size;
    }

    bool can_read() const
    {
        return used_size < sizeof(chunk);
    }
    bool can_write() const
    {
        return used_size > 0;
    }

    void discard_used(size_t num)
    {
        kassert(used_size >= num);
        used_size -= num;
        if (used_size > 0)
            ::memmove(chunk, chunk + num, used_size);
    }

    bool try_write(rawsocket_t rs)
    {
        size_t size = 0;
        ssize_t ret = 0;

        while (size < used_size) {
            do {
#ifdef SO_NOSIGPIPE
                ret = ::write(rs, chunk + size, used_size - size);
#else
                ret = ::send(rs, chunk + size, used_size - size, MSG_NOSIGNAL);
#endif
            } while (-1 == ret && EINTR == errno);

            if (-1 == ret) {
                if (EAGAIN == errno || EWOULDBLOCK == errno)
                    break;
                return false;
            }

            if (0 == ret)
                return false;

            size += ret;
        }

        if (size > 0)
            discard_used(size);

        return true;
    }

    bool try_read(rawsocket_t rs)
    {
        ssize_t ret = 0;
        while (unused_size() > 0) {
            do
                ret = ::read(rs, unused_ptr(), unused_size());
            while (-1 == ret && EINTR == errno);

            if (-1 == ret) {
                if (EAGAIN == errno || EWOULDBLOCK == errno)
                    break;
                return false;
            }

            if (0 == ret)
                return false;

            used_size += ret;
        }
        return true;
    }

    bool check_can_write(const buffer* buf, size_t num) const
    {
        size_t total_size = 0;

        for (size_t i = 0; i < num; ++i) {
            auto b = buf + i;
            kassert(b->size > 0 && nullptr != b->data);
            total_size += b->size;
        }

        return total_size < unused_size();
    }

    void write(const buffer* buf, size_t num)
    {
        for (size_t i = 0; i < num; ++i) {
            auto b = buf + i;
            ::memcpy(unused_ptr(), b->data, b->size);
            used_size += b->size;
        }
    }
};

socket::impl::impl(socket& s, rawsocket_t rs)
    : _s(s)
    , _rs(rs)
{
}
socket::impl::~impl()
{
    kassert(INVALID_RAWSOCKET == _rs);
    kassert(_f.is_close());
}

//初始化 unix socket
bool socket::impl::init(poller& plr, conn_factory& cf)
{
    if (INVALID_RAWSOCKET == _rs)
        return false;

    // 连接描述符加入到监控中
    if (!plr.add(_rs, &_s)) {
        close_rawsocket(_rs);
        _f.mark_close();
        return false;
    }

    //创建连接器
    _c = cf.create_conn();

    //新建 发送缓存和接受缓存
    _rb.reset(new sockbuf());
    _wb.reset(new sockbuf());
    //启动连接，就是把res
    if (!start()) {
        close_rawsocket(_rs);
        if (!_f.is_close())
            _f.mark_close();

        _c->on_disconnect();
        _c = nullptr;

        return false;
    }

    return true;
}

bool socket::impl::write(buffer* buf, size_t num)
{
    if (nullptr == buf || 0 == num)
        return false;

    if (is_closing())
        return false;

    if (!_wb->check_can_write(buf, num))
        return false;

    _wb->write(buf, num);

    if (_wb->can_write() && _f.is_write())
        return try_write();

    return true;
}

void socket::impl::close()
{
    if (!_f.is_close())
        _f.mark_close();

    close_rawsocket(_rs);

    if (_f.is_call())
        return;

    _c->on_disconnect();
    _c = nullptr;

    _s.dispose();
}

bool socket::impl::handle_pollevent(void* evt)
{
    auto ret = true;

    do {
#ifdef __linux__
        auto e = reinterpret_cast<struct epoll_event*>(evt);

        if (0 != (e->events & (EPOLLERR | EPOLLHUP))) {
            ret = false;
            break;
        }

        if (0 != (e->events & EPOLLIN) && !handle_can_read()) {
            ret = false;
            break;
        }

        if (0 != (e->events & EPOLLOUT) && !handle_can_write()) {
            ret = false;
            break;
        }
#else
        auto e = reinterpret_cast<struct kevent*>(evt);

        if (0 != (e->flags & EV_EOF)) {
            ret = false;
            break;
        }

        if (EVFILT_READ == e->filter && !handle_can_read()) {
            ret = false;
            break;
        }

        if (EVFILT_WRITE == e->filter && !handle_can_write()) {
            ret = false;
            break;
        }
#endif
    } while (false);

    if (!ret)
        close();

    return ret;
}

//启动 socket ，就是把socket加入到
//conn 连接器中
bool socket::impl::start()
{
    scoped_call_flag s(_f);
    _c->on_connected(&_s);

    return !_f.is_close();
}

bool socket::impl::handle_can_read()
{
    do {
        if (!_rb->try_read(_rs))
            return false;

        const auto is_full = 0 == _rb->unused_size();
        if (_rb->used_size > 0 && !handle_read())
            return false;

        if (!is_full)
            break;
    } while (true);

    return true;
}

bool socket::impl::handle_can_write()
{
    if (!_f.is_write()) {
        _f.mark_write();
        if (_wb->can_write())
            return try_write();
    }
    return true;
}

bool socket::impl::handle_read()
{
    const auto max_size = _rb->used_size;
    size_t size = 0;
    {
        const auto ptr = _rb->chunk;
        scoped_call_flag s(_f);
        do {
            const auto t = _c->on_recv_data(ptr + size, max_size - size);

            if (0 == t || _f.is_close())
                break;

            size += t;
        } while (size < max_size);
    }

    if (_f.is_close())
        return false;

    if (size > max_size)
        size = max_size;

    _rb->discard_used(size);

    return true;
}

bool socket::impl::try_write()
{
    if (!_wb->try_write(_rs))
        return false;

    if (_wb->used_size > 0)
        _f.unmark_write();

    return true;
}

} // namespace knet
