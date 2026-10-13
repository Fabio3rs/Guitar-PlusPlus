// Adapted from cppapiframework, Copyright (c) 2021 Fabio R. Sluzala.
// Distributed under the MIT license; see LICENSE in this directory.
#include "sqlite3raii.hpp"

#include <limits>
#include <utility>

#if __cplusplus >= 202302L
using std::out_ptr;
#else
template <class T, class Deleter> struct out_ptr {
    T *ptrtmp{};                                               // NOLINT
    std::unique_ptr<T, Deleter> *inptr{};                      // NOLINT
    out_ptr(std::unique_ptr<T, Deleter> &ptr) : inptr(&ptr) {} // NOLINT

    out_ptr(const out_ptr &) = delete;
    out_ptr &operator=(const out_ptr &) = delete;
    out_ptr(out_ptr &&) = delete;
    out_ptr &operator=(out_ptr &&) = delete;

    operator T **() { return &ptrtmp; } // NOLINT

    ~out_ptr() { inptr->reset(ptrtmp); }
};

// Deduction guide to suppress "may not intend to support class template
// argument deduction" warning on some compilers.
template <class T, class Deleter>
out_ptr(std::unique_ptr<T, Deleter> &) -> out_ptr<T, Deleter>;

#endif

namespace {
[[noreturn]] inline void throw_with_errmsg(sqlite3 *db, const char *context,
                                           int rc) {
    const char *errmsg = sqlite3_errmsg(db);
    throw SqliteError(
        rc, std::string(context != nullptr ? context : "SQLite") + ": " +
                (errmsg != nullptr ? errmsg : "unknown sqlite error"));
}
} // namespace

// --- SqliteDb ---------------------------------------------------------------

SqliteDb::SqliteDb(std::string_view path) {
    sqlite3 *raw_db = nullptr;
    const int rc = sqlite3_open(std::string{path}.c_str(), &raw_db);
    db_.reset(raw_db);
    if (rc != SQLITE_OK) {
        throw_with_errmsg(raw_db, "sqlite3_open", rc);
    }
}

void SqliteDb::exec(std::string_view sql) const {
    sqlite3_errmsg_ptr errmsg;
    const int rc = sqlite3_exec(db_.get(), std::string{sql}.c_str(), nullptr,
                                nullptr, out_ptr(errmsg));
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg.get() : "unknown sqlite error";
        throw SqliteError(rc, msg);
    }
}

SqliteStatement SqliteDb::prepare(std::string_view sql,
                                  const char *context) const {
    sqlite3_stmt_ptr stmt;
    const int rc = sqlite3_prepare_v2(db_.get(), std::string{sql}.c_str(), -1,
                                      out_ptr(stmt), nullptr);
    check_sqlite_rc(db_, rc, context);
    return SqliteStatement{db_.get(), std::move(stmt), context};
}

// --- SqliteStatement --------------------------------------------------------

SqliteStatement::SqliteStatement(sqlite3 *db, sqlite3_stmt_ptr stmt,
                                 const char *ctx)
    : db_(db), stmt_(std::move(stmt)), context_((ctx != nullptr) ? ctx : "") {}

SqliteStatement &SqliteStatement::bind(int idx, int value) {
    check_sqlite_rc(db_, sqlite3_bind_int(stmt_.get(), idx, value),
                    context_.c_str());
    return *this;
}

SqliteStatement &SqliteStatement::bind(int idx, const std::string &value) {
    return bind(idx, std::string_view{value});
}

SqliteStatement &SqliteStatement::bind(int idx, const char *value) {
    return bind(idx, std::string_view{(value != nullptr) ? value : ""});
}

SqliteStatement &SqliteStatement::bind(int idx, int64_t value) {
    check_sqlite_rc(db_, sqlite3_bind_int64(stmt_.get(), idx, value),
                    context_.c_str());
    return *this;
}

SqliteStatement &SqliteStatement::bind(int idx, double value) {
    check_sqlite_rc(db_, sqlite3_bind_double(stmt_.get(), idx, value),
                    context_.c_str());
    return *this;
}

SqliteStatement &SqliteStatement::bind(int idx, std::string_view value) {
    if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw SqliteError(SQLITE_TOOBIG, "SQLite text parameter too large");
    }
    check_sqlite_rc(
        db_,
        sqlite3_bind_text(stmt_.get(), idx, value.empty() ? "" : value.data(),
                          static_cast<int>(value.size()), SQLITE_TRANSIENT),
        context_.c_str());
    return *this;
}

SqliteStatement &
SqliteStatement::bind(int idx, const std::optional<std::string_view> &value) {
    if (value.has_value()) {
        return bind(idx, *value);
    }
    check_sqlite_rc(db_, sqlite3_bind_null(stmt_.get(), idx), context_.c_str());
    return *this;
}

int SqliteStatement::step() {
    const int rc = sqlite3_step(stmt_.get());
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        check_sqlite_rc(db_, rc, context_.c_str());
    }
    return rc;
}

void SqliteStatement::reset() {
    check_sqlite_rc(db_, sqlite3_reset(stmt_.get()), context_.c_str());
}

void SqliteStatement::clear() {
    check_sqlite_rc(db_, sqlite3_clear_bindings(stmt_.get()), context_.c_str());
}

// --- SqliteTxn --------------------------------------------------------------

SqliteTxn::SqliteTxn(const SqliteDb &db) : db_(&db) { db_->exec("BEGIN;"); }

SqliteTxn::~SqliteTxn() {
    if (!committed_ && db_ != nullptr) {
        try {
            db_->exec("ROLLBACK;");
        } catch (...) { // NOLINT
            // swallow in destructor
        }
    }
}

void SqliteTxn::commit() {
    if (db_ == nullptr) {
        throw std::runtime_error(
            "SqliteTxn: commit() called on moved-from object");
    }

    db_->exec("COMMIT;");
    committed_ = true;
}

// --- Legacy helpers ---------------------------------------------------------

void exec_or_throw(sqlite3 *db, const char *sql) {
    sqlite3_errmsg_ptr errmsg;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, out_ptr(errmsg));
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg.get() : "unknown sqlite error";
        throw SqliteError(rc, msg);
    }
}

void exec_or_throw(const sqlite3_db_ptr &db, const char *sql) {
    exec_or_throw(db.get(), sql);
}

void check_sqlite_rc(sqlite3 *db, int rc, const char *context) {
    if (rc == SQLITE_OK || rc == SQLITE_ROW || rc == SQLITE_DONE) {
        return;
    }

    throw_with_errmsg(db, context, rc);
}

void check_sqlite_rc(const sqlite3_db_ptr &db, int rc, const char *context) {
    check_sqlite_rc(db.get(), rc, context);
}

sqlite3_stmt_ptr prepare_or_throw(sqlite3 *db, const char *sql,
                                  const char *context) {
    sqlite3_stmt_ptr stmt;
    const int rc = sqlite3_prepare_v2(db, sql, -1, out_ptr(stmt), nullptr);
    check_sqlite_rc(db, rc, context);
    return stmt;
}

sqlite3_stmt_ptr prepare_or_throw(const sqlite3_db_ptr &db, const char *sql,
                                  const char *context) {
    return prepare_or_throw(db.get(), sql, context);
}
