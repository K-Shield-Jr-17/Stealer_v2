#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kReportPath[] = LR"(C:\Windows\Temp\Kisec\network_info.txt)";
constexpr char kBestInterfaceDestination[] = "192.168.50.10";
constexpr int kMaxNodes = 256;

std::string Utf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

std::string UtcTimestamp() {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << time.wYear << '-'
        << std::setw(2) << time.wMonth << '-' << std::setw(2) << time.wDay << 'T'
        << std::setw(2) << time.wHour << ':' << std::setw(2) << time.wMinute << ':'
        << std::setw(2) << time.wSecond << '.' << std::setw(3) << time.wMilliseconds
        << "0000+00:00";
    return out.str();
}

std::string StatusName(IF_OPER_STATUS status) {
    switch (status) {
        case IfOperStatusUp: return "Up";
        case IfOperStatusDown: return "Down";
        case IfOperStatusTesting: return "Testing";
        case IfOperStatusUnknown: return "Unknown";
        case IfOperStatusDormant: return "Dormant";
        case IfOperStatusNotPresent: return "NotPresent";
        case IfOperStatusLowerLayerDown: return "LowerLayerDown";
        default: return "Other(" + std::to_string(static_cast<int>(status)) + ")";
    }
}

std::string SocketAddressText(const SOCKET_ADDRESS& address) {
    if (address.lpSockaddr == nullptr || address.iSockaddrLength < 2) return {};
    std::array<wchar_t, INET6_ADDRSTRLEN + 16> buffer{};
    const void* source = nullptr;

    if (address.lpSockaddr->sa_family == AF_INET &&
        address.iSockaddrLength >= static_cast<int>(sizeof(sockaddr_in))) {
        source = &reinterpret_cast<const sockaddr_in*>(address.lpSockaddr)->sin_addr;
    } else if (address.lpSockaddr->sa_family == AF_INET6 &&
               address.iSockaddrLength >= static_cast<int>(sizeof(sockaddr_in6))) {
        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address.lpSockaddr);
        source = &ipv6->sin6_addr;
        if (InetNtopW(AF_INET6, source, buffer.data(), static_cast<DWORD>(buffer.size())) == nullptr) {
            return {};
        }
        std::string result = Utf8(buffer.data());
        if (ipv6->sin6_scope_id != 0) result += '%' + std::to_string(ipv6->sin6_scope_id);
        return result;
    } else {
        return {};
    }

    return InetNtopW(AF_INET, source, buffer.data(), static_cast<DWORD>(buffer.size()))
               ? Utf8(buffer.data()) : std::string{};
}

template <typename Node>
void AddAddressList(std::vector<std::string>& lines, const char* label, Node* first) {
    std::unordered_set<std::string> seen;
    Node* current = first;
    for (int index = 0; index < kMaxNodes && current != nullptr; ++index) {
        std::string value = SocketAddressText(current->Address);
        std::string key = value;
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (!value.empty() && seen.insert(key).second) {
            lines.emplace_back(std::string(label) + '=' + value);
        }
        current = current->Next;
    }
}

void AddComputerNames(std::vector<std::string>& lines) {
    lines.emplace_back("[GetComputerNameExW]");
    const std::array<std::pair<COMPUTER_NAME_FORMAT, const char*>, 4> formats{{
        {ComputerNameNetBIOS, "ComputerNameNetBIOS"},
        {ComputerNameDnsHostname, "ComputerNameDnsHostname"},
        {ComputerNameDnsDomain, "ComputerNameDnsDomain"},
        {ComputerNameDnsFullyQualified, "ComputerNameDnsFullyQualified"}
    }};

    for (const auto& [format, label] : formats) {
        DWORD size = 0;
        GetComputerNameExW(format, nullptr, &size);
        const DWORD sizeError = GetLastError();
        if (size == 0 && sizeError != ERROR_SUCCESS && sizeError != ERROR_MORE_DATA) {
            lines.emplace_back(std::string(label) + "=ERROR(" + std::to_string(sizeError) + ')');
            continue;
        }
        std::vector<wchar_t> buffer(std::max<DWORD>(size, 1));
        if (!GetComputerNameExW(format, buffer.data(), &size)) {
            lines.emplace_back(std::string(label) + "=ERROR(" +
                               std::to_string(GetLastError()) + ')');
            continue;
        }
        lines.emplace_back(std::string(label) + '=' + Utf8(buffer.data()));
    }
}

DWORD AddAdapterAddresses(std::vector<std::string>& lines) {
    ULONG size = 0;
    constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
    ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size);
    if (result != ERROR_BUFFER_OVERFLOW || size == 0) return result;

    std::vector<unsigned char> storage(size);
    auto* first = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, first, &size);
    if (result != NO_ERROR) return result;

    lines.emplace_back("[GetAdaptersAddresses]");
    lines.emplace_back("ReturnCode=0");
    IP_ADAPTER_ADDRESSES* adapter = first;
    for (int index = 0; index < kMaxNodes && adapter != nullptr; ++index) {
        std::ostringstream mac;
        const ULONG macLength = std::min<ULONG>(adapter->PhysicalAddressLength, 8);
        for (ULONG byte = 0; byte < macLength; ++byte) {
            if (byte != 0) mac << '-';
            mac << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(adapter->PhysicalAddress[byte]);
        }

        lines.emplace_back("");
        lines.emplace_back("Adapter=" + (adapter->FriendlyName ? Utf8(adapter->FriendlyName) : "(unnamed)"));
        lines.emplace_back("Description=" + Utf8(adapter->Description));
        lines.emplace_back("Status=" + StatusName(adapter->OperStatus));
        lines.emplace_back("IfIndex=" + std::to_string(adapter->IfIndex));
        lines.emplace_back("IPv6IfIndex=" + std::to_string(adapter->Ipv6IfIndex));
        lines.emplace_back("MTU=" + std::to_string(adapter->Mtu));
        lines.emplace_back("MAC=" + mac.str());
        lines.emplace_back("DnsSuffix=" + Utf8(adapter->DnsSuffix));
        AddAddressList(lines, "IP", adapter->FirstUnicastAddress);
        AddAddressList(lines, "DNS", adapter->FirstDnsServerAddress);
        AddAddressList(lines, "Gateway", adapter->FirstGatewayAddress);
        adapter = adapter->Next;
    }
    return NO_ERROR;
}

void AddBestInterface(std::vector<std::string>& lines) {
    IN_ADDR destination{};
    DWORD result = InetPtonA(AF_INET, kBestInterfaceDestination, &destination) == 1
                       ? NO_ERROR : ERROR_INVALID_PARAMETER;
    DWORD interfaceIndex = 0;
    if (result == NO_ERROR) result = GetBestInterface(destination.S_un.S_addr, &interfaceIndex);

    lines.emplace_back("[GetBestInterface]");
    lines.emplace_back(std::string("Destination=") + kBestInterfaceDestination);
    lines.emplace_back("ReturnCode=" + std::to_string(result));
    lines.emplace_back(result == NO_ERROR
                           ? "BestInterfaceIndex=" + std::to_string(interfaceIndex)
                           : "BestInterfaceIndex=Unavailable");
}

std::string CreateReport(DWORD& adapterError) {
    std::vector<std::string> lines{
        "KISEC WINDOWS NETWORK INFORMATION",
        "CollectedUtc=" + UtcTimestamp(), ""
    };
    AddComputerNames(lines);
    lines.emplace_back("");
    adapterError = AddAdapterAddresses(lines);
    if (adapterError != NO_ERROR) return {};
    lines.emplace_back("");
    AddBestInterface(lines);

    std::ostringstream report;
    for (const std::string& line : lines) report << line << "\r\n";
    return report.str();
}

}  // namespace

int wmain() {
    DWORD adapterError = NO_ERROR;
    const std::string report = CreateReport(adapterError);
    if (report.empty()) {
        std::cerr << "[ERROR] GetAdaptersAddresses failed (" << adapterError << ").\n";
        return 1;
    }

    std::error_code error;
    fs::create_directories(fs::path(kReportPath).parent_path(), error);
    if (error) {
        std::cerr << "[ERROR] Unable to create report directory (" << error.value() << ").\n";
        return 1;
    }

    std::ofstream output(fs::path(kReportPath), std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "[ERROR] Unable to open the report file.\n";
        return 1;
    }
    constexpr unsigned char bom[]{0xEF, 0xBB, 0xBF};
    output.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    output.write(report.data(), static_cast<std::streamsize>(report.size()));
    output.close();
    if (!output) {
        std::cerr << "[ERROR] Unable to write the report file.\n";
        return 1;
    }

    std::cout << "[OK] Report created: C:\\Windows\\Temp\\Kisec\\network_info.txt\n";
    return 0;
}
