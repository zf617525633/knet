#pragma once
#include "kconnection.h"
#include "kworker.h"
#include "kuserdata.h"
#include <unordered_map>


namespace knet
{
    using connid_t = int32_t;
    using timerid_t = int64_t;

    class tconnection_factory;
    class tconnection : public connection
    {
    public:
        tconnection(connid_t id, tconnection_factory* cf) : _id(id), _cf(cf) {}

        virtual void on_timer(int64_t absms, const userdata& ud) = 0;

        timerid_t add_timer(int64_t absms, const userdata& ud);
        void del_timer(int64_t absms);

        connid_t get_connid() const { return _id; }
        tconnection_factory* get_factory() const { return _cf; }

    private:
        const connid_t _id;
        tconnection_factory* _cf;
    };

    class _tconn_timer;
    class tconnection_factory : public connection_factory
    {
    public:
        tconnection_factory();
        ~tconnection_factory() override;

        virtual void update();

        tconnection* get_conn(connid_t id) const;

        timerid_t add_timer(connid_t cid, int64_t absms, const userdata& ud);
        void del_timer(connid_t cid, int64_t absms);

    protected:
        connid_t get_next_connid() { return _next_cid++; }

        virtual tconnection* create_connection_impl() = 0;
        virtual void destroy_connection_impl(tconnection* tconn) { delete tconn; }

    private:
        connection* create_connection() override;
        void destroy_connection(connection* conn) override;

    private:
        connid_t _next_cid = 0;
        std::unordered_map<connid_t, tconnection*> _tconns;

        void on_timer(connid_t cid, int64_t absms, const userdata& ud);
        friend class _tconn_timer;
        _tconn_timer* _timer = nullptr;
    };
}