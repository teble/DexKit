// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "dexkit.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
void Check(bool condition) {
    if (!condition) { std::cerr << "Archive loading regression failed\n"; std::abort(); }
}

template<class T> void Append(std::vector<uint8_t> &bytes, const T &value) {
    const auto *p = reinterpret_cast<const uint8_t *>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(T));
}

std::shared_ptr<dexkit::MemMap> StoredArchive() {
    using namespace dexkit;
    std::vector<uint8_t> bytes, directory;
    for (const auto &[name, data] : {std::pair{'a', std::string("one")}, {'b', std::string("two")}}) {
        const auto offset = bytes.size();
        LocalFileHeader local{};
        local.signature = SIG_LOCAL_FILE;
        local.comp_size = local.uncomp_size = data.size();
        local.name_len = 1;
        Append(bytes, local);
        bytes.push_back(name);
        bytes.insert(bytes.end(), data.begin(), data.end());
        CentralDirHeader central{};
        central.signature = SIG_CFR;
        central.comp_size32 = central.uncomp_size32 = data.size();
        central.name_len = 1;
        central.lfh_offset32 = offset;
        Append(directory, central);
        directory.push_back(name);
    }
    EOCD end{};
    end.signature = SIG_EOCD;
    end.entries_on_disk = end.entries_total = 2;
    end.cd_offset32 = bytes.size();
    end.cd_size32 = directory.size();
    bytes.insert(bytes.end(), directory.begin(), directory.end());
    Append(bytes, end);
    return std::make_shared<MemMap>(bytes.data(), bytes.size());
}

void BatchOwnership() {
    using namespace dexkit;
    for (size_t threads : {0, 1, 4}) {
        auto mapping = StoredArchive();
        std::weak_ptr<MemMap> lifetime = mapping;
        auto archive = ZipArchive::Open(mapping);
        Check(bool(archive));
        const std::vector<const Entry *> entries{archive->Find("b"), archive->Find("a")};
        Check(entries[0] && entries[1]);
        Check(archive->GetUncompressData(entries, threads, 0, false).empty());
        Check(archive->GetUncompressData(entries, threads, 0, true).empty());
        auto detached = archive->GetUncompressData(entries, threads, 4, true);
        Check(detached.size() == 2);
        Check(std::string_view(reinterpret_cast<const char *>(detached[0]->data()), 3) == "two");
        Check(reinterpret_cast<uintptr_t>(detached[0]->data()) % 4 == 0);
        auto borrowed = archive->GetUncompressData(entries, threads);
        Check(borrowed.size() == 2);
        Check(borrowed[0]->data() == mapping->data() + entries[0]->data_offset);
        Entry invalid = *entries[0];
        ++invalid.comp_size;
        Check(archive->GetUncompressData({&invalid, entries[1]}, threads).empty());
        Check(archive->GetUncompressData({entries[0], nullptr}, threads).empty());
        archive.reset();
        mapping.reset();
        Check(!lifetime.expired());
        borrowed.clear();
        Check(lifetime.expired());
        Check(std::string_view(reinterpret_cast<const char *>(detached[1]->data()), 3) == "one");
    }
}

std::unique_ptr<dexkit::MemMap> OneClassDex() {
    std::vector<uint8_t> bytes(256);
    dex::Header header{};
    std::memcpy(header.magic, "dex\n035", 8);
    header.header_size = 112;
    header.file_size = bytes.size();
    header.endian_tag = dex::kEndianConstant;
    header.string_ids_size = header.type_ids_size = header.class_defs_size = 1;
    header.string_ids_off = 112;
    header.type_ids_off = 116;
    header.class_defs_off = 120;
    header.data_off = 152;
    header.data_size = bytes.size() - header.data_off;
    header.map_off = 240;
    std::memcpy(bytes.data(), &header, header.header_size);
    uint32_t string_offset = header.data_off;
    std::memcpy(bytes.data() + header.string_ids_off, &string_offset, sizeof(string_offset));
    dex::ClassDef class_def{};
    class_def.superclass_idx = class_def.source_file_idx = dex::kNoIndex;
    std::memcpy(bytes.data() + header.class_defs_off, &class_def, sizeof(class_def));
    const std::string_view descriptor = "Lduplicate/Type;";
    bytes[string_offset] = descriptor.size();
    std::memcpy(bytes.data() + string_offset + 1, descriptor.data(), descriptor.size());
    uint32_t map_size = 1;
    dex::MapItem map_item{};
    map_item.type = dex::kHeaderItem;
    map_item.size = 1;
    std::memcpy(bytes.data() + header.map_off, &map_size, sizeof(map_size));
    std::memcpy(bytes.data() + header.map_off + 4, &map_item, sizeof(map_item));
    return std::make_unique<dexkit::MemMap>(bytes.data(), bytes.size());
}

void DuplicateDeclarationOrder() {
    dexkit::DexKit core;
    core.SetThreadNum(2);
    std::vector<std::unique_ptr<dexkit::MemMap>> images;
    images.push_back(OneClassDex());
    images.push_back(OneClassDex());
    Check(core.AddImage(std::move(images)) == dexkit::Error::SUCCESS);
    Check(core.GetDexNum() == 2);
    // Model both worker completion orders explicitly, without a timing race.
    for (auto order : {std::pair{0, 1}, std::pair{1, 0}}) {
        core.PutDeclaredClass("Lduplicate/Type;", order.first, 0);
        core.PutDeclaredClass("Lduplicate/Type;", order.second, 0);
        auto [item, type_index] = core.GetClassDeclaredPair("Lduplicate/Type;");
        Check(item && item->GetDexId() == 1 && type_index == 0);
    }
}
} // namespace

int main() {
    dexkit::DexKit core;
    Check(core.GetThreadNum() == std::max(1U, std::thread::hardware_concurrency()));
    core.SetThreadNum(3);
    Check(core.GetThreadNum() == 3);
    BatchOwnership();
    DuplicateDeclarationOrder();
    std::cout << "Archive batch order, failure and ownership checks passed\n";
}
