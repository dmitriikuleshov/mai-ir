from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import logging
import random
import re
import signal
import time
import xml.etree.ElementTree as ET
from collections.abc import AsyncIterator
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.parse import parse_qsl, quote, urlencode, urlsplit, urlunsplit
from urllib.robotparser import RobotFileParser

import aiohttp
import yaml
from bs4 import BeautifulSoup
from pymongo import ASCENDING, AsyncMongoClient, ReturnDocument, UpdateOne

LOGGER = logging.getLogger("crawler")
RETRYABLE_STATUSES = {429, 500, 502, 503, 504}
TRACKING_PARAMETERS = {"fbclid", "gclid", "mc_cid", "mc_eid", "wt.mc_id"}
LOG_TITLE_WIDTH = 90


def unix_time() -> int:
    return int(time.time())


def normalize_url(url: str) -> str:
    """Return one stable representation of an HTTP URL."""
    parts = urlsplit(url.strip())
    scheme = parts.scheme.lower() or "https"
    hostname = (parts.hostname or "").lower()
    port = parts.port
    if port and not (
        (scheme == "http" and port == 80) or (scheme == "https" and port == 443)
    ):
        hostname = f"{hostname}:{port}"

    path = re.sub(r"/{2,}", "/", parts.path or "/")
    if path != "/":
        path = path.rstrip("/")

    query = []
    for key, value in parse_qsl(parts.query, keep_blank_values=True):
        lowered = key.lower()
        if lowered.startswith("utm_") or lowered in TRACKING_PARAMETERS:
            continue
        query.append((key, value))
    query.sort()
    return urlunsplit((scheme, hostname, path, urlencode(query), ""))


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def shorten_log_value(value: str, width: int = LOG_TITLE_WIDTH) -> str:
    value = re.sub(r"\s+", " ", value).strip()
    if len(value) <= width:
        return value
    return f"{value[: width - 1].rstrip()}…"


def parse_xml_locations(xml_text: str) -> list[dict[str, str | None]]:
    root = ET.fromstring(xml_text.lstrip("\ufeff"))
    result = []
    for item in list(root):
        values = {child.tag.rsplit("}", 1)[-1]: child.text for child in list(item)}
        location = values.get("loc")
        if location:
            result.append({"url": location.strip(), "lastmod": values.get("lastmod")})
    return result


ATOM = "{http://www.w3.org/2005/Atom}"
ARXIV = "{http://arxiv.org/schemas/atom}"


def parse_arxiv_entries(xml_text: str) -> list[dict[str, Any]]:
    root = ET.fromstring(xml_text)
    entries = []
    for entry in root.findall(f"{ATOM}entry"):
        identifier = (entry.findtext(f"{ATOM}id") or "").rstrip("/").split("/")[-1]
        if not identifier:
            continue
        license_node = entry.find(f"{ARXIV}license")
        entries.append(
            {
                "arxiv_id": identifier,
                "source_id": re.sub(r"v\d+$", "", identifier),
                "title": re.sub(
                    r"\s+", " ", entry.findtext(f"{ATOM}title") or ""
                ).strip(),
                "abstract": re.sub(
                    r"\s+", " ", entry.findtext(f"{ATOM}summary") or ""
                ).strip(),
                "authors": [
                    node.findtext(f"{ATOM}name")
                    for node in entry.findall(f"{ATOM}author")
                ],
                "categories": [
                    node.attrib["term"] for node in entry.findall(f"{ATOM}category")
                ],
                "published": entry.findtext(f"{ATOM}published"),
                "updated": entry.findtext(f"{ATOM}updated"),
                "license": license_node.text if license_node is not None else None,
            }
        )
    return entries


def load_config(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        config = yaml.safe_load(stream)
    if not isinstance(config, dict):
        raise TypeError("YAML-конфигурация должна содержать объект верхнего уровня")

    for section in ("db", "logic", "sources"):
        if not isinstance(config.get(section), dict):
            raise TypeError(f"В конфигурации отсутствует секция {section!r}")

    for key in ("uri", "database"):
        if not config["db"].get(key):
            raise ValueError(f"В секции db отсутствует параметр {key!r}")

    delays = config["logic"].get("delay_seconds")
    if not isinstance(delays, dict):
        raise TypeError("logic.delay_seconds должен содержать задержки по источникам")
    user_agent = str(config["logic"].get("user_agent", ""))
    has_contact = (
        "http://" in user_agent or "https://" in user_agent or "mailto:" in user_agent
    )
    if not user_agent or not has_contact:
        raise ValueError("logic.user_agent должен содержать контактный URL или email")
    for source_name, source_config in config["sources"].items():
        if source_config.get("enabled", False) and source_name not in delays:
            raise ValueError(f"Не задана задержка для источника {source_name!r}")
        if source_name in delays and float(delays[source_name]) < 0:
            raise ValueError("Задержка между запросами не может быть отрицательной")
    return config


@dataclass(slots=True)
class HttpResult:
    status: int
    url: str
    text: str
    etag: str | None
    last_modified: str | None


class ResponseTooLarge(RuntimeError):
    pass


class StopRequested(RuntimeError):
    pass


class RateLimiter:
    def __init__(self, delay_seconds: float):
        self.delay_seconds = delay_seconds
        self._lock = asyncio.Lock()
        self._last_request = 0.0

    async def wait(self) -> None:
        async with self._lock:
            remaining = self.delay_seconds - (time.monotonic() - self._last_request)
            if remaining > 0:
                await asyncio.sleep(remaining)
            self._last_request = time.monotonic()


class HttpClient:
    def __init__(
        self,
        session: aiohttp.ClientSession,
        *,
        delay_seconds: float,
        retries: int,
        backoff_seconds: float,
        max_response_bytes: int,
    ):
        self.session = session
        self.rate_limiter = RateLimiter(delay_seconds)
        self.retries = retries
        self.backoff_seconds = backoff_seconds
        self.max_response_bytes = max_response_bytes

    @staticmethod
    async def _read_body(response: aiohttp.ClientResponse, limit: int) -> bytes:
        chunks = []
        size = 0
        async for chunk in response.content.iter_chunked(64 * 1024):
            size += len(chunk)
            if size > limit:
                raise ResponseTooLarge(f"Ответ {response.url} превышает {limit} байт")
            chunks.append(chunk)
        return b"".join(chunks)

    async def get_text(
        self,
        url: str,
        *,
        params: dict[str, Any] | None = None,
        headers: dict[str, str] | None = None,
        max_bytes: int | None = None,
    ) -> HttpResult:
        limit = max_bytes or self.max_response_bytes
        last_error: Exception | None = None
        for attempt in range(self.retries + 1):
            await self.rate_limiter.wait()
            try:
                async with self.session.get(
                    url,
                    params=params,
                    headers=headers,
                    allow_redirects=True,
                ) as response:
                    if response.status in RETRYABLE_STATUSES:
                        retry_after = response.headers.get("Retry-After", "")
                        if retry_after.isdigit():
                            wait_seconds = float(retry_after)
                        else:
                            wait_seconds = self.backoff_seconds * (2**attempt)
                            wait_seconds += random.uniform(0, self.backoff_seconds)
                        LOGGER.warning(
                            "%s вернул %s, повтор через %.1f с",
                            url,
                            response.status,
                            wait_seconds,
                        )
                        await response.read()
                        if attempt < self.retries:
                            await asyncio.sleep(wait_seconds)
                            continue

                    if response.status == 304:
                        return HttpResult(
                            status=304,
                            url=str(response.url),
                            text="",
                            etag=response.headers.get("ETag"),
                            last_modified=response.headers.get("Last-Modified"),
                        )

                    body = await self._read_body(response, limit)
                    encoding = response.charset or "utf-8"
                    text = body.decode(encoding, errors="replace")
                    return HttpResult(
                        status=response.status,
                        url=str(response.url),
                        text=text,
                        etag=response.headers.get("ETag"),
                        last_modified=response.headers.get("Last-Modified"),
                    )
            except (aiohttp.ClientError, TimeoutError) as error:
                last_error = error
                if attempt == self.retries:
                    raise
                wait_seconds = self.backoff_seconds * (2**attempt)
                wait_seconds += random.uniform(0, self.backoff_seconds)
                LOGGER.warning(
                    "Ошибка запроса %s: %s; повтор через %.1f с",
                    url,
                    error,
                    wait_seconds,
                )
                await asyncio.sleep(wait_seconds)
        if last_error:
            raise last_error
        raise RuntimeError(f"Не удалось получить {url}")


class RobotsPolicy:
    def __init__(self, parser: RobotFileParser | None, user_agent: str):
        self.parser = parser
        self.user_agent = user_agent

    @classmethod
    async def load(
        cls,
        http: HttpClient,
        robots_url: str,
        user_agent: str,
    ) -> RobotsPolicy:
        try:
            response = await http.get_text(robots_url, max_bytes=1_000_000)
            if response.status == 200:
                parser = RobotFileParser()
                parser.set_url(robots_url)
                parser.parse(response.text.splitlines())
                return cls(parser, user_agent)
            raise RuntimeError(f"robots.txt {robots_url} вернул HTTP {response.status}")
        except (
            aiohttp.ClientError,
            TimeoutError,
            ResponseTooLarge,
            RuntimeError,
        ) as error:
            raise RuntimeError(
                f"Не удалось проверить правила robots.txt для {robots_url}"
            ) from error

    def allows(self, url: str) -> bool:
        return self.parser is None or self.parser.can_fetch(self.user_agent, url)


class MongoStorage:
    def __init__(self, config: dict[str, Any]):
        self.client = AsyncMongoClient(config["uri"])
        database = self.client[config["database"]]
        self.documents = database[config.get("documents_collection", "documents")]
        self.states = database[config.get("state_collection", "crawl_state")]
        self.queue = database[config.get("queue_collection", "crawl_queue")]

    async def initialize(self) -> None:
        await self.client.admin.command("ping")
        await self.documents.create_index(
            [("source", ASCENDING), ("source_id", ASCENDING)],
            unique=True,
        )
        await self.documents.create_index(
            [("source", ASCENDING), ("url", ASCENDING)],
            unique=True,
        )
        await self.documents.create_index(
            [("source", ASCENDING), ("checked_at", ASCENDING)]
        )
        await self.queue.create_index(
            [("source", ASCENDING), ("kind", ASCENDING), ("key", ASCENDING)],
            unique=True,
        )
        await self.queue.create_index(
            [
                ("source", ASCENDING),
                ("status", ASCENDING),
                ("next_attempt_at", ASCENDING),
            ]
        )

    async def close(self) -> None:
        await self.client.close()

    async def get_document(self, source: str, source_id: str) -> dict[str, Any] | None:
        return await self.documents.find_one({"source": source, "source_id": source_id})

    async def save_document(
        self,
        *,
        source: str,
        source_id: str,
        url: str,
        raw_html: str,
        title: str,
        metadata: dict[str, Any],
        etag: str | None,
        last_modified: str | None,
    ) -> str:
        now = unix_time()
        normalized_url = normalize_url(url)
        digest = sha256_text(raw_html)
        existing = await self.get_document(source, source_id)
        old_metadata = existing.get("metadata", {}) if existing else {}
        merged_metadata = {**old_metadata, **metadata}

        if existing and existing.get("content_hash") == digest:
            await self.documents.update_one(
                {"_id": existing["_id"]},
                {
                    "$set": {
                        "url": normalized_url,
                        "title": title or existing.get("title", ""),
                        "metadata": merged_metadata,
                        "etag": etag or existing.get("etag"),
                        "last_modified": last_modified or existing.get("last_modified"),
                        "checked_at": now,
                    }
                },
            )
            return "unchanged"

        await self.documents.update_one(
            {"source": source, "source_id": source_id},
            {
                "$set": {
                    "source": source,
                    "source_id": source_id,
                    "url": normalized_url,
                    "title": title,
                    "raw_html": raw_html,
                    "metadata": merged_metadata,
                    "content_hash": digest,
                    "etag": etag,
                    "last_modified": last_modified,
                    "raw_size": len(raw_html.encode("utf-8")),
                    "fetched_at": now,
                    "checked_at": now,
                },
                "$setOnInsert": {"first_fetched_at": now},
            },
            upsert=True,
        )
        return "new" if existing is None else "changed"

    async def mark_checked(self, source: str, source_id: str) -> None:
        await self.documents.update_one(
            {"source": source, "source_id": source_id},
            {"$set": {"checked_at": unix_time()}},
        )

    async def document_statistics(self) -> list[dict[str, Any]]:
        pipeline = [
            {
                "$group": {
                    "_id": "$source",
                    "documents": {"$sum": 1},
                    "total_bytes": {"$sum": {"$ifNull": ["$raw_size", 0]}},
                    "average_bytes": {"$avg": {"$ifNull": ["$raw_size", 0]}},
                    "minimum_bytes": {"$min": {"$ifNull": ["$raw_size", 0]}},
                    "maximum_bytes": {"$max": {"$ifNull": ["$raw_size", 0]}},
                }
            },
            {"$sort": {"_id": 1}},
        ]
        statistics = []
        async for row in await self.documents.aggregate(pipeline):
            statistics.append(row)
        return statistics

    async def iter_due_documents(
        self,
        source: str,
        cutoff: int,
        limit: int,
    ) -> AsyncIterator[dict[str, Any]]:
        cursor = (
            self.documents.find(
                {"source": source, "checked_at": {"$lte": cutoff}},
            )
            .sort("checked_at", ASCENDING)
            .limit(limit)
        )
        async for document in cursor:
            yield document

    async def get_state(self, source: str, key: str = "main") -> dict[str, Any]:
        state_id = f"{source}:{key}"
        return await self.states.find_one({"_id": state_id}) or {"_id": state_id}

    async def set_state(self, source: str, key: str, values: dict[str, Any]) -> None:
        state_id = f"{source}:{key}"
        await self.states.update_one(
            {"_id": state_id},
            {"$set": {**values, "updated_at": unix_time()}},
            upsert=True,
        )

    async def enqueue_tasks(self, tasks: list[dict[str, Any]]) -> None:
        if not tasks:
            return
        now = unix_time()
        operations = []
        for task in tasks:
            document = {
                **task,
                "status": "pending",
                "attempts": 0,
                "next_attempt_at": now,
                "created_at": now,
            }
            operations.append(
                UpdateOne(
                    {
                        "source": task["source"],
                        "kind": task["kind"],
                        "key": task["key"],
                    },
                    {"$setOnInsert": document},
                    upsert=True,
                )
            )
        await self.queue.bulk_write(operations, ordered=False)

    async def claim_task(
        self,
        source: str,
        worker_id: str,
        lease_seconds: int,
    ) -> dict[str, Any] | None:
        now = unix_time()
        return await self.queue.find_one_and_update(
            {
                "source": source,
                "$or": [
                    {"status": "pending", "next_attempt_at": {"$lte": now}},
                    {"status": "processing", "lease_until": {"$lte": now}},
                ],
            },
            {
                "$set": {
                    "status": "processing",
                    "worker_id": worker_id,
                    "lease_until": now + lease_seconds,
                    "started_at": now,
                },
                "$inc": {"attempts": 1},
            },
            sort=[("priority", ASCENDING), ("_id", ASCENDING)],
            return_document=ReturnDocument.AFTER,
        )

    async def finish_task(self, task_id: Any) -> None:
        await self.queue.update_one(
            {"_id": task_id},
            {
                "$set": {"status": "done", "finished_at": unix_time()},
                "$unset": {"worker_id": "", "lease_until": ""},
            },
        )

    async def retry_task(self, task_id: Any, error: str, delay_seconds: int) -> None:
        await self.queue.update_one(
            {"_id": task_id},
            {
                "$set": {
                    "status": "pending",
                    "last_error": error[:1000],
                    "next_attempt_at": unix_time() + delay_seconds,
                },
                "$unset": {"worker_id": "", "lease_until": ""},
            },
        )

    async def task_count(self, source: str) -> int:
        return await self.queue.count_documents({"source": source})

    async def reset_due_tasks(self, source: str, cutoff: int) -> int:
        result = await self.queue.update_many(
            {
                "source": source,
                "kind": "category",
                "status": "done",
                "finished_at": {"$lte": cutoff},
            },
            {
                "$set": {"status": "pending", "next_attempt_at": unix_time()},
                "$unset": {"worker_id": "", "lease_until": ""},
            },
        )
        return result.modified_count


class BaseCrawler:
    source = "base"

    def __init__(
        self,
        config: dict[str, Any],
        source_config: dict[str, Any],
        storage: MongoStorage,
        http: HttpClient,
        stop_event: asyncio.Event,
    ):
        self.config = config
        self.source_config = source_config
        self.storage = storage
        self.http = http
        self.stop_event = stop_event
        self.user_agent = config["logic"]["user_agent"]
        self.recrawl_after = int(config["logic"].get("recrawl_after_seconds", 604800))
        self.recrawl_batch_size = int(config["logic"].get("recrawl_batch_size", 1000))
        self.max_documents = source_config.get("max_documents")
        self.robots: RobotsPolicy | None = None

    def is_due(self, document: dict[str, Any] | None) -> bool:
        if document is None:
            return True
        checked_at = int(document.get("checked_at", document.get("fetched_at", 0)))
        return checked_at <= unix_time() - self.recrawl_after

    @staticmethod
    def conditional_headers(document: dict[str, Any] | None) -> dict[str, str]:
        if not document:
            return {}
        headers = {}
        if document.get("etag"):
            headers["If-None-Match"] = document["etag"]
        if document.get("last_modified"):
            headers["If-Modified-Since"] = document["last_modified"]
        return headers

    async def fetch_and_store(
        self,
        *,
        source_id: str,
        url: str,
        title: str,
        metadata: dict[str, Any],
        existing: dict[str, Any] | None = None,
        force: bool = False,
    ) -> str:
        existing = existing or await self.storage.get_document(self.source, source_id)
        if not force and not self.is_due(existing):
            return "fresh"
        normalized_url = normalize_url(url)
        if self.robots and not self.robots.allows(normalized_url):
            LOGGER.warning("%s запрещён robots.txt", normalized_url)
            return "robots"

        try:
            response = await self.http.get_text(
                normalized_url,
                headers=self.conditional_headers(existing),
            )
        except ResponseTooLarge:
            LOGGER.warning(
                "%-15s | %-10s | %9s | %s",
                self.source,
                "TOO_LARGE",
                f">{self.http.max_response_bytes / 1024:.1f} KiB",
                shorten_log_value(title or source_id),
            )
            return "too_large"
        if response.status == 304:
            await self.storage.mark_checked(self.source, source_id)
            LOGGER.debug("%s | без изменений (304) | %s", self.source, normalized_url)
            return "not_modified"
        if response.status != 200:
            LOGGER.warning("%s вернул HTTP %s", normalized_url, response.status)
            if response.status in {404, 410}:
                return f"http_{response.status}"
            raise RuntimeError(f"{normalized_url} вернул HTTP {response.status}")

        if not title:
            soup = BeautifulSoup(response.text, "lxml")
            title_node = soup.select_one("h1") or soup.select_one("title")
            title = title_node.get_text(" ", strip=True) if title_node else ""

        result = await self.storage.save_document(
            source=self.source,
            source_id=source_id,
            url=normalized_url,
            raw_html=response.text,
            title=title,
            metadata=metadata,
            etag=response.etag,
            last_modified=response.last_modified,
        )
        if result in {"new", "changed"}:
            LOGGER.info(
                "%-15s | %-10s | %9.1f KiB | %s",
                self.source,
                result.upper(),
                len(response.text.encode("utf-8")) / 1024,
                shorten_log_value(title or source_id),
            )
        else:
            LOGGER.debug("%s | без изменений | %s", self.source, normalized_url)
        return result

    async def recrawl_due_documents(self) -> int:
        cutoff = unix_time() - self.recrawl_after
        processed = 0
        async for document in self.storage.iter_due_documents(
            self.source,
            cutoff,
            self.recrawl_batch_size,
        ):
            if self.stop_event.is_set():
                break
            try:
                await self.fetch_and_store(
                    source_id=document["source_id"],
                    url=document["url"],
                    title=document.get("title", ""),
                    metadata=document.get("metadata", {}),
                    existing=document,
                    force=True,
                )
                processed += 1
            except Exception as error:  # noqa: BLE001 - ошибка одного документа не останавливает обход
                LOGGER.warning(
                    "Не удалось перепроверить %s: %s", document["url"], error
                )
        if processed:
            LOGGER.info("%s: перепроверено документов: %s", self.source, processed)
        return processed


class WikipediaCrawler(BaseCrawler):
    source = "wikipedia"
    api_url = "https://en.wikipedia.org/w/api.php"

    async def run(self) -> None:
        self.robots = await RobotsPolicy.load(
            self.http,
            "https://en.wikipedia.org/robots.txt",
            self.user_agent,
        )
        await self.recrawl_due_documents()

        cutoff = unix_time() - self.recrawl_after
        reset = await self.storage.reset_due_tasks(self.source, cutoff)
        if reset:
            LOGGER.info("Wikipedia: повторно поставлено в очередь задач: %s", reset)

        if await self.storage.task_count(self.source) == 0:
            seeds = [
                {
                    "source": self.source,
                    "kind": "category",
                    "key": category,
                    "title": category,
                    "depth": 0,
                    "priority": 20,
                }
                for category in self.source_config["seed_categories"]
            ]
            await self.storage.enqueue_tasks(seeds)

        worker_id = f"wikipedia-{unix_time()}"
        lease_seconds = int(self.config["logic"].get("lease_seconds", 300))
        processed_pages = 0
        while not self.stop_event.is_set():
            if self.max_documents is not None and processed_pages >= int(
                self.max_documents
            ):
                break
            task = await self.storage.claim_task(self.source, worker_id, lease_seconds)
            if task is None:
                break
            try:
                if task["kind"] == "category":
                    await self.process_category(task)
                else:
                    await self.process_page(task)
                    processed_pages += 1
                await self.storage.finish_task(task["_id"])
            except StopRequested:
                await self.storage.retry_task(task["_id"], "Остановка по запросу", 0)
                break
            except Exception as error:
                LOGGER.exception("Ошибка задачи Wikipedia %s", task.get("key"))
                await self.storage.retry_task(task["_id"], str(error), 60)
        LOGGER.info("Wikipedia: обработано страниц за запуск: %s", processed_pages)

    async def process_category(self, task: dict[str, Any]) -> None:
        continuation: str | None = None
        while True:
            if self.stop_event.is_set():
                raise StopRequested
            params: dict[str, Any] = {
                "action": "query",
                "format": "json",
                "formatversion": 2,
                "list": "categorymembers",
                "cmtitle": f"Category:{task['title']}",
                "cmnamespace": "0|14",
                "cmtype": "page|subcat",
                "cmlimit": "max",
            }
            if continuation:
                params["cmcontinue"] = continuation
            response = await self.http.get_text(
                self.api_url,
                params=params,
                max_bytes=50_000_000,
            )
            if response.status != 200:
                raise RuntimeError(f"MediaWiki API вернул {response.status}")
            payload = json.loads(response.text)
            tasks = []
            for member in payload.get("query", {}).get("categorymembers", []):
                if member["ns"] == 0:
                    tasks.append(
                        {
                            "source": self.source,
                            "kind": "page",
                            "key": str(member["pageid"]),
                            "page_id": str(member["pageid"]),
                            "title": member["title"],
                            "priority": 10,
                        }
                    )
                elif task["depth"] < int(self.source_config["max_category_depth"]):
                    category = member["title"].removeprefix("Category:")
                    tasks.append(
                        {
                            "source": self.source,
                            "kind": "category",
                            "key": category,
                            "title": category,
                            "depth": task["depth"] + 1,
                            "priority": 20,
                        }
                    )
            await self.storage.enqueue_tasks(tasks)
            continuation = payload.get("continue", {}).get("cmcontinue")
            if not continuation:
                return

    async def process_page(self, task: dict[str, Any]) -> None:
        page_path = quote(task["title"].replace(" ", "_"), safe="()_,:-")
        url = f"https://en.wikipedia.org/wiki/{page_path}"
        await self.fetch_and_store(
            source_id=task["page_id"],
            url=url,
            title=task["title"],
            metadata={"page_id": task["page_id"]},
        )


class ArxivCrawler(BaseCrawler):
    source = "arxiv"
    api_url = "https://export.arxiv.org/api/query"

    async def recrawl_due_documents(self) -> int:
        """Check arXiv metadata in batches and fetch HTML only for changed papers."""
        cutoff = unix_time() - self.recrawl_after
        batch_size = int(self.source_config.get("metadata_batch_size", 100))
        batch: list[dict[str, Any]] = []
        processed = 0

        async for document in self.storage.iter_due_documents(
            self.source,
            cutoff,
            self.recrawl_batch_size,
        ):
            if self.stop_event.is_set():
                break
            batch.append(document)
            if len(batch) == batch_size:
                processed += await self._recrawl_batch(batch)
                batch = []

        if batch and not self.stop_event.is_set():
            processed += await self._recrawl_batch(batch)
        if processed:
            LOGGER.info("arxiv: перепроверено документов: %s", processed)
        return processed

    async def _recrawl_batch(self, documents: list[dict[str, Any]]) -> int:
        arxiv_ids = [
            str(document.get("metadata", {}).get("arxiv_id") or document["source_id"])
            for document in documents
        ]
        response = await self.http.get_text(
            self.api_url,
            params={"id_list": ",".join(arxiv_ids), "max_results": len(arxiv_ids)},
            max_bytes=50_000_000,
        )
        if response.status != 200:
            raise RuntimeError(f"arXiv API вернул {response.status}")
        entries = {
            entry["source_id"]: entry for entry in parse_arxiv_entries(response.text)
        }

        for document in documents:
            if self.stop_event.is_set():
                break
            entry = entries.get(document["source_id"])
            if entry is None:
                await self.fetch_and_store(
                    source_id=document["source_id"],
                    url=document["url"],
                    title=document.get("title", ""),
                    metadata=document.get("metadata", {}),
                    existing=document,
                    force=True,
                )
            elif document.get("metadata", {}).get("updated") == entry["updated"]:
                await self.storage.mark_checked(self.source, document["source_id"])
            else:
                await self.fetch_and_store(
                    source_id=document["source_id"],
                    url=f"https://arxiv.org/html/{entry['arxiv_id']}",
                    title=entry["title"],
                    metadata={**document.get("metadata", {}), **entry},
                    existing=document,
                    force=True,
                )
        return len(documents)

    async def run(self) -> None:
        self.robots = await RobotsPolicy.load(
            self.http,
            "https://arxiv.org/robots.txt",
            self.user_agent,
        )
        await self.recrawl_due_documents()
        processed = 0
        sort_order = str(self.source_config.get("sort_order", "descending"))
        for category in self.source_config["categories"]:
            if self.stop_event.is_set():
                break
            state = await self.storage.get_state(self.source, category)
            can_resume = state.get("sort_order") == sort_order and not state.get(
                "caught_up_at"
            )
            start = int(state.get("start", 0)) if can_resume else 0
            entry_index = int(state.get("entry_index", 0)) if can_resume else 0
            while not self.stop_event.is_set():
                if self.max_documents is not None and processed >= int(
                    self.max_documents
                ):
                    LOGGER.info("arXiv: достигнут лимит запуска %s", self.max_documents)
                    return
                response = await self.http.get_text(
                    self.api_url,
                    params={
                        "search_query": f"cat:{category}",
                        "start": start,
                        "max_results": int(self.source_config.get("batch_size", 100)),
                        "sortBy": "submittedDate",
                        "sortOrder": sort_order,
                    },
                    max_bytes=50_000_000,
                )
                if response.status != 200:
                    raise RuntimeError(f"arXiv API вернул {response.status}")
                entries = parse_arxiv_entries(response.text)
                if not entries:
                    await self.storage.set_state(
                        self.source,
                        category,
                        {
                            "start": start,
                            "entry_index": 0,
                            "sort_order": sort_order,
                            "caught_up_at": unix_time(),
                        },
                    )
                    break

                for index in range(entry_index, len(entries)):
                    if self.stop_event.is_set():
                        return
                    entry = entries[index]
                    source_id = entry["source_id"]
                    existing = await self.storage.get_document(self.source, source_id)
                    metadata = {**entry, "sample_category": category}
                    if (
                        existing
                        and existing.get("metadata", {}).get("updated")
                        == entry["updated"]
                    ):
                        await self.storage.mark_checked(self.source, source_id)
                    else:
                        await self.fetch_and_store(
                            source_id=source_id,
                            url=f"https://arxiv.org/html/{entry['arxiv_id']}",
                            title=entry["title"],
                            metadata=metadata,
                            existing=existing,
                            force=existing is not None,
                        )
                    processed += 1
                    await self.storage.set_state(
                        self.source,
                        category,
                        {
                            "start": start,
                            "entry_index": index + 1,
                            "sort_order": sort_order,
                            "caught_up_at": None,
                        },
                    )
                    if self.max_documents is not None and processed >= int(
                        self.max_documents
                    ):
                        return

                start += len(entries)
                entry_index = 0
                await self.storage.set_state(
                    self.source,
                    category,
                    {
                        "start": start,
                        "entry_index": 0,
                        "sort_order": sort_order,
                        "caught_up_at": None,
                    },
                )
        LOGGER.info("arXiv: обработано записей за запуск: %s", processed)


class MicrosoftLearnCrawler(BaseCrawler):
    source = "microsoft_learn"
    sitemap_index = "https://learn.microsoft.com/_sitemaps/sitemapindex.xml"

    def suitable_url(self, url: str) -> bool:
        excluded = tuple(self.source_config.get("excluded_paths", []))
        return url.startswith("https://learn.microsoft.com/en-us/") and not any(
            part in url for part in excluded
        )

    async def run(self) -> None:
        self.robots = await RobotsPolicy.load(
            self.http,
            "https://learn.microsoft.com/robots.txt",
            self.user_agent,
        )
        state = await self.storage.get_state(self.source)
        discovery_interval = int(
            self.source_config.get("discovery_interval_seconds", self.recrawl_after)
        )
        if (
            state.get("completed_at")
            and int(state["completed_at"]) > unix_time() - discovery_interval
        ):
            LOGGER.info("Microsoft Learn: sitemap уже полностью обработан")
            return

        response = await self.http.get_text(self.sitemap_index, max_bytes=50_000_000)
        if response.status != 200:
            raise RuntimeError(f"Sitemap index вернул {response.status}")
        sitemap_records = parse_xml_locations(response.text)
        products = set(self.source_config.get("products", []))
        excluded_products = set(self.source_config.get("excluded_products", []))
        sitemap_urls = []
        for record in sitemap_records:
            url = str(record["url"])
            filename = url.rsplit("/", 1)[-1]
            if "_en-us_" not in filename:
                continue
            product = filename.split("_en-us_", 1)[0]
            if products and product not in products:
                continue
            if product in excluded_products:
                continue
            sitemap_urls.append(url)

        start_sitemap = state.get("sitemap_url")
        sitemap_index = (
            sitemap_urls.index(start_sitemap) if start_sitemap in sitemap_urls else 0
        )
        url_index = (
            int(state.get("url_index", 0)) if start_sitemap in sitemap_urls else 0
        )
        processed = 0

        for current_sitemap_index in range(sitemap_index, len(sitemap_urls)):
            if self.stop_event.is_set():
                return
            sitemap_url = sitemap_urls[current_sitemap_index]
            sitemap_response = await self.http.get_text(
                sitemap_url, max_bytes=100_000_000
            )
            if sitemap_response.status != 200:
                raise RuntimeError(
                    f"Sitemap {sitemap_url} вернул {sitemap_response.status}"
                )
            records = parse_xml_locations(sitemap_response.text)
            first_index = url_index if current_sitemap_index == sitemap_index else 0
            for current_url_index in range(first_index, len(records)):
                if self.stop_event.is_set():
                    return
                if self.max_documents is not None and processed >= int(
                    self.max_documents
                ):
                    return
                record = records[current_url_index]
                url = str(record["url"])
                if not self.suitable_url(url):
                    await self.storage.set_state(
                        self.source,
                        "main",
                        {
                            "sitemap_url": sitemap_url,
                            "url_index": current_url_index + 1,
                        },
                    )
                    continue

                source_id = normalize_url(url)
                existing = await self.storage.get_document(self.source, source_id)
                sitemap_lastmod = record.get("lastmod")
                if (
                    existing
                    and sitemap_lastmod
                    and existing.get("metadata", {}).get("sitemap_lastmod")
                    == sitemap_lastmod
                ):
                    await self.storage.mark_checked(self.source, source_id)
                else:
                    await self.fetch_and_store(
                        source_id=source_id,
                        url=url,
                        title="",
                        metadata={
                            "sitemap": sitemap_url,
                            "sitemap_lastmod": sitemap_lastmod,
                        },
                        existing=existing,
                        force=existing is not None,
                    )
                processed += 1
                await self.storage.set_state(
                    self.source,
                    "main",
                    {"sitemap_url": sitemap_url, "url_index": current_url_index + 1},
                )

            url_index = 0
            next_sitemap = (
                sitemap_urls[current_sitemap_index + 1]
                if current_sitemap_index + 1 < len(sitemap_urls)
                else None
            )
            await self.storage.set_state(
                self.source,
                "main",
                {"sitemap_url": next_sitemap, "url_index": 0},
            )

        await self.storage.set_state(
            self.source,
            "main",
            {"sitemap_url": None, "url_index": 0, "completed_at": unix_time()},
        )
        LOGGER.info("Microsoft Learn: обработано URL за запуск: %s", processed)


async def run_source(name: str, crawler: BaseCrawler) -> Exception | None:
    try:
        LOGGER.info("Запуск источника %s", name)
        await crawler.run()
        LOGGER.info("Источник %s остановлен", name)
        return None
    except Exception as error:
        LOGGER.exception("Источник %s завершился с ошибкой", name)
        return error


async def log_database_statistics(storage: MongoStorage) -> None:
    statistics = await storage.document_statistics()
    LOGGER.info("Статистика документов в MongoDB:")
    LOGGER.info(
        "%-15s | %10s | %12s | %11s | %11s | %11s",
        "SOURCE",
        "DOCUMENTS",
        "TOTAL MiB",
        "AVG KiB",
        "MIN KiB",
        "MAX KiB",
    )
    if not statistics:
        LOGGER.info("Документы отсутствуют")
        return

    total_documents = 0
    total_bytes = 0
    minimum_bytes = min(int(row["minimum_bytes"]) for row in statistics)
    maximum_bytes = max(int(row["maximum_bytes"]) for row in statistics)
    for row in statistics:
        documents = int(row["documents"])
        source_bytes = int(row["total_bytes"])
        total_documents += documents
        total_bytes += source_bytes
        LOGGER.info(
            "%-15s | %10d | %12.1f | %11.1f | %11.1f | %11.1f",
            row["_id"],
            documents,
            source_bytes / (1024 * 1024),
            float(row["average_bytes"]) / 1024,
            int(row["minimum_bytes"]) / 1024,
            int(row["maximum_bytes"]) / 1024,
        )

    LOGGER.info(
        "%-15s | %10d | %12.1f | %11.1f | %11.1f | %11.1f",
        "TOTAL",
        total_documents,
        total_bytes / (1024 * 1024),
        total_bytes / total_documents / 1024,
        minimum_bytes / 1024,
        maximum_bytes / 1024,
    )


async def run(config: dict[str, Any]) -> None:
    logic = config["logic"]
    logging.basicConfig(
        level=getattr(logging, str(logic.get("log_level", "INFO")).upper()),
        format="%(asctime)s | %(levelname)-8s | %(message)s",
    )
    LOGGER.info(
        "%-15s | %-10s | %10s | %s",
        "SOURCE",
        "STATUS",
        "SIZE",
        "TITLE",
    )
    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()
    for signal_name in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(signal_name, stop_event.set)
        except NotImplementedError:
            pass

    storage = MongoStorage(config["db"])
    await storage.initialize()
    timeout = aiohttp.ClientTimeout(
        total=float(logic.get("request_timeout_seconds", 60))
    )
    connector = aiohttp.TCPConnector(
        limit=int(logic.get("total_connections", 6)),
        limit_per_host=int(logic.get("connections_per_host", 1)),
        ttl_dns_cache=300,
    )
    headers = {"User-Agent": logic["user_agent"], "Accept-Encoding": "gzip, deflate"}

    try:
        async with aiohttp.ClientSession(
            timeout=timeout,
            connector=connector,
            headers=headers,
        ) as session:
            crawler_types = {
                "wikipedia": WikipediaCrawler,
                "arxiv": ArxivCrawler,
                "microsoft_learn": MicrosoftLearnCrawler,
            }
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
                )
                crawler = crawler_type(config, source_config, storage, http, stop_event)
                tasks.append(asyncio.create_task(run_source(source_name, crawler)))

            results = await asyncio.gather(*tasks)
            errors = [result for result in results if result is not None]
            if errors:
                raise RuntimeError(f"Ошибок источников: {len(errors)}")
    finally:
        try:
            await log_database_statistics(storage)
        finally:
            await storage.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Поисковый робот MAI-IR")
    parser.add_argument("config", type=Path, help="путь к YAML-конфигурации")
    arguments = parser.parse_args()
    config = load_config(arguments.config)
    asyncio.run(run(config))


if __name__ == "__main__":
    main()
