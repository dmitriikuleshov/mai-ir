# Лабораторная работа 2. Поисковый робот

## 1. Назначение

Crawler массово собирает сырой корпус и сохраняет его в MongoDB. Он должен:

- обнаруживать документы каждого источника;
- соблюдать `robots.txt` и задержки;
- переживать временные сетевые ошибки;
- не записывать дубли;
- продолжать работу после остановки;
- перепроверять и обновлять ранее скачанные страницы;
- не загружать весь список задач в память.

Реализация находится в `src/crawler/main.py`. Python здесь допустим правилами
курса, потому что это компонент выкачки, а не структура поискового индекса.

## 2. Запуск

```bash
uv sync
docker compose up -d
uv run python src/crawler/main.py config.yaml
```

Остановка — `Ctrl+C`. Встроенного бесконечного цикла нет: crawler проходит
доступную работу и завершается. Периодический запуск при необходимости можно
делать внешним планировщиком.

Тесты:

```bash
uv run pytest -q tests/crawler
```

## 3. Общая архитектура

Три источника запускаются как три `asyncio`-задачи. Они работают параллельно,
потому что имеют разные домены. Внутри одного источника запросы ограничиваются
отдельным `RateLimiter` и выполняются с паузой из `config.yaml`.

```text
                         ┌─ WikipediaCrawler ───────┐
main -> asyncio.gather ──┼─ ArxivCrawler ──────────┼─> MongoStorage
                         └─ MicrosoftLearnCrawler ─┘
                                  │
                              HttpClient
                     rate limit / retries / size limit
```

Ограничения соединений задаются параметрами:

- `total_connections: 6` — общий предел;
- `connections_per_host: 1` — не более одного соединения к домену;
- задержки: Wikipedia 1 с, arXiv 3 с, Microsoft Learn 1 с;
- `request_timeout_seconds: 60`;
- максимум ответа 14 000 000 байт;
- четыре повторные попытки с экспоненциальной задержкой и jitter.

User-Agent содержит контактный email. Конфиг отвергается, если в User-Agent нет
контактного URL или `mailto:`.

Ключевой фрагмент запуска показывает уровень параллелизма: параллельны источники,
но не произвольные запросы одного сайта.

```python
tasks = []
for source_name, crawler_type in crawler_types.items():
    source_config = config["sources"].get(source_name, {})
    if not source_config.get("enabled", False):
        continue

    http = HttpClient(
        session,
        delay_seconds=float(logic["delay_seconds"][source_name]),
        retries=int(logic.get("retries", 4)),
        backoff_seconds=float(logic.get("backoff_seconds", 2)),
        max_response_bytes=int(logic.get("max_response_bytes", 14_000_000)),
        stop_event=stop_event,
    )
    crawler = crawler_type(config, source_config, storage, http, stop_event)
    tasks.append(asyncio.create_task(run_source(source_name, crawler)))

results = await asyncio.gather(*tasks)
```

`asyncio.create_task` даёт три независимые задачи. Собственный `HttpClient` у
каждого источника означает отдельную задержку, а общий `ClientSession` позволяет
переиспользовать соединения.

## 4. HTTP-слой

`HttpClient` отвечает за общие правила:

1. Перед каждым запросом ожидает разрешение `RateLimiter`.
2. Читает тело чанками по 64 KiB.
3. Прерывает чтение, если превышен `max_response_bytes`.
4. Повторяет ответы `429`, `500`, `502`, `503`, `504`.
5. Учитывает заголовок `Retry-After`.
6. Иначе использует backoff `base * 2^attempt + random jitter`.
7. Поддерживает GET и POST.
8. Ожидание можно немедленно прервать через `Ctrl+C`.

До начала обхода каждый источник загружает свой `robots.txt`. Если правила
невозможно проверить, источник завершается с понятной ошибкой: crawler не делает
предположение, что обход автоматически разрешён.

Ограничитель частоты защищён `asyncio.Lock`: даже если позднее внутри одного
источника появятся несколько coroutine, они не смогут одновременно пройти
задержку.

```python
class RateLimiter:
    def __init__(self, delay_seconds: float):
        self.delay_seconds = delay_seconds
        self._lock = asyncio.Lock()
        self._last_request = 0.0

    async def wait(self, stop_event: asyncio.Event | None = None) -> None:
        async with self._lock:
            elapsed = time.monotonic() - self._last_request
            remaining = self.delay_seconds - elapsed
            if remaining > 0:
                await wait_or_stop(stop_event, remaining)
            self._last_request = time.monotonic()
```

`time.monotonic()` выбран вместо системного времени: перевод часов не должен
сломать интервал между HTTP-запросами.

## 5. Wikipedia

### Обнаружение

Начальные категории заданы в `sources.wikipedia.seed_categories`. Через
MediaWiki API `categorymembers` получаются страницы и подкатегории. Подкатегории
обходятся до `max_category_depth: 6`.

### Очередь

Категории и страницы сохраняются в `crawl_queue`. Уникальный индекс по
`(source, kind, key)` не позволяет добавить одну задачу дважды.

Задача имеет состояния:

- `pending` — ожидает обработки;
- `processing` — взята worker с lease;
- `done` — завершена.

Если процесс аварийно завершился, lease истекает, и другой запуск может снова
взять задачу. Категории имеют меньший приоритет, чем реальные страницы, чтобы
очередь одновременно раскрывалась и скачивалась.

### Загрузка

Для страницы формируется канонический URL `/wiki/<title>`. Устойчивый `source_id`
равен MediaWiki `pageid`.

## 6. arXiv

### Почему обход сделан по месяцам

Legacy arXiv API начинает стабильно отвечать ошибками при больших глобальных
смещениях `start`. Поэтому для каждой категории строится запрос по диапазону
`submittedDate` одного месяца. Внутри месяца используется небольшое смещение.

Состояние хранит:

- категорию;
- месяц;
- `start` внутри месяца;
- позицию внутри текущего ответа;
- порядок сортировки.

После исчерпания месяца crawler переходит к предыдущему. Поэтому остановка не
заставляет начинать всё сначала, а глобальный предел `start=10000` не достигается.

### Получение HTML

API возвращает метаданные, после чего загружается
`https://arxiv.org/html/<id-version>`. В `source_id` версия `vN` удаляется, поэтому
новая версия статьи обновляет существующую запись, а не создаёт дубль.

### Повторная проверка

Для старых документов arXiv ID отправляются пакетным POST-запросом. Если сервер не
может обработать пакет, пакет рекурсивно делится пополам. Единичный проблемный ID
откладывается, но не останавливает остальные документы и источники.

HTML повторно скачивается только если поле `updated` изменилось или метаданные API
не содержат документ.

## 7. Microsoft Learn

### Обнаружение

Crawler читает sitemap index, выбирает только файлы `_en-us_`, затем читает
дочерние sitemap. Можно ограничить список `products`; сейчас он пустой, поэтому
разрешены все продукты кроме `excluded_products`.

Отбрасываются URL с путями `/answers/`, `/shows/`, `/events/`, `/users/`,
`/credentials/`, `/api/`, `/previous-versions/`.

### Продолжение обхода

В `crawl_state` сохраняются текущий sitemap и индекс URL. После остановки новый
запуск продолжает с этой позиции.

Если значение `lastmod` в sitemap не изменилось, HTML не скачивается заново:
обновляется только `checked_at`. Полное повторное обнаружение sitemap выполняется
после `discovery_interval_seconds`.

## 8. Дедупликация и обновление

В `documents` созданы два уникальных индекса:

```text
(source, source_id)
(source, url)
```

URL нормализуется:

- схема и домен приводятся к нижнему регистру;
- стандартные порты удаляются;
- повторные `/` схлопываются;
- fragment удаляется;
- tracking-параметры `utm_*`, `fbclid`, `gclid` и другие удаляются;
- query-параметры сортируются.

Для HTML вычисляется SHA-256 `content_hash`. Возможны результаты:

- `new` — документ впервые добавлен;
- `changed` — тот же документ получил новый HTML;
- `unchanged` — HTML скачан, но хеш совпал;
- `not_modified` — сервер вернул HTTP 304;
- `fresh` — срок повторной проверки ещё не наступил;
- `too_large` — ответ превысил ограничение;
- `robots` — URL запрещён.

Для условных запросов используются `ETag` и `Last-Modified` через заголовки
`If-None-Match` и `If-Modified-Since`.

Основной участок сохранения явно разделяет неизменившийся и изменившийся HTML:

```python
digest = sha256_text(raw_html)
existing = await self.get_document(source, source_id)

if existing and existing.get("content_hash") == digest:
    await self.documents.update_one(
        {"_id": existing["_id"]},
        {"$set": {
            "checked_at": now,
            "etag": etag or existing.get("etag"),
            "last_modified": last_modified or existing.get("last_modified"),
        }},
    )
    return "unchanged"

await self.documents.update_one(
    {"source": source, "source_id": source_id},
    {"$set": {
        "raw_html": raw_html,
        "content_hash": digest,
        "raw_size": len(raw_html.encode("utf-8")),
        "fetched_at": now,
        "checked_at": now,
    }},
    upsert=True,
)
```

`checked_at` меняется при любой успешной проверке, а `fetched_at` — только при
записи нового содержимого. Поэтому можно отдельно ответить на вопросы «когда
проверяли?» и «когда HTML реально изменился?».

## 9. Схема сырого документа

Основные поля `documents`:

| Поле | Значение |
|---|---|
| `_id` | MongoDB ObjectId |
| `source` | `wikipedia`, `arxiv` или `microsoft_learn` |
| `source_id` | устойчивый ID внутри источника |
| `url` | нормализованный URL |
| `title` | заголовок |
| `raw_html` | исходный HTML |
| `metadata` | специфичные метаданные источника |
| `content_hash` | SHA-256 HTML |
| `raw_size` | размер UTF-8 HTML в байтах |
| `etag` | HTTP ETag, если сервер его дал |
| `last_modified` | HTTP Last-Modified |
| `first_fetched_at` | время первой загрузки |
| `fetched_at` | время последнего изменения HTML |
| `checked_at` | время последней успешной проверки |

## 10. Логи и остановка

Начало лога содержит заголовок таблицы:

```text
SOURCE          | STATUS     |       SIZE | TITLE
wikipedia       | NEW        |  117.4 KiB | Uber Entertainment
```

Полный URL не печатается в каждой строке, чтобы таблица оставалась читаемой.
После завершения или `Ctrl+C` выводится статистика MongoDB по источникам: число
документов, суммарный, средний, минимальный и максимальный размер.

Ошибка одного источника регистрируется одной строкой без пользовательского
traceback. Остальные источники завершают работу. Общий exit code становится `1`,
что позволяет автоматизации заметить неполный запуск.

## 11. Проверка базы

Количество документов:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d = db.getSiblingDB("mai_ir");
printjson(d.documents.aggregate([
  {$group: {_id: "$source", documents: {$sum: 1}, bytes: {$sum: "$raw_size"}}},
  {$sort: {_id: 1}}
]).toArray());'
```

Один документ без вывода всего HTML:

```bash
docker exec mai-ir-mongodb mongosh --quiet --eval '
const d = db.getSiblingDB("mai_ir");
printjson(d.documents.findOne({}, {
  source: 1, source_id: 1, title: 1, url: 1,
  raw_size: 1, content_hash: 1, metadata: 1
}));'
```

Текущее распределение сырых документов:

| Источник | Документов | Суммарный HTML, байт | Средний HTML, байт |
|---|---:|---:|---:|
| arXiv | 11 038 | 3 390 833 246 | 307 196 |
| Microsoft Learn | 80 899 | 5 832 967 643 | 72 102 |
| Wikipedia | 78 456 | 13 822 205 661 | 176 178 |

## 12. Тестирование

Тесты проверяют:

- нормализацию URL и удаление tracking-параметров;
- стабильность SHA-256;
- парсинг sitemap и Atom arXiv;
- месячный запрос arXiv без большого глобального offset;
- обязательные параметры конфигурации;
- условный HTTP 304;
- пропуск недавно проверенного документа;
- ограничение размера ответа;
- пакетную перепроверку arXiv и деление ошибочного пакета;
- корректное чтение chunked response;
- немедленное прерывание backoff через `Ctrl+C`;
- отсутствие traceback при штатной остановке и ошибке источника.

## 13. Что показать преподавателю

1. `config.yaml`: User-Agent, задержки, категории и лимиты.
2. Запуск crawler и выровненный лог нескольких новых документов.
3. Остановку через `Ctrl+C` и итоговую таблицу.
4. Коллекции `documents`, `crawl_state`, `crawl_queue` в MongoDB.
5. Уникальные индексы MongoDB.
6. Повторный запуск: уже свежие или неизменившиеся документы не дублируются.
7. Тесты `uv run pytest -q tests/crawler`.

## 14. Ограничения

- Сетевые API могут временно возвращать ошибки независимо от crawler.
- `max_documents: null` означает отсутствие лимита одного запуска; для короткой
  демонстрации можно временно поставить небольшое число.
- Текущий запуск не является daemon: периодичность обеспечивает внешний scheduler.
- Тематика Microsoft Learn шире исходных seed-категорий Wikipedia и arXiv. Для
  более узкого корпуса можно заполнить `products`.

## 15. Подробная трассировка одного HTTP-запроса

Вызов начинается не в `HttpClient`, а в конкретном crawler. Например,
`WikipediaCrawler.process_page` передаёт `source_id`, URL, title и metadata в
`fetch_and_store`.

```text
process_page
  -> BaseCrawler.fetch_and_store
       -> MongoStorage.get_document
       -> BaseCrawler.is_due
       -> RobotsPolicy.allows
       -> BaseCrawler.conditional_headers
       -> HttpClient.get_text
            -> RateLimiter.wait
            -> ClientSession.request
            -> HttpClient._read_body
       -> MongoStorage.save_document
```

Разбор решений:

1. Existing document читается до HTTP, чтобы не обращаться к свежей странице.
2. Robots проверяется после нормализации URL.
3. ETag/Last-Modified добавляются только если уже известны.
4. RateLimiter выполняется на каждой попытке, включая retry.
5. Response body читается до decoding, чтобы ограничение считалось по bytes.
6. SHA-256 считается по decoded UTF-8 string; для одного и того же сохранённого
   текста результат стабилен.
7. Mongo upsert выполняется по `(source, source_id)`, а unique URL остаётся второй
   линией защиты.

## 16. Состояния документа и времени

Три timestamp отвечают на разные вопросы:

| Поле | Вопрос |
|---|---|
| `first_fetched_at` | когда документ впервые появился в корпусе? |
| `fetched_at` | когда последний раз изменился сохранённый HTML? |
| `checked_at` | когда источник последний раз успешно проверялся? |

Пример:

```text
день 1: insert
first_fetched_at = 100
fetched_at       = 100
checked_at       = 100

день 8: HTTP 304
first_fetched_at = 100
fetched_at       = 100
checked_at       = 800

день 15: HTML changed
first_fetched_at = 100
fetched_at       = 1500
checked_at       = 1500
```

Если использовать только одно поле, нельзя отличить давно неизменный, но недавно
проверенный документ от давно не проверявшегося.

## 17. Полная таблица решений об обновлении

| Existing | Due | Metadata changed | HTTP | Hash | Действие |
|---|---|---|---|---|---|
| нет | — | — | 200 | — | insert `new` |
| да | нет | — | запрос не делается | — | `fresh` |
| да | да | — | 304 | — | обновить `checked_at` |
| да | да | — | 200 | совпал | metadata + `checked_at`, `unchanged` |
| да | да | — | 200 | отличается | заменить HTML, `changed` |
| любой | — | — | 404/410 | — | документ не обновляется |
| любой | — | — | 429/5xx | — | retry/backoff |
| любой | — | — | body > limit | — | `too_large` |
| любой | — | — | robots deny | — | `robots` |

Для arXiv и Learn metadata может остановить процесс ещё раньше: неизменные
`updated` или sitemap `lastmod` позволяют не делать HTML GET.

## 18. Очередь Wikipedia подробно

Пример category task:

```javascript
{
  source: "wikipedia",
  kind: "category",
  key: "Computer science",
  title: "Computer science",
  depth: 0,
  priority: 20,
  status: "pending",
  attempts: 0,
  next_attempt_at: 1790000000
}
```

Пример page task:

```javascript
{
  source: "wikipedia",
  kind: "page",
  key: "5323",
  page_id: "5323",
  title: "Computer science",
  priority: 10,
  status: "processing",
  worker_id: "wikipedia-...",
  lease_until: 1790000300
}
```

Сортировка идёт по `priority` по возрастанию, поэтому page=10 обрабатывается раньше
category=20. Это не даёт crawler бесконечно раскрывать дерево, не скачивая уже
найденные страницы.

Lease решает сценарий:

```text
worker взял task -> status processing -> процесс погиб
                                      -> lease_until прошёл
                                      -> следующий worker снова claim task
```

`attempts` увеличивается при каждом claim. При обычной ошибке задача возвращается
в pending с `next_attempt_at = now + 60`.

## 19. Состояние arXiv подробно

Для каждой категории отдельный state, например `arxiv:cs.IR`:

```javascript
{
  _id: "arxiv:cs.IR",
  discovery_mode: "monthly",
  month: "2026-10",
  start: 300,
  entry_index: 17,
  sort_order: "descending",
  caught_up_at: null
}
```

Если остановка случилась после 17-й записи страницы API, новый запуск повторно
получит тот же batch, но продолжит с `entry_index=17`. Уже обработанные записи не
создадут дублей даже при повторе благодаря source_id.

Переходы:

```text
entries found -> process from entry_index -> start += len(entries)
empty result  -> month = previous_month -> start=0, entry_index=0
earliest hit  -> caught_up_at = now
```

## 20. Состояние Microsoft Learn подробно

Основной state:

```javascript
{
  _id: "microsoft_learn:main",
  sitemap_url: "https://learn.microsoft.com/...xml",
  url_index: 1842,
  completed_at: null
}
```

Состояние обновляется после каждого URL, включая исключённый. Иначе один
неподходящий URL зацикливал бы следующий запуск.

После последнего sitemap:

```text
sitemap_url = null
url_index = 0
completed_at = now
```

До истечения `discovery_interval_seconds` полный sitemap не обходится снова.

## 21. Сетевые сбои и их границы

### Retryable

`429`, `500`, `502`, `503`, `504`, `aiohttp.ClientError`, timeout. Они могут быть
временными, поэтому повторяются.

### Не retryable на общем уровне

Обычные `4xx`, кроме 429. Повтор того же запроса без изменения, скорее всего, не
поможет. `404` и `410` возвращаются как статус документа.

### Почему jitter

Если несколько клиентов получили 503 одновременно и ждут строго 2, 4, 8 секунд,
они снова атакуют сервер одновременно. Случайная добавка разносит повторы.

### Почему ошибка source не уничтожает процесс немедленно

`asyncio.gather` ждёт все wrapper tasks. `run_source` превращает exception в
результат. Поэтому два источника могут корректно закончить/сохраниться, после чего
общий код возвращает exit 1.

## 22. Оценка памяти и масштабирование

Crawler не хранит все документы в Python. Основные потенциально большие объекты:

- один HTTP body, ограниченный `max_response_bytes`;
- один Atom/API batch;
- один sitemap XML, для которого установлен отдельный больший limit;
- список URL конкретного набора sitemap;
- небольшие Mongo bulk operation lists.

Очередь Wikipedia находится в MongoDB. arXiv и Learn хранят позиции, а не полный
исторический список в памяти.

## 23. Контрольные вопросы с объяснениями

### Почему AsyncMongoClient?

Обычный синхронный запрос к Mongo блокировал бы event loop во время network I/O.
AsyncMongoClient позволяет другим source tasks работать, пока одна ждёт базу.

### Почему нельзя запустить 100 соединений и поставить sleep после batch?

Requests уже уйдут burst-ом до sleep. Rate limiter перед каждым request и
`limit_per_host=1` контролируют фактическую нагрузку.

### Что будет, если две копии crawler запустить одновременно?

Unique indexes защитят документы и queue keys. Lease не даст двум worker штатно
взять одну Wikipedia task. Но два arXiv/Learn discovery могут делать лишние
повторные запросы, поэтому эксплуатационно лучше один экземпляр.

### Почему SHA-256, а не сравнение строк?

Хеш фиксированного размера удобно хранить, индексировать и переносить downstream.
Вероятность практической коллизии SHA-256 пренебрежимо мала для задачи.

### Почему raw HTML, а не compressed blob?

Простая строка удобна для просмотра и повторного parser. Цена — место в MongoDB.
Сжатие хранения можно рассматривать отдельно, не смешивая его с логикой crawler.


