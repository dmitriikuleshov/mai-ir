import asyncio
import subprocess
import sys
from pathlib import Path

import pytest
from aiohttp import ClientSession, web

from crawler import (
    ArxivCrawler,
    BaseCrawler,
    HttpClient,
    HttpResult,
    ResponseTooLarge,
    load_config,
    normalize_url,
    parse_arxiv_entries,
    parse_xml_locations,
    sha256_text,
    shorten_log_value,
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


def make_dummy_crawler(storage, http):
    config = {
        "logic": {
            "user_agent": "test-agent",
            "recrawl_after_seconds": 60,
            "recrawl_batch_size": 10,
        }
    }
    return DummyCrawler(config, {}, storage, http, asyncio.Event())


def test_normalize_url_removes_fragment_and_tracking_parameters():
    url = "HTTPS://Learn.Microsoft.com:443/en-us/test/?b=2&utm_source=x&a=1#part"
    assert normalize_url(url) == "https://learn.microsoft.com/en-us/test?a=1&b=2"


def test_sha256_is_stable():
    assert sha256_text("document") == sha256_text("document")
    assert sha256_text("document") != sha256_text("another document")


def test_long_log_title_is_shortened():
    assert shorten_log_value("A   title\nwith spaces", 30) == "A title with spaces"
    assert shorten_log_value("A very long document title", 12) == "A very long…"


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


def test_config_contains_required_sections():
    config = load_config(Path("config.yaml"))
    assert config["db"]["database"] == "mai_ir"
    assert set(config["logic"]["delay_seconds"]) == {
        "wikipedia",
        "arxiv",
        "microsoft_learn",
    }


def test_invalid_config_is_rejected(tmp_path: Path):
    config_path = tmp_path / "bad.yaml"
    config_path.write_text("db: {}\nlogic: {}\nsources: {}\n", encoding="utf-8")
    with pytest.raises((TypeError, ValueError)):
        load_config(config_path)


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
    assert http.calls[0][1]["params"]["id_list"] == "2601.00001v2"


def test_command_line_help():
    result = subprocess.run(
        [sys.executable, "crawler.py", "--help"],
        capture_output=True,
        check=False,
        text=True,
    )
    assert result.returncode == 0
    assert "config" in result.stdout


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
