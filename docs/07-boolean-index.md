# Лабораторная работа 7. Булев индекс

## 1. Что такое булев индекс

Прямое представление документа выглядит так:

```text
document -> terms
```

Для поиска оно неудобно: при каждом запросе пришлось бы читать все документы.
Инвертированный индекс меняет направление:

```text
term -> sorted list of document IDs
```

Пример:

```text
cloud     -> [1, 4, 8, 15]
database  -> [2, 4, 9, 15]
security  -> [1, 2, 15]
```

Список идентификаторов называется posting list. Если списки отсортированы,
пересечение и объединение выполняются линейным проходом.

Код находится в `src/boolean_index`:

- `main.cpp` — чтение MongoDB, блочная сортировка и слияние;
- `format.hpp` — собственный бинарный формат, общий для индексатора и поиска.

## 2. Входные данные

Индексатор читает `document_terms` — только документы, прошедшие порог
лингвистики. По умолчанию используется поле:

```yaml
boolean_index:
  term_field: lemmas
```

Выбор лемм означает, что `queries` и `query` попадают в один posting list.
При необходимости конфигурация допускает `tokens`, но тогда запрос и индекс
должны использовать одинаковую нормализацию.

MongoDB используется только как источник пар «лемма — документ». Готовые MongoDB
text indexes, Elasticsearch или библиотека инвертированного индекса не
используются.

## 3. Зачем нужен внутренний docID

MongoDB ObjectId занимает 12 байт и неудобен для плотных posting lists. При чтении
документов в порядке `_id` каждому назначается последовательный `uint32_t`:

```text
ObjectId(...) -> docID 0
ObjectId(...) -> docID 1
ObjectId(...) -> docID 2
```

В posting list один документ занимает 4 байта. Таблица `documents.bin` хранит
обратное соответствие `docID -> ObjectId`. При выводе результата поисковик получает
ObjectId и читает заголовок с URL из `processed_documents`.

`uint32_t` позволяет адресовать до 4 294 967 295 документов, что достаточно для
условий курса.

## 4. Почему нельзя собрать всё в одном словаре RAM

На текущем корпусе уже есть 73 536 673 пары «лемма — документ». На миллионе
документов их будет в несколько раз больше. Хранить все строки и списки в
`unordered_map` не только запрещено правилами курса, но и не масштабируется по
памяти.

Поэтому используется алгоритм внешней блочной сортировки, близкий по идее к BSBI:

```text
MongoDB stream
    │
    ├─ 500 000 pairs -> sort -> block_000000.bin
    ├─ 500 000 pairs -> sort -> block_000001.bin
    ├─ 500 000 pairs -> sort -> block_000002.bin
    └─ ...
                     │
                     v
             bounded multiway merge
                     │
                     v
             final inverted index
```

Корпус целиком не находится в RAM. Объём памяти ограничивается размером одного
блока плюс буферами слияния.

## 5. Построение временных блоков

Параметр:

```yaml
block_postings: 500000
```

задаёт приблизительное число пар в одном блоке.

Для каждого документа индексатор:

1. записывает его ObjectId в таблицу документов;
2. читает уникальные леммы из `document_terms.lemmas`;
3. добавляет `(term, docID)` в собственный динамический массив;
4. при достижении лимита вызывает `qsort` по термину, затем по `docID`;
5. удаляет соседние дубли;
6. записывает отсортированный временный файл;
7. освобождает память блока.

Формат записи временного блока:

```text
uint32 term_length
byte[term_length] UTF-8 term
uint32 docID
```

Ключевой участок `flush_block` показывает, что сортировка выполняется собственной
структурой и стандартной C-функцией `qsort`, а не STL-контейнером:

```cpp
std::qsort(buffer->items,
           buffer->size,
           sizeof(PostingPair),
           compare_pairs);

for (size_t index = 0; index < buffer->size; ++index) {
    const PostingPair *pair = &buffer->items[index];

    // После сортировки одинаковые пары находятся рядом.
    if (previous_term != nullptr &&
        std::strcmp(previous_term, pair->term) == 0 &&
        previous_document == pair->document_id) {
        continue;
    }

    const uint32_t length =
        static_cast<uint32_t>(std::strlen(pair->term));
    std::fwrite(&length, sizeof(length), 1, file);
    std::fwrite(pair->term, 1, length, file);
    std::fwrite(&pair->document_id,
                sizeof(pair->document_id), 1, file);
}
```

`compare_pairs` сначала сравнивает строки, затем числовой `docID`. Поэтому каждый
временный файл уже имеет тот же логический порядок, что и будущий индекс.

## 6. Многопутевое слияние

Для каждого открытого блока хранится только текущая запись. Собственная бинарная
куча выбирает минимальную пару `(term, docID)`. После чтения следующей записи
элемент просеивается вниз.

Одновременно открывается максимум 128 блоков (`MERGE_FAN_IN`). Если блоков больше,
они сначала группами сливаются в промежуточные блоки. Проходы повторяются, пока
число файлов не станет не больше 128.

Это важно для корпуса более миллиона документов: без многоэтапного слияния можно
превысить системный лимит открытых файлов.

Ограничение fan-in оформлено явно:

```cpp
constexpr size_t MERGE_FAN_IN = 128;

while (blocks->size > MERGE_FAN_IN) {
    BlockList reduced{};

    for (size_t begin = 0;
         begin < blocks->size;
         begin += MERGE_FAN_IN) {
        const size_t remaining = blocks->size - begin;
        const size_t count = remaining < MERGE_FAN_IN
            ? remaining
            : MERGE_FAN_IN;

        char name[96];
        std::snprintf(name, sizeof(name),
                      "merge_%03zu_%06zu.bin", pass, reduced.size);
        char path[PATH_SIZE];
        join_path(path, sizeof(path), temporary_directory, name);
        merge_raw_group(blocks->paths + begin, count, path);

        char *stored_path = duplicate_text(path, std::strlen(path));
        block_list_add(&reduced, stored_path);
    }

    *blocks = reduced;
}
```

После каждого прохода число файлов уменьшается примерно в 128 раз. Для 147 блоков
текущего корпуса понадобился один промежуточный проход: `147 -> 2`.

В финальном проходе:

- термин один раз записывается в `terms.bin`;
- все его уникальные `docID` последовательно записываются в `postings.bin`;
- смещения и размеры записываются в `lexicon.bin`.

## 7. Собственный бинарный формат

### lexicon.bin

Начинается с 32-байтного `IndexHeader`:

| Поле | Тип | Назначение |
|---|---|---|
| magic | 8 bytes | `MAIIRIDX` |
| version | `uint32` | версия формата |
| document_count | `uint32` | число документов |
| term_count | `uint64` | число терминов |
| posting_count | `uint64` | число пар термин–документ |

Далее следуют 24-байтные записи `LexiconEntry`:

| Поле | Тип | Назначение |
|---|---|---|
| term_offset | `uint64` | позиция строки в `terms.bin` |
| postings_offset | `uint64` | позиция списка в `postings.bin` |
| term_length | `uint32` | длина UTF-8 строки |
| postings_count | `uint32` | число `docID` |

Записи отсортированы по термину, поэтому поиск выполняется бинарно без загрузки
лексикона целиком.

Формат закреплён структурами фиксированного размера:

```cpp
#pragma pack(push, 1)
struct IndexHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
    uint64_t term_count;
    uint64_t posting_count;
};

struct LexiconEntry {
    uint64_t term_offset;
    uint64_t postings_offset;
    uint32_t term_length;
    uint32_t postings_count;
};
#pragma pack(pop)

static_assert(sizeof(IndexHeader) == 32);
static_assert(sizeof(LexiconEntry) == 24);
```

`static_assert` не даёт незаметно изменить формат из-за padding компилятора.

### terms.bin

Содержит подряд UTF-8 строки терминов без завершающего `\0`. Их границы задаются
`term_offset` и `term_length` из лексикона.

### postings.bin

Содержит подряд `uint32_t docID`. Для каждого термина список строго возрастает и
не содержит дублей.

### documents.bin

Начинается с 16-байтного заголовка `MAIIRDOC`, затем содержит по 12 байт MongoDB
ObjectId для каждого `docID`.

## 8. Безопасная публикация результата

Итоговые файлы сначала пишутся с расширением `.tmp`. После закрытия и обновления
заголовка они переименовываются в рабочие имена. Старые итоговые файлы не
обрезаются в момент начала долгого построения.

Временные блоки создаются в уникальном каталоге `blocks-XXXXXX` внутри каталога
индекса и удаляются после успешного завершения.

## 9. Сборка и запуск

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
./.build/boolean_index config.yaml
```

Пример финального сообщения:

```text
Индекс готов: документов 151182, терминов 3565769,
пар термин-документ 73536673
```

Файлы создаются в `results/boolean_index` и игнорируются Git.

Проверка размеров:

```bash
du -h results/boolean_index/* | sort -h
```

Текущий результат:

| Файл | Приблизительный размер |
|---|---:|
| `documents.bin` | 1,8 MiB |
| `terms.bin` | 43 MiB |
| `lexicon.bin` | 82 MiB |
| `postings.bin` | 281 MiB |
| Всего | около 408 MiB |

## 10. Карта основных функций

| Функция | Назначение |
|---|---|
| `load_config` | читает только параметры MongoDB и индексатора |
| `add_document_terms` | извлекает термины BSON-документа |
| `posting_buffer_add` | расширяет собственный массив пар |
| `flush_block` | сортирует и записывает один блок |
| `block_reader_next` | потоково читает следующую запись блока |
| `heap_sift_down` | поддерживает собственную min-heap |
| `merge_raw_group` | объединяет не более 128 сырых блоков |
| `reduce_blocks` | выполняет промежуточные проходы |
| `merge_blocks` | создаёт финальные lexicon/terms/postings |
| `finalize_documents` | завершает отображение docID → ObjectId |
| `build_index` | координирует весь процесс |

## 11. Когда перестраивать индекс

Текущая версия строит индекс полностью заново. Её нужно запускать:

- после первого полного лингвистического прогона;
- после добавления и обработки новых документов;
- после изменения `parser_version`;
- после переключения `term_field` между `lemmas` и `tokens`.

Полная перестройка проще для доказательства согласованности: итоговый индекс точно
соответствует текущему `document_terms`. Инкрементальное обновление можно добавить
позже, но оно требует удаления старых posting и ведения поколений индекса.

## 12. Тестирование

`tests/unit/test_boolean_index.cpp` отдельно проверяет бинарный контракт,
извлечение обычных и технических термов из BSON, пустые и некорректные поля,
копирование терма заданной длины, сортировку и удаление дублей во временном
блоке, слияние нескольких блоков, пустой индекс, лексикон, posting lists и
таблицу ObjectId, включая таблицу без документов.

Запуск:

```bash
./.build/boolean_index_unit_tests
```

Реальная интеграционная проверка выполнена на 151 182 документах. При построении
получилось 147 исходных блоков, которые промежуточным проходом были сведены к двум,
после чего создан финальный индекс.

## 13. Что показать преподавателю

1. Схему `term -> sorted docIDs`.
2. Почему ObjectId заменяется на компактный `uint32`.
3. Значение `block_postings` и ограничение памяти.
4. Формат временной записи.
5. Собственную min-heap и fan-in 128.
6. Четыре итоговых файла и их размеры.
7. Заголовки `IndexHeader`, `LexiconEntry`, `DocumentHeader`.
8. Финальные числа терминов и posting.
9. Отсутствие STL-контейнеров и готовых поисковых библиотек.
10. Причину полной, а не инкрементальной перестройки.

## 14. Сложность и ограничения

- Формирование блоков: `O(P log B)`, где `P` — число пар, `B` — размер блока.
- Слияние: примерно `O(P log K)`, где `K <= 128` в одном проходе.
- Диск: строки терминов, 24 байта на термин, 4 байта на posting и 12 байт на
  документ плюс заголовки.

Ограничения текущей версии:

- нет инкрементального добавления posting;
- нет сжатия docID и словаря;
- нет skip pointers;
- нет позиций термина для фразового поиска;
- бинарный формат рассчитан на little-endian машину текущего окружения;
- фактический корпус пока меньше формального миллиона документов.

## 15. Полный ручной пример построения индекса

Пусть после лемматизации есть четыре документа:

```text
D0: cloud database
D1: cloud index
D2: database security
D3: cloud database security
```

### Шаг 1. Назначение docID

```text
Mongo ObjectId A -> 0
Mongo ObjectId B -> 1
Mongo ObjectId C -> 2
Mongo ObjectId D -> 3
```

`documents.bin` после header содержит `A|B|C|D`, каждый по 12 bytes.

### Шаг 2. Создание пар

```text
(cloud, 0)
(database, 0)
(cloud, 1)
(index, 1)
(database, 2)
(security, 2)
(cloud, 3)
(database, 3)
(security, 3)
```

Если `cloud` встретилось в D0 десять раз, пара всё равно одна, потому что
`document_terms` содержит уникальный term с count, а индексатор count не разворачивает.

### Шаг 3. Сортировка

```text
(cloud, 0)
(cloud, 1)
(cloud, 3)
(database, 0)
(database, 2)
(database, 3)
(index, 1)
(security, 2)
(security, 3)
```

### Шаг 4. Финальные списки

```text
cloud    -> [0, 1, 3]
database -> [0, 2, 3]
index    -> [1]
security -> [2, 3]
```

Общее число posting — 9, а не сумма частот слов.

## 16. Как эти данные располагаются в файлах

`terms.bin` содержит bytes без delimiters:

```text
clouddatabaseindexsecurity
```

Offsets:

```text
cloud:    offset=0,  length=5
database: offset=5,  length=8
index:    offset=13, length=5
security: offset=18, length=8
```

`postings.bin` как последовательность uint32:

```text
[0,1,3, 0,2,3, 1, 2,3]
```

При 4 bytes на docID offsets в bytes:

```text
cloud:    postings_offset=0,  count=3
database: postings_offset=12, count=3
index:    postings_offset=24, count=1
security: postings_offset=28, count=2
```

`lexicon.bin` после header содержит:

```text
{term_offset=0,  postings_offset=0,  term_length=5, postings_count=3}
{term_offset=5,  postings_offset=12, term_length=8, postings_count=3}
{term_offset=13, postings_offset=24, term_length=5, postings_count=1}
{term_offset=18, postings_offset=28, term_length=8, postings_count=2}
```

Этого примера достаточно, чтобы вручную объяснить роль каждого поля формата.

## 17. Собственная min-heap подробно

При merge есть несколько readers:

```text
reader 0 current = (cloud, 0)
reader 1 current = (cloud, 3)
reader 2 current = (database, 2)
```

Heap хранит не строки, а индексы readers, например `[0,1,2]`. Comparator сравнивает:

1. `reader.term` через `strcmp`;
2. если term равен — `document_id`;
3. если равны оба — номер reader для детерминизма.

После извлечения root файл не закрывается: reader читает следующую запись, затем
root просеивается вниз. Если файл закончился, root заменяется последним элементом
heap, size уменьшается.

Почему heap лучше линейного поиска минимума:

```text
linear: O(K) на каждый posting -> O(P*K)
heap:   O(log K)              -> O(P log K)
```

При `K<=128` разница существенна на десятках миллионов posting.

## 18. Дедупликация при слиянии

Одинаковая пара может попасть в разные blocks, например если upstream данные
аномальны или граница flush разделила повторы. Merge хранит:

```text
current_term
last_document
has_last_document
```

docID записывается только если term новый или docID отличается. Поэтому финальный
posting строго возрастающий и уникальный независимо от границ blocks.

## 19. Инварианты корректного индекса

После построения должны выполняться:

1. `IndexHeader.document_count == DocumentHeader.document_count`.
2. `term_count` равно числу LexiconEntry.
3. `posting_count` равно сумме `postings_count` entries.
4. Term strings lexicographically strictly increase.
5. Внутри posting list docID strictly increase.
6. Каждый docID `< document_count`.
7. `term_offset + term_length <= size(terms.bin)`.
8. `postings_offset + postings_count*4 <= size(postings.bin)`.
9. В `documents.bin` ровно `document_count*12` bytes после header.

Search проверяет magic/version и согласованность document counts. Более глубокий
offline validator можно добавить как отдельную утилиту будущей работы.

## 20. Оценка объёма

Для текущего индекса:

```text
postings: 73,536,673 * 4 bytes ≈ 280.5 MiB
lexicon:   3,565,769 * 24 bytes ≈ 81.6 MiB
documents:   151,182 * 12 bytes ≈ 1.73 MiB
terms: сумма UTF-8 длин ≈ 43 MiB
```

Числа `du` совпадают с формулой с учётом headers и округления filesystem.

Это объясняет, почему сжатие posting lists — отдельная важная лабораторная:
несжатые uint32 доминируют в размере.

## 21. Оценка памяти блока

`PostingPair` содержит pointer и uint32 с padding, плюс отдельно heap string.
При 500 тысячах pairs память — не просто `500000*12`: есть структура, строки и
allocator overhead. Параметр выбран как компромисс между:

- слишком маленьким block → много files и merge passes;
- слишком большим block → высокий peak RAM.

Архитектура не предполагает точный фиксированный memory limit, но объём растёт от
block size, а не от полного корпуса.

## 22. Что произойдёт при сбое построения

### До финального merge

Новый build пишет blocks и три основных файла во временные пути. Однако
`documents.bin` в текущей последовательности публикуется после чтения MongoDB и до
финального merge. Поэтому предыдущие lexicon/postings ещё могут существовать уже с
новой document map. После любой нештатной остановки build нужно запустить заново до
успешной финальной строки, а search во время build запускать нельзя.

### Во время финальной записи

Могут остаться `.tmp` и временный каталог. Следующий запуск создаст новый уникальный
block directory и перезапишет `.tmp`.

### Во время последовательного rename

Текущая версия не имеет единого manifest generation switch, поэтому запуск search
строго одновременно с финальными rename не гарантирован. Эксплуатационное правило:
не запускать rebuild и search одновременно. Улучшение — generation directories и
атомарный symlink/manifest.

## 23. Альтернативы и почему они не выбраны

### MongoDB aggregation `$unwind/$group`

Проще написать, но структура и сортировка индекса фактически выполнялись бы СУБД,
что противоречит учебной цели.

### Hash map term -> vector<docID>

Не подходит по STL и требует памяти пропорционально всему индексу.

### Сразу писать posting в произвольном порядке

Search потребовал бы hash sets или сортировку при каждом запросе. Один раз
отсортировать при build выгоднее.

### Хранить ObjectId прямо в postings

12 bytes вместо 4, сложнее intersections и примерно тройной объём.

## 24. Контрольные вопросы

### Это BSBI или SPIMI?

Ближе к BSBI: создаются пары term-docID, каждый block сортируется, затем выполняется
external merge. SPIMI обычно строит dictionary/postings внутри блока без сортировки
всех пар.

### Почему lexicon fixed-size, если terms variable-size?

Fixed entry даёт random access для binary search. Строки вынесены отдельно, чтобы
не резервировать максимальную длину для каждого term.

### Почему postings_offset в bytes, а не номер uint32?

`fseeko` работает в bytes; формат явно хранит готовый файловый offset. Оба варианта
возможны, но byte offset уменьшает вычисления и делает тип данных независимее.

### Зачем сортировать documents по `_id`?

Детерминированное назначение docID. При неизменной Mongo collection повторный
build создаёт то же отображение и облегчает сравнение результатов.

### Что ограничивает максимальный индекс?

docID — uint32, значит до ~4.29 млрд документов. Offsets/count totals — uint64,
поэтому файлы могут превышать 4 GiB. Count одного posting list в LexiconEntry —
uint32, что также достаточно до полного пространства docID.


