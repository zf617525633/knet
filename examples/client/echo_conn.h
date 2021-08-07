#pragma once
#include <knet/kconn_factory.h>
#include "../echo_mgr.h"

using namespace knet;
//客户端服务端 echo，连接器，接受数据，发送数据打印数据
class cecho_conn : public conn {
public:
    explicit cecho_conn(conn_factory& cf);

private:
    void do_on_connected() override;
    size_t do_on_recv_data(char* data, size_t size) override;
    void do_on_timer(int64_t ms, const userdata& ud) override;

    void generate_packages();
    bool send_package();
    int32_t check_package(char* data, size_t size);

private:
    char _buf[128 * 1024] = {};
    uint32_t _used_buf_size = 0;
    uint32_t _send_buf_size = 0;
    uint32_t _recv_buf_size = 0;

    uint32_t _next_send_pkg_id = 0;
    uint32_t _next_recv_pkg_id = 0;
};

class cecho_conn_factory : public conn_factory {
public:
    cecho_conn_factory();
    explicit cecho_conn_factory(connid_gener gener);

private:
    conn* do_create_conn() override;
    void do_destroy_conn(conn* c) override;
};

class cecho_conn_factory_creator : public conn_factory_creator {
    conn_factory* do_create_factory(connid_gener gener) override
    {
        return new cecho_conn_factory(gener);
    }
};
