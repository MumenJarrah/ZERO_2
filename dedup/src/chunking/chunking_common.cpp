// chunking_common.cpp (Updated to write only unique chunks across files)
/*
#include "chunking_common.hpp"
#include "hashing_common.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>

extern bool disable_hashing;

// Global seen hashes to persist across multiple file chunking
static std::unordered_set<std::string> global_seen_hashes;

File_Chunk::File_Chunk(uint64_t _chunk_size) {
    chunk_size = _chunk_size;
    chunk_data = std::make_unique<char[]>(_chunk_size);
}

File_Chunk::File_Chunk(const File_Chunk& other) : File_Chunk(other.chunk_size) {
    memcpy(this->chunk_data.get(), other.chunk_data.get(), this->chunk_size);
    if (other.chunk_hash) {
        this->chunk_hash = std::make_unique<Hash>(*other.chunk_hash);
    }
}

File_Chunk::File_Chunk(File_Chunk&& other) noexcept {
    this->chunk_size = other.chunk_size;
    this->chunk_data = std::move(other.chunk_data);
    this->chunk_hash = std::move(other.chunk_hash);
    other.chunk_data.release();
    other.chunk_hash.release();
}

uint64_t File_Chunk::get_size() const { return chunk_size; }
char* File_Chunk::get_data() const { return chunk_data.get(); }

BYTE* File_Chunk::get_hash() const {
    if (chunk_hash) return chunk_hash->getHash();
    return nullptr;
}

void File_Chunk::init_hash(HashingTech hashing_tech, uint64_t size) {
    chunk_hash = std::make_unique<Hash>(hashing_tech, size);
}

std::string File_Chunk::to_string() const {
    if (chunk_hash)
        return chunk_hash->toString() + "," + std::to_string(chunk_size);
    return "INVALID HASH";
}

void File_Chunk::print() const {
    std::cout << "\tChunk Size: " << chunk_size << std::endl;
    if (chunk_hash)
        std::cout << "\tChunk Hash: " << chunk_hash->toString() << std::endl;
    std::cout << "\tChunk Data: ";
    for (uint64_t i = 0; i < chunk_size; ++i)
        std::cout << chunk_data[i];
    std::cout << std::endl;
}

uint64_t Chunking_Technique::get_file_size(std::istream* file_ptr) {
    file_ptr->seekg(0, std::ios_base::end);
    uint64_t file_size = file_ptr->tellg();
    file_ptr->seekg(0, std::ios_base::beg);
    return file_size;
}

std::unique_ptr<std::istream> Chunking_Technique::read_file_to_buffer(std::string file_path) {
    std::unique_ptr<std::istream> buffers;
    const uint64_t buffer_size = pow(2, 20);
    auto buffer = std::make_unique<char[]>(buffer_size);

    std::ifstream file_ptr(file_path, std::ios::binary);
    file_ptr.seekg(0, std::ios::end);
    uint64_t length = file_ptr.tellg();
    file_ptr.seekg(0, std::ios::beg);
    auto ss = std::make_unique<std::stringstream>();

    uint64_t curr_bytes_read = 0;
    uint64_t bytes_to_read = std::min(buffer_size, length);
    while (curr_bytes_read < length) {
        file_ptr.read(buffer.get(), bytes_to_read);
        ss->write(buffer.get(), bytes_to_read);
        curr_bytes_read += bytes_to_read;
        bytes_to_read = std::min(buffer_size, length - curr_bytes_read);
    }

    file_ptr.close();
    return ss;
}

std::vector<std::string> Chunking_Technique::chunk_file(std::string file_path) {
    std::vector<std::string> hashes;
    std::ifstream file_ptr(file_path, std::ios::in | std::ios::binary);
    chunk_stream(hashes, file_ptr);
    return hashes;
}

int64_t Chunking_Technique::create_chunk(std::vector<std::string>& hashes,
                                         char* buffer,
                                         uint64_t buffer_end) {
    return create_chunk(hashes, buffer, buffer_end, global_seen_hashes);
}

int64_t Chunking_Technique::create_chunk(std::vector<std::string>& hashes,
                                         char* buffer,
                                         uint64_t buffer_end,
                                         std::unordered_set<std::string>& seen) {
    auto begin_chunking = std::chrono::high_resolution_clock::now();
    uint64_t chunk_size = find_cutpoint(buffer, buffer_end);
    auto end_chunking = std::chrono::high_resolution_clock::now();
    total_time_chunking += (end_chunking - begin_chunking);

    File_Chunk new_chunk{chunk_size};
    memcpy(new_chunk.get_data(), buffer, chunk_size);

    if (!disable_hashing) {
        auto begin_hashing = std::chrono::high_resolution_clock::now();
        hash_method->hash_chunk(new_chunk);
        auto end_hashing = std::chrono::high_resolution_clock::now();
        total_time_hashing += (end_hashing - begin_hashing);
    }

    std::string hash_str = new_chunk.to_string();
    hashes.emplace_back(hash_str);
    std::string hash_val = hash_str.substr(0, hash_str.find(','));

    if (seen.insert(hash_val).second) {
        std::filesystem::create_directories("chunk_store");
        static std::ofstream dedup_data("chunk_store/dedup_data.bin", std::ios::binary | std::ios::app);
        static std::ofstream manifest("chunk_store/manifest.txt", std::ios::app);

        uint64_t offset = dedup_data.tellp();
        dedup_data.write(new_chunk.get_data(), new_chunk.get_size());
        manifest << hash_val << "," << new_chunk.get_size() << "," << offset << "\n";
    }

    return chunk_size;
}

void Chunking_Technique::chunk_stream(std::vector<std::string>& hashes, std::istream& stream) {
    std::vector<char> buffer;
    int64_t buffer_size = stream_buffer_size ? stream_buffer_size : 1024 * 1024;
    int64_t bytes_left = get_file_size(&stream);
    buffer.resize(buffer_size);

    int64_t chunk_size = buffer_size;
    int64_t buffer_end = 0;

    while (true) {
        uint32_t bytes_to_read = std::min(buffer_size, std::min(bytes_left, chunk_size));
        stream.read(buffer.data() + buffer_end, bytes_to_read);
        if (stream.gcount() == 0) break;

        buffer_end += bytes_to_read;
        chunk_size = create_chunk(hashes, buffer.data(), buffer_end, global_seen_hashes);
        buffer_end -= chunk_size;
        memmove(&buffer[0], &buffer[chunk_size], buffer_end);
        bytes_left -= bytes_to_read;
    }

    while (buffer_end > 0) {
        chunk_size = create_chunk(hashes, buffer.data(), buffer_end, global_seen_hashes);
        buffer_end -= chunk_size;
        memmove(&buffer[0], &buffer[chunk_size], buffer_end);
    }
}
*/
// NEW code
#include "chunking_common.hpp"
#include "hashing_common.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

extern bool disable_hashing;

// Global seen hashes to persist across multiple file chunking
static std::unordered_set<std::string> global_seen_hashes;

File_Chunk::File_Chunk(uint64_t _chunk_size) {
    chunk_size = _chunk_size;
    chunk_data = std::make_unique<char[]>(_chunk_size);
}

File_Chunk::File_Chunk(const File_Chunk& other) : File_Chunk(other.chunk_size) {
    memcpy(this->chunk_data.get(), other.chunk_data.get(), this->chunk_size);
    if (other.chunk_hash) {
        this->chunk_hash = std::make_unique<Hash>(*other.chunk_hash);
    }
}

File_Chunk::File_Chunk(File_Chunk&& other) noexcept {
    this->chunk_size = other.chunk_size;
    this->chunk_data = std::move(other.chunk_data);
    this->chunk_hash = std::move(other.chunk_hash);
    other.chunk_data.release();
    other.chunk_hash.release();
}

uint64_t File_Chunk::get_size() const { return chunk_size; }
char* File_Chunk::get_data() const { return chunk_data.get(); }

BYTE* File_Chunk::get_hash() const {
    if (chunk_hash) return chunk_hash->getHash();
    return nullptr;
}

void File_Chunk::init_hash(HashingTech hashing_tech, uint64_t size) {
    chunk_hash = std::make_unique<Hash>(hashing_tech, size);
}

std::string File_Chunk::to_string() const {
    if (chunk_hash)
        return chunk_hash->toString() + "," + std::to_string(chunk_size);
    return "INVALID HASH";
}

void File_Chunk::print() const {
    std::cout << "\tChunk Size: " << chunk_size << std::endl;
    if (chunk_hash)
        std::cout << "\tChunk Hash: " << chunk_hash->toString() << std::endl;
    std::cout << "\tChunk Data: ";
    for (uint64_t i = 0; i < chunk_size; ++i)
        std::cout << chunk_data[i];
    std::cout << std::endl;
}

uint64_t Chunking_Technique::get_file_size(std::istream* file_ptr) {
    file_ptr->seekg(0, std::ios_base::end);
    uint64_t file_size = file_ptr->tellg();
    file_ptr->seekg(0, std::ios_base::beg);
    return file_size;
}

std::unique_ptr<std::istream> Chunking_Technique::read_file_to_buffer(std::string file_path) {
    std::unique_ptr<std::istream> buffers;
    const uint64_t buffer_size = pow(2, 20);
    auto buffer = std::make_unique<char[]>(buffer_size);

    std::ifstream file_ptr(file_path, std::ios::binary);
    file_ptr.seekg(0, std::ios::end);
    uint64_t length = file_ptr.tellg();
    file_ptr.seekg(0, std::ios::beg);
    auto ss = std::make_unique<std::stringstream>();

    uint64_t curr_bytes_read = 0;
    uint64_t bytes_to_read = std::min(buffer_size, length);
    while (curr_bytes_read < length) {
        file_ptr.read(buffer.get(), bytes_to_read);
        ss->write(buffer.get(), bytes_to_read);
        curr_bytes_read += bytes_to_read;
        bytes_to_read = std::min(buffer_size, length - curr_bytes_read);
    }

    file_ptr.close();
    return ss;
}

std::vector<std::string> Chunking_Technique::chunk_file(std::string file_path) {
    std::vector<std::string> hashes;
    std::ifstream file_ptr(file_path, std::ios::in | std::ios::binary);
    chunk_stream(hashes, file_ptr);
    return hashes;
}

// ---- 4 MiB segmented chunk storage logic ----
namespace {

constexpr uint64_t kSegSize = 4ULL * 1024 * 1024; // 4 MiB

static std::ofstream curr_seg;
static uint32_t seg_index = 0;
static uint64_t curr_size = 0;
static std::mutex seg_mu;

void open_next_segment() {
    if (curr_seg.is_open()) curr_seg.close();
    std::filesystem::create_directories("chunk_store");
    std::string name = "chunk_store/dedup_data_" + std::to_string(seg_index) + ".bin";
    curr_seg.open(name, std::ios::binary | std::ios::trunc);
    if (!curr_seg)
        throw std::runtime_error("Failed to open segment file: " + name);
    curr_size = 0;
}

} // namespace

int64_t Chunking_Technique::create_chunk(std::vector<std::string>& hashes,
                                         char* buffer,
                                         uint64_t buffer_end) {
    return create_chunk(hashes, buffer, buffer_end, global_seen_hashes);
}

int64_t Chunking_Technique::create_chunk(std::vector<std::string>& hashes,
                                         char* buffer,
                                         uint64_t buffer_end,
                                         std::unordered_set<std::string>& seen) {
    auto begin_chunking = std::chrono::high_resolution_clock::now();
    uint64_t chunk_size = find_cutpoint(buffer, buffer_end);
    auto end_chunking = std::chrono::high_resolution_clock::now();
    total_time_chunking += (end_chunking - begin_chunking);

    File_Chunk new_chunk{chunk_size};
    memcpy(new_chunk.get_data(), buffer, chunk_size);

    if (!disable_hashing) {
        auto begin_hashing = std::chrono::high_resolution_clock::now();
        hash_method->hash_chunk(new_chunk);
        auto end_hashing = std::chrono::high_resolution_clock::now();
        total_time_hashing += (end_hashing - begin_hashing);
    }

    std::string hash_str = new_chunk.to_string();
    hashes.emplace_back(hash_str);
    std::string hash_val = hash_str.substr(0, hash_str.find(','));

    if (seen.insert(hash_val).second) {
        static std::ofstream manifest("chunk_store/manifest.txt", std::ios::app);

        std::lock_guard<std::mutex> lk(seg_mu);

        if (!curr_seg.is_open()) open_next_segment();

        if (curr_size + new_chunk.get_size() > kSegSize) {
            ++seg_index;
            open_next_segment();
        }

        uint64_t offset_in_seg = curr_size;
        curr_seg.write(new_chunk.get_data(), new_chunk.get_size());
        curr_size += new_chunk.get_size();

        manifest << hash_val << "," << new_chunk.get_size() << "," << seg_index << "," << offset_in_seg << "\n";
    }

    return chunk_size;
}

void Chunking_Technique::chunk_stream(std::vector<std::string>& hashes, std::istream& stream) {
    std::vector<char> buffer;
    int64_t buffer_size = stream_buffer_size ? stream_buffer_size : 1024 * 1024;
    int64_t bytes_left = get_file_size(&stream);
    buffer.resize(buffer_size);

    int64_t chunk_size = buffer_size;
    int64_t buffer_end = 0;

    while (true) {
        uint32_t bytes_to_read = std::min(buffer_size, std::min(bytes_left, chunk_size));
        stream.read(buffer.data() + buffer_end, bytes_to_read);
        if (stream.gcount() == 0) break;

        buffer_end += bytes_to_read;
        chunk_size = create_chunk(hashes, buffer.data(), buffer_end, global_seen_hashes);
        buffer_end -= chunk_size;
        memmove(&buffer[0], &buffer[chunk_size], buffer_end);
        bytes_left -= bytes_to_read;
    }

    while (buffer_end > 0) {
        chunk_size = create_chunk(hashes, buffer.data(), buffer_end, global_seen_hashes);
        buffer_end -= chunk_size;
        memmove(&buffer[0], &buffer[chunk_size], buffer_end);
    }
}

