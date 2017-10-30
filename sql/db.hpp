#pragma once

#include <string>
#include <tuple>
#include <sstream>
#include <type_traits>
#include <libpq-fe.h>
#include <iostream>
#include <set>
#include <vector>
#include <list>

namespace db
{

using namespace std;

template<typename T>
struct Field {
    using type = T;
    T value;
    T operator=(const T& val) { value = val; }
    Field(const T& arg) : value(arg) {}
    Field() : value() {}
};

template<typename U, typename T>
typename U::type& get(T& obj)
{
    return get<U>(obj.value).value;
}

template<typename...ColName>
struct ColNameExpr {
    struct is_expr { static constexpr const bool value = true; };
    using column_types = tuple<ColName...>;
    column_types columns;
};

template<typename Tuple, size_t...Is>
auto generate_column_statement(const Tuple& tup, index_sequence<Is...>)
{
    ostringstream oss;
    auto l = { (oss << (oss.str().empty() ? "" : ",") << std::get<Is>(tup).column, 0)... };
    (void)l;
    return oss.str();
}

template<typename Tuple, size_t...Is>
auto generate_value_statement(const Tuple& tup, index_sequence<Is...>)
{
    ostringstream oss;
    auto unused = { (oss << (oss.str().empty() ? "" : ",") << "?", std::get<Is>(tup).column, 0)... };
    (void)unused;
    return oss.str();
}

template<typename Column, typename Literal>
struct EqualExpr
{
    EqualExpr() : literal{} {}

    struct is_expr { static constexpr const bool value = true; };

    using param_type = tuple<Literal>;
    const Literal literal;
    static constexpr const uint8_t parameterCount = 1;

    EqualExpr(const Literal& lit) : literal(lit) {}

    const string operator()() const
    {
        ostringstream oss;
        oss << Column::column << "=" << "?";
        return oss.str();
    }
};

template<typename Column, typename Literal>
struct NotEqualExpr
{
    NotEqualExpr() : literal{} {}

    struct is_expr { static constexpr const bool value = true; };

    using param_type = tuple<Literal>;
    const Literal literal;
    static constexpr const uint8_t parameterCount = 1;
    
    NotEqualExpr(const Literal& lit) : literal(lit) {}

    const string operator()() const
    {
        ostringstream oss;
        oss << Column::column << "!=" << "?";
        return oss.str();
    }
};

template <typename SeqA, typename SeqB> struct Concat;
template <typename...T1s, typename...T2s>
struct Concat<tuple<T1s...>, tuple<T2s...>>
{
    using type = tuple<T1s..., T2s...>;
};

template<typename T1, typename T2>
struct AndExpr
{
    struct is_expr { static constexpr const bool value = true; };
    const uint32_t parameterCount;
    using param_type = typename Concat<typename T1::param_type, typename T2::param_type>::type;
    using type = AndExpr<T1,T2>;

    const T1 expr1;
    const T2 expr2;

    AndExpr() : parameterCount(0) {}

    AndExpr(const T1& a, const T2& b)
        : parameterCount(a.parameterCount + b.parameterCount) {}

    const string operator()() const
    {
        string e1 = expr1();
        string e2 = expr2();
        return e1 + " AND "s + e2;
    }
};

template<typename T1, typename T2>
struct OrExpr
{
    struct is_expr { static constexpr const bool value = true; };
    const uint32_t parameterCount;
    using param_type = typename Concat<typename T1::param_type, typename T2::param_type>::type;
    using type = OrExpr<T1, T2>;

    const T1 expr1;
    const T2 expr2;

    OrExpr() : parameterCount(0) {}

    OrExpr(const T1& a, const T2& b)
        : parameterCount(a.parameterCount + b.parameterCount) {}

    const string operator()() const
    {
        string e1 = expr1();
        string e2 = expr2();
        return e1 + " OR "s + e2;
    }
};

template<typename Column, typename Literal>
EqualExpr<Column, Literal> operator==(const Column& column, const Literal& lit)
{
    EqualExpr<Column, Literal> expr(lit);
    return expr;
}

template<typename Column, typename Literal>
NotEqualExpr<Column, Literal> operator!=(const Column& column, const Literal& lit)
{
    NotEqualExpr<Column, Literal> expr(lit);
    return expr;
}

// Equal x Equal
template<typename...T1, typename...T2>
auto operator||(const EqualExpr<T1...>& expr1, const EqualExpr<T2...>& expr2)
{
    return OrExpr<EqualExpr<T1...>, EqualExpr<T2...>>(expr1, expr2);
}
// Equal x Or
template<typename...T1, typename...T2>
auto operator||(const EqualExpr<T1...>& expr1, const OrExpr<T2...>& expr2)
{
    return OrExpr<EqualExpr<T1...>, OrExpr<T2...>>(expr1, expr2);
}
// Equal x And
template<typename...T1, typename...T2>
auto operator||(const EqualExpr<T1...>& expr1, const AndExpr<T2...>& expr2)
{
    return OrExpr<EqualExpr<T1...>, AndExpr<T2...>>(expr1, expr2);
}

// Or x Or
template<typename...T1, typename...T2>
auto operator||(const OrExpr<T1...>& expr1, const OrExpr<T2...>& expr2)
{
    return OrExpr<OrExpr<T1...>, OrExpr<T2...>>(expr1, expr2);
}

// Or x Equal
template<typename...T1, typename...T2>
auto operator||(const OrExpr<T1...>& expr1, const EqualExpr<T2...>& expr2)
{
    return OrExpr<OrExpr<T1...>, EqualExpr<T2...>>(expr1, expr2);
}

// Or x And
template<typename...T1, typename...T2>
auto operator||(const OrExpr<T1...>& expr1, const AndExpr<T2...>& expr2)
{
    return OrExpr<OrExpr<T1...>, AndExpr<T2...>>(expr1, expr2);
}

// And x And
template<typename...T1, typename...T2>
auto operator||(const AndExpr<T1...>& expr1, const AndExpr<T2...>& expr2)
{
    return OrExpr<AndExpr<T1...>, AndExpr<T2...>>(expr1, expr2);
}

// And x Or
template<typename...T1, typename...T2>
auto operator||(const AndExpr<T1...>& expr1, const OrExpr<T2...>& expr2)
{
    return AndExpr<AndExpr<T1...>, OrExpr<T2...>>(expr1, expr2);
}

// And x Equal
template<typename...T1, typename...T2>
auto operator||(const AndExpr<T1...>& expr1, const EqualExpr<T2...>& expr2)
{
    return OrExpr<AndExpr<T1...>, EqualExpr<T2...>>(expr1, expr2);
}

// Equal x Equal
template<typename...T1, typename...T2>
auto operator&&(const EqualExpr<T1...>& expr1, const EqualExpr<T2...>& expr2)
{
    return AndExpr<EqualExpr<T1...>, EqualExpr<T2...>>(expr1, expr2);
}

// Equal x Or
template<typename...T1, typename...T2>
auto operator&&(const EqualExpr<T1...>& expr1, const OrExpr<T2...>& expr2)
{
    return AndExpr<EqualExpr<T1...>, OrExpr<T2...>>(expr1, expr2);
}

// Equal x And
template<typename...T1, typename...T2>
auto operator&&(const EqualExpr<T1...>& expr1, const AndExpr<T2...>& expr2)
{
    return AndExpr<EqualExpr<T1...>, AndExpr<T2...>>(expr1, expr2);
}

// And x Or
template<typename...T1, typename...T2>
auto operator&&(const AndExpr<T1...>& expr1, const OrExpr<T2...>& expr2)
{
    return AndExpr<AndExpr<T1...>, OrExpr<T2...>>(expr1, expr2);
}

// And x And
template<typename...T1, typename...T2>
auto operator&&(const AndExpr<T1...>& expr1, const AndExpr<T2...>& expr2)
{
    return AndExpr<AndExpr<T1...>, AndExpr<T2...>>(expr1, expr2);
}

// And x Equal
template<typename...T1, typename...T2>
auto operator&&(const AndExpr<T1...>& expr1, const EqualExpr<T2...>& expr2)
{
    return AndExpr<AndExpr<T1...>, EqualExpr<T2...>>(expr1, expr2);
}

template<typename TSelectExpr, typename TCondExpr>
struct QualifiedSelectExpr {
    struct is_expr { static constexpr const bool value = true; };
    TSelectExpr expr;
    TCondExpr cond;
    using param_type = typename TCondExpr::param_type;
    param_type params;

    const uint32_t parameterCount;
    QualifiedSelectExpr(const TSelectExpr& exprArg, const TCondExpr& condArg)
        : expr(exprArg),
        cond(condArg),
        parameterCount(condArg.parameterCount)
        {}

    const string operator()() const
    {
        ostringstream query;
        query << expr() << " WHERE " << cond() << ";" << endl;
        return query.str();
    }
};

template<typename ColNameExpr, typename FromExpr, typename AliasExpr>
struct SelectExpr {
    struct is_expr { static constexpr const bool value = true; };
    using type = SelectExpr;
    using column_types = typename ColNameExpr::column_types;
    column_types columns;
    string from;

    static constexpr const uint32_t parameterCount = 0;
    SelectExpr(const column_types& colsArg, const string& fromArg)
        : columns(colsArg), from(fromArg)
    {
    }

    const string operator()() const
    {
        static constexpr int tuple_param_count = tuple_size<typename ColNameExpr::column_types>::value;
        ostringstream query;
        query << "SELECT "
            << generate_column_statement(columns, make_index_sequence<tuple_param_count>{})
            << " FROM " << FromExpr::table_name << " AS " << AliasExpr::table_name;

        return query.str();
    }

    template<typename T>
    const QualifiedSelectExpr<type, T> where(const T& cond)
    {
        return QualifiedSelectExpr<type, T>(*this, cond);
    }
};

template<typename ColNameExpr, typename ToExpr>
struct InsertExpr {
    struct is_expr { static constexpr const bool value = true; };
    using type = InsertExpr;
    using column_types = typename ColNameExpr::column_types;
    column_types columns;
    string from;

    static constexpr const uint32_t parameterCount = 0;
    InsertExpr(const column_types& colsArg, const string& fromArg)
        : columns(colsArg), from(fromArg)
    {
    }

    const string operator()() const
    {
        static constexpr int tuple_param_count = tuple_size<typename ColNameExpr::column_types>::value;
        ostringstream query;
        query << "INSERT INTO " << ToExpr::table_name << " ("
            << generate_column_statement(columns, make_index_sequence<tuple_param_count>{})
            << ") VALUES (" << generate_value_statement(columns, make_index_sequence<tuple_param_count>{}) << ");";

        return query.str();
    }
};

template<typename FromExpr, typename AliasExpr = FromExpr, typename...Columns>
SelectExpr<ColNameExpr<Columns...>, FromExpr, AliasExpr> select(const Columns&... col)
{
    return SelectExpr<ColNameExpr<Columns...>, FromExpr, AliasExpr>(make_tuple(col...), FromExpr::table_name);
}

template<typename ToExpr, typename...Columns>
InsertExpr<ColNameExpr<Columns...>, ToExpr> insert(const Columns&... col)
{
    auto columns = make_tuple(col...);
    InsertExpr<ColNameExpr<Columns...>, ToExpr> insertExpr(columns, ToExpr::table_name);
    return insertExpr;
}

template<typename T, typename TKey = typename T::primary_key, typename Tuple = typename T::type, size_t...Is>
Tuple pack_tuple(PGresult* result, const uint32_t numRow, const uint32_t ncolumns, index_sequence<Is...>)
{
    Tuple value;
    auto unused = {0, ((get<Is>(value) = PQgetvalue(result, numRow, Is), 0))... };
    (void)unused;
    return value;
}

template<typename TConnFactory>
class Postgres
{
public:
    template<typename T, typename...Ts>
    static auto execute(const string& statement, const vector<string>& params, const tuple<Ts...>& types, const T&)
    {
        list<T> results;
        PGconn* conn = TConnFactory::get();
        auto statementHandle = prepareStatement(conn, statement, types);

        auto vals = vector<const char*>(params.size(), nullptr);
        for (auto k = 0; k < params.size(); ++k) {
            vals[k] = params[k].c_str();
        }

        auto result = PQexecPrepared(
            conn,
            statementHandle.c_str(),
            static_cast<uint32_t>(params.size()),
            vals.empty() ? nullptr : vals.data(),
            nullptr,
            nullptr,
            0);

        auto status = PQresultStatus(result);
        if (status != PGRES_TUPLES_OK) {
            if (status == PGRES_COMMAND_OK) {
                PQclear(result);
                return results;
            }
            auto errmsg = PQerrorMessage(conn);
            cerr << errmsg << endl;
            PQclear(result);
            throw runtime_error("Error executing command"s + errmsg);
        }

        auto rowcount = PQntuples(result);
        auto colcount = PQnfields(result);

        for (auto row = 0; row < rowcount; ++row) {
            using tuple_type = typename T::type;
            static constexpr int tuple_param_count = tuple_size<tuple_type>::value;
            auto elem = pack_tuple<T>(result, row, colcount, make_index_sequence<tuple_param_count>{});

            results.push_back(elem);
        }

        PQclear(result);

        return results;
    }

    template<typename T, typename...Ts>
    static auto select(const string& statement, const vector<string>& params, const tuple<Ts...>& types, const T&)
    {
        return execute(statement, params, types, T{});
    }


    template<typename T, typename...Ts>
    static auto insert(const string& statement, const vector<string>& params, const tuple<Ts...>& types, const T&)
    {
        execute(statement, params, types, T{});
    }


private:
    template<typename...ParamTypes>
    static auto prepareStatement(PGconn* conn, const string& statement, const tuple<ParamTypes...>& types)
    {
        ostringstream statementName;
        statementName << statement;

        auto result = PQdescribePrepared(conn, statementName.str().c_str());
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            ostringstream alteredStatement;
            uint32_t i = 1;
            for (auto& c : statement) {
                if (c == '?') {
                    alteredStatement << '$' << i++;
                }
                else {
                    alteredStatement << c;
                }
            }

            auto result = PQprepare(conn, statementName.str().c_str(),
                alteredStatement.str().c_str(), tuple_size<tuple<ParamTypes...>>::value, nullptr);
            auto status = PQresultStatus(result);
            if (status != PGRES_COMMAND_OK) {
                PQclear(result);
                auto errmsg = PQerrorMessage(conn);
                cerr << errmsg << endl;
                throw runtime_error("Error establishing a database connection"s + errmsg);
            }
        }
        PQclear(result);
        return statementName.str();
    }
};

template<typename T, typename TKey = typename T::primary_key, typename Tuple = typename T::type, size_t...Is>
auto unpack_tuple_and_select(const TKey& key, index_sequence<Is...>)
{
    Tuple tmp;
    return select<T>(std::get<Is>(tmp)...).where(TKey{} == key.value);
}

template<typename T, typename TKey = typename T::primary_key, typename Tuple = typename T::type, size_t...Is>
auto unpack_tuple_and_insert(const Tuple& tup, index_sequence<Is...>)
{
    return insert<T>(std::get<Is>(tup)...);
}

template<typename T, typename TKey = typename T::primary_key, typename Tuple = typename T::type, size_t...Is>
vector<string> unpack_tuple_values_to_string_vec(const Tuple& tup, index_sequence<Is...>)
{
    ostringstream oss;
    vector<string> result;

    auto unused = { 0, ((oss << std::get<Is>(tup).value, result.emplace_back(oss.str()), oss = ostringstream(), 0))... };
    (void)unused;
    return result;
}

template<typename T, typename DbDriver, typename TKey = typename T::primary_key>
class Repository {
public:
    static constexpr int tuple_param_count = tuple_size<typename T::type>::value;

    static T get(const TKey& key)
    {
        auto val = unpack_tuple_and_select<T>(key, make_index_sequence<tuple_param_count>{});

        ostringstream keyStr;
        keyStr << key.value;
        auto result = DbDriver::select(val(), { keyStr.str() }, T::type{}, T{});
        if (!result.empty()) {
            return result.front();
        } else { return T(); }
    };

    static bool add(const T& entity)
    {
        auto query = unpack_tuple_and_insert<T>(entity.value, make_index_sequence<tuple_param_count>{});
        //cout << typeid(x).name() << endl;
        //cout << x() << endl;

        auto params = unpack_tuple_values_to_string_vec<T>(entity.value, make_index_sequence<tuple_param_count>{});

        DbDriver::insert(query(), params, entity.value, T{});
        return false;
    }
};
}
