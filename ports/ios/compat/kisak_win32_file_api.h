#pragma once
#include <string.h>

// POSIX-backed implementations of the Win32 file calls the fastfile loader and
// asset registry use directly. HANDLEs wrap file descriptors. ReadFileEx
// completes synchronously, so SleepEx has nothing left to wait for.

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED 5
#endif
#ifndef ERROR_HANDLE_EOF
#define ERROR_HANDLE_EOF 38
#endif

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        PVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef void (*LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED overlapped);

inline DWORD &Kisak_LastError()
{
    static thread_local DWORD lastError = 0;
    return lastError;
}

inline DWORD GetLastError() { return Kisak_LastError(); }
inline void SetLastError(DWORD error) { Kisak_LastError() = error; }

inline HANDLE Kisak_HandleFromFd(int fd) { return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd) + 1); }
inline int Kisak_FdFromHandle(HANDLE handle) { return static_cast<int>(reinterpret_cast<intptr_t>(handle) - 1); }

inline void Kisak_NativePath(char *dst, size_t size, const char *src)
{
    size_t i = 0;
    for (; src[i] && i + 1 < size; ++i)
        dst[i] = src[i] == '\\' ? '/' : src[i];
    dst[i] = 0;
}

// Engine paths are built with Windows separators in many places.
inline FILE *Kisak_fopen(const char *name, const char *mode)
{
    char path[1024];
    Kisak_NativePath(path, sizeof(path), name);
    return fopen(path, mode);
}

inline void Kisak_SetErrnoError()
{
    SetLastError(errno == ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED);
}

inline HANDLE CreateFileA(const char *name, DWORD access, DWORD shareMode, void *security, DWORD disposition, DWORD flags, HANDLE templateFile)
{
    (void)shareMode; (void)security; (void)flags; (void)templateFile;
    const bool read = (access & 0x80000000u) != 0;
    const bool write = (access & 0x40000000u) != 0;
    int openFlags = read && write ? O_RDWR : write ? O_WRONLY : O_RDONLY;
    switch (disposition)
    {
    case 1: openFlags |= O_CREAT | O_EXCL; break;   // CREATE_NEW
    case 2: openFlags |= O_CREAT | O_TRUNC; break;  // CREATE_ALWAYS
    case 3: break;                                   // OPEN_EXISTING
    case 4: openFlags |= O_CREAT; break;            // OPEN_ALWAYS
    case 5: openFlags |= O_TRUNC; break;            // TRUNCATE_EXISTING
    default: break;
    }
    char path[1024];
    Kisak_NativePath(path, sizeof(path), name);
    const int fd = open(path, openFlags | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        if (disposition == 3)
        {
            const int openErrno = errno;
            fprintf(stderr, "CreateFileA: open('%s') failed: %s (errno %d)\n", path, strerror(openErrno), openErrno);
            const int retry = open(path, O_RDONLY | O_CLOEXEC);
            fprintf(stderr, "CreateFileA: retry %s (errno %d)\n", retry >= 0 ? "ok" : "fail", retry >= 0 ? 0 : errno);
            if (retry >= 0)
                close(retry);
            char prefix[1024];
            for (size_t i = 1; path[i - 1]; ++i)
            {
                if (path[i] != '/' && path[i] != 0)
                    continue;
                memcpy(prefix, path, i);
                prefix[i] = 0;
                struct stat li, si;
                const int l = lstat(prefix, &li);
                const int le = l ? errno : 0;
                const int st = stat(prefix, &si);
                const int se = st ? errno : 0;
                fprintf(stderr, "  prefix %s: lstat=%d(%d) link=%d stat=%d(%d)\n", prefix, l, le, l == 0 && S_ISLNK(li.st_mode), st, se);
            }
        }
        Kisak_SetErrnoError();
        return INVALID_HANDLE_VALUE;
    }
    SetLastError(0);
    return Kisak_HandleFromFd(fd);
}

inline BOOL CloseHandle(HANDLE handle)
{
    return close(Kisak_FdFromHandle(handle)) == 0;
}

inline DWORD GetFileSize(HANDLE handle, DWORD *sizeHigh)
{
    struct stat info;
    if (fstat(Kisak_FdFromHandle(handle), &info) != 0)
        return 0xFFFFFFFFu;
    if (sizeHigh)
        *sizeHigh = static_cast<DWORD>(static_cast<uint64_t>(info.st_size) >> 32);
    return static_cast<DWORD>(info.st_size);
}

inline BOOL ReadFile(HANDLE handle, void *buffer, DWORD size, DWORD *bytesRead, LPOVERLAPPED overlapped)
{
    (void)overlapped;
    const ssize_t result = read(Kisak_FdFromHandle(handle), buffer, size);
    if (bytesRead)
        *bytesRead = result > 0 ? static_cast<DWORD>(result) : 0;
    if (result < 0)
    {
        Kisak_SetErrnoError();
        return 0;
    }
    return 1;
}

inline BOOL WriteFile(HANDLE handle, const void *buffer, DWORD size, DWORD *bytesWritten, LPOVERLAPPED overlapped)
{
    (void)overlapped;
    const ssize_t result = write(Kisak_FdFromHandle(handle), buffer, size);
    if (bytesWritten)
        *bytesWritten = result > 0 ? static_cast<DWORD>(result) : 0;
    if (result < 0)
    {
        Kisak_SetErrnoError();
        return 0;
    }
    return 1;
}

inline BOOL ReadFileEx(HANDLE handle, void *buffer, DWORD size, LPOVERLAPPED overlapped, LPOVERLAPPED_COMPLETION_ROUTINE completion)
{
    const off_t offset = static_cast<off_t>((static_cast<uint64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset);
    const ssize_t result = pread(Kisak_FdFromHandle(handle), buffer, size, offset);
    if (result < 0)
    {
        Kisak_SetErrnoError();
        return 0;
    }
    if (result == 0)
    {
        SetLastError(ERROR_HANDLE_EOF);
        return 0;
    }
    if (completion)
        completion(0, static_cast<DWORD>(result), overlapped);
    return 1;
}

inline DWORD SleepEx(DWORD milliseconds, BOOL alertable)
{
    (void)milliseconds; (void)alertable;
    return 0;
}

inline BOOL DeleteFileA(const char *name)
{
    char path[1024];
    Kisak_NativePath(path, sizeof(path), name);
    return unlink(path) == 0;
}
