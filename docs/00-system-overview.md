# Общая схема системы

## 1. Назначение проекта

Проект строит поисковую систему по разноплановому техническому корпусу. Данные
поступают из трёх независимых источников:

- англоязычная Wikipedia;
- научные статьи arXiv с HTML-представлением;
- техническая документация Microsoft Learn.

Работа разделена на последовательные этапы. Результат каждого этапа становится
входом следующего:

```text
Wikipedia API ───────────┐
arXiv API + arxiv.org ───┼─> crawler ─> MongoDB.documents
Microsoft Learn sitemap ─┘                    │
                                             v
                                  C++ linguistic processor
                                             │
                    ┌────────────────────────┼───────────────────────┐
                    v                        v                       v
        processed_documents         document_terms        global frequencies
                                             │
                                             v
                                 C++ boolean index builder
                                             │
                                             v
                        lexicon + terms + postings + document map
                                             │
                                             v
                                  C++ boolean search CLI
                                             │
                                             v
                              titles and URLs from MongoDB
```

MongoDB хранит исходные и обработанные документы. Сам инвертированный индекс
реализован независимо в собственном бинарном формате. MongoDB не выполняет за
программу булев поиск.

## 2. Соответствие правилам курса

В [общих требованиях курса](https://github.com/toshunster/MAI-IR/blob/main/README.md)
зафиксировано следующее:

- crawler и вспомогательная обвязка могут быть написаны на Python;
- основные компоненты поисковой системы должны быть написаны на C или C++;
- структуры индекса и поиска нужно реализовать самостоятельно;
- STL разрешён только для токенизации;
- консольный поиск должен читать запросы из `stdin` и писать результат в `stdout`;
- решение должно быть применимо к корпусу, не помещающемуся целиком в RAM.

В проекте Python используется для ноутбуков и crawler. Выделение текста,
токенизация, лемматизация, построение индекса и поиск реализованы на C++. В
индексаторе и поиске нет STL-контейнеров: используются собственные динамические
массивы, `malloc`, `realloc`, `qsort`, бинарный поиск и собственная куча.

Внешние библиотеки решают только инфраструктурные задачи:

- `libmongoc` и `libbson` — доступ к MongoDB;
- `libxml2` — безопасный разбор HTML и XPath;
- `aiohttp` — HTTP-клиент crawler;
- `PyMongo` — асинхронный доступ crawler к MongoDB;
- BeautifulSoup — получение заголовка в crawler и анализ примеров в ноутбуках.

Поисковые структуры и операции над ними в эти библиотеки не вынесены.

## 3. Структура репозитория

| Путь | Назначение |
|---|---|
| `src/crawler/` | асинхронный поисковый робот на Python |
| `src/linguistics/` | выделение текста, токенизация, лемматизация и статистика на C++ |
| `src/boolean_index/` | построение собственного инвертированного индекса |
| `src/boolean_search/` | чтение индекса и выполнение булевых запросов |
| `tests/crawler/` | тесты crawler на pytest |
| `tests/linguistics/` | C++-тесты лингвистического модуля |
| `tests/boolean_search/` | C++-тесты индекса и булевых операций |
| `notebooks/` | три ноутбука анализа источников |
| `resources/` | исключения WordNet и лицензия словаря |
| `results/` | локальный индекс, статистика и SVG-график |
| `config.yaml` | единая конфигурация всех этапов |
| `compose.yaml` | локальная MongoDB 8.0 |
| `CMakeLists.txt` | сборка всех C++-компонентов и тестов |
| `check.sh` | повторяемая сборка и запуск CTest |

`results/boolean_index`, `.build`, кеши Python и выборки ноутбуков не добавляются
в Git. Это восстанавливаемые или слишком большие локальные данные.

Ключевые параметры всех этапов находятся в одном YAML. На защите удобно начать с
этого фрагмента, потому что он связывает MongoDB, лингвистику и индекс:

```yaml
db:
  uri: mongodb://localhost:27017
  database: mai_ir
  documents_collection: documents
  processed_collection: processed_documents
  document_terms_collection: document_terms

linguistics:
  batch_size: 100
  minimum_words: 200
  parser_version: 7
  lemma_dictionary: resources/wordnet_exceptions.tsv

boolean_index:
  output_directory: results/boolean_index
  term_field: lemmas
  block_postings: 500000

boolean_search:
  result_limit: 20
```

Здесь видно, что индекс строится не из сырого HTML, а из `document_terms`, и что
поиск показывает не более 20 документов, хотя считает полный размер результата.

## 4. Требования к окружению

Проверенная среда — Linux/WSL. Для Python используется `uv`, для C++ — CMake.

```bash
sudo apt update
sudo apt install build-essential cmake libmongoc-dev libbson-dev libxml2-dev libsnappy1v5

uv sync
docker compose up -d
```

При вопросе `Continue? [Y/n]` нужно ввести латинскую `Y` или просто нажать Enter.
Команду установки пакетов и команду CMake нужно выполнять отдельно.

Проверка MongoDB:

```bash
docker compose ps
docker exec mai-ir-mongodb mongosh --quiet --eval \
  'db.getSiblingDB("mai_ir").runCommand({ping: 1})'
```

Сборка C++:

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
```

Полная проверка:

```bash
./check.sh
uv run pytest -q tests/crawler
```

## 5. Коллекции MongoDB

База называется `mai_ir`.

| Коллекция | Содержимое | Кто записывает | Кто читает |
|---|---|---|---|
| `documents` | исходный HTML, URL, заголовок, метаданные и хеш | crawler | лингвистика |
| `crawl_state` | позиции обхода arXiv и Microsoft sitemap | crawler | crawler |
| `crawl_queue` | очередь категорий и страниц Wikipedia | crawler | crawler |
| `processed_documents` | извлечённый текст и метрики | лингвистика | поиск для выдачи |
| `document_terms` | частоты токенов и лемм каждого принятого документа | лингвистика | индексатор |
| `token_frequencies` | глобальная частота каждого токена | лингвистика | статистика |
| `lemma_frequencies` | глобальная частота каждой леммы | лингвистика | статистика |
| `linguistics_state` | блокировка и флаг согласованности частот | лингвистика | лингвистика |

Один и тот же MongoDB `_id` переносится из `documents` в
`processed_documents` и `document_terms`. Поэтому документ можно связать между
всеми этапами без дополнительного join-ключа.

## 6. Полный порядок запуска

```bash
# 1. Python-зависимости и MongoDB
uv sync
docker compose up -d

# 2. Сбор технического корпуса; остановка — Ctrl+C
uv run python src/crawler/main.py config.yaml

# 3. Сборка C++
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel

# 4. Выделение текста, токенизация, лемматизация и статистика
./.build/linguistics config.yaml

# 5. Полная перестройка булева индекса
./.build/boolean_index config.yaml

# 6. Интерактивный поиск
./.build/boolean_search config.yaml
```

Создание воспроизводимого дампа поиска:

```bash
printf 'database AND security\n(retrieval OR search) AND NOT cloud\n' \
  | ./.build/boolean_search config.yaml \
  > results/boolean_search.txt
```

## 7. Текущие количественные результаты

На момент последнего полного прогона:

- 170 393 сырых документа в `documents`;
- 151 182 документа прошли порог 200 токенов;
- 19 211 документов отклонены как слишком короткие;
- 396 830 385 токенов;
- 3 634 478 уникальных токенов;
- 3 565 769 уникальных лемм;
- 73 536 673 пары «лемма — документ» в булевом индексе.

Распределение принятых документов:

| Источник | Документов | Токенов |
|---|---:|---:|
| arXiv | 11 033 | 78 780 952 |
| Microsoft Learn | 65 666 | 187 154 782 |
| Wikipedia | 74 483 | 130 894 651 |

Эти значения берутся из `results/statistics.json` и будут изменяться после новых
запусков crawler и лингвистики.

## 8. Главное, что нужно уметь объяснить

1. Почему сырой HTML сохраняется отдельно от выделенного текста.
2. Как crawler не создаёт дубли и как понимает, что страницу нужно обновить.
3. Почему лингвистика обрабатывает MongoDB пакетами, а не загружает корпус целиком.
4. Чем токен отличается от леммы.
5. Почему `document_terms` является источником восстановления глобальных частот.
6. Что такое инвертированный индекс и posting list.
7. Почему индексатор сначала создаёт сортированные блоки, а затем сливает их.
8. Как выполняются `AND`, `OR` и `NOT` над упорядоченными списками `docID`.
9. Почему числовой `docID` хранится отдельно от MongoDB ObjectId.
10. Какие части ещё нужны для полного выполнения всех требований: миллион
    документов и HTML-веб-интерфейс.

## 9. Полный жизненный цикл одного документа

Рассмотрим документ Wikipedia. Это не отдельный специальный путь: arXiv и
Microsoft Learn проходят те же стадии после сохранения HTML.

### Стадия 1. Обнаружение

`WikipediaCrawler.process_category` получает из MediaWiki API объект:

```json
{
  "pageid": 5323,
  "ns": 0,
  "title": "Computer science"
}
```

Из него создаётся задача `page` в `crawl_queue`. В очередь не помещается HTML —
только то, что нужно для будущей загрузки. Уникальный индекс по
`(source, kind, key)` не позволяет создать вторую задачу для того же `pageid`.

### Стадия 2. Загрузка

`process_page` строит URL, затем вызывает общий `fetch_and_store`:

```text
source    = wikipedia
source_id = 5323
url       = https://en.wikipedia.org/wiki/Computer_science
```

Общий HTTP-слой проверяет robots.txt, выдерживает паузу, делает условный запрос и
ограничивает размер ответа. `MongoStorage.save_document` вычисляет SHA-256 и
создаёт или обновляет запись `documents`.

### Стадия 3. Проверка актуальности лингвистики

C++-программа читает `_id`, `source`, `raw_html`, `content_hash`. Затем одним
пакетным запросом подгружает существующий `processed_documents`.

```text
совпадает content_hash?
        │
        ├─ нет -> обработать заново
        └─ да
            │
            └─ совпадает parser_version?
                   ├─ нет -> обработать заново
                   └─ да  -> пропустить
```

Так различаются два вида изменения: внешний документ изменился или изменился наш
алгоритм.

### Стадия 4. Извлечение и токенизация

`extract_text` выбирает HTML-контейнер источника. `linguistics_process` нормализует
Unicode, создаёт токены, леммы и частотные массивы.

```text
"Databases store indexed records."
        │ tokenize
        v
["databases", "store", "indexed", "records"]
        │ lemmatize
        v
["database", "store", "index", "record"]
```

Если токенов меньше 200, документ остаётся в `processed_documents` как rejected,
но не получает `document_terms`.

### Стадия 5. Глобальные частоты

Для нового документа его локальные counts прибавляются к глобальным. Для
изменённого сначала вычитается старая версия и прибавляется новая. Поэтому
`lemma_frequencies["database"]` отражает актуальный корпус, а не сумму всех
исторических версий.

### Стадия 6. Построение индекса

Индексатор читает только `_id` и `lemmas`. MongoDB ObjectId получает внутренний
`docID`. Каждая уникальная лемма документа создаёт одну пару:

```text
("database", docID)
("index", docID)
("record", docID)
("store", docID)
```

Внутридокументная частота здесь не дублирует docID: для булевой модели достаточно
знать факт присутствия. Сам count остаётся в MongoDB для будущего TF-IDF.

### Стадия 7. Запрос

Для `databases AND indexed` поисковик получает леммы `database` и `index`, читает
два posting list и пересекает их. Полученный `docID` переводится через
`documents.bin` в ObjectId, после чего MongoDB возвращает title и URL.

## 10. Главные инварианты системы

Инвариант — условие, которое должно оставаться истинным после любого штатного
запуска.

| Инвариант | Чем обеспечивается |
|---|---|
| Один источник/ID соответствует одному raw document | unique `(source, source_id)` |
| Один source/URL не записывается дважды | unique `(source, url)` |
| `_id` одинаков между raw, processed и terms | upsert по исходному ObjectId |
| Rejected document отсутствует в `document_terms` | remove при `accepted=false` |
| Глобальная частота равна сумме document terms | delta protocol и rebuild при dirty |
| Posting list отсортирован и уникален | qsort, dedup и merge |
| Все docID posting существуют в documents map | docID назначается перед добавлением terms |
| Версия файлов поиска согласована | magic/version/header checks |
| Запрос и корпус нормализуются одинаково | общий `linguistics_core` |

Если преподаватель спрашивает «как вы доказываете корректность?», нужно говорить
не только о тестах, но и об этих инвариантах.

## 11. Почему компоненты разделены именно так

### Почему crawler не выделяет текст

HTML меняется редко, а правила parser можно улучшать много раз. Если crawler сразу
сохранит только текст, после изменения XPath придётся повторно обращаться к сайтам.
Разделение экономит сеть и сохраняет исходный материал для отладки.

### Почему локальные и глобальные частоты хранятся отдельно

Глобальная коллекция быстро отвечает на вопросы о словаре и Zipf. Локальная
`document_terms` позволяет удалить вклад изменившегося документа и восстановить
глобальную коллекцию после сбоя.

### Почему индекс не хранится в MongoDB

Это требование курса и архитектурное решение. MongoDB удобна для документов,
очередей и обновлений. Собственный индекс позволяет показать формат posting lists,
внешнюю сортировку и булевы операции без скрытой библиотечной реализации.

### Почему метаданные выдачи остаются в MongoDB

В posting нужны только компактные docID. Заголовок и URL переменной длины не должны
раздувать индекс и повторяться в нескольких файлах. Таблица ObjectId связывает
компактный индекс с документным хранилищем.

## 12. Режимы повторного запуска

| Что изменилось | Что запускать |
|---|---|
| Ничего, нужна проверка | тесты и boolean search |
| Появились новые URL | crawler → linguistics → boolean_index |
| Изменился HTML существующей страницы | crawler → linguistics → boolean_index |
| Изменились XPath/token rules | увеличить parser_version → linguistics → boolean_index |
| Изменились только параметры search output | только boolean_search |
| Потеряны global frequencies | linguistics восстановит из document_terms |
| Потеряны binary index files | boolean_index построит их заново |
| Потерян raw HTML | нужен повторный crawler; downstream его не восстановит |

## 13. Где искать причину ошибки

| Симптом | Первый объект проверки |
|---|---|
| Crawler не стартует | `docker compose ps`, URI, User-Agent config |
| Один источник молчит | robots.txt, crawl_state/queue, enabled, max_documents |
| Документ есть, но text пуст | raw_html и XPath конкретного источника |
| Документ rejected | token_count и minimum_words |
| Частоты выглядят неверно | dirty state и document_terms |
| Индексатор даёт 0 документов | count `document_terms` и term_field |
| Search не открывает индекс | четыре `.bin`, magic и version |
| Термин ничего не находит | его лемма и наличие в lemma_frequencies |
| В выдаче нет title | ObjectId в documents.bin и processed_documents |

## 14. Минимальный словарь терминов

| Термин | Значение в проекте |
|---|---|
| Corpus | множество документов для поиска |
| Raw document | исходный HTML и metadata |
| Parsed text | основной текст после удаления разметки и шума |
| Token | минимальная поисковая единица из текста |
| Lemma | нормализованная словарная форма токена |
| Vocabulary | множество уникальных токенов или лемм |
| Forward index | документ → его термины |
| Inverted index | термин → документы |
| Posting | один docID в списке термина |
| Posting list | отсортированный список docID термина |
| Lexicon | таблица терминов и адресов posting lists |
| docID | внутренний компактный номер документа |
| Idempotency | повторный запуск не создаёт дубли и не искажает counts |
| Cursor | потоковое чтение результатов MongoDB |
| Upsert | update существующего или insert нового документа |
| Lease | временное владение задачей/блокировкой |
| Fan-in | число файлов, одновременно участвующих в merge |


