/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WRITER_H
#define _WRITER_H

#include "asprof.h"
#include "log.h"


class Writer {
  protected:
    int _err;

    Writer() : _err(0) {
    }

  public:
    Writer& operator<<(char c);
    Writer& operator<<(const char* s);
    Writer& operator<<(int n);
    Writer& operator<<(long n);

    bool good() const {
        return _err == 0;
    }

    virtual void write(const char* data, size_t len) = 0;
};

class FileWriter : public Writer {
  private:
    int _fd;
    char* _buf;
    size_t _size;

    enum { BUF_SIZE = 8192 };

    void flush(const char* data, size_t len);

  public:
    FileWriter(const char* file_name);
    FileWriter(int fd);
    ~FileWriter();

    bool is_open() const {
        return _fd >= 0;
    }

    virtual void write(const char* data, size_t len);
};

class LogWriter : public Writer {
    LogLevel _logLevel;

  public:
    LogWriter(LogLevel logLevel = LOG_INFO) : _logLevel(logLevel) {
    }

    virtual void write(const char* data, size_t len);
};

class BufferWriter : public Writer {
  private:
    char* _buf;
    size_t _size;
    size_t _capacity;

  public:
    BufferWriter(size_t capacity = 256);
    ~BufferWriter();

    char* buf() const {
        return _buf;
    }

    size_t size() const {
        return _size;
    }

    virtual void write(const char* data, size_t len);
};

class CallbackWriter : public Writer {
  private:
    asprof_writer_t _output_callback;

  public:
    CallbackWriter(asprof_writer_t output_callback) : _output_callback(output_callback) {
    }

    virtual void write(const char* data, size_t len);
};

#endif // _WRITER_H
