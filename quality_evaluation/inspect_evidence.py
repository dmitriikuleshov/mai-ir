"""Выводит проверяемые фрагменты общего пула без названий поисковых систем.

Пример: uv run python quality_evaluation/inspect_evidence.py Q01 Q02
Подбор фрагментов помогает просмотру, но не назначает оценки релевантности.
"""

import json
import re
import sys
from pathlib import Path

from collect import canonical, save

HERE = Path(__file__).resolve().parent


def build_pool():
    queries = json.loads((HERE / "queries.json").read_text())
    documents = json.loads((HERE / "documents.json").read_text())
    runs = [json.loads(path.read_text()) for path in sorted((HERE / "runs").glob("*.json"))]
    pool = {}
    existing_path = HERE / "pool.json"
    existing = json.loads(existing_path.read_text()) if existing_path.exists() else {}
    for query in queries:
        hits = {}
        for run in runs:
            for hit in run[query["id"]]["results"][:5]:
                key = canonical(hit["url"])
                hits.setdefault(key, hit)
        rows = []
        terms = set(re.findall(r"[a-z0-9]+", query["query"].lower()))
        previous = {row["key"]: row["id"] for row in existing.get(query["id"], [])}
        next_number = max((int(value.split("D")[1]) for value in previous.values()), default=0)
        for key, hit in sorted(hits.items()):
            evidence_id = previous.get(key)
            if evidence_id is None:
                next_number += 1
                evidence_id = f"{query['id']}D{next_number:02d}"
            document = documents[key]
            text = document["text"]
            paragraphs = [p.strip() for p in re.split(r"\n+|(?<=[.!?])\s+", text) if len(p.strip()) > 60]
            ranked = sorted(enumerate(paragraphs), key=lambda p: (
                len(terms.intersection(re.findall(r"[a-z0-9]+", p[1].lower()))), -p[0]
            ), reverse=True)
            excerpt = "\n".join(paragraph[:1000] for _, paragraph in ranked[:3])
            rows.append({
                "id": evidence_id, "key": key,
                "title": document["title"], "url": hit["url"],
                "text_origin": document["text_origin"], "text_characters": len(text),
                "fetch_error": document["fetch_error"],
                "lead": text[:800], "excerpt": excerpt,
                "search_snippet": hit.get("snippet", ""),
            })
        pool[query["id"]] = sorted(rows, key=lambda row: row["id"])
    save(HERE / "pool.json", pool)
    return queries, pool


if __name__ == "__main__":
    queries, pool = build_pool()
    requested = set(sys.argv[1:])
    for query in queries:
        if requested and query["id"] not in requested:
            continue
        print(query["id"], query["query"], "Цель:", query["intent"])
        for row in pool[query["id"]]:
            print(row["id"], row["title"], row["url"], "chars=", row["text_characters"])
            print("LEAD:", row["lead"][:230].replace("\n", " "))
            print("PASSAGE:", (row["excerpt"] or row["search_snippet"])[:1050].replace("\n", " "))
