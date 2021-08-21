#pragma once
#ifndef GUITARPP_CTEXT_H_
#define GUITARPP_CTEXT_H_

// OLD CTEXT CLASS

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN64
typedef int64_t integer;
#else
typedef int integer;
#endif

class CText {
    class tstring : public std::string {
      public:
        int to_int() { return std::stoi(*this); }

        long to_long() { return std::stol(*this); }

        long long to_longlong() { return std::stol(*this); }

        float to_float() { return std::stof(*this); }

        double to_double() { return std::stod(*this); }

        tstring(const std::string &value) : std::string(value) {}
        tstring(const char *value) : std::string(value) {}

        tstring() : std::string() {}
    };

    std::string push;

  public:
    typedef struct {
        tstring name, content;
    } field_t;

    typedef struct {
        std::string name;
        std::deque<field_t> fields;

        integer getFieldIDByName(const std::string &fieldName) {
            for (size_t i = 0, size = fields.size(); i < size; i++) {
                if (fields[i].name == fieldName) {
                    return i;
                }
            }

            return -1;
        }

        tstring &operator[](const std::string &fieldName) {
            integer fieldID = getFieldIDByName(fieldName);

            if (fieldID == -1) {
                throw std::logic_error("Field not found");
            }

            return fields[fieldID].content;
        }
    } table_t;

  private:
    size_t fileSize{};
    std::fstream file;
    std::deque<table_t> tables;
    std::string fileName;

  public:
    void Parse();

    const bool is_open() { return file.good(); }

    void open(const char *name, bool autoParse = true);
    void save();

    integer getTableIDByName(const std::string &tableName) {
        if (tableName.length() == 0)
            return 0;

        for (size_t i = 0, size = tables.size(); i < size; i++) {
            if (tables[i].name == tableName) {
                return i;
            }
        }

        return -1;
    }

    table_t &operator[](const std::string &tableName) {
        integer tableID = getTableIDByName(tableName);

        if (tableID == -1) {
            throw std::logic_error("Table not found");
        }

        return tables[tableID];
    }

    CText();
    CText(const char *name, bool autoParse = true);
    CText(const char *name, const std::string &pushIfEmpty,
          bool autoParse = true);
};

#endif
