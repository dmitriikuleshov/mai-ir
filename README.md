# Лабораторные работы по информационному поиску

Проект собирает технический корпус из Wikipedia, arXiv HTML и Microsoft Learn,
хранит исходные документы в MongoDB и выполняет их лингвистическую обработку.

Подробное описание всех компонентов, команды запуска и сценарий защиты находятся
в [`docs/`](docs/README.md).

## Быстрый запуск всего проекта

Все команды ниже выполняются из корня репозитория. Последовательность соответствует
потоку данных: MongoDB → crawler → лингвистическая обработка → булев индекс → поиск.

### 1. Установить зависимости

Для Python-компонентов используется `uv`, для C++-компонентов — CMake и системные
библиотеки MongoDB и libxml2. Перед началом должны быть установлены Docker с
поддержкой Compose и `uv`:

```bash
sudo apt update
sudo apt install build-essential cmake libmongoc-dev libbson-dev libxml2-dev
uv sync
```

### 2. Запустить и проверить MongoDB

```bash
docker compose up -d
docker compose ps
docker exec mai-ir-mongodb mongosh --quiet --eval \
  'db.adminCommand({ping: 1})'
```

В последней команде ожидается поле `ok: 1`. Данные сохраняются в Docker volume
`mai-ir_mongodb_data`, поэтому обычный `docker compose down` не удаляет корпус.

### 3. Скачать документы

```bash
uv run python src/crawler/main.py config.yaml
```

Три источника обходятся параллельно. Остановить crawler можно через `Ctrl+C`:
состояние обхода останется в MongoDB, а следующий запуск продолжит работу.

Проверить количество документов и распределение по источникам:

```bash
docker exec mai-ir-mongodb mongosh --quiet mai_ir --eval '
print("Всего документов:", db.documents.countDocuments());
printjson(db.documents.aggregate([
  {$group: {_id: "$source", documents: {$sum: 1}}},
  {$sort: {_id: 1}}
]).toArray());
'
```

Посмотреть один сохранённый документ и состояние продолжения обхода:

```bash
docker exec mai-ir-mongodb mongosh --quiet mai_ir --eval '
printjson(db.documents.findOne({}, {
  _id: 0, source: 1, source_id: 1, title: 1, url: 1,
  fetched_at: 1, checked_at: 1, content_hash: 1
}));
printjson(db.crawl_state.find({}, {_id: 0}).limit(10).toArray());
'
```

### 4. Собрать C++-компоненты и запустить тесты

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
ctest --test-dir .build --output-on-failure
uv run pytest -q tests/crawler
```

Первые три команды можно заменить скриптом, который заново конфигурирует проект,
собирает его и запускает все C++-тесты:

```bash
./check.sh
uv run pytest -q tests/crawler
```

### 5. Выполнить лингвистическую обработку

```bash
./.build/linguistics config.yaml
```

Программа потоково читает сырой HTML из MongoDB и записывает результаты в
`processed_documents`, `document_terms`, `token_frequencies` и
`lemma_frequencies`. Повторный запуск пропускает документы с теми же
`content_hash` и `parser_version`.

Проверить число принятых документов и созданные файлы статистики:

```bash
docker exec mai-ir-mongodb mongosh --quiet mai_ir --eval '
print("Обработано:", db.processed_documents.countDocuments());
print("Принято:", db.processed_documents.countDocuments({accepted: true}));
printjson(db.processed_documents.aggregate([
  {$match: {accepted: true}},
  {$group: {_id: "$source", documents: {$sum: 1}}},
  {$sort: {_id: 1}}
]).toArray());
'
ls -lh results/statistics.json results/zipf.svg
uv run python -m json.tool results/statistics.json | less
```

Из просмотра `less` можно выйти клавишей `q`.

### 6. Построить булев индекс и выполнить запросы

Сначала должен быть выполнен предыдущий этап, потому что индекс строится по леммам
из коллекции `document_terms`:

```bash
./.build/boolean_index config.yaml
ls -lh results/boolean_index
```

Интерактивный режим поиска завершается сочетанием `Ctrl+D`:

```bash
./.build/boolean_search config.yaml
```

Для воспроизводимой проверки через стандартные потоки:

```bash
printf 'database AND security\ncloud OR retrieval\nNOT database AND security\n' \
  | ./.build/boolean_search config.yaml \
  > results/boolean_search.txt
sed -n '1,160p' results/boolean_search.txt
```

Поддерживаются операторы `NOT`, `AND`, `OR` и скобки. Приоритет операторов:
`NOT > AND > OR`.

### 7. Открыть ноутбуки первой лабораторной

```bash
uv run jupyter notebook notebooks
```

В открывшемся интерфейсе последовательно запустить `wikipedia.ipynb`,
`arxiv.ipynb` и `microsoft_learn.ipynb` командой **Run All**.

### 8. Собрать отчёты

При установленном Typst исходники отчётов собираются следующими командами:

```bash
typst compile --root reports --font-path /mnt/c/Windows/Fonts \
  reports/source/lesson01.typ reports/pdf/lesson01.pdf
typst compile --root reports --font-path /mnt/c/Windows/Fonts \
  reports/source/lesson02.typ reports/pdf/lesson02.pdf
```

Путь `/mnt/c/Windows/Fonts` нужен в WSL для Times New Roman и Courier New. Если
эти шрифты установлены непосредственно в Linux, параметр `--font-path` можно
убрать.

## Диагностика

Статус и последние сообщения MongoDB:

```bash
docker compose ps
docker compose logs --tail=50 mongodb
```

Если порт `27017` занят другим процессом:

```bash
ss -ltnp | grep 27017
docker ps --filter publish=27017
```

Если C++-программа сообщает об отсутствующей динамической библиотеке:

```bash
ldd .build/linguistics | grep 'not found' || true
sudo apt install libmongoc-dev libbson-dev libxml2-dev
```

Остановить MongoDB без удаления данных:

```bash
docker compose down
```

Команда `docker compose down -v` намеренно не используется: флаг `-v` удалит
volume вместе со всеми загруженными документами.

## Структура проекта

| Каталог | Что находится внутри |
|---|---|
| `src/` | робот, лингвистика, построение индекса и поиск |
| `tests/` | автоматические тесты компонентов проекта |
| `notebooks/` | три ноутбука и их локальные примеры документов |
| `resources/` | словарь WordNet и его лицензия |
| `results/` | статистика, график и локальные файлы поискового индекса |
| `reports/` | исходники Typst, изображения и готовые PDF-отчёты |
| `docs/` | подробные объяснения компонентов и сценарий защиты |

В корне находятся общая конфигурация, файлы сборки и `check.sh`, который собирает
C++-программу и запускает тесты.

Каталоги `.build/`, кеши и вспомогательные файлы результатов не добавляются в
Git. Итоговые `statistics.json` и `zipf.svg` сохраняются в репозитории.

## Поток данных и основные компоненты

```text
Wikipedia / arXiv / Microsoft Learn
                  │
                  ▼
       crawler (Python, aiohttp)
                  │
                  ▼
       MongoDB: documents + crawl_state
                  │
                  ▼
       linguistics (C++): HTML → текст → токены → леммы
                  │
                  ├── MongoDB: processed_documents
                  ├── MongoDB: document_terms
                  ├── MongoDB: token_frequencies / lemma_frequencies
                  └── results/statistics.json + results/zipf.svg
                  │
                  ▼
       boolean_index (C++): дисковый инвертированный индекс
                  │
                  ▼
       boolean_search (C++): запрос → список документов
```

Python используется только для анализа источников и выкачки. Лингвистика,
построение индекса и поиск реализованы на C++20. В основных структурах данных
индекса и поиска STL не используется: память управляется через
`malloc`/`realloc`, сортировка выполняется `qsort`, а списки документов хранятся
в собственном бинарном формате. `libmongoc` отвечает только за доступ к MongoDB,
`libxml2` — за безопасный разбор HTML.

## Реализованные пункты оценивания из README преподавателя

Нумерация и названия сохранены по
[README курса](https://github.com/toshunster/MAI-IR/blob/main/README.md). Здесь
описаны только выполненные в текущей ветке пункты: что делает компонент, где
находятся исходники и данные, как его запустить и как проверить результат.

| № | Пункт | Основная реализация |
|---:|---|---|
| 1 | Добыча корпуса документов | `notebooks/` |
| 2 | Поисковый робот | `src/crawler/main.py` |
| 3 | Токенизация | `src/linguistics/` |
| 4 | Лемматизация | `src/linguistics/` и `resources/` |
| 5 | Закон Ципфа | `src/linguistics/main.cpp` |
| 7 | Булев индекс | `src/boolean_index/` |
| 8 | Булев поиск | `src/boolean_search/` |
| 17 | Автотесты | `tests/` и `check.sh` |

### 1. Добыча корпуса документов

**Что сделано.** Выбраны три источника англоязычных технических документов:
Wikipedia, научные статьи arXiv в категориях Computer Science и документация
Microsoft Learn. В `notebooks/` находятся три независимых исследования по 30
документов: загрузка примеров, разбор HTML, измерение размера и длины текста,
анализ метаданных и пригодности источника для дальнейшей индексации.

**Где находится.** Исследовательский код и сохранённые примеры находятся в
`notebooks/`. Рабочий корпус хранится в коллекции `documents` базы `mai_ir`, а не
в Git. Сейчас в MongoDB находится 170 393 сырых документа, из которых после
лингвистической фильтрации принято 151 182. Корпус объединён технической
тематикой Computer Science.

**Как запустить анализ источников:**

```bash
uv run jupyter notebook notebooks
```

В интерфейсе Jupyter открыть `wikipedia.ipynb`, `arxiv.ipynb` и
`microsoft_learn.ipynb`, затем выполнить **Run All**. Массовая загрузка относится
к следующему пункту и выполняется crawler.

**Как проверить.** В каждом ноутбуке должны появиться таблица 30 документов,
описательная статистика длины и график распределения. Массовый корпус проверяется
MongoDB-командами из шага 3 раздела «Быстрый запуск всего проекта».

### 2. Поисковый робот

**Что сделано.** Поисковый робот принимает единственный позиционный
аргумент — путь к YAML. Секции `db`, `logic` и `sources` задают MongoDB,
ограничения HTTP и параметры трёх источников. Wikipedia, arXiv и Microsoft Learn
работают как параллельные асинхронные задачи, но запросы к одному домену идут
последовательно с собственной задержкой. Учитываются `robots.txt`,
`Retry-After`, максимальный размер ответа, timeout, retry и экспоненциальный
backoff.

В `documents` сохраняются нормализованный URL, `source`, `source_id`, сырой HTML,
SHA-256 и Unix timestamps. Уникальные индексы `(source, source_id)` и
`(source, url)` защищают от дублей. Очередь Wikipedia и позиции обхода всех
источников хранятся в `crawl_queue` и `crawl_state`, поэтому после `Ctrl+C`
следующий запуск продолжает работу, а не начинает её сначала.

Повторный обход сравнивает `ETag`, `Last-Modified`, даты sitemap/arXiv и SHA-256.
Неизменившийся документ только получает новый `checked_at`; изменившийся HTML
перезаписывается. arXiv обрабатывается месячными диапазонами, чтобы не использовать
нестабильный большой глобальный offset. Ошибочный пакет API делится пополам, а
единичный проблемный ID откладывается без остановки других источников.

**Где находится.** Реализация — `src/crawler/main.py`, конфигурация —
`config.yaml`, MongoDB для локального запуска — `compose.yaml`. Сырые документы
записываются в `documents`, очередь Wikipedia — в `crawl_queue`, позиции обхода —
в `crawl_state`.

**Запуск и остановка:**

```bash
uv run python src/crawler/main.py config.yaml
```

Остановка — `Ctrl+C`.

**Как проверить.** В терминале должны параллельно появляться строки `NEW`,
`UPDATED` или `UNCHANGED` для трёх источников. Количество документов, один пример
записи и сохранённое состояние обхода проверяются командами из шага 3 раздела
«Быстрый запуск всего проекта».

### 3. Токенизация

**Что сделано.** Лингвистический обработчик потоково читает `raw_html` из MongoDB. Для
каждого источника используются отдельные XPath основного содержимого и
служебных блоков. После удаления разметки текст нормализуется и разбивается на
токены.

Токенизатор приводит ASCII к нижнему регистру, унифицирует типографские дефисы и
апострофы и сохраняет значимые технические формы: `C++`, `C#`, `.NET`,
`node.js`, `client-server`, `sha-256`, `snake_case` и `3.14`. Стоп-слова не
удаляются. Токены длиннее `maximum_token_length` отбрасываются. Документы короче
`minimum_words: 200` токенов сохраняются с `accepted: false` и не входят в
индексируемый корпус.

Результат документа находится в `processed_documents`, а токены и их частоты —
в `document_terms.tokens` и `token_frequencies`. Обработка выполняется пакетами,
поэтому весь корпус не загружается в оперативную память.

**Где находится.** Выделение текста, токенизатор и собственные динамические
массивы реализованы в `src/linguistics/core.cpp` и
`src/linguistics/core.hpp`. Потоковое чтение MongoDB и запись результатов — в
`src/linguistics/main.cpp`. Параметры `minimum_words` и
`maximum_token_length` задаются в секции `linguistics` файла `config.yaml`.

**Запуск:**

```bash
./.build/linguistics config.yaml
```

Эта команда одновременно выполняет пункты 3–5. Перед ней нужно собрать C++-код и
запустить MongoDB по инструкции выше.

**Как проверить:**

```bash
docker exec mai-ir-mongodb mongosh --quiet mai_ir --eval '
printjson(db.processed_documents.findOne({accepted: true}, {
  _id: 0, source: 1, title: 1, text_size: 1, word_count: 1, token_count: 1
}));
printjson(db.document_terms.findOne({}, {_id: 0, source_id: 1, tokens: {$slice: 10}}));
'
```

### 4. Лемматизация / стемминг

**Что сделано.** Выбрана лемматизация, а не стемминг. Один и тот же модуль из
`src/linguistics/core.cpp` применяется при обработке корпуса и при нормализации
слов поискового запроса. Это гарантирует совпадение термов документа и запроса.

Лемматизатор сочетает правила английской морфологии, встроенные нерегулярные
формы и 5 940 исключений Princeton WordNet из
`resources/wordnet_exceptions.tsv`. Для каждого документа сохраняются частоты
лемм в `document_terms.lemmas`, глобальные частоты — в `lemma_frequencies`.

Повторный запуск сравнивает `content_hash` и `parser_version`. Неизменившиеся
документы пропускаются, а изменившиеся обновляют глобальные частоты дельтами.
Флаг `dirty`, порядок записи коллекций и lease-блокировка обеспечивают
восстановление согласованности после аварийной остановки.

**Где находится.** Морфологические правила реализованы в
`src/linguistics/core.cpp`, а словарь исключений — в
`resources/wordnet_exceptions.tsv`. Частоты документа записываются в
`document_terms.lemmas`, глобальные частоты — в `lemma_frequencies`.

**Запуск и просмотр результата:**

```bash
./.build/linguistics config.yaml
docker exec mai-ir-mongodb mongosh --quiet mai_ir --eval '
printjson(db.document_terms.findOne({}, {_id: 0, source_id: 1, lemmas: {$slice: 10}}));
'
```

**Как проверить.** В выводе MongoDB должны быть пары `[лемма, частота]`.
Автоматические примеры морфологии дополнительно проверяются тестом
`linguistics_morphology` из пункта 17.

### 5. Закон Ципфа

**Что сделано.** После обработки корпуса программа читает глобальные частоты,
сортирует термы по убыванию и строит распределение «ранг — частота». Кроме
классической зависимости Ципфа вычисляется аппроксимация Ципфа–Мандельброта.

Численные результаты, параметры аппроксимации, частотные словари и контрольные
словоформы записываются в `results/statistics.json`. Иллюстрация наблюдаемого и
теоретического распределений записывается в `results/zipf.svg`.

**Где находится.** Подсчёт рангов, аппроксимация Мандельброта, запись JSON и
построение SVG реализованы в `src/linguistics/main.cpp`. Готовые результаты
находятся в `results/statistics.json` и `results/zipf.svg`.

**Запуск и проверка:**

```bash
./.build/linguistics config.yaml
uv run python -m json.tool results/statistics.json | less
```

Открыть график в WSL можно командой:

```bash
explorer.exe "$(wslpath -w results/zipf.svg)"
```

### 7. Булев индекс

**Что сделано.** `src/boolean_index` строит собственный дисковый инвертированный
индекс по `document_terms.lemmas`. Документы читаются MongoDB-курсором. Пары
«терм — внутренний номер документа» накапливаются только до
`block_postings`, сортируются и записываются во временные блоки. Затем блоки
сливаются. Поэтому потребление памяти ограничено размером блока, а не размером
корпуса.

Индекс состоит из четырёх файлов:

- `terms.bin` — строки термов;
- `lexicon.bin` — смещения термов и posting lists;
- `postings.bin` — отсортированные `uint32_t` номеров документов;
- `documents.bin` — соответствие внутреннего номера MongoDB ObjectId.

Формат описан собственными структурами в `src/boolean_index/format.hpp` и имеет
magic bytes и номер версии. Основные структуры индекса не используют STL.

**Где находится.** Построитель находится в `src/boolean_index/main.cpp`, формат
файлов — в `src/boolean_index/format.hpp`, параметры — в секции
`boolean_index` файла `config.yaml`. Результат записывается в
`results/boolean_index/`.

**Построение и проверка файлов:**

```bash
./.build/boolean_index config.yaml
ls -lh results/boolean_index
```

Перед построением должна завершиться лингвистическая обработка. Повторный запуск
перестраивает локальные файлы индекса по актуальной коллекции `document_terms`.
Проверка считается успешной, если созданы четыре перечисленных бинарных файла, а
следующий пункт находит по ним документы.

### 8. Булев поиск

**Что сделано.** `src/boolean_search` разбирает выражения с `NOT`, `AND`, `OR` и
скобками. Приоритет: `NOT > AND > OR`. Терм запроса проходит тот же токенизатор и
лемматизатор, что и документы. Терм ищется бинарным поиском в лексиконе, posting
list читается с диска. `AND` выполняется пересечением двух отсортированных списков,
`OR` — слиянием без дублей, `NOT` — дополнением до множества всех документов.

Консольная программа читает запросы из стандартного ввода и пишет выдачу в
стандартный вывод. Для каждого результата показываются заголовок, источник и URL.

**Где находится.** Парсер запроса и операции над posting lists реализованы в
`src/boolean_search/engine.cpp`, консольный ввод-вывод и получение карточек
документов из MongoDB — в `src/boolean_search/main.cpp`. Поиск читает файлы из
`results/boolean_index/`; число показываемых результатов задаётся параметром
`boolean_search.result_limit` в `config.yaml`.

**Интерактивный запуск:**

```bash
./.build/boolean_search config.yaml
```

**Воспроизводимый дамп:**

```bash
printf '(database OR storage) AND security\nNOT cloud AND retrieval\n' \
  | ./.build/boolean_search config.yaml \
  > results/boolean_search.txt
sed -n '1,160p' results/boolean_search.txt
```

**Как проверить.** В `results/boolean_search.txt` для каждого запроса должны быть
число совпадений и карточки документов с заголовком, источником и URL. Приоритет
операторов удобно проверить запросами со скобками и без них. Интерактивный режим
завершается `Ctrl+D`.

### 17. Автотесты (юнит-тесты)

**Что сделано.** В проекте 18 Python-тестов crawler и 8 отдельных CTest-сценариев
C++. Проверяются нормализация URL, SHA-256, sitemap и Atom, YAML, условные HTTP-
запросы, retry, остановка, токенизация, морфология, HTML extraction, WordNet,
восстановление после сбоя, отсутствие STL в лингвистическом модуле, аппроксимация
Мандельброта и операции булева поиска.

**Где находится.** Python-тесты crawler находятся в
`tests/crawler/test_crawler.py`, C++-тесты лингвистики — в
`tests/linguistics/test_core.cpp`, тесты булева поиска — в
`tests/boolean_search/test_engine.cpp`. Регистрация CTest находится в
`CMakeLists.txt`, единая сборка и запуск C++-тестов — в `check.sh`.

**Запустить все существующие автотесты:**

```bash
./check.sh
uv run pytest -q tests/crawler
```

Ожидаемый результат текущей версии: 8 из 8 CTest и 18 из 18 pytest проходят.
При ошибке CTest печатает вывод конкретного тесткейса благодаря
`--output-on-failure` в `check.sh`.

## Фактические результаты текущего корпуса

Из 170 393 сырых документов принято 151 182. Получено 396 830 385 токенов:

| Источник | Принято документов | Токенов |
|---|---:|---:|
| arXiv | 11 033 | 78 780 952 |
| Microsoft Learn | 65 666 | 187 154 782 |
| Wikipedia | 74 483 | 130 894 651 |

Средняя длина токена — 5,77 символа. Словарь содержит 3 634 478 различных токенов
и 3 565 769 лемм. Скорость чистой токенизации — около 75 694 KiB/с. Полный
миграционный прогон parser version 7 занял около 46,5 минуты с учётом MongoDB I/O
и расчёта статистики.

Архитектура crawler, лингвистического обработчика и индексатора потоковая и не
требует помещения всего корпуса в оперативную память.
