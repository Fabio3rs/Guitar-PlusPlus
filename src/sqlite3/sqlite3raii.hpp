// Adapted from cppapiframework, Copyright (c) 2021 Fabio R. Sluzala.
// Distributed under the MIT license; see LICENSE in this directory.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>

struct SQLiteFinalizer {
    void operator()(sqlite3_stmt *ptr) const noexcept { sqlite3_finalize(ptr); }
};

struct SQLiteClose {
    void operator()(sqlite3 *ptr) const noexcept { sqlite3_close_v2(ptr); }
};

struct SQLiteFree {
    void operator()(void *ptr) const noexcept { sqlite3_free(ptr); }
};

using sqlite3_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteFinalizer>;
using sqlite3_db_ptr = std::unique_ptr<sqlite3, SQLiteClose>;
using sqlite3_errmsg_ptr = std::unique_ptr<char, SQLiteFree>;

class SqliteError : public std::runtime_error {
  public:
    int code;
    SqliteError(int value, const std::string &message)
        : std::runtime_error(message), code(value) {}
};

class SqliteDb;
class SqliteStatement;
class SqliteTxn;

// Lightweight RAII wrapper around sqlite3* and sqlite3_stmt*
class SqliteDb {
  public:
    explicit SqliteDb(std::string_view path);
    [[nodiscard]] sqlite3 *get() const noexcept { return db_.get(); }
    [[nodiscard]] sqlite3_db_ptr release() noexcept { return std::move(db_); }

    [[nodiscard]] auto lastInsertRowId() const -> int64_t {
        return sqlite3_last_insert_rowid(db_.get());
    }

    void exec(std::string_view sql) const;
    SqliteStatement prepare(std::string_view sql, const char *context) const;

  private:
    sqlite3_db_ptr db_;
};

class SqliteStatement {
  public:
    SqliteStatement(sqlite3 *db, sqlite3_stmt_ptr stmt, const char *ctx);

    SqliteStatement &bind(int idx, int value);
    SqliteStatement &bind(int idx, int64_t value);
    SqliteStatement &bind(int idx, double value);
    SqliteStatement &bind(int idx, const std::string &value);
    SqliteStatement &bind(int idx, const char *value);
    SqliteStatement &bind(int idx, std::string_view value);
    SqliteStatement &bind(int idx,
                          const std::optional<std::string_view> &value);

    [[nodiscard]] int step();
    void reset();
    void clear();

    [[nodiscard]] sqlite3_stmt *get() const noexcept { return stmt_.get(); }

  private:
    sqlite3 *db_;
    sqlite3_stmt_ptr stmt_;
    std::string context_;
};

class SqliteTxn {
  public:
    explicit SqliteTxn(const SqliteDb &db);
    ~SqliteTxn();
    SqliteTxn(const SqliteTxn &) = delete;
    SqliteTxn &operator=(const SqliteTxn &) = delete;
    SqliteTxn(SqliteTxn &&) = delete;
    SqliteTxn &operator=(SqliteTxn &&) = delete;

    void commit();

  private:
    const SqliteDb *db_;
    bool committed_ = false;
};

// Backwards-compatible helpers
void exec_or_throw(sqlite3 *db, const char *sql);
void exec_or_throw(const sqlite3_db_ptr &db, const char *sql);

void check_sqlite_rc(sqlite3 *db, int rc, const char *context);
void check_sqlite_rc(const sqlite3_db_ptr &db, int rc, const char *context);

sqlite3_stmt_ptr prepare_or_throw(sqlite3 *db, const char *sql,
                                  const char *context);
sqlite3_stmt_ptr prepare_or_throw(const sqlite3_db_ptr &db, const char *sql,
                                  const char *context);
