# Лабораторная работа 8. Булев поиск

## 1. Назначение

Консольная программа читает по одному запросу из стандартного ввода, выполняет
его над собственным инвертированным индексом и печатает результаты в стандартный
вывод. Это выполняет консольную часть интерфейсного требования курса и позволяет
создавать текстовые дампы тестирования.

Код разделён на:

- `src/boolean_search/engine.cpp` — индекс, parser и операции над списками;
- `src/boolean_search/engine.hpp` — публичный интерфейс ядра;
- `src/boolean_search/main.cpp` — конфиг, stdin/stdout и метаданные MongoDB.

## 2. Синтаксис запросов

Поддерживаются:

- `AND` — документ содержит оба выражения;
- `OR` — документ содержит хотя бы одно выражение;
- `NOT` — дополнение относительно всех документов индекса;
- круглые скобки.

Операторы регистронезависимы. Приоритет:

```text
NOT > AND > OR
```

Поэтому:

```text
cloud OR database AND security
```

эквивалентно:

```text
cloud OR (database AND security)
```

Примеры:

```text
database AND security
(retrieval OR search) AND NOT cloud
arxiv OR wikipedia
NOT cloud AND kubernetes
```

Между двумя терминами обязательно писать оператор. `cloud security` является
ошибкой, а не неявным `AND`.

## 3. Разбор запроса

Используется собственный recursive descent parser. Грамматика:

```text
expression := or_expression
or_expression := and_expression (OR and_expression)*
and_expression := unary_expression (AND unary_expression)*
unary_expression := NOT unary_expression | primary
primary := TERM | '(' expression ')'
```

Такая структура непосредственно задаёт нужный приоритет и позволяет выдавать
понятные ошибки:

- ожидался термин;
- ожидалась закрывающая скобка;
- после выражения остались лишние данные;
- термин не удалось однозначно нормализовать.

AST отдельно не хранится: parser вычисляет результат по мере разбора и освобождает
промежуточные массивы.

Приоритет операторов получается из вложенности функций, а не из таблицы STL:

```cpp
BooleanResult parse_unary(Parser *parser, bool *ok);  // NOT
BooleanResult parse_and(Parser *parser, bool *ok);    // вызывает parse_unary
BooleanResult parse_or(Parser *parser, bool *ok);     // вызывает parse_and

BooleanResult parse_or(Parser *parser, bool *ok) {
    BooleanResult left = parse_and(parser, ok);
    while (*ok && parser->current.type == TokenType::OR) {
        advance(parser);
        BooleanResult right = parse_and(parser, ok);
        BooleanResult merged = result_or(&left, &right);
        boolean_result_destroy(&left);
        boolean_result_destroy(&right);
        left = merged;
    }
    return left;
}
```

Поскольку `parse_or` получает операнды только через `parse_and`, все `AND` внутри
них вычисляются раньше внешнего `OR`.

## 4. Нормализация термина запроса

Каждый `TERM` перед поиском передаётся в тот же `linguistics_process`, который
обрабатывал корпус. Используются те же:

- lower case;
- правила технических токенов;
- WordNet exceptions;
- правила лемматизации.

Это предотвращает расхождение индекса и запроса:

```text
queries -> query -> posting list "query"
retrieved -> retrieve -> posting list "retrieve"
```

Если одна лексема запроса превращается не ровно в одну лемму, parser сообщает
ошибку. Фразы с пробелами текущий булев поиск не поддерживает.

## 5. Поиск термина в словаре

`lexicon.bin` отсортирован по UTF-8 термину. `find_term` выполняет бинарный поиск:

1. вычисляет середину диапазона записей;
2. читает один фиксированный `LexiconEntry`;
3. по `term_offset` читает строку из `terms.bin`;
4. сравнивает её с искомой леммой;
5. сужает диапазон.

Лексикон из 3,5 миллиона терминов не загружается целиком в память. После нахождения
записи читается соответствующий диапазон `postings.bin`.

Основной шаг бинарного поиска читает только одну запись и одну строку:

```cpp
while (left < right) {
    const uint64_t middle = left + (right - left) / 2;
    const uint64_t offset = sizeof(IndexHeader) +
                            middle * sizeof(LexiconEntry);

    seek_file(lexicon, offset);
    read_exact(lexicon, &entry, sizeof(entry));
    compare_term(index, &entry, term, &buffer, &capacity, &comparison);

    if (comparison < 0) {
        left = middle + 1;
    } else {
        right = middle;
    }
}
```

Смещения `uint64_t` позволяют адресовать индекс больше 4 GiB.

## 6. Операции над posting lists

Все списки отсортированы и не содержат дублей.

### AND

Два указателя идут по спискам. При равных `docID` документ добавляется в результат;
иначе двигается указатель меньшего значения.

```text
[1, 4, 8, 15]
[2, 4, 9, 15]
AND -> [4, 15]
```

Сложность `O(m + n)`, память не больше `min(m, n)`.

Реализация пересечения использует ровно два индекса:

```cpp
size_t first = 0;
size_t second = 0;

while (first < left->size && second < right->size) {
    const uint32_t a = left->document_ids[first];
    const uint32_t b = right->document_ids[second];

    if (a == b) {
        output.document_ids[output.size++] = a;
        ++first;
        ++second;
    } else if (a < b) {
        ++first;
    } else {
        ++second;
    }
}
```

Ни один указатель не движется назад, поэтому сложность линейная.

### OR

В результат переносится меньшее значение; равные значения переносятся один раз.

```text
[1, 4, 8]
[2, 4, 9]
OR -> [1, 2, 4, 8, 9]
```

Сложность `O(m + n)`, память не больше `m + n`.

### NOT

Один проход по диапазону `[0, document_count)` пропускает `docID`, присутствующие
во входном списке.

```text
all documents: [0, 1, 2, 3, 4]
term:          [1, 4]
NOT ->         [0, 2, 3]
```

Сложность `O(N)`. Дополнение может быть большим, поэтому запросы с широким `NOT`
потребляют больше памяти.

## 7. Получение метаданных

Ядро возвращает массив внутренних `docID`. Для первых `result_limit: 20`
документов консольная обвязка:

1. читает 12-байтный ObjectId из `documents.bin`;
2. выполняет поиск `_id` в `processed_documents`;
3. печатает заголовок, источник и URL.

Если MongoDB недоступна, поиск по бинарному индексу теоретически возможен, но
текущая CLI не сможет сформировать нормальную выдачу с заголовками. ObjectId
сохраняется в выводе для диагностики.

Выдача отсортирована по `docID`, а не по релевантности. Это корректно для чистого
булева поиска: каждый найденный документ удовлетворяет выражению, но score пока
не вычисляется.

## 8. Сборка и интерактивный запуск

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
./.build/boolean_search config.yaml
```

При интерактивном вводе подсказка печатается в `stderr`, чтобы она не попадала в
дамп `stdout`. Завершение — `Ctrl+D`.

Пример:

```text
Введите запрос с AND, OR, NOT и скобками. Ctrl+D — выход.
> database AND security
```

## 9. Стандартные потоки и дампы

Один запрос:

```bash
printf 'database AND security\n' | ./.build/boolean_search config.yaml
```

Несколько запросов из файла:

```bash
./.build/boolean_search config.yaml \
  < queries.txt \
  > results/boolean_search.txt
```

Или без промежуточного файла:

```bash
printf 'database AND security\n(retrieval OR search) AND NOT cloud\n' \
  | ./.build/boolean_search config.yaml \
  > results/boolean_search.txt
```

Ошибки запуска и интерактивная подсказка идут в `stderr`. Результат запросов идёт
в `stdout`, поэтому перенаправление соответствует требованиям курса.

## 10. Пример результата

```text
Запрос: database AND security
Найдено документов: 4991
1. Outline of computer science
   Источник: wikipedia | doc=0 | oid=...
   https://en.wikipedia.org/wiki/Outline_of_computer_science
...
... показано 20 из 4991
```

Число `4991` относится к текущему индексу и изменится после обновления корпуса.

## 11. Карта основных функций

| Функция | Назначение |
|---|---|
| `boolean_index_open` | открывает четыре файла и проверяет magic/version |
| `find_term` | бинарно ищет термин в дисковом лексиконе |
| `load_postings` | читает posting list найденного термина |
| `result_and` | пересекает два отсортированных списка |
| `result_or` | объединяет списки без дублей |
| `result_not` | строит дополнение по всему пространству docID |
| `next_token` | лексический анализ запроса |
| `parse_primary` | термин или выражение в скобках |
| `parse_unary` | унарный `NOT` |
| `parse_and` | цепочка `AND` |
| `parse_or` | цепочка `OR` |
| `boolean_index_document_oid` | переводит docID в ObjectId |
| `print_document` | получает и печатает метаданные MongoDB |

## 12. Тестирование

Тест создаёт синтетический индекс:

```text
cloud    -> [0, 1]
database -> [1, 2]
security -> [2, 3]
```

Проверяются:

- поиск одного термина;
- `AND`;
- `OR`;
- `NOT`;
- приоритет `AND` перед `OR`;
- изменение результата скобками;
- синтаксическая ошибка `cloud AND`;
- чтение заголовков и таблицы документов.

Запуск:

```bash
./.build/boolean_search_tests
# или вся проверка
./check.sh
```

## 13. Что показать преподавателю

1. Формальную грамматику и приоритет операторов.
2. Два запроса, различающихся только скобками.
3. Лемматизацию `queries` перед поиском.
4. Бинарный поиск термина без загрузки словаря целиком.
5. На бумаге выполнить пересечение двух маленьких списков.
6. Интерактивный запуск.
7. Запуск через pipe и сохранение stdout в файл.
8. Ошибку незакрытой скобки или отсутствующего операнда.
9. Автотесты.

Полезная демонстрационная последовательность:

```text
database AND security
database OR security
database AND NOT security
cloud OR database AND security
(cloud OR database) AND security
```

## 14. Ограничения и следующая работа

- нет ранжирования: порядок по `docID`;
- нет фраз, кавычек и координат терминов;
- нет неявного `AND`;
- нет skip pointers и сжатия posting lists;
- posting list одного термина и промежуточный результат находятся в RAM;
- метаданные первых результатов читаются отдельными MongoDB-запросами;
- нет HTML-веб-интерфейса, хотя он требуется общей программой курса.

Следующий логичный шаг — веб-обвязка над тем же `boolean_search_core`. Ядро не
нужно переписывать: новая форма ввода должна передавать строку запроса той же
функции и преобразовывать результат в HTML.

## 15. Полная трассировка одного запроса

Запрос:

```text
(cloud OR database) AND NOT security
```

### Лексический анализ

```text
LEFT
TERM("cloud")
OR
TERM("database")
RIGHT
AND
NOT
TERM("security")
END
```

`next_token` пропускает spaces, отдельно распознаёт parentheses, затем читает bytes
до space/parenthesis. Слова AND/OR/NOT сравниваются case-insensitive.

### Синтаксический вызов

```text
parse_or
  parse_and
    parse_unary
      parse_primary '('          
        parse_or
          parse_and cloud
          OR
          parse_and database
    AND
    parse_unary NOT
      parse_unary security
```

### Нормализация terms

Каждый из трёх terms отдельно проходит `linguistics_process`. Для этих форм lemmas
не меняются.

### Чтение lists

Пусть:

```text
cloud    -> [0,1,3]
database -> [0,2,3]
security -> [2,3]
all docs -> [0,1,2,3]
```

### Вычисление

```text
cloud OR database = [0,1,2,3]
NOT security      = [0,1]
AND               = [0,1]
```

### Формирование выдачи

Для 0 и 1 читаются ObjectId из `documents.bin`, затем title/source/url из MongoDB.
`result.size=2`, limit 20 ничего не обрезает.

## 16. Как parser сообщает ошибки

| Запрос | Причина | Сообщение |
|---|---|---|
| пустая строка | нет expression | `пустой запрос` |
| `cloud AND` | после AND нет operand | `ожидался поисковый термин` |
| `AND cloud` | expression начинается с operator | `ожидался поисковый термин` |
| `(cloud OR database` | нет RIGHT | `ожидалась закрывающая скобка` |
| `cloud database` | нет operator | `после выражения остались лишние данные` |
| `()` | empty primary | `ожидался поисковый термин` |

Ошибка одного input line не завершает CLI: печатается блок ошибки, затем читается
следующая строка. Это удобно для batch dump.

## 17. Почему нет неявного AND

`cloud security` можно интерпретировать как AND, phrase или ошибку. Текущий parser
требует явный operator, поэтому grammar однозначна, тесты проще, а пользователь
видит точную логику. Неявный AND можно добавить на уровне lexer/parser позже.

## 18. Память результата

Index lexicon остаётся на диске, но найденный posting list загружается в массив
uint32. Для одного термина в половине корпуса:

```text
151182 / 2 * 4 ≈ 295 KiB
1,000,000 / 2 * 4 ≈ 1.9 MiB
```

Промежуточные OR могут достигать `N*4` bytes. NOT тоже до `N*4`. Это значительно
меньше raw corpus, но не полностью streaming search.

Для очень большого web service нужны iterator-based operations или roaring-like
compressed structures; они не входят в текущий этап.

## 19. Корректность set operations

### AND invariant

Перед каждой итерацией все элементы до `first` и `second` уже окончательно
рассмотрены. Если `a<b`, `a` не может встретиться позже в B, потому что B sorted,
поэтому его безопасно пропустить.

### OR invariant

На каждом шаге в output переносится минимальный ещё не обработанный docID. Равные
значения переносятся один раз. Поэтому output sorted и unique.

### NOT invariant

Input sorted. Для каждого document от 0 до N-1 либо текущий input равен ему и
пропускается, либо document добавляется. Output является точным complement.

## 20. Бинарный поиск: ручной пример

Lexicon terms:

```text
0 cloud
1 database
2 index
3 security
```

Ищем `index`:

```text
left=0 right=4
middle=2 -> "index" == target -> found
```

Ищем `cache`:

```text
left=0 right=4
middle=2 -> "index" > "cache" -> right=2
middle=1 -> "database" > "cache" -> right=1
middle=0 -> "cloud" > "cache" -> right=0
not found -> empty posting
```

Отсутствующий term не является ошибкой запроса: он просто обозначает пустое
множество. Поэтому `cache OR cloud` вернёт cloud, а `cache AND cloud` — пусто.

## 21. Зачем проверяются magic и version

Без проверки старый или случайный файл интерпретировался бы как offsets, что могло
привести к чтению произвольных позиций и огромным allocations.

Open проверяет:

- четыре файла существуют;
- magic `MAIIRIDX`;
- format version;
- magic `MAIIRDOC`;
- одинаковое document count в index и document map.

Это fail-fast до чтения пользовательских запросов.

## 22. Разделение core и CLI

`engine.cpp` не знает о terminal formatting и MongoDB title queries. Его interface:

```cpp
BooleanResult boolean_search(BooleanIndex *index,
                             const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             char *error,
                             size_t error_size);
```

`main.cpp` отвечает за:

- YAML;
- stdin loop;
- human-readable stdout;
- ObjectId → Mongo metadata;
- result limit.

Поэтому web interface должен переиспользовать engine и заменить только transport
и rendering.

## 23. Как должен выглядеть будущий web interface

Минимальная архитектура:

```text
GET /              -> HTML form
GET /search?q=...  -> validate query
                    -> boolean_search(core)
                    -> resolve metadata
                    -> HTML result list
```

Нельзя запускать отдельный binary index build на каждый HTTP request. Index должен
быть открыт один раз при старте service и закрыт при shutdown.

Нужно HTML-escape title/query/error, ограничить длину query и не передавать строку
в shell. Search core принимает bytes напрямую, поэтому shell injection не нужен.

## 24. Производительность запросов

Пусть `V` — terms, `m/n` — list sizes, `N` — documents.

| Операция | Время | Память результата |
|---|---:|---:|
| term lookup | `O(log V)` file probes | term buffer |
| load posting | `O(m)` I/O | `4m` bytes |
| AND | `O(m+n)` | до `4*min(m,n)` |
| OR | `O(m+n)` | до `4*(m+n)` |
| NOT | `O(N)` | до `4*(N-m)` |
| metadata | до result_limit Mongo queries | малая |

Текущая metadata resolution делает до 20 отдельных queries. Оптимизация — собрать
ObjectId в `$in` и восстановить порядок в памяти.

## 25. Контрольные вопросы

### Почему `database AND security` возвращает тысячи документов?

Булева модель проверяет наличие terms где угодно в документе, не близость и не
важность. Длинные документы часто содержат оба слова. Ranking/positions нужны для
более точной выдачи.

### Что сильнее: NOT или AND?

NOT. Grammar `parse_and -> parse_unary`, а unary сначала обрабатывает NOT.
`NOT cloud AND database` означает `(NOT cloud) AND database`.

### Можно ли искать слово `AND` как term?

Нет, lexer всегда распознаёт его как operator независимо от регистра. Нужен escape
или quoted term syntax, которых сейчас нет.

### Почему result order не alphabetical и не relevance?

Set operations сохраняют docID order. Это обеспечивает линейность. Булева модель
не определяет score.

### Зачем MongoDB при search, если индекс содержит документы?

Индекс содержит только term presence и ObjectId map. Текст/title/url намеренно не
дублируются. Само множество результатов вычисляется без MongoDB text search.

### Что будет после обновления MongoDB без rebuild?

Binary index останется snapshot старого `document_terms`. Поэтому operational
порядок всегда `linguistics -> boolean_index`. Generation timestamp/manifest можно
добавить для автоматической проверки freshness.


