/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/buffer/io/device_throughput.hxx>

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#ifdef WINDOWS
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <iphlpapi.h>
#	include <windows.h>
#	include <winioctl.h>
#elifdef LINUX
#	include <cstring>
#	include <linux/ethtool.h>
#	include <linux/sockios.h>
#	include <net/if.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <sys/stat.h>
#	include <sys/statfs.h>
#	include <sys/sysmacros.h>
#	include <unistd.h>
#elifdef MACOS
#	include <CoreFoundation/CoreFoundation.h>
#	include <IOKit/IOBSD.h>
#	include <IOKit/IOKitLib.h>
#	include <IOKit/storage/IOMedia.h>
#	include <cstring>
#	include <net/if.h>
#	include <sys/mount.h>
#	include <sys/socket.h>
#	include <unistd.h>
#endif

using namespace StormByte::Buffer::IO;

namespace {
	constexpr std::size_t MiB = 1024ull * 1024ull;
	constexpr std::size_t GiB = 1024ull * MiB;

	enum class Kind {
		Hdd,
		SataSsd,
		NvmeGen3,
		NvmeGen4,
		NvmeGen5,
		UsbHdd,
		UsbStick,
		NetFallback
	};

	struct Preset {
		Kind kind;
		DeviceThroughput rate;
	};

	constexpr std::array<Preset, 8> kRate {{
		{ Kind::Hdd,         { 150 * MiB, 150 * MiB } },
		{ Kind::SataSsd,     { 500 * MiB, 500 * MiB } },
		{ Kind::NvmeGen3,    {   3 * GiB,   3 * GiB } },
		{ Kind::NvmeGen4,    {   6 * GiB,   6 * GiB } },
		{ Kind::NvmeGen5,    {  10 * GiB,  10 * GiB } },
		{ Kind::UsbHdd,      { 100 * MiB, 100 * MiB } },
		{ Kind::UsbStick,    {  30 * MiB,  12 * MiB } },
		{ Kind::NetFallback, {  30 * MiB,  30 * MiB } }
	}};

	constexpr DeviceThroughput Rate(const Kind kind) noexcept {
		for (const auto& row : kRate) {
			if (row.kind == kind)
				return row.rate;
		}
		return kRate.front().rate;
	}

	constexpr DeviceThroughput NvmeByGen(const int gen) noexcept {
		if (gen >= 5)
			return Rate(Kind::NvmeGen5);
		if (gen >= 4)
			return Rate(Kind::NvmeGen4);
		return Rate(Kind::NvmeGen3);
	}

	constexpr DeviceThroughput FromLinkBps(const std::uint64_t link_bps) noexcept {
		if (link_bps == 0)
			return Rate(Kind::NetFallback);
		const std::size_t useful = static_cast<std::size_t>(link_bps * 80ull / 100ull / 8ull);
		if (useful == 0)
			return Rate(Kind::NetFallback);
		return { useful, useful };
	}

	std::filesystem::path ExistingAncestor(std::filesystem::path path) noexcept {
		std::error_code ec;
		while (!path.empty() && !std::filesystem::exists(path, ec)) {
			const auto parent = path.parent_path();
			if (parent == path)
				break;
			path = parent;
		}
		return path;
	}

	std::string Lower(std::string s) {
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	bool IsNetworkFsName(const std::string& type) noexcept {
		const auto t = Lower(type);
		return t == "nfs" || t == "nfs4" || t == "cifs" || t == "smb" || t == "smb2" ||
			t == "smb3" || t == "smbfs" || t == "afpfs" || t == "afp" || t == "webdav" ||
			t == "9p" || t == "afs" || t.find("fuse") != std::string::npos;
	}

#ifdef LINUX
	std::string ReadSys(const std::filesystem::path& path) {
		std::ifstream in(path);
		if (!in)
			return {};
		std::string s;
		std::getline(in, s);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
			s.pop_back();
		return s;
	}

	std::filesystem::path BlockRoot(const std::filesystem::path& sys_dev) {
		std::error_code ec;
		auto cur = std::filesystem::canonical(sys_dev, ec);
		if (ec)
			return {};
		for (int i = 0; i < 8; ++i) {
			if (std::filesystem::exists(cur / "queue" / "rotational", ec))
				return cur;
			const auto parent = cur.parent_path();
			if (parent == cur)
				break;
			cur = parent;
		}
		return {};
	}

	int NvmeGen(const std::filesystem::path& block) {
		const auto name = block.filename().string();
		if (name.rfind("nvme", 0) != 0)
			return 0;
		const auto link = Lower(ReadSys(block / "device" / "device" / "current_link_speed"));
		if (link.find("32.0") != std::string::npos)
			return 5;
		if (link.find("16.0") != std::string::npos)
			return 4;
		if (link.find("8.0") != std::string::npos)
			return 3;
		return 3;
	}

	std::string FsMagic(const std::filesystem::path& path) {
		struct statfs st {};
		if (statfs(path.c_str(), &st) != 0)
			return {};
		switch (st.f_type) {
			case 0x6969: return "nfs";
			case 0xFF534D42: return "cifs";
			default: break;
		}
		return {};
	}

	std::string DefaultIface() {
		std::ifstream in("/proc/net/route");
		if (!in)
			return {};
		std::string line;
		std::getline(in, line);
		while (std::getline(in, line)) {
			std::istringstream ss(line);
			std::string iface, dest;
			if (!(ss >> iface >> dest))
				continue;
			if (dest == "00000000")
				return iface;
		}
		return {};
	}

	std::uint64_t NicBits(const std::string& iface) {
		if (iface.empty())
			return 0;
		const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (fd < 0)
			return 0;
		struct ifreq ifr {};
		struct ethtool_cmd cmd {};
		std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
		cmd.cmd = ETHTOOL_GSET;
		ifr.ifr_data = reinterpret_cast<char*>(&cmd);
		std::uint64_t bits = 0;
		if (ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
			const unsigned int mbps = ethtool_cmd_speed(&cmd);
			if (mbps != 0 && mbps != static_cast<unsigned int>(-1))
				bits = static_cast<std::uint64_t>(mbps) * 1000ull * 1000ull;
		}
		::close(fd);
		return bits;
	}

	DeviceThroughput ProbeLinux(const std::filesystem::path& path) {
		const auto root = ExistingAncestor(path);
		if (IsNetworkFsName(FsMagic(root)))
			return FromLinkBps(NicBits(DefaultIface()));

		struct stat st {};
		if (stat(root.c_str(), &st) != 0)
			return Rate(Kind::Hdd);

		const auto sys = std::filesystem::path("/sys/dev/block") /
			(std::to_string(gnu_dev_major(st.st_dev)) + ":" +
				std::to_string(gnu_dev_minor(st.st_dev)));
		const auto block = BlockRoot(sys);
		if (block.empty())
			return Rate(Kind::Hdd);

		const bool rotational = ReadSys(block / "queue" / "rotational") == "1";
		const bool removable = ReadSys(block / "removable") == "1";
		const bool nvme = block.filename().string().rfind("nvme", 0) == 0;

		if (removable)
			return Rate(rotational ? Kind::UsbHdd : Kind::UsbStick);
		if (rotational)
			return Rate(Kind::Hdd);
		if (nvme)
			return NvmeByGen(NvmeGen(block));
		return Rate(Kind::SataSsd);
	}
#elifdef WINDOWS
	DeviceThroughput WindowsNic() {
		DWORD index = 0;
		if (GetBestInterface(htonl(0x08080808), &index) != NO_ERROR)
			return Rate(Kind::NetFallback);
		MIB_IF_ROW2 row {};
		row.InterfaceIndex = index;
		if (GetIfEntry2(&row) != NO_ERROR || row.TransmitLinkSpeed == 0)
			return Rate(Kind::NetFallback);
		return FromLinkBps(row.TransmitLinkSpeed);
	}

	DeviceThroughput ProbeWindows(const std::filesystem::path& path) {
		const auto root = ExistingAncestor(path);
		const auto wide = root.wstring();
		if (wide.size() < 2 || wide[1] != L':')
			return Rate(Kind::Hdd);

		wchar_t drive[] = { wide[0], L':', L'\\', 0 };
		const UINT type = GetDriveTypeW(drive);
		if (type == DRIVE_REMOTE)
			return WindowsNic();
		if (type == DRIVE_CDROM)
			return Rate(Kind::Hdd);

		wchar_t volume[] = { L'\\', L'\\', L'.', L'\\', wide[0], L':', 0 };
		const HANDLE disk = CreateFileW(volume, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (disk == INVALID_HANDLE_VALUE) {
			if (type == DRIVE_REMOVABLE)
				return Rate(Kind::UsbStick);
			return Rate(Kind::Hdd);
		}

		STORAGE_PROPERTY_QUERY query {};
		query.QueryType = PropertyStandardQuery;
		query.PropertyId = StorageDeviceSeekPenaltyProperty;
		DEVICE_SEEK_PENALTY_DESCRIPTOR penalty {};
		DWORD got = 0;
		const bool has_penalty = DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query), &penalty, sizeof(penalty), &got, nullptr) != 0;

		query.PropertyId = StorageAdapterProperty;
		STORAGE_ADAPTER_DESCRIPTOR adapter {};
		const bool has_adapter = DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query), &adapter, sizeof(adapter), &got, nullptr) != 0;
		CloseHandle(disk);

		const bool seek_penalty = has_penalty && penalty.IncursSeekPenalty;
		const bool nvme = has_adapter && adapter.BusType == BusTypeNvme;
		const bool removable = type == DRIVE_REMOVABLE;

		if (removable)
			return Rate(seek_penalty ? Kind::UsbHdd : Kind::UsbStick);
		if (seek_penalty)
			return Rate(Kind::Hdd);
		if (nvme)
			return Rate(Kind::NvmeGen3);
		return Rate(Kind::SataSsd);
	}
#elifdef MACOS
	std::string CfToString(CFTypeRef ref) {
		if (!ref)
			return {};
		if (CFGetTypeID(ref) == CFStringGetTypeID()) {
			char buf[256];
			if (CFStringGetCString(static_cast<CFStringRef>(ref), buf, sizeof(buf),
					kCFStringEncodingUTF8))
				return buf;
		}
		if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
			double n = 0;
			CFNumberGetValue(static_cast<CFNumberRef>(ref), kCFNumberDoubleType, &n);
			return std::to_string(n);
		}
		return {};
	}

	bool CfBool(CFTypeRef ref, const bool fallback) {
		if (ref && CFGetTypeID(ref) == CFBooleanGetTypeID())
			return CFBooleanGetValue(static_cast<CFBooleanRef>(ref));
		return fallback;
	}

	int PcieGenFromGt(const std::string& raw) {
		const auto s = Lower(raw);
		if (s.find("32") != std::string::npos)
			return 5;
		if (s.find("16") != std::string::npos)
			return 4;
		if (s.find("8") != std::string::npos)
			return 3;
		return 3;
	}

	DeviceThroughput FromIoRegistry(const std::string& bsd) {
		if (bsd.empty())
			return Rate(Kind::Hdd);

		CFMutableDictionaryRef match = IOBSDNameMatching(kIOMainPortDefault, 0, bsd.c_str());
		if (!match)
			return Rate(Kind::Hdd);
		io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, match);
		if (!service)
			return Rate(Kind::Hdd);

		bool solid = false;
		bool removable = false;
		bool nvme = false;
		bool sata = false;
		int gen = 3;

		io_service_t cursor = service;
		for (int depth = 0; depth < 12 && cursor; ++depth) {
			CFMutableDictionaryRef props = nullptr;
			if (IORegistryEntryCreateCFProperties(cursor, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
				solid = CfBool(CFDictionaryGetValue(props, CFSTR("Solid State")), solid);
				removable = CfBool(CFDictionaryGetValue(props, CFSTR("Removable")), removable) ||
					CfBool(CFDictionaryGetValue(props, CFSTR("Ejectable")), removable);

				if (const auto proto = CFDictionaryGetValue(props, CFSTR("Protocol Characteristics"))) {
					if (CFGetTypeID(proto) == CFDictionaryGetTypeID()) {
						const auto interconnect = CfToString(CFDictionaryGetValue(
							static_cast<CFDictionaryRef>(proto), CFSTR("Physical Interconnect")));
						const auto low = Lower(interconnect);
						if (low.find("pci") != std::string::npos || low.find("nvme") != std::string::npos)
							nvme = true;
						if (low.find("sata") != std::string::npos || low.find("ata") != std::string::npos)
							sata = true;
					}
				}

				io_name_t cname {};
				if (IOObjectGetClass(cursor, cname) == KERN_SUCCESS) {
					const auto cls = Lower(cname);
					if (cls.find("nvme") != std::string::npos)
						nvme = true;
					if (cls.find("sata") != std::string::npos)
						sata = true;
					if (cls.find("pci") != std::string::npos) {
						const auto speed = CfToString(CFDictionaryGetValue(props, CFSTR("negotiated-link-speed")));
						if (!speed.empty())
							gen = PcieGenFromGt(speed);
					}
				}
				CFRelease(props);
			}

			io_service_t parent = 0;
			if (IORegistryEntryGetParentEntry(cursor, kIOServicePlane, &parent) != KERN_SUCCESS)
				break;
			if (cursor != service)
				IOObjectRelease(cursor);
			cursor = parent;
		}
		if (cursor && cursor != service)
			IOObjectRelease(cursor);
		IOObjectRelease(service);

		if (removable)
			return Rate(solid ? Kind::UsbStick : Kind::UsbHdd);
		if (nvme)
			return NvmeByGen(gen);
		if (solid || sata)
			return Rate(Kind::SataSsd);
		return Rate(Kind::Hdd);
	}

	DeviceThroughput ProbeMac(const std::filesystem::path& path) {
		const auto root = ExistingAncestor(path);
		struct statfs st {};
		if (statfs(root.c_str(), &st) != 0)
			return Rate(Kind::Hdd);
		if (IsNetworkFsName(st.f_fstypename) || (st.f_flags & MNT_LOCAL) == 0)
			return Rate(Kind::NetFallback);

		std::string bsd = st.f_mntfromname;
		if (bsd.rfind("/dev/", 0) == 0)
			bsd.erase(0, 5);
		return FromIoRegistry(bsd);
	}
#endif
}

DeviceThroughput StormByte::Buffer::IO::ProbeDeviceThroughput(
		const std::filesystem::path& path) noexcept {
#ifdef WINDOWS
	return ProbeWindows(path);
#elifdef MACOS
	return ProbeMac(path);
#elifdef LINUX
	return ProbeLinux(path);
#else
	static_cast<void>(path);
	return Rate(Kind::Hdd);
#endif
}
