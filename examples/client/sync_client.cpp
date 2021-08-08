#include "echo_conn.h"
#include <iostream>
#include <knet/kconnector.h>
#include <knet/kworker.h>
#include <knet/kutils.h>
//同步调用
int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // parse command line
    const char* ip = argc > 1 ? argv[1] : "localhost";
    const char* port = argc > 2 ? argv[2] : "8888";
    const auto client_num = argc > 3 ? std::atoi(argv[3]) : 1;
    const auto max_delay_ms = argc > 4 ? std::atoi(argv[4]) : 10;

    // log parameter info
    std::cout << "Hi, KNet(Sync Client)" << std::endl
              << "ip:" << ip << std::endl
              << "port: " << port << std::endl
              << "client_num: " << client_num << std::endl
              << "max_delay_ms: " << max_delay_ms << std::endl;

    // parse ip address
    const auto fa = family_t::Ipv4;
    address addr;
    //此处的ip和port为服务端的ip和port，只有连接之后才能知道自己的ip和port
    if (!address::resolve_one(ip, port, fa, addr)) {
        std::cerr << "resolve address " << ip << ":" << port << " failed!" << std::endl;
        return -1;
    }

    // create worker
    cecho_conn_factory cf;
    worker wkr(cf);

    // create connector
    // 主要工作就是创建连接，并开始连接
    //然后加入worker到连接器中，真正干活的是worker
    connector cnctor(wkr);

    // check console input
    auto& mgr = echo_mgr::get_instance();
    mgr.set_is_server(false);
    //检查命令行输入
    mgr.check_console_input();
    mgr.set_max_delay_ms(max_delay_ms);
    mgr.set_enable_log(true);

    auto last_ms = now_ms();
    while (true) {
        const auto beg_ms = now_ms();
        const auto delta_ms = (beg_ms > last_ms ? beg_ms - last_ms : 0);
        last_ms = beg_ms;

        //这个worker就是刚刚传入到connector中的worker
        //此时worker开始不间断的干活
        wkr.update();

        const auto conn_num = mgr.get_conn_num();
        if (mgr.get_disconnect_all()) {
            if (0 == conn_num)
                break;
        } else if (conn_num < client_num) {
            if (!cnctor.connect(addr))
                std::cerr << "connect failed! address: " << addr << std::endl;
        }

        mgr.update(delta_ms);

        const auto end_ms = now_ms();
        const auto cost_ms = end_ms > beg_ms ? end_ms - beg_ms : 0;
        constexpr int64_t min_interval_ms = 50;
        sleep_ms(cost_ms < min_interval_ms ? min_interval_ms - cost_ms : 1);
    }

    return 0;
}
