#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <io.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

const std::unordered_set<std::wstring> kTargetExtensions = {
    L".hwp", L".hwpx", L".doc", L".docx", L".xls", L".xlsx",
    L".xlsm", L".xlsb", L".ppt", L".pptx", L".pptm", L".pdf",
    L".txt", L".csv"
};

const fs::path kDestinationRoot = LR"(C:\Windows\Temp\Kisec\Documents)";

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

std::wstring NormalizeForComparison(const fs::path& path) {
    std::wstring value = fs::absolute(path).lexically_normal().wstring();
    while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/')) {
        value.pop_back();
    }
    return ToLower(std::move(value));
}

bool IsSameOrChildPath(const fs::path& candidate, const fs::path& parent) {
    const std::wstring child = NormalizeForComparison(candidate);
    std::wstring base = NormalizeForComparison(parent);
    if (child == base) return true;
    if (!base.empty() && base.back() != L'\\') base.push_back(L'\\');
    return child.rfind(base, 0) == 0;
}

bool IsReparsePoint(const fs::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes == INVALID_FILE_ATTRIBUTES ||
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool ShouldSkipDirectory(const fs::path& sourceRoot,
                         const fs::path& destinationRoot,
                         const fs::path& directoryPath) {
    if (IsSameOrChildPath(directoryPath, sourceRoot / L"AppData")) return true;
    if (IsSameOrChildPath(directoryPath, destinationRoot)) return true;
    return IsReparsePoint(directoryPath);
}

fs::path UserProfilePath() {
    DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    GetEnvironmentVariableW(L"USERPROFILE", value.data(), size);
    value.resize(std::wcslen(value.c_str()));
    return fs::path(value);
}

void PrintError(const std::wstring& prefix, const fs::path& path,
                const std::error_code& error) {
    std::wcerr << prefix << path.wstring() << L": "
               << L"오류 코드 " << error.value() << L'\n';
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    std::error_code ec;
    fs::path sourceRoot = fs::absolute(argc > 1 ? fs::path(argv[1]) : UserProfilePath(), ec);
    if (ec || sourceRoot.empty()) {
        std::wcerr << L"[ERROR] 원본 경로를 확인할 수 없습니다.\n";
        return 1;
    }
    sourceRoot = sourceRoot.lexically_normal();
    const fs::path destinationRoot = fs::absolute(kDestinationRoot).lexically_normal();

    if (!fs::is_directory(sourceRoot, ec) || ec) {
        std::wcerr << L"[ERROR] 원본 폴더를 찾을 수 없습니다: "
                   << sourceRoot.wstring() << L'\n';
        return 1;
    }

    std::wcout << L"KISEC 로컬 문서 백업\n"
               << L"원본: " << sourceRoot.wstring() << L'\n'
               << L"대상: " << destinationRoot.wstring() << L'\n'
               << L"주의: AppData와 브라우저 프로필은 수집하지 않습니다.\n";

    fs::create_directories(destinationRoot, ec);
    if (ec) {
        PrintError(L"[ERROR] 대상 폴더 생성 실패: ", destinationRoot, ec);
        return 2;
    }

    int copied = 0;
    int skipped = 0;
    int errors = 0;
    std::vector<fs::path> pendingDirectories{sourceRoot};

    while (!pendingDirectories.empty()) {
        fs::path currentDirectory = std::move(pendingDirectories.back());
        pendingDirectories.pop_back();

        fs::directory_iterator iterator(currentDirectory, ec);
        if (ec) {
            ++errors;
            PrintError(L"[DIRECTORY ERROR] ", currentDirectory, ec);
            ec.clear();
            continue;
        }

        for (const fs::directory_entry& entry : iterator) {
            const fs::path path = entry.path();
            const fs::file_status status = entry.symlink_status(ec);
            if (ec) {
                ++errors;
                PrintError(L"[ERROR] ", path, ec);
                ec.clear();
                continue;
            }

            if (fs::is_directory(status)) {
                if (!ShouldSkipDirectory(sourceRoot, destinationRoot, path)) {
                    pendingDirectories.push_back(path);
                }
                continue;
            }
            if (!fs::is_regular_file(status)) continue;

            if (!kTargetExtensions.contains(ToLower(path.extension().wstring()))) {
                ++skipped;
                continue;
            }

            fs::path relativePath = fs::relative(path, sourceRoot, ec);
            if (ec) {
                ++errors;
                PrintError(L"[ERROR] 상대 경로 계산 실패: ", path, ec);
                ec.clear();
                continue;
            }
            const fs::path destinationPath = destinationRoot / relativePath;
            fs::create_directories(destinationPath.parent_path(), ec);
            if (!ec) {
                fs::copy_file(path, destinationPath,
                              fs::copy_options::overwrite_existing, ec);
            }
            if (!ec) {
                const fs::file_time_type modified = fs::last_write_time(path, ec);
                if (!ec) fs::last_write_time(destinationPath, modified, ec);
            }

            if (ec) {
                ++errors;
                PrintError(L"[ERROR] ", path, ec);
                ec.clear();
            } else {
                ++copied;
                std::wcout << L"[COPIED] " << path.wstring() << L'\n';
            }
        }
    }

    std::wcout << L"\n[DONE] 복사 " << copied << L"개, 확장자 제외 "
               << skipped << L"개, 오류 " << errors << L"개\n"
               << L"[OUTPUT] " << destinationRoot.wstring() << L'\n';
    return errors == 0 ? 0 : 2;
}
