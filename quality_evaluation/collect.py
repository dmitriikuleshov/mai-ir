"""Сохраняет реальные выдачи и тексты для воспроизводимой оценки качества.

Запуск из корня: uv run python quality_evaluation/collect.py
Существующие выдачи не перезапрашиваются: для нового эксперимента нужен новый каталог.
"""

from __future__ import annotations

import base64
import argparse
import hashlib
import json
import re
import struct
import subprocess
import time
import xml.etree.ElementTree as ET
from datetime import UTC, datetime
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlencode, urlsplit, urlunsplit

import requests
import yaml
from bson import ObjectId
from bs4 import BeautifulSoup
from pymongo import MongoClient

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parent
HEADERS = {"User-Agent": "MAI-IR-quality-evaluation/1.0 (mailto:workdimila@gmail.com)"}
SCOPE = "(site:en.wikipedia.org OR site:arxiv.org/html OR site:learn.microsoft.com/en-us)"


def save(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def canonical(url: str) -> str:
    p = urlsplit(url)
    path = unquote(p.path).rstrip("/")
    # abs, HTML и PDF разных версий одной статьи не создают дубли в пуле оценок.
    if p.hostname in {"arxiv.org", "export.arxiv.org"}:
        match = re.fullmatch(r"/(?:abs|html|pdf)/(.+?)(?:\.pdf)?", path)
        if match:
            identifier = re.sub(r"v\d+$", "", match.group(1))
            return "https://arxiv.org/abs/" + identifier
    query = [(k, v) for k, values in parse_qs(p.query).items() for v in values
             if not k.startswith("utm_") and k not in {"WT.mc_id", "wt.mc_id"}]
    return urlunsplit(("https", p.netloc.lower(), path, urlencode(sorted(query)), ""))


def allowed(url: str) -> bool:
    p = urlsplit(url)
    return ((p.hostname == "en.wikipedia.org" and p.path.startswith("/wiki/"))
            or (p.hostname == "arxiv.org" and p.path.startswith("/html/"))
            or (p.hostname == "learn.microsoft.com" and p.path.startswith("/en-us/")))


def bing_url(url: str) -> str:
    if url.startswith("https://www.bing.com/ck/a"):
        value = parse_qs(urlsplit(url).query).get("u", [""])[0]
        if value.startswith("a1"):
            value = value[2:]
            return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4)).decode()
    return url


def snapshot() -> None:
    index = ROOT / "results/boolean_index"
    with (index / "lexicon.bin").open("rb") as stream:
        magic, version, documents, terms, postings = struct.unpack("<8sIIQQ", stream.read(32))
    files = {}
    for path in sorted(index.glob("*.bin")):
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
                digest.update(block)
        files[path.name] = {"bytes": path.stat().st_size, "sha256": digest.hexdigest()}
    save(DIRECTORY / "snapshot.json", {
        "started_at": datetime.now(UTC).isoformat(), "magic": magic.decode(),
        "index_version": version, "indexed_documents": documents,
        "terms": terms, "postings": postings, "files": files,
        "queries_sha256": hashlib.sha256((DIRECTORY / "queries.json").read_bytes()).hexdigest(),
        "search_config": yaml.safe_load((ROOT / "configs/search.yaml").read_text()),
        "linguistics_statistics": json.loads((ROOT / "results/statistics.json").read_text()),
        "judgment_pool_depth": 5, "retrieval_depth": 10,
    })


def local_run(queries: list[dict]) -> None:
    target = DIRECTORY / "runs/tfidf.json"
    if target.exists():
        return
    result = subprocess.run(
        [str(ROOT / ".build/tfidf_search"), "configs/search.yaml"], cwd=ROOT,
        input="\n".join(q["query"] for q in queries) + "\n",
        text=True, capture_output=True, timeout=900,
    )
    (DIRECTORY / "raw").mkdir(exist_ok=True)
    (DIRECTORY / "raw/tfidf_stdout.txt").write_text(result.stdout, encoding="utf-8")
    (DIRECTORY / "raw/tfidf_stderr.txt").write_text(result.stderr, encoding="utf-8")
    if result.returncode or "Ошибка" in result.stdout:
        raise RuntimeError(f"TF-IDF завершился с ошибкой: {result.stderr}")
    blocks = re.split(r"^Запрос: ", result.stdout, flags=re.M)[1:]
    if len(blocks) != len(queries):
        raise RuntimeError("Число ответов TF-IDF не совпадает с числом запросов")
    runs = {}
    pattern = re.compile(
        r"^(\d+)\. (.*)\n   score=([\d.]+) \| источник: (.*?) \| doc=(\d+) \| oid=([\da-f]+)\n   (https?://[^\n]+)",
        re.M,
    )
    for query, block in zip(queries, blocks):
        if block.splitlines()[0] != query["query"]:
            raise RuntimeError("Нарушен порядок ответов")
        hits = [{"rank": int(m[0]), "title": m[1], "score": float(m[2]),
                 "source": m[3], "doc_id": int(m[4]), "oid": m[5], "url": m[6]}
                for m in pattern.findall(block)][:10]
        runs[query["id"]] = {"status": "ok", "query": query["query"], "results": hits}
    save(target, runs)


def arxiv_search_query(query: str) -> str:
    """Сохраняет исходные термы и специальные имена; все термы соединяются AND."""
    return " AND ".join('all:"' + term.replace('"', '\\"') + '"'
                        for term in query.split())


def parse_arxiv_feed(text: str) -> tuple[list[dict], int]:
    """Разбирает порядок Atom-выдачи; ошибка API не считается пустой выдачей."""
    root = ET.fromstring(text)
    namespace = {"atom": "http://www.w3.org/2005/Atom",
                 "os": "http://a9.com/-/spec/opensearch/1.1/"}
    total = root.findtext("os:totalResults", namespaces=namespace)
    hits, seen = [], set()
    for entry in root.findall("atom:entry", namespace):
        url = entry.findtext("atom:id", "", namespace)
        if "api/errors" in url:
            raise ValueError(entry.findtext("atom:summary", "Ошибка arXiv API", namespace))
        key = canonical(url)
        if key in seen:
            continue
        seen.add(key)
        abstract = " ".join(entry.findtext("atom:summary", "", namespace).split())
        hits.append({"rank": len(hits) + 1,
                     "title": " ".join(entry.findtext("atom:title", "", namespace).split()),
                     "url": url.replace("http://", "https://", 1),
                     "snippet": abstract, "abstract": abstract,
                     "published": entry.findtext("atom:published", "", namespace),
                     "updated": entry.findtext("atom:updated", "", namespace)})
    if total is None:
        raise ValueError("arXiv API не вернул totalResults")
    return hits, int(total)


def external_runs(queries: list[dict], engines=("wikipedia", "bing", "microsoft_learn", "arxiv")) -> None:
    session = requests.Session()
    session.headers.update(HEADERS)
    for engine in engines:
        target = DIRECTORY / f"runs/{engine}.json"
        runs = json.loads(target.read_text()) if target.exists() else {}
        for query in queries:
            qid = query["id"]
            if qid in runs:
                continue
            started = datetime.now(UTC).isoformat()
            try:
                total_results = None
                if engine == "arxiv":
                    response = session.get("https://export.arxiv.org/api/query", params={
                        "search_query": arxiv_search_query(query["query"]),
                        "start": 0, "max_results": 10,
                        "sortBy": "relevance", "sortOrder": "descending",
                    }, timeout=40)
                    if response.status_code != 200:
                        raise RuntimeError(f"arXiv: HTTP {response.status_code}")
                    hits, total_results = parse_arxiv_feed(response.text)
                elif engine == "wikipedia":
                    response = session.get("https://en.wikipedia.org/w/api.php", params={
                        "action": "query", "format": "json", "list": "search",
                        "srsearch": query["query"], "srlimit": 10, "srprop": "snippet",
                    }, timeout=30)
                    payload = response.json()
                    if response.status_code != 200 or "error" in payload:
                        raise RuntimeError(f"Wikipedia: HTTP {response.status_code}: {payload}")
                    hits = [{"rank": i, "title": hit["title"], "page_id": str(hit["pageid"]),
                             "url": "https://en.wikipedia.org/wiki/" + hit["title"].replace(" ", "_"),
                             "snippet": BeautifulSoup(hit.get("snippet", ""), "lxml").get_text(" ", strip=True)}
                            for i, hit in enumerate(payload["query"]["search"], 1)]
                elif engine == "microsoft_learn":
                    response = session.get("https://learn.microsoft.com/api/search", params={
                        "search": query["query"], "locale": "en-us", "$top": 10,
                    }, timeout=30)
                    if response.status_code != 200:
                        raise RuntimeError(f"Microsoft Learn: HTTP {response.status_code}")
                    hits, seen = [], set()
                    for original_rank, hit in enumerate(response.json().get("results", []), 1):
                        url = hit.get("url", "")
                        key = canonical(url)
                        if not allowed(url) or key in seen:
                            continue
                        seen.add(key)
                        hits.append({"rank": len(hits) + 1, "original_rank": original_rank,
                                     "title": hit.get("title", ""), "url": url,
                                     "snippet": hit.get("description", "")})
                else:
                    response = session.get("https://www.bing.com/search", params={
                        "q": query["query"] + " " + SCOPE, "count": 50, "setlang": "en",
                    }, timeout=30)
                    soup = BeautifulSoup(response.text, "lxml")
                    if response.status_code != 200 or "captcha" in response.url.lower():
                        raise RuntimeError(f"Bing: HTTP {response.status_code}, {response.url}")
                    organic = soup.select("li.b_algo")
                    if not organic:
                        raise RuntimeError("Не найдены органические результаты Bing; это не нулевая релевантность")
                    hits, seen = [], set()
                    for raw_rank, node in enumerate(organic, 1):
                        anchor = node.select_one("h2 a")
                        if anchor is None:
                            continue
                        url = bing_url(anchor.get("href", ""))
                        key = canonical(url)
                        if not allowed(url) or key in seen:
                            continue
                        seen.add(key)
                        snippet = node.select_one(".b_caption p") or node.select_one("p")
                        hits.append({"rank": len(hits) + 1, "original_rank": raw_rank,
                                     "title": anchor.get_text(" ", strip=True), "url": url,
                                     "snippet": snippet.get_text(" ", strip=True) if snippet else ""})
                    hits = hits[:10]
                suffix = "xml" if engine == "arxiv" else "html" if engine == "bing" else "json"
                raw_path = DIRECTORY / f"raw/{engine}_{qid}.{suffix}"
                raw_path.parent.mkdir(exist_ok=True)
                raw_path.write_text(response.text, encoding="utf-8")
                runs[qid] = {"status": "ok", "query": query["query"], "collected_at": started,
                             "request_url": response.url, "results": hits}
                if total_results is not None:
                    runs[qid]["total_results"] = total_results
            except (requests.RequestException, ET.ParseError, ValueError, RuntimeError) as error:
                runs[qid] = {"status": "unavailable", "query": query["query"],
                             "collected_at": started, "error": str(error), "results": []}
            save(target, runs)
            print(engine, qid, runs[qid]["status"], len(runs[qid]["results"]), flush=True)
            time.sleep(3.2 if engine == "arxiv" else 2)


def documents(queries: list[dict]) -> None:
    target = DIRECTORY / "documents.json"
    saved = json.loads(target.read_text()) if target.exists() else {}
    runs = {p.stem: json.loads(p.read_text()) for p in (DIRECTORY / "runs").glob("*.json")}
    db = MongoClient("mongodb://localhost:27017").mai_ir
    session = requests.Session()
    session.headers.update(HEADERS)
    for query in queries:
        for engine, run in runs.items():
            for hit in run[query["id"]]["results"][:5]:
                key = canonical(hit["url"])
                if key in saved:
                    continue
                source = ("wikipedia" if "en.wikipedia.org" in key else
                          "arxiv" if "arxiv.org" in key else "microsoft_learn")
                raw = None
                if hit.get("oid"):
                    item = db.processed_documents.find_one({"_id": ObjectId(hit["oid"])})
                else:
                    identity = ({"source": source, "source_id": key.removeprefix("https://arxiv.org/abs/")}
                                if source == "arxiv" else {"source": source, "url": hit["url"]})
                    raw = db.documents.find_one(identity, {"_id": 1})
                    item = db.processed_documents.find_one({"_id": raw["_id"]}) if raw else None
                if engine == "arxiv" and hit.get("abstract"):
                    # Для новых статей фиксируется аннотация именно возвращённой API-версии.
                    text = hit["title"] + "\n\n" + hit["abstract"]
                    origin = "arxiv_api_title_abstract"
                else:
                    text = item.get("text", "") if item else ""
                    origin = "mongodb_processed_text" if text else ""
                error = None
                if not text:
                    try:
                        time.sleep(3 if source == "arxiv" else 0.75)
                        response = session.get(hit["url"], timeout=25)
                        response.raise_for_status()
                        soup = BeautifulSoup(response.text, "lxml")
                        node = (soup.select_one("#mw-content-text .mw-parser-output")
                                or soup.select_one("article") or soup.select_one("main") or soup.body)
                        if node:
                            for unwanted in node.select("script,style,nav,header,footer"):
                                unwanted.decompose()
                            text = node.get_text("\n", strip=True)
                        origin = "live_document"
                    except requests.RequestException as failure:
                        error = str(failure)
                saved[key] = {"url": hit["url"], "title": hit["title"], "source": source,
                              "text": text, "text_origin": origin, "fetch_error": error,
                              "accepted_in_mongo": bool(item and item.get("accepted")),
                              "oid": str(item["_id"]) if item else None,
                              "collected_at": datetime.now(UTC).isoformat()}
                print("document", len(saved), source, "ok" if text else "unavailable", flush=True)
        save(target, saved)
    db.client.close()
    candidates = {bytes.fromhex(d["oid"]) for d in saved.values() if d.get("oid")}
    table = (ROOT / "results/boolean_index/documents.bin").read_bytes()
    indexed = {table[offset:offset + 12] for offset in range(16, len(table), 12)
               if table[offset:offset + 12] in candidates}
    for document in saved.values():
        document["in_index"] = bool(document.get("oid") and bytes.fromhex(document["oid"]) in indexed)
    save(target, saved)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Сбор выдач без перезаписи уже сохранённых запросов")
    parser.add_argument("--engines", nargs="+", choices=("wikipedia", "bing", "microsoft_learn", "arxiv"))
    args = parser.parse_args()
    queries = json.loads((DIRECTORY / "queries.json").read_text())
    if not (DIRECTORY / "snapshot.json").exists():
        snapshot()
    local_run(queries)
    external_runs(queries, args.engines or ("wikipedia", "bing", "microsoft_learn", "arxiv"))
    documents(queries)
