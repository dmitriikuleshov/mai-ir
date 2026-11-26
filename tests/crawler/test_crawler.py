import asyncio
import json
import logging
import subprocess
import sys
from pathlib import Path

import pytest
from aiohttp import ClientSession, web
from pymongo.errors import DuplicateKeyError

from crawler.main import (
    ArxivCrawler,
    BaseCrawler,
    HttpClient,
    HttpResult,
    MongoStorage,
    ResponseTooLarge,
    RateLimited,
    RobotsPolicy,
    SimpleLogFilter,
    StopRequested,
    WikipediaCrawler,
    arxiv_month_query,
    load_config,
    log_download_progress,
    normalize_url,
    parse_arxiv_entries,
    parse_xml_locations,
    previous_month,
    rate_limit_delay,
    run_source,
    sha256_text,
    shorten_log_value,
    wikipedia_category_is_technical,
)


class FakeStorage:
    def __init__(self):
        self.checked = False

    async def get_document(self, source, source_id):
        return None

    async def mark_checked(self, source, source_id):
        self.checked = True


class FakeHttp:
    def __init__(self, result):
        self.result = result
        self.calls = []
        self.max_response_bytes = 14_000_000

    async def get_text(self, url, **kwargs):
        self.calls.append((url, kwargs))
        if isinstance(self.result, Exception):
            raise self.result
        return self.result


class DummyCrawler(BaseCrawler):
    source = "dummy"


# Проверяется Retry-After в секундах и в формате HTTP-даты с нижней границей паузы.
@pytest.mark.parametrize("value, expected", [
    ("", 3600), ("bad date", 3600), ("30", 3600),
    ("7200", 7200), ("Thu, 01 Jan 1970 02:00:00 GMT", 6200),
])
def test_rate_limit_delay(value, expected, monkeypatch):
    monkeypatch.setattr("crawler.main.time.time", lambda: 1000)
    assert rate_limit_delay(value, 3600) == expected


# Проверяется сохранение паузы источника и отсутствие запросов при раннем повторном запуске.
def test_rate_limit_cooldown_survives_source_restart(caplog):
    class Storage:
        def __init__(self):
            self.state = {}

        async def get_state(self, source, key):
            assert key == "rate_limit"
            return self.state

        async def set_state(self, source, key, values):
            self.state = values

    class Crawler(DummyCrawler):
        calls = 0

        async def run(self):
            self.calls += 1
            raise RateLimited("https://example.com", 3600)

    storage = Storage()
    crawler = Crawler({"logic": {"user_agent": "test"}}, {}, storage, None, asyncio.Event())
    with caplog.at_level("INFO", logger="crawler"):
        assert asyncio.run(run_source("dummy", crawler)) is None
        assert asyncio.run(run_source("dummy", crawler)) is None
    assert crawler.calls == 1
    assert storage.state["retry_at"] > 0
    assert "HTTP 429" in caplog.text


# Проверяется, что HTTP 429 не вызывает серию повторов и сразу приостанавливает источник.
def test_http_429_stops_after_one_request():
    async def exercise():
        requests = 0

        async def handler(request):
            nonlocal requests
            requests += 1
            return web.Response(status=429, headers={"Retry-After": "7200"})

        app = web.Application()
        app.router.add_get("/limited", handler)
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        port = site._server.sockets[0].getsockname()[1]
        try:
            async with ClientSession() as session:
                http = HttpClient(
                    session, delay_seconds=0, retries=4,
                    backoff_seconds=2, max_response_bytes=100,
                )
                with pytest.raises(RateLimited) as error:
                    await http.get_text(f"http://127.0.0.1:{port}/limited")
                assert error.value.seconds == 7200
                assert requests == 1
        finally:
            await runner.cleanup()

    asyncio.run(exercise())


# Проверяется сохранение offset при ограничении пачек и продолжение категории в следующем запуске.
def test_arxiv_batch_budget_preserves_resume_position():
    class Storage(FakeStorage):
        def __init__(self):
            super().__init__()
            self.state = {}

        async def get_state(self, source, category):
            return self.state.copy()

        async def set_state(self, source, category, values):
            self.state = values.copy()

        async def save_document(self, **kwargs):
            return "new"

    class Http(FakeHttp):
        async def get_text(self, url, **kwargs):
            self.calls.append(kwargs["params"]["start"])
            number = len(self.calls)
            return HttpResult(
                status=200, url=url, etag=None, last_modified=None,
                text=f'''<feed xmlns="http://www.w3.org/2005/Atom"><entry>
                <id>https://arxiv.org/abs/2611.{number:05d}v1</id>
                <title>Technical paper</title><updated>2026-11-01</updated>
                </entry></feed>''',
            )

    storage, http = Storage(), Http(None)
    crawler = ArxivCrawler(
        {"logic": {"user_agent": "test"}},
        {"batch_size": 1, "category_batches_per_run": 2},
        storage, http, asyncio.Event(),
    )

    async def download(**kwargs):
        return "new"

    crawler.fetch_and_store = download
    assert asyncio.run(crawler.crawl_category("cs.CV", 0)) == 2
    assert http.calls == [0, 1]
    assert storage.state["start"] == 2
    assert storage.state["entry_index"] == 0
    assert asyncio.run(crawler.crawl_category("cs.CV", 0)) == 2
    assert http.calls == [0, 1, 2, 3]
    assert storage.state["start"] == 4


# Проверяются новые инженерные ветви и отсечение нетехнических категорий реальным конфигом.
@pytest.mark.parametrize("category, allowed", [
    ("Fluid mechanics", True), ("Semiconductor thin films", True),
    ("Chemical engineering", True), ("Stochastic processes", True),
    ("Computational biology", True), ("Materials scientists", False),
    ("Physics journals", False), ("Physics academic journals", False),
    ("Power (politics)", False), ("Mathematics awards", False),
    ("Inorganic compounds by element", True),
    ("Heterocyclic compounds", True), ("Human genes", True),
    ("Silicate minerals", True), ("Spiral galaxies", True),
    ("Variable stars", True), ("Atmospheric physics", True),
    ("Astronomers", False),
    ("Astronomy organizations", False), ("Biochemistry journals", False),
])
def test_expanded_wikipedia_technical_filter(category, allowed):
    config = load_config(Path("configs/crawler.yaml"))["sources"]["wikipedia"]
    assert wikipedia_category_is_technical(
        category, config["category_required_keywords"], config["category_excluded_keywords"]
    ) is allowed


# Проверяется добавление новых стартовых категорий при уже существующей очереди обхода.
def test_wikipedia_enqueues_seeds_on_every_start(monkeypatch):
    class Storage(FakeStorage):
        def __init__(self):
            super().__init__()
            self.tasks = []

        async def reset_due_tasks(self, *args):
            return 0

        async def task_count(self, *args):
            raise AssertionError("Наличие старых задач не должно мешать добавлению корней")

        async def enqueue_tasks(self, tasks):
            self.tasks.extend(tasks)

        async def claim_task(self, *args):
            return None

    class Crawler(WikipediaCrawler):
        async def recrawl_due_documents(self):
            return 0

    async def robots(*args):
        return None

    monkeypatch.setattr(RobotsPolicy, "load", robots)
    storage = Storage()
    crawler = Crawler(
        {"logic": {"user_agent": "test"}},
        {"discovery_version": 4, "seed_categories": ["Computer science", "Electronics"]},
        storage, FakeHttp(None), asyncio.Event(),
    )
    asyncio.run(crawler.run())
    assert [task["key"] for task in storage.tasks] == [
        "v4:Computer science", "v4:Electronics",
    ]
    assert all(task["priority"] == 0 for task in storage.tasks)


# Проверяется пакетное исключение свежих страниц с сохранением новых страниц и категорий.
@pytest.mark.parametrize("include_new_tasks", [False, True])
def test_wikipedia_queue_skips_only_fresh_pages(include_new_tasks):
    class Documents:
        def find(self, query, projection):
            assert query["source"] == "wikipedia"
            assert query["checked_at"] == {"$gt": 100}

            async def rows():
                yield {"source_id": "1"}
            return rows()

    class Queue:
        def __init__(self):
            self.operations = []

        async def bulk_write(self, operations, **kwargs):
            assert operations
            self.operations.extend(operations)

    storage = object.__new__(MongoStorage)
    storage.documents, storage.queue = Documents(), Queue()
    tasks = [{"source": "wikipedia", "kind": "page", "page_id": "1", "key": "v4:1"}]
    if include_new_tasks:
        tasks.extend([
            {"source": "wikipedia", "kind": "page", "page_id": "2", "key": "v4:2"},
            {"source": "wikipedia", "kind": "category", "key": "v4:Algorithms"},
        ])
    assert asyncio.run(storage.enqueue_tasks(tasks, skip_fresh_since=100)) == 1
    assert len(storage.queue.operations) == (2 if include_new_tasks else 0)


# Проверяется переход к следующей категории arXiv после HTTP 500 без сброса позиции первой.
def test_arxiv_continues_after_category_api_failure(monkeypatch, caplog):
    class Storage(FakeStorage):
        def __init__(self):
            super().__init__()
            self.states = []

        async def get_state(self, source, category):
            return {
                "discovery_mode": "monthly", "sort_order": "descending",
                "month": "2026-11", "start": 2000, "entry_index": 0,
            }

        async def set_state(self, source, category, values):
            self.states.append((category, values))

    class Http(FakeHttp):
        async def get_text(self, url, **kwargs):
            self.calls.append((url, kwargs))
            return HttpResult(
                status=500 if len(self.calls) == 1 else 200,
                text='<feed xmlns="http://www.w3.org/2005/Atom"/>',
                url=url, etag=None, last_modified=None,
            )

    class Crawler(ArxivCrawler):
        async def recrawl_due_documents(self):
            return 0

    async def robots(*args):
        return None

    monkeypatch.setattr(RobotsPolicy, "load", robots)
    storage, http = Storage(), Http(None)
    crawler = Crawler(
        {"logic": {"user_agent": "test"}},
        {"categories": ["cs.AI", "cs.DB"], "earliest_month": "2026-11"},
        storage, http, asyncio.Event(),
    )
    asyncio.run(crawler.run())
    assert len(http.calls) == 2
    assert http.calls[0][1]["params"]["start"] == 2000
    assert "cs.DB" in http.calls[1][1]["params"]["search_query"]
    assert all(category == "cs.DB" for category, _ in storage.states)
    assert "cs.AI отложена" in caplog.text


# Проверяется, что завершённая категория проверяет два последних месяца, а не весь архив.
def test_arxiv_completed_category_checks_only_recent_months(monkeypatch):
    class Storage(FakeStorage):
        def __init__(self):
            super().__init__()
            self.states = []

        async def get_state(self, source, category):
            return {"caught_up_at": 1}

        async def set_state(self, source, category, values):
            self.states.append(values)

    monkeypatch.setattr("crawler.main.current_month", lambda: "2026-11")
    storage = Storage()
    http = FakeHttp(HttpResult(
        status=200, text='<feed xmlns="http://www.w3.org/2005/Atom"/>',
        url="https://export.arxiv.org/api/query", etag=None, last_modified=None,
    ))
    crawler = ArxivCrawler(
        {"logic": {"user_agent": "test"}}, {}, storage, http, asyncio.Event()
    )
    assert asyncio.run(crawler.crawl_category("cs.IR", 0)) == 0
    assert len(http.calls) == 2
    assert "20261101" in http.calls[0][1]["params"]["search_query"]
    assert "20261001" in http.calls[1][1]["params"]["search_query"]
    assert all(state["scan_earliest_month"] == "2026-10" for state in storage.states)
    assert storage.states[-1]["caught_up_at"]


# Проверяется, что остановка arXiv по запросу не превращается в ошибку категории.
def test_arxiv_category_propagates_stop_requested():
    class Storage(FakeStorage):
        async def get_state(self, source, category):
            raise StopRequested

    crawler = ArxivCrawler(
        {"logic": {"user_agent": "test"}}, {},
        Storage(), FakeHttp(None), asyncio.Event(),
    )
    with pytest.raises(StopRequested):
        asyncio.run(crawler.crawl_category("cs.AI", 0))


# Проверяется пропуск дублирующего URL без повторной задачи и без учёта как загрузки.
def test_duplicate_url_is_skipped(caplog):
    class Storage(FakeStorage):
        async def save_document(self, **kwargs):
            raise DuplicateKeyError("duplicate URL", 11000, {
                "keyPattern": {"source": 1, "url": 1},
            })

    crawler = make_dummy_crawler(Storage(), FakeHttp(HttpResult(
        status=200, text="<p>Test</p>", url="https://example.com/test",
        etag=None, last_modified=None,
    )))
    result = asyncio.run(crawler.fetch_and_store(
        source_id="new-id", url="https://example.com/test", title="Test", metadata={}
    ))
    assert result == "duplicate_url"
    assert crawler.downloaded == 0
    assert "URL уже сохранён" in caplog.text


# Проверяется, что прочие конфликты уникальных ключей не скрываются как дубликаты URL.
def test_other_duplicate_key_errors_are_not_swallowed():
    class Storage(FakeStorage):
        async def save_document(self, **kwargs):
            raise DuplicateKeyError("duplicate ID", 11000, {
                "keyPattern": {"source": 1, "source_id": 1},
            })

    crawler = make_dummy_crawler(Storage(), FakeHttp(HttpResult(
        status=200, text="<p>Test</p>", url="https://example.com/test",
        etag=None, last_modified=None,
    )))
    with pytest.raises(DuplicateKeyError):
        asyncio.run(crawler.fetch_and_store(
            source_id="1", url="https://example.com/test", title="Test", metadata={}
        ))
    assert crawler.downloaded == 0


# Проверяется, что простой режим скрывает предупреждения, сохраняя статистику и ошибки.
@pytest.mark.parametrize("level, visible", [
    (logging.INFO, True), (logging.WARNING, False),
    (logging.ERROR, True), (logging.CRITICAL, True),
])
def test_simple_log_filter(level, visible):
    record = logging.LogRecord("crawler", level, __file__, 1, "Test", (), None)
    assert SimpleLogFilter().filter(record) is visible


# Проверяется, что simple скрывает документные логи и считает только успешные загрузки.
@pytest.mark.parametrize("result", ["new", "changed", "unchanged"])
def test_simple_mode_counts_downloads_without_document_logs(result, caplog):
    class Storage(FakeStorage):
        async def save_document(self, **kwargs):
            return result

    crawler = make_dummy_crawler(
        Storage(), FakeHttp(HttpResult(
            status=200, text="<p>Test</p>", url="https://example.com/test",
            etag=None, last_modified=None,
        ))
    )
    crawler.simple_log = True
    with caplog.at_level("DEBUG", logger="crawler"):
        asyncio.run(crawler.fetch_and_store(
            source_id="1", url="https://example.com/test", title="Test", metadata={}
        ))
    assert crawler.downloaded == 1
    assert caplog.text == ""


# Проверяется прирост за каждый интервал, общий итог и вывод остатка при остановке.
def test_simple_progress_reports_interval_deltas(monkeypatch, caplog):
    crawler = make_dummy_crawler(FakeStorage(), FakeHttp(None))
    calls = 0

    async def exercise():
        finished = asyncio.Event()

        async def wait_for_interval(awaitable, timeout):
            nonlocal calls
            awaitable.close()
            assert timeout == 60
            calls += 1
            crawler.downloaded += 3 if calls == 1 else 2
            if calls == 1:
                raise TimeoutError
            finished.set()

        monkeypatch.setattr(asyncio, "wait_for", wait_for_interval)
        await log_download_progress([crawler], finished)

    with caplog.at_level("INFO", logger="crawler"):
        asyncio.run(exercise())
    assert len(caplog.records) == 2
    assert "dummy: 3 | за запуск: 3" in caplog.records[0].getMessage()
    assert "dummy: 2 | за запуск: 5" in caplog.records[1].getMessage()
    assert "последний интервал" in caplog.records[1].getMessage()


def make_dummy_crawler(storage, http):
    config = {
        "logic": {
            "user_agent": "test-agent",
            "recrawl_after_seconds": 60,
            "recrawl_batch_size": 10,
        }
    }
    return DummyCrawler(config, {}, storage, http, asyncio.Event())


# Проверяется канонизация URL с удалением фрагмента и tracking-параметров.
def test_normalize_url_removes_fragment_and_tracking_parameters():
    url = "HTTPS://Learn.Microsoft.com:443/en-us/test/?b=2&utm_source=x&a=1#part"
    assert normalize_url(url) == "https://learn.microsoft.com/en-us/test?a=1&b=2"


# Проверяется стабильность SHA-256 и различие хешей разных документов.
def test_sha256_is_stable():
    assert sha256_text("document") == sha256_text("document")
    assert sha256_text("document") != sha256_text("another document")


# Проверяется очистка пробелов и сокращение длинного заголовка для лога.
def test_long_log_title_is_shortened():
    assert shorten_log_value("A   title\nwith spaces", 30) == "A title with spaces"
    assert shorten_log_value("A very long document title", 12) == "A very long…"


# Проверяется чтение URL и необязательной даты изменения из sitemap XML.
def test_parse_sitemap_locations_with_last_modified_date():
    xml = """<?xml version="1.0" encoding="UTF-8"?>
    <urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
      <url><loc>https://example.com/a</loc><lastmod>2026-01-01</lastmod></url>
      <url><loc>https://example.com/b</loc></url>
    </urlset>"""
    assert parse_xml_locations(xml) == [
        {"url": "https://example.com/a", "lastmod": "2026-01-01"},
        {"url": "https://example.com/b", "lastmod": None},
    ]


# Проверяется разбор метаданных одной публикации из Atom-ответа arXiv.
def test_parse_arxiv_atom_entry():
    xml = """<feed xmlns="http://www.w3.org/2005/Atom"
      xmlns:arxiv="http://arxiv.org/schemas/atom">
      <entry>
        <id>https://arxiv.org/abs/2601.00001v2</id>
        <title>  Test   paper </title>
        <summary> Test abstract </summary>
        <author><name>Student</name></author>
        <category term="cs.IR" />
        <published>2026-01-01T00:00:00Z</published>
        <updated>2026-01-02T00:00:00Z</updated>
        <arxiv:license>http://creativecommons.org/licenses/by/4.0/</arxiv:license>
      </entry>
    </feed>"""
    entry = parse_arxiv_entries(xml)[0]
    assert entry["arxiv_id"] == "2601.00001v2"
    assert entry["source_id"] == "2601.00001"
    assert entry["title"] == "Test paper"
    assert entry["categories"] == ["cs.IR"]


# Проверяется формирование месячного запроса arXiv без большого общего offset.
def test_arxiv_month_query_avoids_large_global_offset():
    assert previous_month("2026-01") == "2025-12"
    assert previous_month("2026-10") == "2026-09"
    assert arxiv_month_query("cs.IR", "2024-02") == (
        "cat:cs.IR AND submittedDate:[202402010000 TO 202402292359]"
    )


# Проверяется наличие обязательных настроек базы, логики и трёх источников.
def test_config_contains_required_sections():
    config = load_config(Path("configs/crawler.yaml"))
    assert config["db"]["database"] == "mai_ir"
    assert set(config["logic"]["delay_seconds"]) == {
        "wikipedia",
        "arxiv",
        "microsoft_learn",
    }


# Проверяется допуск технических категорий и отсечение посторонних ветвей Wikipedia.
def test_wikipedia_category_filter_keeps_technical_branches():
    required = ["computer", "algorithm", "human computer interaction"]
    excluded = ["magazine", "newspaper", "sports", "scientists", "fiction"]

    assert wikipedia_category_is_technical("Algorithms", required, excluded)
    assert wikipedia_category_is_technical(
        "Human–computer interaction", required, excluded
    )
    assert not wikipedia_category_is_technical(
        "Computer magazines", required, excluded
    )
    assert not wikipedia_category_is_technical(
        "Newspapers published in Australia", required, excluded
    )
    assert not wikipedia_category_is_technical(
        "Computer scientists", required, excluded
    )
    assert not wikipedia_category_is_technical(
        "Computer fiction", required, excluded
    )


# Проверяется, что обход Wikipedia сохраняет путь категорий и не ставит нетехнические ветви в очередь.
def test_wikipedia_category_tasks_are_versioned_and_filtered():
    class QueueStorage:
        def __init__(self):
            self.tasks = []

        async def enqueue_tasks(self, tasks, **kwargs):
            self.tasks.extend(tasks)
            return 0

    payload = {
        "query": {
            "categorymembers": [
                {"ns": 0, "pageid": 101, "title": "Binary search algorithm"},
                {"ns": 14, "pageid": 201, "title": "Category:Algorithms"},
                {
                    "ns": 14,
                    "pageid": 202,
                    "title": "Category:Computer magazines",
                },
                {
                    "ns": 14,
                    "pageid": 203,
                    "title": "Category:Australian newspapers",
                },
            ]
        }
    }
    storage = QueueStorage()
    http = FakeHttp(
        HttpResult(
            200,
            "https://en.wikipedia.org/w/api.php",
            json.dumps(payload),
            None,
            None,
        )
    )
    config = {
        "logic": {
            "user_agent": "test-agent",
            "recrawl_after_seconds": 60,
            "recrawl_batch_size": 10,
        }
    }
    source_config = {
        "max_category_depth": 3,
        "category_required_keywords": ["computer", "algorithm"],
        "category_excluded_keywords": ["magazine", "newspaper"],
    }
    crawler = WikipediaCrawler(
        config, source_config, storage, http, asyncio.Event()
    )
    task = {
        "title": "Computer science",
        "depth": 0,
        "root_category": "Computer science",
        "category_path": ["Computer science"],
        "discovery_version": 2,
    }

    asyncio.run(crawler.process_category(task))

    assert [(queued["kind"], queued["title"]) for queued in storage.tasks] == [
        ("page", "Binary search algorithm"),
        ("category", "Algorithms"),
    ]
    assert all(queued["key"].startswith("v2:") for queued in storage.tasks)
    assert storage.tasks[0]["discovery_category"] == "Computer science"
    assert storage.tasks[1]["category_path"] == ["Computer science", "Algorithms"]


# Проверяется отказ при неполной конфигурации crawler.
def test_invalid_config_is_rejected(tmp_path: Path):
    config_path = tmp_path / "bad.yaml"
    config_path.write_text("db: {}\nlogic: {}\nsources: {}\n", encoding="utf-8")
    with pytest.raises((TypeError, ValueError)):
        load_config(config_path)


# Проверяется обработка HTTP 304 и обновление времени проверки документа.
def test_conditional_request_marks_document_as_checked():
    storage = FakeStorage()
    http = FakeHttp(HttpResult(304, "https://example.com/a", "", "tag", None))
    crawler = make_dummy_crawler(storage, http)
    existing = {
        "source_id": "1",
        "url": "https://example.com/a",
        "etag": '"old-tag"',
        "checked_at": 0,
    }

    result = asyncio.run(
        crawler.fetch_and_store(
            source_id="1",
            url=existing["url"],
            title="Document",
            metadata={},
            existing=existing,
            force=True,
        )
    )

    assert result == "not_modified"
    assert storage.checked
    assert http.calls[0][1]["headers"] == {"If-None-Match": '"old-tag"'}


# Проверяется пропуск повторного скачивания недавно проверенного документа.
def test_recent_document_is_not_downloaded_again():
    storage = FakeStorage()
    http = FakeHttp(HttpResult(200, "https://example.com/a", "html", None, None))
    crawler = make_dummy_crawler(storage, http)
    existing = {"source_id": "1", "checked_at": 10**12}

    result = asyncio.run(
        crawler.fetch_and_store(
            source_id="1",
            url="https://example.com/a",
            title="Document",
            metadata={},
            existing=existing,
        )
    )

    assert result == "fresh"
    assert http.calls == []


# Проверяется пропуск слишком большого ответа без остановки всего источника.
def test_oversized_document_is_skipped_without_stopping_source():
    storage = FakeStorage()
    http = FakeHttp(ResponseTooLarge("response is too large"))
    crawler = make_dummy_crawler(storage, http)

    result = asyncio.run(
        crawler.fetch_and_store(
            source_id="large-1",
            url="https://example.com/large",
            title="Large document",
            metadata={},
        )
    )

    assert result == "too_large"
    assert len(http.calls) == 1


# Проверяется перепроверка arXiv по метаданным до повторного запроса HTML.
def test_arxiv_recrawl_uses_metadata_before_html():
    storage = FakeStorage()
    xml = """<feed xmlns="http://www.w3.org/2005/Atom">
      <entry>
        <id>https://arxiv.org/abs/2601.00001v2</id>
        <title>Test paper</title>
        <updated>2026-01-02T00:00:00Z</updated>
      </entry>
    </feed>"""
    http = FakeHttp(
        HttpResult(200, "https://export.arxiv.org/api/query", xml, None, None)
    )
    config = {
        "logic": {
            "user_agent": "test-agent",
            "recrawl_after_seconds": 60,
            "recrawl_batch_size": 10,
        }
    }
    crawler = ArxivCrawler(config, {}, storage, http, asyncio.Event())
    document = {
        "source_id": "2601.00001",
        "url": "https://arxiv.org/html/2601.00001v2",
        "metadata": {"arxiv_id": "2601.00001v2", "updated": "2026-01-02T00:00:00Z"},
    }

    processed = asyncio.run(crawler._recrawl_batch([document]))

    assert processed == 1
    assert storage.checked
    assert len(http.calls) == 1
    assert http.calls[0][1]["method"] == "POST"
    assert http.calls[0][1]["data"]["id_list"] == "2601.00001v2"


# Проверяется деление сбойного arXiv-пакета и откладывание отдельных документов.
def test_arxiv_failed_metadata_batch_is_split_and_deferred():
    storage = FakeStorage()
    http = FakeHttp(
        HttpResult(500, "https://export.arxiv.org/api/query", "", None, None)
    )
    config = {
        "logic": {
            "user_agent": "test-agent",
            "recrawl_after_seconds": 60,
            "recrawl_batch_size": 10,
        }
    }
    crawler = ArxivCrawler(config, {}, storage, http, asyncio.Event())
    documents = [
        {
            "source_id": f"2601.0000{index}",
            "url": f"https://arxiv.org/html/2601.0000{index}",
            "metadata": {},
        }
        for index in range(1, 5)
    ]

    processed = asyncio.run(crawler._recrawl_batch(documents))

    assert processed == 0
    assert len(http.calls) == 7
    assert [
        len(call[1]["data"]["id_list"].split(",")) for call in http.calls
    ] == [4, 2, 1, 1, 2, 1, 1]


# Проверяется успешный запуск справки консольной утилиты crawler.
def test_command_line_help():
    result = subprocess.run(
        [sys.executable, "src/crawler/main.py", "--help"],
        capture_output=True,
        check=False,
        text=True,
    )
    assert result.returncode == 0
    assert "config" in result.stdout


# Проверяется полное чтение HTTP-ответа, получаемого несколькими частями.
def test_http_client_reads_chunked_response_completely():
    async def scenario():
        async def handler(request):
            response = web.StreamResponse(headers={"Content-Type": "text/plain"})
            await response.prepare(request)
            await response.write(b"first-")
            await asyncio.sleep(0.01)
            await response.write(b"second")
            await response.write_eof()
            return response

        app = web.Application()
        app.router.add_get("/document", handler)
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        port = site._server.sockets[0].getsockname()[1]
        try:
            async with ClientSession() as session:
                http = HttpClient(
                    session,
                    delay_seconds=0,
                    retries=0,
                    backoff_seconds=0,
                    max_response_bytes=100,
                )
                result = await http.get_text(f"http://127.0.0.1:{port}/document")
                assert result.text == "first-second"
        finally:
            await runner.cleanup()

    asyncio.run(scenario())


# Проверяется немедленное прерывание ожидания повтора при запросе остановки.
def test_http_retry_delay_is_interrupted_by_stop_event():
    async def scenario():
        requests = 0

        async def handler(request):
            nonlocal requests
            requests += 1
            return web.Response(status=500)

        app = web.Application()
        app.router.add_get("/temporary-error", handler)
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        port = site._server.sockets[0].getsockname()[1]
        stop_event = asyncio.Event()
        try:
            async with ClientSession() as session:
                http = HttpClient(
                    session,
                    delay_seconds=0,
                    retries=4,
                    backoff_seconds=30,
                    max_response_bytes=100,
                    stop_event=stop_event,
                )
                task = asyncio.create_task(
                    http.get_text(f"http://127.0.0.1:{port}/temporary-error")
                )
                await asyncio.sleep(0.05)
                stop_event.set()
                with pytest.raises(StopRequested):
                    await asyncio.wait_for(task, timeout=1)
                assert requests == 1
        finally:
            await runner.cleanup()

    asyncio.run(scenario())


# Проверяется, что штатная остановка источника не журналируется как ошибка.
def test_stopped_source_is_not_reported_as_error(caplog):
    class StoppedCrawler:
        async def run(self):
            raise StopRequested

    result = asyncio.run(run_source("test", StoppedCrawler()))

    assert result is None
    assert "завершился с ошибкой" not in caplog.text


# Проверяется краткое журналирование ошибки источника без traceback в терминале.
def test_failed_source_is_logged_without_traceback(caplog):
    class FailedCrawler:
        async def run(self):
            raise RuntimeError("temporary failure")

    result = asyncio.run(run_source("test", FailedCrawler()))

    assert isinstance(result, RuntimeError)
    assert "temporary failure" in caplog.text
    assert "Traceback" not in caplog.text
