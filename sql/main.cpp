#include "db.hpp"
#include <iostream>
#include <utility>
#include <tuple>
#include <typeinfo>
#include <chrono>

using namespace std;
using namespace db;

struct ConnFactory
{
    static PGconn* get()
    {
        if (conn == nullptr) {
            const char *keywords[] = { "host", "hostaddr", "port", "dbname", "user", "password", "sslmode", nullptr };
            const char *values[] = { "localhost", "127.0.0.1", "5432", "optimizer", "optimizer", "dupa88", "disable", nullptr };
            conn = PQconnectdbParams(keywords, values, 0);
            auto status = PQstatus(conn);
            if (status != CONNECTION_OK) {
                auto errmsg = PQerrorMessage(conn);
                cerr << errmsg << endl;
                throw runtime_error("Error establishing a database connection"s + errmsg);
            }
        }
        return conn;
    }

    static PGconn* conn;
};

PGconn* ConnFactory::conn = nullptr;

namespace Model {
struct Customer {
    static constexpr auto table_name = "customers";
    struct id : Field<int> {
        id(int arg) : Field(arg) {}
        id() = default;
        static constexpr auto column = "id";
        id(const char* s) : Field(stoi(s)) {}
    };
    struct company : Field<string> {
        static constexpr const auto column = "company";
        company() = default;
        company(const char* s) : Field(s) {}
    };
    struct email : Field<string> {
        static constexpr const auto column = "email";
        email() = default;
        email(const char* s) : Field(s) {}
    };
    using type = tuple<id, company, email>;
    using primary_key = id;
    using Repository = Repository<Customer, Postgres<ConnFactory>>;

    Customer(const type& args) : value(args) {}
    Customer() {}
    type value;
};

constexpr const char* const Customer::id::column;
constexpr const char* const Customer::company::column;
constexpr const char* const Customer::email::column;

namespace keys {
static Customer::id id_;
static Customer::company name_;
}

}

int main()
{
    using namespace Model;
    using namespace Model::keys;

    //auto selectExpr = select<Customer>(id_, name_).where(id_ == 2);
    //cout << typeid(selectExpr).name() << endl;
    //cout << selectExpr() << endl;

    //auto expr2 = select<Customer>(id_, name_);

    //cout << AndExpr<EqualExpr<User::id, int>, EqualExpr<User::id, int>>::is_expr::value << endl;

    (id_ == 2 && id_ == 2);
    //cout << typeid(expr3).name() << endl;
    //expr2.where(id_ == 2 && id_ == 2);

    using eqType = EqualExpr<Customer::id, int>;
    eqType::param_type t1;
    cout << "EqualExpr<User::id, int>::param_type = " << typeid(t1).name() << endl;

    Concat<eqType::param_type, eqType::param_type>::type t2;
    cout << "Concat<eqType::param_type, eqType::param_type>::type = " << typeid(t2).name() << endl;

    AndExpr<eqType, eqType>::type t3;
    cout << typeid(t3).name() << endl;

    AndExpr<eqType, eqType>::param_type t4;
    cout << typeid(t4).name() << endl;

    cout << typeid(id_ == 1 && id_ == 2 && id_ == 3).name() << endl;
    cout << typeid(id_ == 1 || id_ == 2).name() << endl;
    cout << typeid(id_ == 1 || id_ == 2 || id_ == 3).name() << endl;

    cout << typeid(id_ == 1 || id_ == 2 && id_ == 3).name() << endl;
    cout << typeid(id_ == 2 && id_ == 3 || id_ == 3).name() << endl;

    //auto x = select<Customer>(name_).where(id_ == 2 && name_ == "mateusz"s);

    Customer::type values(0, "acme", "test@test.com");
    Customer u(values);

    
    for (auto i = 1u; i <= 100; ++i) {
        get<Customer::id>(u.value) = i;
        Customer::Repository::add(u);
    }
    for (auto i = 1u; i <= 100; ++i) {
        auto x3 = Customer::Repository::get(i);
    }

    /*
    auto it = User::Repository::cbegin();
    for (auto& user : User::Repository::get()) {
        // iterate over the set of users
    }

    auto user = User::Repository::get(2);

    auto user = User::Repository::get(id_ == 2);
    */
}
