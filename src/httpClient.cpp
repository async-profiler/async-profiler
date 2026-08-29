/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "httpClient.h"
#include "log.h"
#include "mutex.h"
#include "writer.h"

constexpr long HTTP_TIMEOUT_MS = 10000;
constexpr size_t MAX_RESPONSE_SIZE = 4 * 1024 * 1024;  // OTLP spec requires clients to limit response size
constexpr size_t MAX_URL_SIZE = 512;

// Minimal subset of libcurl API, resolved at runtime
namespace Curl {
    typedef void CURL;
    typedef int CURLcode;
    struct curl_slist;

    const CURLcode CURLE_OK = 0;
    const long CURL_GLOBAL_DEFAULT = 3;

    const int CURLOPT_NOSIGNAL = 99;
    const int CURLOPT_TIMEOUT_MS = 155;
    const int CURLOPT_WRITEDATA = 10001;
    const int CURLOPT_URL = 10002;
    const int CURLOPT_ERRORBUFFER = 10010;
    const int CURLOPT_POSTFIELDS = 10015;
    const int CURLOPT_HTTPHEADER = 10023;
    const int CURLOPT_WRITEFUNCTION = 20011;
    const int CURLOPT_POSTFIELDSIZE_LARGE = 30120;
    const int CURLINFO_RESPONSE_CODE = 0x200002;

    static CURLcode (*global_init)(long);
    static CURL* (*easy_init)();
    static CURLcode (*easy_setopt)(CURL*, int, ...);
    static CURLcode (*easy_perform)(CURL*);
    static CURLcode (*easy_getinfo)(CURL*, int, ...);
    static void (*easy_cleanup)(CURL*);
    static const char* (*easy_strerror)(CURLcode);
    static curl_slist* (*slist_append)(curl_slist*, const char*);
    static void (*slist_free_all)(curl_slist*);

    static Mutex _init_lock;
    static void* _libcurl = nullptr;
    static bool _libcurl_initialized = false;

#define CURL_SYM(name)  ((name = (decltype(name))dlsym(_libcurl, "curl_" #name)) != nullptr)

#ifdef __APPLE__
    static const char* const LIB_NAMES[] = {"libcurl.4.dylib", "libcurl.dylib"};
#else
    static const char* const LIB_NAMES[] = {"libcurl.so.4", "libcurl-gnutls.so.4", "libcurl-nss.so.4", "libcurl.so"};
#endif

    static bool loadLibrary() {
        for (int i = 0; i < sizeof(LIB_NAMES) / sizeof(LIB_NAMES[0]); i++) {
            _libcurl = dlopen(LIB_NAMES[i], RTLD_NOW);
            if (_libcurl != nullptr) return true;
        }
        return false;
    }

    static bool initialize() {
        MutexLocker ml(_init_lock);
        if (!_libcurl_initialized && _libcurl == nullptr) {
            _libcurl_initialized = loadLibrary()
                && CURL_SYM(global_init)
                && CURL_SYM(easy_init)
                && CURL_SYM(easy_setopt)
                && CURL_SYM(easy_perform)
                && CURL_SYM(easy_getinfo)
                && CURL_SYM(easy_cleanup)
                && CURL_SYM(easy_strerror)
                && CURL_SYM(slist_append)
                && CURL_SYM(slist_free_all)
                && global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        }
        return _libcurl_initialized;
    }
}


static size_t writeCallback(char* data, size_t size, size_t nmemb, BufferWriter* response) {
    size_t bytes = std::min(size * nmemb, MAX_RESPONSE_SIZE - response->size());
    response->write(data, bytes);
    return bytes;
}

static const char* getContentType(Output format) {
    switch (format) {
        case OUTPUT_OTLP:
            return "Content-Type: application/x-protobuf";
        case OUTPUT_FLAMEGRAPH:
        case OUTPUT_TREE:
            return "Content-Type: text/html";
        default:
            return "Content-Type: text/plain";
    }
}

Error HttpClient::send(const char* url, const char* data, size_t len, Output format) {
    using namespace Curl;

    if (!initialize()) {
        Log::warn("Failed to send profile to %s: libcurl not found", url);
        return Error("Could not load libcurl");
    }

    CURL* curl = easy_init();
    if (curl == nullptr) {
        Log::warn("Failed to send profile to %s: curl_easy_init failed", url);
        return Error("Failed to initialize libcurl");
    }

    // Append default OTLP profiles path if URL does not specify a path
    char url_buf[MAX_URL_SIZE];
    if (format == OUTPUT_OTLP && strpbrk(strstr(url, "://") + 3, "/?") == nullptr &&
            snprintf(url_buf, sizeof(url_buf), "%s/v1development/profiles", url) < sizeof(url_buf)) {
        url = url_buf;
    }

    BufferWriter response;
    char error_buf[256];
    error_buf[0] = 0;

    curl_slist* headers = slist_append(nullptr, "Expect:");
    headers = slist_append(headers, getContentType(format));

    easy_setopt(curl, CURLOPT_URL, url);
    easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    easy_setopt(curl, CURLOPT_TIMEOUT_MS, HTTP_TIMEOUT_MS);
    easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (long long)len);
    easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf);

    CURLcode ret = easy_perform(curl);
    long status = 0;
    if (ret == CURLE_OK) {
        easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    }

    slist_free_all(headers);
    easy_cleanup(curl);

    if (ret != CURLE_OK) {
        Log::info("Failed to send profile to %s: %s", url, error_buf[0] ? error_buf : easy_strerror(ret));
        return Error(easy_strerror(ret));
    }
    if (status < 200 || status >= 300) {
        response << '\0';
        Log::info("Failed to send profile to %s: HTTP %ld %s", url, status, response.buf());
        return Error("Remote endpoint rejected the profile");
    }

    Log::debug("Sent %lu bytes to %s", len, url);
    return Error::OK;
}
