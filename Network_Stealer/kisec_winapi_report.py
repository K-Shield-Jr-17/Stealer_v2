r"""Create a local Windows network-information report using three Win32 APIs.

No information is transmitted. The report is written to:
    C:\Windows\Temp\Kisec\network_info.txt
"""

from __future__ import annotations

import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path
import socket
import struct
import sys


REPORT_PATH = Path(r"C:\Windows\Temp\Kisec\network_info.txt")
BEST_INTERFACE_DESTINATION = "192.168.50.10"
MAX_NODES = 256

AF_UNSPEC = 0
AF_INET = 2
AF_INET6 = 23
GAA_FLAG_INCLUDE_PREFIX = 0x0010
GAA_FLAG_INCLUDE_GATEWAYS = 0x0080
ERROR_BUFFER_OVERFLOW = 111
ERROR_MORE_DATA = 234
NO_ERROR = 0


class _AdapterHeaderFields(ctypes.Structure):
    _fields_ = [("Length", wintypes.ULONG), ("IfIndex", wintypes.ULONG)]


class _AdapterHeader(ctypes.Union):
    _anonymous_ = ("fields",)
    _fields_ = [("Alignment", ctypes.c_ulonglong), ("fields", _AdapterHeaderFields)]


class SocketAddress(ctypes.Structure):
    _fields_ = [("Sockaddr", ctypes.c_void_p), ("SockaddrLength", ctypes.c_int)]


class _AddressHeaderFields(ctypes.Structure):
    _fields_ = [("Length", wintypes.ULONG), ("Reserved", wintypes.ULONG)]


class _AddressHeader(ctypes.Union):
    _anonymous_ = ("fields",)
    _fields_ = [("Alignment", ctypes.c_ulonglong), ("fields", _AddressHeaderFields)]


class AdapterAddress(ctypes.Structure):
    pass


AdapterAddress._anonymous_ = ("header",)
AdapterAddress._fields_ = [
    ("header", _AddressHeader),
    ("Next", ctypes.POINTER(AdapterAddress)),
    ("Address", SocketAddress),
]


class AdapterAddresses(ctypes.Structure):
    pass


AdapterAddresses._anonymous_ = ("header",)
AdapterAddresses._fields_ = [
    ("header", _AdapterHeader),
    ("Next", ctypes.POINTER(AdapterAddresses)),
    ("AdapterName", ctypes.c_char_p),
    ("FirstUnicastAddress", ctypes.POINTER(AdapterAddress)),
    ("FirstAnycastAddress", ctypes.c_void_p),
    ("FirstMulticastAddress", ctypes.c_void_p),
    ("FirstDnsServerAddress", ctypes.POINTER(AdapterAddress)),
    ("DnsSuffix", ctypes.c_wchar_p),
    ("Description", ctypes.c_wchar_p),
    ("FriendlyName", ctypes.c_wchar_p),
    ("PhysicalAddress", ctypes.c_ubyte * 8),
    ("PhysicalAddressLength", wintypes.ULONG),
    ("Flags", wintypes.ULONG),
    ("Mtu", wintypes.ULONG),
    ("IfType", wintypes.ULONG),
    ("OperStatus", ctypes.c_int),
    ("Ipv6IfIndex", wintypes.ULONG),
    ("ZoneIndices", wintypes.ULONG * 16),
    ("FirstPrefix", ctypes.c_void_p),
    ("TransmitLinkSpeed", ctypes.c_ulonglong),
    ("ReceiveLinkSpeed", ctypes.c_ulonglong),
    ("FirstWinsServerAddress", ctypes.c_void_p),
    ("FirstGatewayAddress", ctypes.POINTER(AdapterAddress)),
]


def _read_socket_address(address: SocketAddress) -> str | None:
    if not address.Sockaddr or address.SockaddrLength < 2:
        return None

    family = ctypes.c_ushort.from_address(address.Sockaddr).value
    if family == AF_INET and address.SockaddrLength >= 8:
        packed = ctypes.string_at(address.Sockaddr + 4, 4)
        return socket.inet_ntop(socket.AF_INET, packed)

    if family == AF_INET6 and address.SockaddrLength >= 28:
        packed = ctypes.string_at(address.Sockaddr + 8, 16)
        text = socket.inet_ntop(socket.AF_INET6, packed)
        scope_id = ctypes.c_ulong.from_address(address.Sockaddr + 24).value
        return f"{text}%{scope_id}" if scope_id else text

    return None


def _read_address_list(first: ctypes.POINTER(AdapterAddress)) -> list[str]:
    values: list[str] = []
    current = first
    for _ in range(MAX_NODES):
        if not current:
            break
        node = current.contents
        value = _read_socket_address(node.Address)
        if value and value not in values:
            values.append(value)
        current = node.Next
    return values


def _status_name(value: int) -> str:
    return {
        1: "Up",
        2: "Down",
        3: "Testing",
        4: "Unknown",
        5: "Dormant",
        6: "NotPresent",
        7: "LowerLayerDown",
    }.get(value, f"Other({value})")


def get_computer_names(kernel32: ctypes.WinDLL) -> list[str]:
    get_computer_name_ex = kernel32.GetComputerNameExW
    get_computer_name_ex.argtypes = [
        ctypes.c_int,
        wintypes.LPWSTR,
        ctypes.POINTER(wintypes.DWORD),
    ]
    get_computer_name_ex.restype = wintypes.BOOL

    formats = {
        0: "ComputerNameNetBIOS",
        1: "ComputerNameDnsHostname",
        2: "ComputerNameDnsDomain",
        3: "ComputerNameDnsFullyQualified",
    }
    lines = ["[GetComputerNameExW]"]
    for value, label in formats.items():
        size = wintypes.DWORD(0)
        ctypes.set_last_error(0)
        get_computer_name_ex(value, None, ctypes.byref(size))
        error = ctypes.get_last_error()
        if size.value == 0 and error not in (NO_ERROR, ERROR_MORE_DATA):
            lines.append(f"{label}=ERROR({error})")
            continue

        buffer = ctypes.create_unicode_buffer(max(size.value, 1))
        if not get_computer_name_ex(value, buffer, ctypes.byref(size)):
            lines.append(f"{label}=ERROR({ctypes.get_last_error()})")
            continue
        lines.append(f"{label}={buffer.value}")
    return lines


def get_adapter_addresses(iphlpapi: ctypes.WinDLL) -> list[str]:
    get_adapters_addresses = iphlpapi.GetAdaptersAddresses
    get_adapters_addresses.argtypes = [
        wintypes.ULONG,
        wintypes.ULONG,
        ctypes.c_void_p,
        ctypes.POINTER(AdapterAddresses),
        ctypes.POINTER(wintypes.ULONG),
    ]
    get_adapters_addresses.restype = wintypes.ULONG

    size = wintypes.ULONG(0)
    flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS
    result = get_adapters_addresses(AF_UNSPEC, flags, None, None, ctypes.byref(size))
    if result != ERROR_BUFFER_OVERFLOW or size.value == 0:
        raise OSError(result, "GetAdaptersAddresses size query failed")

    buffer = ctypes.create_string_buffer(size.value)
    first = ctypes.cast(buffer, ctypes.POINTER(AdapterAddresses))
    result = get_adapters_addresses(AF_UNSPEC, flags, None, first, ctypes.byref(size))
    if result != NO_ERROR:
        raise OSError(result, "GetAdaptersAddresses failed")

    lines = ["[GetAdaptersAddresses]", f"ReturnCode={result}"]
    current = first
    for _ in range(MAX_NODES):
        if not current:
            break
        adapter = current.contents
        mac_length = min(int(adapter.PhysicalAddressLength), len(adapter.PhysicalAddress))
        mac = "-".join(f"{adapter.PhysicalAddress[i]:02X}" for i in range(mac_length))
        lines.extend(
            [
                "",
                f"Adapter={adapter.FriendlyName or '(unnamed)'}",
                f"Description={adapter.Description or ''}",
                f"Status={_status_name(adapter.OperStatus)}",
                f"IfIndex={adapter.IfIndex}",
                f"IPv6IfIndex={adapter.Ipv6IfIndex}",
                f"MTU={adapter.Mtu}",
                f"MAC={mac}",
                f"DnsSuffix={adapter.DnsSuffix or ''}",
            ]
        )
        lines.extend(f"IP={item}" for item in _read_address_list(adapter.FirstUnicastAddress))
        lines.extend(f"DNS={item}" for item in _read_address_list(adapter.FirstDnsServerAddress))
        lines.extend(f"Gateway={item}" for item in _read_address_list(adapter.FirstGatewayAddress))
        current = adapter.Next
    return lines


def get_best_interface(iphlpapi: ctypes.WinDLL, destination: str) -> list[str]:
    get_best_interface_api = iphlpapi.GetBestInterface
    get_best_interface_api.argtypes = [wintypes.ULONG, ctypes.POINTER(wintypes.ULONG)]
    get_best_interface_api.restype = wintypes.DWORD

    destination_value = struct.unpack("=I", socket.inet_aton(destination))[0]
    interface_index = wintypes.ULONG(0)
    result = get_best_interface_api(destination_value, ctypes.byref(interface_index))
    lines = [
        "[GetBestInterface]",
        f"Destination={destination}",
        f"ReturnCode={result}",
    ]
    if result == NO_ERROR:
        lines.append(f"BestInterfaceIndex={interface_index.value}")
    else:
        lines.append("BestInterfaceIndex=Unavailable")
    return lines


def create_report() -> str:
    if sys.platform != "win32":
        raise RuntimeError("This program runs only on Windows.")

    kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
    iphlpapi = ctypes.WinDLL("iphlpapi.dll", use_last_error=True)
    sections = [
        "KISEC WINDOWS NETWORK INFORMATION",
        f"CollectedUtc={datetime.now(timezone.utc).isoformat()}",
        "",
        *get_computer_names(kernel32),
        "",
        *get_adapter_addresses(iphlpapi),
        "",
        *get_best_interface(iphlpapi, BEST_INTERFACE_DESTINATION),
    ]
    return "\n".join(sections) + "\n"


def main() -> int:
    try:
        report = create_report()
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(report, encoding="utf-8-sig", newline="\r\n")
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    print(f"[OK] Report created: {REPORT_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
