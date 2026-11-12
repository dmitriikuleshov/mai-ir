# Быстрый показ всех выполненных лабораторных за 10 минут

Это короткая рабочая шпаргалка для формата, в котором нужно быстро показать весь
проект. Она не заменяет подробные документы `01`–`10`: здесь оставлены только
порядок действий, команды и короткие формулировки для рассказа.

За 10 минут показываются выполненные пункты курса:

1. добыча и анализ корпуса;
2. поисковый робот;
3. токенизация;
4. лемматизация;
5. закон Ципфа–Мандельброта;
7. булев индекс;
8. булев поиск;
17. автотесты.

## Главное правило

На защите не нужно заново:

- массово скачивать корпус;
- обрабатывать 170 тысяч документов;
- строить индекс размером в сотни мегабайт.

Эти операции зависят от сети и занимают минуты или часы. Нужно показать команды
запуска, уже сохранённые результаты, несколько документов в MongoDB, готовый
индекс, живой поисковый запрос и тесты.

## Распределение 10 минут

| Время | Что показывать |
|---|---|
| 0:00–0:40 | архитектура и общий конвейер |
| 0:40–1:40 | анализ трёх источников |
| 1:40–3:20 | crawler и MongoDB |
| 3:20–5:10 | выделение текста, токенизация и лемматизация |
| 5:10–6:10 | статистика и закон Ципфа–Мандельброта |
| 6:10–7:20 | устройство булева индекса |
| 7:20–8:50 | живой булев поиск |
| 8:50–9:40 | автотесты |
| 9:40–10:00 | итог |

## 1. Что подготовить заранее

Все команды выполняются из корня проекта:

```bash
cd /home/ubuntuwsl/mai/mai-ir
```

### 1.1. Проверить Docker Desktop и MongoDB

Docker Desktop должен быть запущен, а для используемого WSL-дистрибутива должна
быть включена WSL Integration.

```bash
docker compose up -d
docker compose ps
docker compose exec -T mongodb mongosh --quiet --eval \
  'db.adminCommand({ping: 1})'
```

Перед защитой обязательно должны выполняться два условия:

- в `docker compose ps` контейнер `mai-ir-mongodb` имеет статус `Up`;
- команда `ping` возвращает `ok: 1`.

### 1.2. Собрать программы и проверить тесты

```bash
uv sync
./check.sh
uv run pytest -q tests/crawler
```

Ожидаемый итог:

```text
100% tests passed, 0 tests failed out of 8
18 passed
```

### 1.3. Проверить готовые результаты

```bash
ls -lh results/statistics.json results/zipf.svg
ls -lh results/boolean_index
ls -lh reports/pdf
```

В `results/boolean_index` должны находиться:

```text
documents.bin
lexicon.bin
postings.bin
terms.bin
```

### 1.4. Проверить поиск до защиты

```bash
printf 'database AND security\n' \
  | ./.build/boolean_search config.yaml \
  | sed -n '1,20p'
```

Нормальная выдача должна содержать заголовок, источник и URL. Если вместо них
появилось `метаданные не найдены`, сначала проверить MongoDB:

```bash
docker compose ps
docker compose exec -T mongodb mongosh --quiet --eval \
  'db.adminCommand({ping: 1})'
```

Поисковый индекс хранится локально и может открыться даже при выключенной
MongoDB, но карточки результатов берутся из `processed_documents`. Поэтому
выключенная база приводит именно к сообщению о недоступных метаданных.

Если MongoDB работает, но сообщение остаётся, перед защитой один раз перестроить
индекс по актуальным `document_terms`:

```bash
./.build/boolean_index config.yaml
```

### 1.5. Открыть нужные окна

До начала рассказа подготовить:

1. терминал WSL в корне проекта;
2. редактор с `config.yaml`;
3. три вкладки Jupyter с ноутбуками;
4. `results/zipf.svg`;
5. MongoDB Compass с заранее открытой базой `mai_ir`.

Для Compass использовать строку подключения:

```text
mongodb://127.0.0.1:27017/mai_ir
```

Чтобы не тратить время на навигацию, заранее открыть или запомнить порядок:

```text
documents → Documents
documents → Aggregations
documents → Indexes
crawl_state
crawl_queue
processed_documents
document_terms
linguistics_state
```

Jupyter запускается в отдельном терминале:

```bash
uv run jupyter notebook notebooks
```

Открыть:

```text
notebooks/wikipedia.ipynb
notebooks/arxiv.ipynb
notebooks/microsoft_learn.ipynb
```

## 2. Начало рассказа — 40 секунд

### Что показать

Открыть в редакторе `docs/00-system-overview.md` и показать схему конвейера. Затем
переключиться на `config.yaml`. Структуру папок в терминал выводить не нужно.

### Что сказать

> Я покажу единый поисковый конвейер. Python-crawler получает технические
> документы из Wikipedia, arXiv HTML и Microsoft Learn и сохраняет сырой HTML в
> MongoDB. C++-обработчик выделяет текст, токенизирует и лемматизирует его. Затем
> C++-индексатор строит собственный дисковый инвертированный индекс, а консольная
> утилита выполняет булевы запросы.

После этого на несколько секунд открыть `config.yaml` и показать секции:

```text
db
logic
sources
linguistics
boolean_index
boolean_search
```

## 3. Анализ источников — 1 минута

### Что показать в Jupyter

Быстро переключить три уже открытые вкладки:

1. `wikipedia.ipynb` — таблица и график для 30 документов;
2. `arxiv.ipynb` — таблица и график для 30 HTML-статей;
3. `microsoft_learn.ipynb` — таблица и график для 30 страниц документации.

Не запускать **Run All** во время короткой защиты. Результаты должны быть
выполнены и сохранены заранее.

### Что сказать

> В первой работе я исследовал по 30 документов каждого источника: способ
> получения URL, структуру HTML, метаданные, длину текста и существующий поиск.
> Wikipedia имеет устойчивый page ID, arXiv — paper ID и Atom API, Microsoft
> Learn — официальные sitemap. Все три источника подходят для технического
> корпуса, но требуют разных правил выделения основного текста.

## 4. Поисковый робот и MongoDB — 1 минута 40 секунд

MongoDB в основном сценарии показывается через MongoDB Compass, а не через
терминальные запросы. Команды `mongosh` сохранены отдельно в разделе 12 как
запасной вариант.

### 4.1. Подтвердить, что MongoDB запущена

```bash
docker compose ps
```

После этой единственной команды перейти в уже открытый MongoDB Compass.

### 4.2. Показать сырые документы в Compass

1. Слева открыть базу `mai_ir`.
2. Открыть коллекцию `documents` и вкладку **Documents**.
3. В поле **Filter** вставить:

   ```javascript
   { source: "wikipedia" }
   ```

4. В поле **Project** вставить:

   ```javascript
   {
     source: 1,
     source_id: 1,
     title: 1,
     url: 1,
     raw_size: 1,
     fetched_at: 1,
     checked_at: 1,
     content_hash: 1
   }
   ```

5. Нажать **Find** или **Apply**.

Проекция специально не показывает большое поле `raw_html`. В одной записи
указать преподавателю на источник, устойчивый ID, URL, размер, хеш и два времени:

- `fetched_at` — когда сохранённое содержимое изменилось;
- `checked_at` — когда источник проверялся последний раз.

### 4.3. Показать объём корпуса в Compass

В `documents` открыть вкладку **Aggregations**. Добавить две стадии.

Первая стадия `$group`:

```javascript
{
  _id: "$source",
  documents: { $sum: 1 }
}
```

Вторая стадия `$sort`:

```javascript
{ _id: 1 }
```

Нажать **Run**. Должны появиться три строки: `arxiv`, `microsoft_learn` и
`wikipedia`.

### 4.4. Показать отсутствие дублей

В коллекции `documents` открыть вкладку **Indexes** и показать два индекса с
признаком `unique`:

```text
source_1_source_id_1
source_1_url_1
```

### 4.5. Показать продолжение обхода

1. Открыть коллекцию `crawl_state` и показать состояние arXiv и Microsoft Learn.
2. Открыть `crawl_queue`, задать фильтр:

   ```javascript
   { source: "wikipedia" }
   ```

3. Показать поля `status`, `kind`, `title`, `depth` и `attempts`.

Подробная инструкция по интерфейсу Compass находится в
`docs/10-mongodb-demonstration.md`.

### Что сказать

> Crawler асинхронно обходит три источника, но соблюдает отдельную задержку на
> домен, robots.txt, Retry-After, timeout и backoff. В documents сохраняются
> нормализованный URL, сырой HTML, SHA-256 и timestamps. Два уникальных индекса
> защищают от дублей. Позиции обхода и очередь хранятся в MongoDB, поэтому после
> Ctrl+C программа продолжает работу с сохранённого места.

### Команда настоящего запуска

Показать команду, но в 10-минутном сценарии не запускать её надолго:

```bash
uv run python src/crawler/main.py config.yaml
```

Если преподаватель просит живой запуск, выполнить её и через несколько строк
лога нажать `Ctrl+C`. Сетевые источники могут отвечать медленно, поэтому данные в
MongoDB являются основным доказательством завершённого обхода.

## 5. Токенизация и лемматизация — 1 минута 50 секунд

Результаты этого этапа продолжают показываться в MongoDB Compass.

### 5.1. Показать обработанный документ

1. Открыть коллекцию `processed_documents` и вкладку **Documents**.
2. В поле **Filter** вставить:

   ```javascript
   { accepted: true }
   ```

3. В поле **Project** вставить:

   ```javascript
   {
     source: 1,
     source_id: 1,
     title: 1,
     text_size: 1,
     word_count: 1,
     token_count: 1,
     parser_version: 1,
     accepted: 1
   }
   ```

4. Нажать **Find** или **Apply** и открыть одну запись.

### 5.2. Показать токены и леммы

1. Открыть коллекцию `document_terms`.
2. Открыть один документ.
3. Развернуть первые элементы массивов `tokens` и `lemmas`.

Каждый элемент имеет вид `[терм, частота]`. Например, `["database", 7]`
означает семь вхождений терма в документе.

### 5.3. Показать согласованность частот

1. Открыть коллекцию `linguistics_state`.
2. В поле **Filter** вставить:

   ```javascript
   { _id: "frequencies" }
   ```

3. Показать `dirty: false` и `parser_version: 7`. `dirty: false` означает, что
   глобальные частоты согласованы с `document_terms`.

### Что сказать про выделение текста

> C++-модуль читает raw_html потоково из MongoDB. Для Wikipedia, arXiv и
> Microsoft Learn используются разные XPath основного содержимого и шума. В
> processed_documents сохраняется уже очищенный основной текст. Документы короче
> 200 токенов не входят в индексируемый корпус.

### Что сказать про токенизацию

> Токенизатор сохраняет технические формы C++, C#, .NET, node.js, snake_case,
> sha-256 и числа с точкой. Частоты токенов одного документа сохраняются в
> document_terms, глобальные — в token_frequencies.

### Что сказать про лемматизацию

> Лемматизатор сочетает собственные правила английской морфологии, нерегулярные
> формы и 5940 исключений WordNet. Один и тот же код используется для документов
> и поискового запроса. Поэтому запрос и индекс нормализуются одинаково.

### Команда полного запуска

```bash
./.build/linguistics config.yaml
```

В коротком показе команду только назвать. Полный прогон уже выполнен, а повторный
запуск всё равно сначала проверяет состояние большого корпуса.

## 6. Закон Ципфа–Мандельброта — 1 минута

### Команда краткого вывода статистики

```bash
uv run python -c '
import json
s = json.load(open("results/statistics.json", encoding="utf-8"))
print("documents:", s["corpus"]["documents"])
print("tokens:", s["corpus"]["tokens"])
print("token vocabulary:", s["corpus"]["token_vocabulary"])
print("lemma vocabulary:", s["corpus"]["lemma_vocabulary"])
print("tokenization KiB/s:", round(s["corpus"]["tokenization_kib_per_second"], 1))
print("Mandelbrot:", s["mandelbrot"])
'
```

### Открыть график в WSL

```bash
explorer.exe "$(wslpath -w results/zipf.svg)"
```

### Что сказать

> Для принятого корпуса получено 151182 документа и 396830385 токенов. Термы
> сортируются по убыванию частоты, после чего строится график «ранг — частота» и
> подбираются параметры Ципфа–Мандельброта. Числа сохраняются в statistics.json,
> график — в zipf.svg.

## 7. Булев индекс — 1 минута 10 секунд

### Показать готовые файлы

```bash
ls -lh results/boolean_index
```

Ожидаются четыре файла:

```text
terms.bin      строки термов
lexicon.bin    смещения термов и posting lists
postings.bin   отсортированные номера документов
documents.bin  соответствие номера документа MongoDB ObjectId
```

### Показать основные структуры формата

```bash
sed -n '1,90p' src/boolean_index/format.hpp
```

### Что сказать

> Индексатор написан на C++ без STL-контейнеров. Он читает document_terms
> MongoDB-курсором, накапливает ограниченный блок пар «терм — документ», сортирует
> блоки и сливает их на диске. Поэтому весь корпус не должен помещаться в RAM.
> Лексикон содержит смещения, а posting lists хранят отсортированные uint32 ID.

### Команда построения

```bash
./.build/boolean_index config.yaml
```

В коротком показе индекс повторно не строить: достаточно показать команду,
архитектуру и готовые файлы.

## 8. Булев поиск — 1 минута 30 секунд

### Выполнить живые запросы

```bash
printf 'database AND security\n(database OR storage) AND retrieval\nNOT cloud AND retrieval\n' \
  | ./.build/boolean_search config.yaml \
  | sed -n '1,70p'
```

### Что сказать

> Запрос читается из stdin, а результат пишется в stdout. Поддерживаются NOT, AND,
> OR и скобки с приоритетом NOT выше AND выше OR. Терм запроса проходит тот же
> лемматизатор. AND пересекает отсортированные списки двумя указателями, OR
> выполняет слияние без дублей, NOT строит дополнение относительно всех
> документов. Для результата из MongoDB добавляются заголовок, источник и URL.

Если преподаватель хочет интерактивный режим:

```bash
./.build/boolean_search config.yaml
```

Ввести запрос и завершить программу через `Ctrl+D`.

## 9. Автотесты — 50 секунд

Полная сборка уже выполнена при подготовке. На защите достаточно быстро повторить
сами тесты:

```bash
ctest --test-dir .build --output-on-failure
uv run pytest -q tests/crawler
```

### Что сказать

> В CTest зарегистрировано 65 GoogleTest-сценариев: 62 unit-теста и три
> интеграционных. Они проверяют лингвистический конвейер, конфигурацию,
> HTML extraction, статистику, восстановление, булев индекс и поиск.
> Дополнительно 18 pytest проверяют crawler.

## 10. Завершение — 20 секунд

Сказать:

> В результате получен воспроизводимый конвейер от трёх интернет-источников до
> консольного булева поиска. Сырой корпус и состояние обхода находятся в MongoDB,
> лингвистика и поисковые компоненты написаны на C++, индекс строится потоково и
> хранится в собственном бинарном формате. Все текущие компоненты покрыты
> автоматическими тестами.

При необходимости показать отчёты:

```bash
ls -lh reports/pdf
```

## 11. Все терминальные команды основного показа

MongoDB между первой и второй командой показывается в Compass по разделам 4 и 5.
Структура каталогов в терминал не выводится.

```bash
# 1. Состояние окружения
docker compose ps

# 2. Статистика корпуса после показа MongoDB в Compass
uv run python -c '
import json
s = json.load(open("results/statistics.json", encoding="utf-8"))
print("documents:", s["corpus"]["documents"])
print("tokens:", s["corpus"]["tokens"])
print("token vocabulary:", s["corpus"]["token_vocabulary"])
print("lemma vocabulary:", s["corpus"]["lemma_vocabulary"])
print("Mandelbrot:", s["mandelbrot"])
'

# 3. Файлы индекса
ls -lh results/boolean_index

# 4. Живой поиск
printf 'database AND security\n(database OR storage) AND retrieval\nNOT cloud AND retrieval\n' \
  | ./.build/boolean_search config.yaml \
  | sed -n '1,70p'

# 5. Автотесты
ctest --test-dir .build --output-on-failure
uv run pytest -q tests/crawler

# 6. Отчёты
ls -lh reports/pdf
```

## 12. Запасной показ MongoDB через терминал

Этот раздел не используется в основном сценарии. Он нужен, если Compass не
открывается или преподаватель просит показать точные запросы вручную.

### 12.1. Сырой корпус и состояние crawler

```bash
docker compose exec -T mongodb mongosh --quiet mai_ir --eval '
print("=== DOCUMENTS BY SOURCE ===");
printjson(db.documents.aggregate(
  [{$group: {_id: "$source", documents: {$sum: 1}}}, {$sort: {_id: 1}}],
  {hint: {source: 1, source_id: 1}}
).toArray());
print("=== RAW DOCUMENT ===");
printjson(db.documents.findOne(
  {source: "wikipedia"},
  {_id: 0, source: 1, source_id: 1, title: 1, url: 1,
   raw_size: 1, fetched_at: 1, checked_at: 1, content_hash: 1}
));
print("=== CRAWL STATE ===");
printjson(db.crawl_state.find({}).limit(3).toArray());
'
```

### 12.2. Результаты лингвистической обработки

```bash
docker compose exec -T mongodb mongosh --quiet mai_ir --eval '
print("=== PROCESSED DOCUMENT ===");
printjson(db.processed_documents.findOne(
  {accepted: true},
  {_id: 0, source: 1, source_id: 1, title: 1, text_size: 1,
   word_count: 1, token_count: 1, parser_version: 1, accepted: 1}
));
print("=== TOKENS AND LEMMAS ===");
printjson(db.document_terms.findOne(
  {},
  {_id: 0, source_id: 1, tokens: {$slice: 8}, lemmas: {$slice: 8}}
));
print("=== PROCESSOR STATE ===");
printjson(db.linguistics_state.findOne({_id: "frequencies"}));
'
```

## 13. Аварийный план

### Docker недоступен

Если команда `docker` сообщает, что она не найдена или WSL Integration выключена:

1. запустить Docker Desktop;
2. открыть **Settings → Resources → WSL Integration**;
3. включить используемый Ubuntu-дистрибутив;
4. нажать **Apply & Restart**;
5. снова выполнить `docker compose up -d`.

Без MongoDB можно показать ноутбуки, `statistics.json`, `zipf.svg`, файлы индекса
и C++-тесты, но полноценные карточки результатов поиска будут недоступны.

### Поиск пишет «метаданные не найдены»

Сначала проверить `docker compose ps` и `ping`. Это сообщение часто означает,
что локальный бинарный индекс открылся, но MongoDB выключена. Если база доступна,
перестроить индекс заранее командой:

```bash
./.build/boolean_index config.yaml
```

### Нет времени на Compass

Использовать две готовые команды `mongosh` из раздела 12. Подробный показ
Compass и терминала находится в `10-mongodb-demonstration.md`.

### Не открывается график

Показать, что файл существует:

```bash
ls -lh results/zipf.svg
```

Затем открыть его из проводника Windows по пути проекта или показать значения из
`statistics.json`.

