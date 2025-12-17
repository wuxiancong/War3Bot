/**
 * @file decodekey.cpp
 * @brief CD Key 解码器 C 语言接口封装 (C API Wrapper)
 *
 * 作用:
 * 此文件将 C++ 类 `CDKeyDecoder` 的功能封装为标准的 C 语言函数接口。
 * 它的存在使得 bncsutil 库可以被非 C++ 语言（如 C, Python ctypes, VB 等）调用，
 * 用于处理暴雪游戏（StarCraft, Diablo II, WarCraft III）的 CD Key 解码和哈希计算。
 *
 * 主要功能:
 * 1. 对象池管理: 维护一个全局的解码器指针数组 (`decoders`)，允许同时操作多个 Key。
 * 2. 线程安全: 使用互斥锁 (Mutex/Critical Section) 保护全局对象池，防止多线程竞争。
 * 3. 接口导出: 提供 `kd_create`, `kd_calculateHash`, `kd_free` 等 C 风格函数。
 */
#ifndef DECODECDKEY_CPP
#define DECODECDKEY_CPP

#include <bncsutil/mutil.h>
#include <bncsutil/cdkeydecoder.h>
#include <bncsutil/decodekey.h>

#ifdef MOS_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif
#include <cstdlib>
#include <cstring>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_DECODERS_SIZE 4
#define MUTEX_TIMEOUT_MS 6000L

#ifndef CLOCKS_PER_SEC
#  ifdef CLK_TCK
#    define CLOCKS_PER_SEC CLK_TCK
#  else
#    define CLOCKS_PER_SEC 1000000
#  endif
#endif

// 全局解码器对象池
CDKeyDecoder **decoders;
unsigned int numDecoders = 0;
unsigned int sizeDecoders = 0;

#ifdef MOS_WINDOWS
//	HANDLE mutex;
CRITICAL_SECTION kd_control;
#else
pthread_mutex_t mutex;
#endif

// 锁定解码器池（线程安全）
int kd_lock_decoders() {
#ifdef MOS_WINDOWS
    /*	DWORD dwWaitResult;
    dwWaitResult = WaitForSingleObject(mutex, MUTEX_TIMEOUT_MS);
    switch (dwWaitResult) {
        case WAIT_OBJECT_0:
            // success
            break;
        case WAIT_TIMEOUT:
            return 0;
            break;
        case WAIT_ABANDONED:
            // weird, but should be OK
            break;
        default:
            return 0;
            break;
    }*/
    EnterCriticalSection(&kd_control);
#else
    int err = 0;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    // 注意：这里 wait_time 的设置在某些 Linux 系统上可能需要绝对时间而不是相对时间
    // 但为了保持原逻辑兼容性，暂不修改逻辑结构
    struct timespec wait_time = {0, MUTEX_TIMEOUT_MS * 1000};

    // 这里原代码逻辑有点奇怪，pthread_cond_timedwait 通常需要配合 mutex 使用
    // 且 mutex 应该先 lock。但在 C API 封装层通常只做简单的 try lock
    // 为了编译通过暂保持原样，建议实际使用中检查 pthread 逻辑
    err = pthread_cond_timedwait(&cond, &mutex, &wait_time);
    switch (err) {
    case 0:
        // success
        break;
    default:
        // error
        return 0;
    }
#endif
    return 1;
}

#ifdef MOS_WINDOWS
#define kd_unlock_decoders() LeaveCriticalSection(&kd_control)
#else
#define kd_unlock_decoders() pthread_mutex_unlock(&mutex)
#endif

// 快速一次性解码函数
MEXP(int) kd_quick(const char* cd_key, uint32_t client_token,
         uint32_t server_token, uint32_t* public_value,
         uint32_t *product, char* hash_buffer, size_t buffer_len)
{
    CDKeyDecoder kd(cd_key, std::strlen(cd_key));
    size_t hash_len;

    if (!kd.isKeyValid())
        return 0;

    *public_value = kd.getVal1();
    *product = kd.getProduct();

    hash_len = kd.calculateHash(client_token, server_token);
    if (!hash_len || hash_len > buffer_len)
        return 0;

    kd.getHash(hash_buffer);
    return 1;
}

// 初始化全局资源
MEXP(int) kd_init() {
    static int has_run = 0;

    if (has_run)
        return 1;

#ifdef MOS_WINDOWS
    /*mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL)
        return 0;*/
    InitializeCriticalSection(&kd_control);
#else
    if (pthread_mutex_init(&mutex, NULL))
        return 0;
#endif
    numDecoders = 0;
    sizeDecoders = 0;
    decoders = (CDKeyDecoder**) 0;
    has_run = 1;

#if DEBUG
    bncsutil_debug_message("Initialized key decoding C API.");
#endif

    return 1;
}

unsigned int kd_findAvailable() {
    unsigned int i;
    CDKeyDecoder **d;

    d = decoders;
    for (i = 0; i < sizeDecoders; i++) {
        if (*d == (CDKeyDecoder*) 0)
            return i;
        d++;
    }

    // no room available, must expand
    decoders = (CDKeyDecoder**) realloc(decoders, sizeof(CDKeyDecoder*) *
                                                       (sizeDecoders + DEFAULT_DECODERS_SIZE));
    if (!decoders)
        return (unsigned int) -1;

    memset(decoders + sizeDecoders, 0,
           sizeof(CDKeyDecoder*) * DEFAULT_DECODERS_SIZE); // zero new memory

    i = sizeDecoders;
    sizeDecoders += DEFAULT_DECODERS_SIZE;
    return i;
}

MEXP(int) kd_create(const char *cdkey, int keyLength) {
    unsigned int i;
    CDKeyDecoder **d;
    static int dcs_initialized = 0;

    if (!dcs_initialized) {
        if (!kd_init())
            return -1;
        dcs_initialized = 1;
    }

    if (!kd_lock_decoders()) return -1;

    i = kd_findAvailable();
    if (i == (unsigned int) -1)
        return -1;

    d = (decoders + i);
    *d = new CDKeyDecoder(cdkey, keyLength);
    if (!(**d).isKeyValid()) {
        delete *d;
        *d = (CDKeyDecoder*) 0;
        return -1;
    }

    numDecoders++;

    kd_unlock_decoders();
    return (int) i;
}

MEXP(int) kd_free(int decoder) {
    CDKeyDecoder *d;

    if (!kd_lock_decoders()) return 0;

    if ((unsigned int) decoder >= sizeDecoders)
        return 0;

    d = *(decoders + decoder);
    if (!d)
        return 0;

    delete d;
    *(decoders + decoder) = (CDKeyDecoder*) 0;

    kd_unlock_decoders();
    return 1;
}

MEXP(int) kd_val2Length(int decoder) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->getVal2Length();

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_product(int decoder) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->getProduct();

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_val1(int decoder) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->getVal1();

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_val2(int decoder) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->getVal2();

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_longVal2(int decoder, char *out) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->getLongVal2(out);

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_calculateHash(int decoder, uint32_t clientToken,
                 uint32_t serverToken)
{
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = (int) d->calculateHash(clientToken, serverToken);

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_getHash(int decoder, char *out) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = (int) d->getHash(out);

    kd_unlock_decoders();
    return value;
}

MEXP(int) kd_isValid(int decoder) {
    CDKeyDecoder *d;
    int value;

    if (!kd_lock_decoders()) return -1;

    if ((unsigned int) decoder >= sizeDecoders)
        return -1;

    d = *(decoders + decoder);
    if (!d)
        return -1;

    value = d->isKeyValid();

    kd_unlock_decoders();
    return value;
}

#ifdef __cplusplus
}
#endif

#endif /* dECODECDKEY_CPP */
