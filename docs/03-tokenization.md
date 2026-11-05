# Лабораторная работа 3. Токенизация

## 1. Место в конвейере

Токенизация работает не с сетью и не с файлами ноутбуков. Она потоково читает
сырой HTML из `MongoDB.documents`, сначала выделяет основной текст, затем разбивает
его на токены.

```text
documents.raw_html
       │
       v
выбор основного HTML-узла и удаление служебных блоков
       │
       v
нормализация пробелов и Unicode-символов
       │
       v
токенизация
       │
       ├─> processed_documents: text и метрики
       └─> document_terms: частоты токенов документа
```

Код находится в:

- `src/linguistics/main.cpp` — MongoDB, HTML и поток обработки;
- `src/linguistics/core.cpp` — токенизатор и частоты;
- `src/linguistics/core.hpp` — публичные структуры и функции.

Компонент написан на C++20. STL-контейнеры не используются.

## 2. Сборка и запуск

```bash
docker compose up -d
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
./.build/linguistics config.yaml
```

Остановка — `Ctrl+C`. Программа заканчивает текущий пакет, сохраняет согласованное
состояние, освобождает блокировку и возвращает код 130.

Все лингвистические тесты:

```bash
ctest --test-dir .build --output-on-failure
```

Только токенизация:

```bash
./.build/linguistics_tests tokenization
```

## 3. Выделение текста

Для HTML используется `libxml2` в recovery-режиме с запретом сетевых обращений
парсера (`HTML_PARSE_NONET`). Для каждого источника выбирается свой основной узел:

| Источник | Основной XPath |
|---|---|
| Wikipedia | `//*[@id='mw-content-text']` |
| arXiv | `//article` с классом `ltx_document` |
| Microsoft Learn | `//main` |

Если специализированный узел не найден, используется `main`, затем корневой узел
документа.

У всех источников удаляются `script`, `style`, `nav`, `aside`, `footer`, `header`,
`form`, `svg`, `noscript`, `template`, `button`.

Дополнительные правила:

- Wikipedia: `mw-editsection`, `navbox`, `vertical-navbox`, `reflist`, `toc`;
- arXiv: bibliography, page footer и MathML `annotation`;
- Microsoft Learn: feedback, contributors, recommendations и metadata.

После `xmlNodeGetContent` последовательности обычных и Unicode-пробелов
схлопываются в один ASCII-пробел.

Выбор корня в реальном C++-коде:

```cpp
const char *root_xpath = "//main";

if (std::strcmp(source, "wikipedia") == 0) {
    root_xpath = "//*[@id='mw-content-text']";
} else if (std::strcmp(source, "arxiv") == 0) {
    root_xpath =
        "//article[contains(concat(' ', normalize-space(@class), ' '), "
        "' ltx_document ')]";
}

xmlNodePtr root = first_xpath_node(context, root_xpath);
if (root == nullptr && std::strcmp(root_xpath, "//main") != 0) {
    root = first_xpath_node(context, "//main");
}
if (root == nullptr) {
    root = xmlDocGetRootElement(document);
}
```

Здесь видна цепочка fallback: специфичный контейнер → `main` → весь документ.
Пустой текст из-за небольшого изменения HTML поэтому менее вероятен.

## 4. Правила токенизации

Перед разбиением выполняется нормализация:

- ASCII приводится к нижнему регистру;
- типографские апострофы `’` и `‘` заменяются на `'`;
- типографские дефисы `‐`, `‑`, `‒`, `–`, `—` заменяются на `-`;
- full-width ASCII приводится к обычному ASCII;
- Unicode-пробелы приводятся к пробелу.

Токен состоит из букв или цифр и может содержать внутренние символы `.`, `_`,
`'`, `-`, если после них идёт следующий буквенно-цифровой символ.

Специально сохраняются технические формы:

- `C++` → `c++`;
- `C#` → `c#`;
- `.NET` → `.net`;
- `Node.js` → `node.js`;
- `state-of-the-art`;
- `snake_case`;
- `sha-256`;
- `3.14`.

Английское притяжательное окончание удаляется: `model's` → `model`.
Токены длиннее `maximum_token_length: 100` символов отбрасываются как шум.
Стоп-слова не удаляются. Это осознанно: полный словарь нужен для статистики и
будущего фразового поиска, а решение о стоп-словах лучше принимать на уровне
конкретного поискового алгоритма.

Ключевой цикл сохраняет внутреннюю пунктуацию только тогда, когда после неё идёт
продолжение слова:

```cpp
if (character_width == 1 && inner_separator(text[position]) &&
    position + 1 < text_size) {
    size_t following_width = 1;
    if (word_character(
            reinterpret_cast<const unsigned char *>(text + position + 1),
            text_size - position - 1,
            &following_width)) {
        token[output_size++] = text[position++];
        ++characters;
        continue;
    }
}
```

Поэтому точка внутри `node.js` сохраняется, а точка в конце предложения — нет.
Отдельная проверка после цикла добавляет `++` и `#`, что сохраняет `c++` и `c#`.

## 5. Частоты внутри документа

Токены временно хранятся в собственном динамическом массиве `WordArray`.
Вместо `std::vector` используется `malloc/realloc`.

После токенизации:

1. массив сортируется через `qsort`;
2. одинаковые соседние строки объединяются;
3. создаётся `FrequencyArray` из пар `(term, count)`.

Пример представления в BSON:

```javascript
tokens: [
  ["database", 7],
  ["index", 12],
  ["query", 4]
]
```

Массив уже отсортирован по термину. Это удобно для восстановления частот и
построения последующего индекса.

## 6. Потоковая обработка корпуса

Программа читает `documents` MongoDB cursor с сортировкой по `_id` и
`batchSize: 100`. В памяти находится только текущий пакет и данные текущих
документов, а не весь корпус.

Для каждого документа:

1. проверяется `content_hash` и `parser_version`;
2. неизменившийся результат пропускается;
3. из HTML выделяется текст;
4. выполняется токенизация и лемматизация;
5. документ с числом токенов не меньше `minimum_words: 200` принимается;
6. короткий документ помечается `accepted: false` и `too_short`;
7. результаты пакета записываются bulk-операциями.

`parser_version` — версия алгоритма обработки, сейчас `7`. Если изменить правила
извлечения или токенизации, номер нужно увеличить. Тогда даже при неизменившемся
HTML документы будут обработаны заново.

## 7. Что сохраняется

В `processed_documents`:

- исходные `source`, `source_id`, `title`, `url`, `content_hash`;
- `parser_version`;
- `accepted` и причина отказа;
- выделенный `text` только для принятого документа;
- `text_size`, `word_count`, `token_count`;
- суммарное число символов токенов;
- число токенов, изменённых лемматизацией;
- время extraction, tokenization и lemmatization;
- `processed_at`.

В `document_terms` для принятого документа:

- тот же `_id`, что у сырого документа;
- источник и `source_id`;
- `content_hash`;
- массив `tokens`;
- массив `lemmas`.

Для отклонённого документа запись `document_terms` удаляется, чтобы он не попал в
глобальные частоты и поисковый индекс.

Фильтр актуальности не допускает лишней повторной обработки:

```cpp
static bool document_is_current(const Document *document,
                                const Config *config) {
    return document->processed != nullptr &&
           integer_field(document->processed, "parser_version") ==
               config->parser_version &&
           std::strcmp(
               utf8_field(document->processed, "content_hash"),
               utf8_field(document->raw, "content_hash")) == 0;
}
```

Обе части обязательны: хеш отвечает за изменение документа, версия — за изменение
нашего кода обработки.

## 8. Текущие результаты

Из 170 393 обработанных сырых документов принято 151 182 и отклонено 19 211.

| Источник | Принято документов | Токенов | Средняя длина токена |
|---|---:|---:|---:|
| arXiv | 11 033 | 78 780 952 | 5,46 |
| Microsoft Learn | 65 666 | 187 154 782 | 6,26 |
| Wikipedia | 74 483 | 130 894 651 | 5,24 |
| Всего | 151 182 | 396 830 385 | 5,77 |

Общий размер выделенного текста — 2 848 473 808 байт. Измеренная скорость
токенизации — около 75 694 KiB/s. Время относится к CPU-части токенизации и не
включает весь MongoDB I/O.

## 9. Проверка результата

Пример обработанного документа:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d = db.getSiblingDB("mai_ir");
printjson(d.processed_documents.findOne({accepted: true}, {
  source: 1, source_id: 1, title: 1, url: 1,
  text_size: 1, token_count: 1, parser_version: 1, accepted: 1
}));'
```

Первые токены документа:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d = db.getSiblingDB("mai_ir");
printjson(d.document_terms.findOne({}, {tokens: {$slice: 10}}));'
```

Число принятых и отклонённых документов:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d = db.getSiblingDB("mai_ir");
printjson(d.processed_documents.aggregate([
  {$group: {_id: {source: "$source", accepted: "$accepted"}, n: {$sum: 1}}},
  {$sort: {"_id.source": 1, "_id.accepted": 1}}
]).toArray());'
```

## 10. Тестирование

Проверяются:

- `C++`, `.NET`, дефисы, точки и притяжательное окончание;
- Unicode-дефисы, апострофы и пробелы;
- отбрасывание слишком длинного токена;
- извлечение полезного текста и удаление шума у всех трёх источников;
- параметры YAML и относительные пути;
- отсутствие запрещённых STL-заголовков.

## 11. Что показать преподавателю

1. XPath-правила для трёх источников.
2. Тест с `C++`, `.NET`, `state-of-the-art`, `model's`.
3. Один `processed_documents` с размером и числом токенов.
4. Его массив `tokens` в `document_terms`.
5. Пакетную обработку и проверку `content_hash + parser_version`.
6. `results/statistics.json` и скорость токенизации.
7. Тест `./.build/linguistics_tests tokenization`.

## 12. Ограничения

- Правила ориентированы главным образом на английский технический текст.
- Unicode-буквы сохраняются, но не приводятся к нижнему регистру без отдельной
  Unicode-библиотеки.
- Стоп-слова и HTML-entities вроде `lt`/`gt` остаются в словаре.
- При изменении структуры сайта XPath может потребовать обновления и увеличения
  `parser_version`.

## 13. Пошаговая токенизация одной строки

Вход:

```text
Node.js model’s state‑of‑the‑art API uses C++ version 3.14.
```

### Нормализация Unicode

```text
model’s              -> model's
state‑of‑the‑art     -> state-of-the-art
```

ASCII uppercase переводится в lower case уже при накоплении токена.

### Поиск начала токена

Цикл пропускает разделители, пока `word_character` не вернёт true. Исключение —
точка перед словом: `.NET` начинается с точки, если следующий символ word.

### Накопление

```text
Node.js
^^^^  word chars
    ^ inner dot preserved because 'j' follows

state-of-the-art
     ^  ^   ^ inner hyphens preserved
```

### Специальный suffix

После чтения `c` основной цикл видит `+` как не-word, но специальная ветка
добавляет два плюса. Аналогично один `#`.

### Possessive

Если последние bytes токена — `'s`, они удаляются.

### Итог

```text
node.js
model
state-of-the-art
api
uses
c++
version
3.14
```

После лемматизации `uses` станет `use`, остальные технические формы сохранятся.

## 14. UTF-8 и понятие длины

UTF-8 символ занимает 1–4 bytes. `decode_utf8` возвращает code point и width.
Токенизатор отдельно считает:

- `output_size` — bytes в буфере;
- `characters` — число code points для ограничения длины;
- `token_characters` — суммарная длина принятых токенов для статистики.

Это важно: токен из 80 не-ASCII символов может занимать больше 100 bytes, но его
логическая длина остаётся 80 символов.

Некорректная UTF-8 последовательность обрабатывается безопасно по width=1, а не
читает память за границей входа.

## 15. Внутренние структуры памяти

Упрощённо:

```cpp
struct WordArray {
    char **items;     // указатели на отдельные строки
    size_t size;      // занятые элементы
    size_t capacity;  // выделенные элементы
};

struct TermFrequency {
    char *term;
    uint64_t count;
};

struct FrequencyArray {
    TermFrequency *items;
    size_t size;
};
```

При заполнении capacity удваивается. Это даёт амортизированное `O(1)` добавление.
После `qsort` легко посчитать runs одинаковых строк.

Почему строки копируются: token buffer переиспользуется для следующего слова, а
результат должен жить до конца документа.

Почему `linguistics_result_destroy` обязателен: каждый term владеет выделенной
строкой, затем освобождаются arrays. Без destroy на сотнях тысяч документов была
бы утечка памяти.

## 16. Почему frequency array, а не последовательность позиций

Текущие задачи требуют:

- глобальные частоты;
- закон Ципфа;
- булево присутствие термина;
- будущий TF-IDF.

Для них достаточно `(term, count)`. Позиции каждого вхождения значительно
увеличили бы объём. Но без позиций нельзя сделать точный phrase search — это
осознанное ограничение текущего этапа.

## 17. Границы токенов: таблица примеров

| Вход | Токены | Причина |
|---|---|---|
| `hello,world` | `hello`, `world` | comma — separator |
| `node.js` | `node.js` | inner dot |
| `.NET` | `.net` | leading dot before word |
| `3.14` | `3.14` | inner dot between digits |
| `file_name` | `file_name` | underscore preserved |
| `client-server` | `client-server` | inner hyphen |
| `model's` | `model` | possessive removed |
| `users'` | `users` и separator | terminal apostrophe not inner |
| `C++` | `c++` | special suffix |
| `C#` | `c#` | special suffix |
| `a/b` | `a`, `b` | slash separator |
| `foo::bar` | `foo`, `bar` | colon separator |

## 18. Сложность обработки документа

Пусть `T` — bytes текста, `W` — число токенов.

- normalization: `O(T)`;
- token scan: `O(T)`;
- lemmatization: примерно `O(W log D)` для dictionary binary search;
- sorting tokens: `O(W log W)`;
- aggregation runs: `O(W)`.

Основная память: normalized text, token strings и lemma strings одного документа.
Корпус целиком не хранится.

## 19. Что происходит с пустым или плохим документом

```text
HTML parser failed / root empty
 -> text=""
 -> token_count=0
 -> accepted=false
 -> rejection_reason="too_short"
 -> text unset
 -> document_terms removed
```

Документ не исчезает бесследно: в `processed_documents` остаются source, URL,
размер и причина. Это позволяет найти систематическую ошибку XPath по источнику.

Полезный запрос диагностики:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d=db.getSiblingDB("mai_ir");
printjson(d.processed_documents.aggregate([
  {$match: {accepted: false}},
  {$group: {_id: "$source", n: {$sum: 1}, zero: {$sum: {$cond: [{$eq: ["$token_count", 0]}, 1, 0]}}}}
]).toArray());'
```

## 20. Контрольные вопросы

### Почему `word_count` и `token_count` сейчас равны?

В записи оба поля заполняются `result.token_count` для совместимости терминов
отчёта. Это число токенов алгоритма, а не простой `split()` ноутбука.

### Почему не использовать regex?

Технические формы и UTF-8 требуют явного контроля code points, внутренних
разделителей и длины. Regex мог бы работать, но скрыл бы основную логику и зависел
от Unicode semantics библиотеки.

### Почему не удалять числа?

Версии, стандарты, CVE, алгоритмы и размеры (`3.14`, `sha-256`) значимы в
техническом поиске.

### Что требует увеличения parser_version?

Изменение XPath, нормализации, token boundaries, maximum length semantics или
лемматизации. Изменение только размера batch не меняет результат и версии не
требует.


