#include "format.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr size_t PATH_SIZE = 4096;

struct Config {
    char uri[1024];
    char database[256];
    char terms_collection[256];
    char output_directory[PATH_SIZE];
    char term_field[32];
    size_t block_postings;
    size_t progress_every_documents;
};

struct PostingPair {
    char *term;
    uint32_t document_id;
};

struct PostingBuffer {
    PostingPair *items;
    size_t size;
    size_t capacity;
};

struct BlockList {
    char **paths;
    size_t size;
    size_t capacity;
};

struct BlockReader {
    FILE *file;
    char *term;
    uint32_t document_id;
    bool active;
};

struct ReaderHeap {
    size_t *items;
    size_t size;
    BlockReader *readers;
};

constexpr size_t MERGE_FAN_IN = 128;

void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

char *trim(char *value) {
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    char *end = value + std::strlen(value);
    while (end > value &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return value;
}

bool copy_value(char *target, size_t target_size, const char *value) {
    if (std::strlen(value) >= target_size) {
        return false;
    }
    std::snprintf(target, target_size, "%s", value);
    return true;
}

bool parse_positive_size(const char *value, size_t *result) {
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno != 0 || end == value || *trim(end) != '\0' || parsed == 0 ||
        parsed > SIZE_MAX) {
        return false;
    }
    *result = static_cast<size_t>(parsed);
    return true;
}

Config load_config(const char *path) {
    Config config{};
    copy_value(config.uri, sizeof(config.uri), "mongodb://localhost:27017");
    copy_value(config.database, sizeof(config.database), "mai_ir");
    copy_value(config.terms_collection, sizeof(config.terms_collection),
               "document_terms");
    copy_value(config.output_directory, sizeof(config.output_directory),
               "results/boolean_index");
    copy_value(config.term_field, sizeof(config.term_field), "lemmas");
    config.block_postings = 500000;
    config.progress_every_documents = 1000;

    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fail("не удалось открыть config.yaml");
    }
    char line[8192];
    char section[128] = "";
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char *comment = std::strchr(line, '#');
        if (comment != nullptr) {
            *comment = '\0';
        }
        const size_t indentation = std::strspn(line, " \t");
        char *content = trim(line);
        if (*content == '\0' || *content == '-') {
            continue;
        }
        char *colon = std::strchr(content, ':');
        if (colon == nullptr) {
            continue;
        }
        *colon = '\0';
        char *key = trim(content);
        char *value = trim(colon + 1);
        if (indentation == 0 && *value == '\0') {
            if (!copy_value(section, sizeof(section), key)) {
                fail("слишком длинное имя раздела конфигурации");
            }
            continue;
        }
        bool valid = true;
        if (std::strcmp(section, "db") == 0) {
            if (std::strcmp(key, "uri") == 0) {
                valid = copy_value(config.uri, sizeof(config.uri), value);
            } else if (std::strcmp(key, "database") == 0) {
                valid = copy_value(config.database, sizeof(config.database), value);
            } else if (std::strcmp(key, "document_terms_collection") == 0) {
                valid = copy_value(config.terms_collection,
                                   sizeof(config.terms_collection), value);
            }
        } else if (std::strcmp(section, "boolean_index") == 0) {
            if (std::strcmp(key, "output_directory") == 0) {
                valid = copy_value(config.output_directory,
                                   sizeof(config.output_directory), value);
            } else if (std::strcmp(key, "term_field") == 0) {
                valid = copy_value(config.term_field, sizeof(config.term_field), value);
            } else if (std::strcmp(key, "block_postings") == 0) {
                valid = parse_positive_size(value, &config.block_postings);
            } else if (std::strcmp(key, "progress_every_documents") == 0) {
                valid = parse_positive_size(value,
                                            &config.progress_every_documents);
            }
        }
        if (!valid) {
            std::fclose(file);
            fail("некорректное значение в config.yaml");
        }
    }
    std::fclose(file);
    if ((std::strcmp(config.term_field, "lemmas") != 0 &&
         std::strcmp(config.term_field, "tokens") != 0) ||
        config.output_directory[0] == '\0') {
        fail("term_field должен быть lemmas или tokens");
    }
    return config;
}

void make_directories(const char *path) {
    char buffer[PATH_SIZE];
    if (!copy_value(buffer, sizeof(buffer), path)) {
        fail("слишком длинный путь индекса");
    }
    for (char *cursor = buffer + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
                fail("не удалось создать каталог индекса");
            }
            *cursor = '/';
        }
    }
    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        fail("не удалось создать каталог индекса");
    }
}

void join_path(char *target, size_t target_size, const char *directory,
               const char *name) {
    const size_t directory_size = std::strlen(directory);
    const size_t name_size = std::strlen(name);
    if (directory_size + name_size + 2 > target_size) {
        fail("слишком длинный путь файла индекса");
    }
    std::memcpy(target, directory, directory_size);
    target[directory_size] = '/';
    std::memcpy(target + directory_size + 1, name, name_size + 1);
}

char *duplicate_text(const char *text, size_t size) {
    char *copy = static_cast<char *>(std::malloc(size + 1));
    if (copy != nullptr) {
        std::memcpy(copy, text, size);
        copy[size] = '\0';
    }
    return copy;
}

void posting_buffer_destroy(PostingBuffer *buffer) {
    for (size_t index = 0; index < buffer->size; ++index) {
        std::free(buffer->items[index].term);
    }
    std::free(buffer->items);
    *buffer = {};
}

bool posting_buffer_add(PostingBuffer *buffer, const char *term, size_t term_size,
                        uint32_t document_id) {
    if (buffer->size == buffer->capacity) {
        size_t capacity = buffer->capacity == 0 ? 4096 : buffer->capacity * 2;
        PostingPair *items = static_cast<PostingPair *>(
            std::realloc(buffer->items, capacity * sizeof(PostingPair)));
        if (items == nullptr) {
            return false;
        }
        buffer->items = items;
        buffer->capacity = capacity;
    }
    char *copy = duplicate_text(term, term_size);
    if (copy == nullptr) {
        return false;
    }
    buffer->items[buffer->size++] = {copy, document_id};
    return true;
}

int compare_pairs(const void *first, const void *second) {
    const PostingPair *left = static_cast<const PostingPair *>(first);
    const PostingPair *right = static_cast<const PostingPair *>(second);
    const int comparison = std::strcmp(left->term, right->term);
    if (comparison != 0) {
        return comparison;
    }
    if (left->document_id < right->document_id) {
        return -1;
    }
    return left->document_id > right->document_id ? 1 : 0;
}

void block_list_add(BlockList *blocks, char *path) {
    if (blocks->size == blocks->capacity) {
        const size_t capacity = blocks->capacity == 0 ? 16 : blocks->capacity * 2;
        char **paths = static_cast<char **>(
            std::realloc(blocks->paths, capacity * sizeof(char *)));
        if (paths == nullptr) {
            fail("недостаточно памяти для списка блоков");
        }
        blocks->paths = paths;
        blocks->capacity = capacity;
    }
    blocks->paths[blocks->size++] = path;
}

void flush_block(PostingBuffer *buffer, BlockList *blocks,
                 const char *temporary_directory) {
    if (buffer->size == 0) {
        return;
    }
    std::qsort(buffer->items, buffer->size, sizeof(PostingPair), compare_pairs);
    char name[64];
    std::snprintf(name, sizeof(name), "block_%06zu.bin", blocks->size);
    char path[PATH_SIZE];
    join_path(path, sizeof(path), temporary_directory, name);
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        fail("не удалось создать временный блок");
    }
    const char *previous_term = nullptr;
    uint32_t previous_document = 0;
    for (size_t index = 0; index < buffer->size; ++index) {
        const PostingPair *pair = &buffer->items[index];
        if (previous_term != nullptr && std::strcmp(previous_term, pair->term) == 0 &&
            previous_document == pair->document_id) {
            continue;
        }
        const size_t length = std::strlen(pair->term);
        if (length > UINT32_MAX) {
            fail("слишком длинный термин");
        }
        const uint32_t stored_length = static_cast<uint32_t>(length);
        if (std::fwrite(&stored_length, sizeof(stored_length), 1, file) != 1 ||
            std::fwrite(pair->term, 1, length, file) != length ||
            std::fwrite(&pair->document_id, sizeof(pair->document_id), 1, file) != 1) {
            fail("ошибка записи временного блока");
        }
        previous_term = pair->term;
        previous_document = pair->document_id;
    }
    if (std::fclose(file) != 0) {
        fail("ошибка закрытия временного блока");
    }
    block_list_add(blocks, duplicate_text(path, std::strlen(path)));
    posting_buffer_destroy(buffer);
    std::fprintf(stderr, "Индекс: записан блок %zu\n", blocks->size);
}

bool add_document_terms(const bson_t *document, const char *field,
                        uint32_t document_id, PostingBuffer *buffer) {
    bson_iter_t field_iterator;
    if (!bson_iter_init_find(&field_iterator, document, field) ||
        !BSON_ITER_HOLDS_ARRAY(&field_iterator)) {
        return false;
    }
    bson_iter_t terms;
    if (!bson_iter_recurse(&field_iterator, &terms)) {
        return false;
    }
    while (bson_iter_next(&terms)) {
        if (!BSON_ITER_HOLDS_ARRAY(&terms)) {
            return false;
        }
        bson_iter_t pair;
        if (!bson_iter_recurse(&terms, &pair) || !bson_iter_next(&pair) ||
            !BSON_ITER_HOLDS_UTF8(&pair)) {
            return false;
        }
        uint32_t length = 0;
        const char *term = bson_iter_utf8(&pair, &length);
        if (length != 0 && !posting_buffer_add(buffer, term, length, document_id)) {
            fail("недостаточно памяти для блока индекса");
        }
    }
    return true;
}

bool block_reader_next(BlockReader *reader) {
    std::free(reader->term);
    reader->term = nullptr;
    uint32_t length = 0;
    if (std::fread(&length, sizeof(length), 1, reader->file) != 1) {
        reader->active = false;
        return std::feof(reader->file) != 0;
    }
    reader->term = static_cast<char *>(std::malloc(static_cast<size_t>(length) + 1));
    if (reader->term == nullptr ||
        std::fread(reader->term, 1, length, reader->file) != length ||
        std::fread(&reader->document_id, sizeof(reader->document_id), 1,
                   reader->file) != 1) {
        reader->active = false;
        return false;
    }
    reader->term[length] = '\0';
    reader->active = true;
    return true;
}

bool reader_before(const ReaderHeap *heap, size_t first, size_t second) {
    const BlockReader *left = &heap->readers[first];
    const BlockReader *right = &heap->readers[second];
    const int comparison = std::strcmp(left->term, right->term);
    return comparison < 0 ||
           (comparison == 0 && left->document_id < right->document_id) ||
           (comparison == 0 && left->document_id == right->document_id &&
            first < second);
}

void heap_sift_down(ReaderHeap *heap, size_t position) {
    while (true) {
        const size_t left = position * 2 + 1;
        const size_t right = left + 1;
        size_t smallest = position;
        if (left < heap->size &&
            reader_before(heap, heap->items[left], heap->items[smallest])) {
            smallest = left;
        }
        if (right < heap->size &&
            reader_before(heap, heap->items[right], heap->items[smallest])) {
            smallest = right;
        }
        if (smallest == position) {
            return;
        }
        const size_t temporary = heap->items[position];
        heap->items[position] = heap->items[smallest];
        heap->items[smallest] = temporary;
        position = smallest;
    }
}

void merge_raw_group(char **input_paths, size_t input_count,
                     const char *output_path) {
    FILE *output = std::fopen(output_path, "wb");
    BlockReader *readers = static_cast<BlockReader *>(
        std::calloc(input_count, sizeof(BlockReader)));
    size_t *heap_items = static_cast<size_t *>(
        std::malloc(input_count * sizeof(size_t)));
    if (output == nullptr || readers == nullptr || heap_items == nullptr) {
        fail("не удалось подготовить промежуточное слияние");
    }
    ReaderHeap heap{heap_items, 0, readers};
    for (size_t index = 0; index < input_count; ++index) {
        readers[index].file = std::fopen(input_paths[index], "rb");
        if (readers[index].file == nullptr || !block_reader_next(&readers[index])) {
            fail("не удалось прочитать блок при промежуточном слиянии");
        }
        if (readers[index].active) {
            heap.items[heap.size++] = index;
        }
    }
    if (heap.size > 1) {
        for (size_t position = heap.size / 2; position > 0; --position) {
            heap_sift_down(&heap, position - 1);
        }
    }
    char *previous_term = nullptr;
    uint32_t previous_document = 0;
    bool has_previous = false;
    while (heap.size != 0) {
        const size_t reader_index = heap.items[0];
        BlockReader *reader = &readers[reader_index];
        if (!has_previous || std::strcmp(previous_term, reader->term) != 0 ||
            previous_document != reader->document_id) {
            const size_t length = std::strlen(reader->term);
            if (length > UINT32_MAX) {
                fail("слишком длинный термин при слиянии");
            }
            const uint32_t stored_length = static_cast<uint32_t>(length);
            if (std::fwrite(&stored_length, sizeof(stored_length), 1, output) != 1 ||
                std::fwrite(reader->term, 1, length, output) != length ||
                std::fwrite(&reader->document_id, sizeof(reader->document_id), 1,
                            output) != 1) {
                fail("ошибка записи промежуточного блока");
            }
            std::free(previous_term);
            previous_term = duplicate_text(reader->term, length);
            if (previous_term == nullptr) {
                fail("недостаточно памяти при слиянии блоков");
            }
            previous_document = reader->document_id;
            has_previous = true;
        }
        if (!block_reader_next(reader)) {
            fail("повреждён блок при промежуточном слиянии");
        }
        if (reader->active) {
            heap_sift_down(&heap, 0);
        } else {
            heap.items[0] = heap.items[--heap.size];
            if (heap.size != 0) {
                heap_sift_down(&heap, 0);
            }
        }
    }
    std::free(previous_term);
    for (size_t index = 0; index < input_count; ++index) {
        std::free(readers[index].term);
        std::fclose(readers[index].file);
    }
    std::free(readers);
    std::free(heap_items);
    if (std::fclose(output) != 0) {
        fail("ошибка закрытия промежуточного блока");
    }
}

void reduce_blocks(BlockList *blocks, const char *temporary_directory) {
    size_t pass = 0;
    while (blocks->size > MERGE_FAN_IN) {
        BlockList reduced{};
        for (size_t begin = 0; begin < blocks->size; begin += MERGE_FAN_IN) {
            const size_t remaining = blocks->size - begin;
            const size_t count = remaining < MERGE_FAN_IN
                                     ? remaining
                                     : MERGE_FAN_IN;
            char name[96];
            std::snprintf(name, sizeof(name), "merge_%03zu_%06zu.bin", pass,
                          reduced.size);
            char path[PATH_SIZE];
            join_path(path, sizeof(path), temporary_directory, name);
            merge_raw_group(blocks->paths + begin, count, path);
            char *stored_path = duplicate_text(path, std::strlen(path));
            if (stored_path == nullptr) {
                fail("недостаточно памяти для промежуточного блока");
            }
            block_list_add(&reduced, stored_path);
            for (size_t index = begin; index < begin + count; ++index) {
                unlink(blocks->paths[index]);
                std::free(blocks->paths[index]);
            }
        }
        std::free(blocks->paths);
        *blocks = reduced;
        ++pass;
        std::fprintf(stderr,
                     "Индекс: промежуточное слияние, осталось блоков %zu\n",
                     blocks->size);
    }
}

bool write_lexicon_entry(FILE *lexicon, uint64_t term_offset,
                         uint64_t postings_offset, uint32_t term_length,
                         uint32_t postings_count) {
    const boolean_index_format::LexiconEntry entry{
        term_offset, postings_offset, term_length, postings_count};
    return std::fwrite(&entry, sizeof(entry), 1, lexicon) == 1;
}

void merge_blocks(const BlockList *blocks, const char *output_directory,
                  uint32_t document_count) {
    char lexicon_path[PATH_SIZE];
    char terms_path[PATH_SIZE];
    char postings_path[PATH_SIZE];
    join_path(lexicon_path, sizeof(lexicon_path), output_directory,
              "lexicon.bin.tmp");
    join_path(terms_path, sizeof(terms_path), output_directory, "terms.bin.tmp");
    join_path(postings_path, sizeof(postings_path), output_directory,
              "postings.bin.tmp");
    FILE *lexicon = std::fopen(lexicon_path, "wb+");
    FILE *terms = std::fopen(terms_path, "wb");
    FILE *postings = std::fopen(postings_path, "wb");
    if (lexicon == nullptr || terms == nullptr || postings == nullptr) {
        fail("не удалось создать итоговые файлы индекса");
    }
    boolean_index_format::IndexHeader header{};
    std::memcpy(header.magic, boolean_index_format::INDEX_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = document_count;
    if (std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось записать заголовок индекса");
    }

    BlockReader *readers = static_cast<BlockReader *>(
        std::calloc(blocks->size, sizeof(BlockReader)));
    size_t *heap_items = static_cast<size_t *>(
        std::malloc(blocks->size * sizeof(size_t)));
    if ((blocks->size != 0) && (readers == nullptr || heap_items == nullptr)) {
        fail("недостаточно памяти для слияния блоков");
    }
    ReaderHeap heap{heap_items, 0, readers};
    for (size_t index = 0; index < blocks->size; ++index) {
        readers[index].file = std::fopen(blocks->paths[index], "rb");
        if (readers[index].file == nullptr || !block_reader_next(&readers[index])) {
            fail("не удалось прочитать временный блок");
        }
        if (readers[index].active) {
            heap.items[heap.size++] = index;
        }
    }
    if (heap.size > 1) {
        for (size_t position = heap.size / 2; position > 0; --position) {
            heap_sift_down(&heap, position - 1);
        }
    }

    char *current_term = nullptr;
    uint32_t current_length = 0;
    uint64_t current_term_offset = 0;
    uint64_t current_postings_offset = 0;
    uint32_t current_postings_count = 0;
    uint32_t last_document = 0;
    bool has_last_document = false;
    while (heap.size != 0) {
        const size_t reader_index = heap.items[0];
        BlockReader *reader = &readers[reader_index];
        const size_t length = std::strlen(reader->term);
        if (current_term == nullptr || std::strcmp(current_term, reader->term) != 0) {
            if (current_term != nullptr &&
                !write_lexicon_entry(lexicon, current_term_offset,
                                      current_postings_offset, current_length,
                                      current_postings_count)) {
                fail("ошибка записи лексикона");
            }
            std::free(current_term);
            current_term = duplicate_text(reader->term, length);
            if (current_term == nullptr || length > UINT32_MAX) {
                fail("недостаточно памяти при слиянии");
            }
            current_length = static_cast<uint32_t>(length);
            current_term_offset = static_cast<uint64_t>(ftello(terms));
            current_postings_offset = static_cast<uint64_t>(ftello(postings));
            current_postings_count = 0;
            has_last_document = false;
            if (std::fwrite(current_term, 1, length, terms) != length) {
                fail("ошибка записи словаря");
            }
            ++header.term_count;
        }
        if (!has_last_document || last_document != reader->document_id) {
            if (std::fwrite(&reader->document_id, sizeof(reader->document_id), 1,
                            postings) != 1) {
                fail("ошибка записи списка документов");
            }
            last_document = reader->document_id;
            has_last_document = true;
            ++current_postings_count;
            ++header.posting_count;
        }

        if (!block_reader_next(reader)) {
            fail("повреждён временный блок");
        }
        if (reader->active) {
            heap_sift_down(&heap, 0);
        } else {
            heap.items[0] = heap.items[--heap.size];
            if (heap.size != 0) {
                heap_sift_down(&heap, 0);
            }
        }
    }
    if (current_term != nullptr &&
        !write_lexicon_entry(lexicon, current_term_offset, current_postings_offset,
                              current_length, current_postings_count)) {
        fail("ошибка записи последнего термина лексикона");
    }
    std::free(current_term);
    if (fseeko(lexicon, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось обновить заголовок индекса");
    }
    for (size_t index = 0; index < blocks->size; ++index) {
        std::free(readers[index].term);
        std::fclose(readers[index].file);
    }
    std::free(readers);
    std::free(heap_items);
    if (std::fclose(lexicon) != 0 || std::fclose(terms) != 0 ||
        std::fclose(postings) != 0) {
        fail("ошибка закрытия итогового индекса");
    }

    const char *temporary[] = {lexicon_path, terms_path, postings_path};
    const char *names[] = {"lexicon.bin", "terms.bin", "postings.bin"};
    for (size_t index = 0; index < 3; ++index) {
        char final_path[PATH_SIZE];
        join_path(final_path, sizeof(final_path), output_directory, names[index]);
        if (rename(temporary[index], final_path) != 0) {
            fail("не удалось заменить старый индекс новым");
        }
    }
    std::fprintf(stderr,
                 "Индекс готов: документов %u, терминов %llu, пар термин-документ %llu\n",
                 header.document_count,
                 static_cast<unsigned long long>(header.term_count),
                 static_cast<unsigned long long>(header.posting_count));
}

void finalize_documents(FILE *documents, const char *temporary_path,
                        const char *output_directory, uint32_t count) {
    boolean_index_format::DocumentHeader header{};
    std::memcpy(header.magic, boolean_index_format::DOCUMENT_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = count;
    if (fseeko(documents, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, documents) != 1 ||
        std::fclose(documents) != 0) {
        fail("не удалось завершить таблицу документов");
    }
    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), output_directory, "documents.bin");
    if (rename(temporary_path, final_path) != 0) {
        fail("не удалось сохранить таблицу документов");
    }
}

void build_index(const Config *config) {
    make_directories(config->output_directory);
    char temporary_template[PATH_SIZE];
    join_path(temporary_template, sizeof(temporary_template),
              config->output_directory, "blocks-XXXXXX");
    if (mkdtemp(temporary_template) == nullptr) {
        fail("не удалось создать временный каталог");
    }
    char documents_path[PATH_SIZE];
    join_path(documents_path, sizeof(documents_path), config->output_directory,
              "documents.bin.tmp");
    FILE *documents = std::fopen(documents_path, "wb+");
    if (documents == nullptr) {
        fail("не удалось создать таблицу документов");
    }
    boolean_index_format::DocumentHeader empty_header{};
    if (std::fwrite(&empty_header, sizeof(empty_header), 1, documents) != 1) {
        fail("не удалось записать таблицу документов");
    }

    mongoc_client_t *client = mongoc_client_new(config->uri);
    if (client == nullptr) {
        fail("не удалось создать клиент MongoDB");
    }
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config->database, config->terms_collection);
    bson_t filter = BSON_INITIALIZER;
    bson_t options = BSON_INITIALIZER;
    bson_t projection;
    bson_t sort;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    BSON_APPEND_INT32(&projection, config->term_field, 1);
    bson_append_document_end(&options, &projection);
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    BSON_APPEND_INT32(&sort, "_id", 1);
    bson_append_document_end(&options, &sort);
    BSON_APPEND_INT32(&options, "batchSize", 100);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);

    PostingBuffer buffer{};
    BlockList blocks{};
    uint32_t document_count = 0;
    const bson_t *document = nullptr;
    while (mongoc_cursor_next(cursor, &document)) {
        bson_iter_t id_iterator;
        if (!bson_iter_init_find(&id_iterator, document, "_id") ||
            !BSON_ITER_HOLDS_OID(&id_iterator)) {
            fail("document_terms содержит документ без ObjectId");
        }
        if (document_count == UINT32_MAX) {
            fail("число документов превышает формат индекса");
        }
        const bson_oid_t *oid = bson_iter_oid(&id_iterator);
        if (std::fwrite(oid->bytes, 1, sizeof(oid->bytes), documents) !=
            sizeof(oid->bytes)) {
            fail("ошибка записи идентификатора документа");
        }
        if (!add_document_terms(document, config->term_field, document_count,
                                &buffer)) {
            fail("некорректный массив терминов в document_terms");
        }
        ++document_count;
        if (buffer.size >= config->block_postings) {
            flush_block(&buffer, &blocks, temporary_template);
        }
        if (document_count % config->progress_every_documents == 0) {
            std::fprintf(stderr, "Индекс: прочитано документов %u\n",
                         document_count);
        }
    }
    bson_error_t error{};
    if (mongoc_cursor_error(cursor, &error)) {
        std::fprintf(stderr, "Ошибка MongoDB: %s\n", error.message);
        std::exit(EXIT_FAILURE);
    }
    flush_block(&buffer, &blocks, temporary_template);
    finalize_documents(documents, documents_path, config->output_directory,
                       document_count);
    reduce_blocks(&blocks, temporary_template);
    merge_blocks(&blocks, config->output_directory, document_count);

    for (size_t index = 0; index < blocks.size; ++index) {
        unlink(blocks.paths[index]);
        std::free(blocks.paths[index]);
    }
    std::free(blocks.paths);
    rmdir(temporary_template);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const Config config = load_config(argv[1]);
    mongoc_init();
    build_index(&config);
    mongoc_cleanup();
    return EXIT_SUCCESS;
}
