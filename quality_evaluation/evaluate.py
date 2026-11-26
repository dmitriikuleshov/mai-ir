"""Считает метрики по сохранённым выдачам и проверяемой разметке, без обращения к сети."""

from __future__ import annotations

import csv
import argparse
import hashlib
import json
import math
from pathlib import Path
from statistics import mean

from collect import canonical as canonical_url

HERE = Path(__file__).resolve().parent
PRIMARY = ("tfidf", "wikipedia", "microsoft_learn")


def prepare_judgments(directory: Path) -> None:
    """Связывает оценки с URL и запрещает расчёт по неполной или устаревшей разметке."""
    pool = json.loads((directory / "pool.json").read_text())
    assessments = json.loads((directory / "assessments.json").read_text())
    ids = {document["id"] for documents in pool.values() for document in documents}
    if ids != set(assessments):
        raise ValueError(f"Неполная разметка: отсутствуют {sorted(ids - assessments.keys())}; "
                         f"лишние {sorted(assessments.keys() - ids)}")
    snapshot = json.loads((directory / "snapshot.json").read_text())
    if snapshot["queries_sha256"] != hashlib.sha256((directory / "queries.json").read_bytes()).hexdigest():
        raise ValueError("Запросы изменились после фиксации эксперимента")
    judgments = {}
    for qid, documents in pool.items():
        judgments[qid] = {}
        for document in documents:
            value = assessments[document["id"]]
            if type(value["grade"]) is not int or value["grade"] not in (0, 1, 2, 3):
                raise ValueError(f"Некорректная оценка {document['id']}")
            judgments[qid][document["key"]] = {**value, "evidence_id": document["id"]}
    (directory / "judgments.json").write_text(
        json.dumps(judgments, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def metrics(grades: list[int], pool_grades: list[int], k: int) -> dict[str, float]:
    """P использует оценки 2–3; DCG и ERR учитывают все четыре градации 0–3."""
    if k < 1 or any(type(g) is not int or g not in (0, 1, 2, 3) for g in grades + pool_grades):
        raise ValueError("Ожидаются k >= 1 и целые оценки от 0 до 3")
    top = grades[:k]
    dcg = sum((2 ** grade - 1) / math.log2(rank + 1)
              for rank, grade in enumerate(top, 1))
    ideal = sorted(pool_grades, reverse=True)[:k]
    idcg = sum((2 ** grade - 1) / math.log2(rank + 1)
               for rank, grade in enumerate(ideal, 1))
    probability_continue = 1.0
    err = 0.0
    for rank, grade in enumerate(top, 1):
        satisfaction = (2 ** grade - 1) / 8
        err += probability_continue * satisfaction / rank
        probability_continue *= 1 - satisfaction
    return {
        "P": sum(grade >= 2 for grade in top) / k,
        "DCG": dcg, "NDCG": dcg / idcg if idcg else 0.0, "ERR": err,
    }


def evaluate(directory: Path = HERE) -> tuple[list[dict], list[dict]]:
    prepare_judgments(directory)
    queries = json.loads((directory / "queries.json").read_text())
    judgments = json.loads((directory / "judgments.json").read_text())
    runs = {p.stem: json.loads(p.read_text()) for p in (directory / "runs").glob("*.json")}
    rows = []
    for engine, run in sorted(runs.items()):
        for query in queries:
            qid = query["id"]
            item = run[qid]
            if item["status"] != "ok":
                continue  # Недоступность поисковика не является нулевой релевантностью.
            pool = judgments[qid]
            grades, seen = [], set()
            for hit in item["results"][:5]:
                key = canonical_url(hit["url"])
                if key in seen:
                    raise ValueError(f"Повтор документа в {engine}/{qid}: {key}")
                seen.add(key)
                if key not in pool:
                    raise ValueError(f"Нет оценки для {engine}/{qid}: {key}")
                grades.append(pool[key]["grade"])
            for k in (1, 3, 5):
                row = {"system": engine, "query_id": qid, "k": k,
                       "returned_top5": len(grades)}
                row.update(metrics(grades, [v["grade"] for v in pool.values()], k))
                rows.append(row)
    summary = []
    for engine in sorted(runs):
        for k in (1, 3, 5):
            selected = [row for row in rows if row["system"] == engine and row["k"] == k]
            if not selected:
                continue
            row = {"system": engine, "k": k, "queries": len(selected)}
            row.update({metric: mean(r[metric] for r in selected)
                        for metric in ("P", "DCG", "NDCG", "ERR")})
            summary.append(row)
    for name, data in (("metrics.csv", rows), ("summary.csv", summary)):
        if not data:
            raise ValueError("Нет доступных выдач для оценки")
        with (directory / name).open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(data[0]))
            writer.writeheader()
            writer.writerows(data)
    return rows, summary


def write_outputs(rows: list[dict], summary: list[dict], directory: Path = HERE) -> None:
    """Сохраняет численные результаты, выдачи с оценками и график сравнения."""
    for name, values in (("metrics.csv", rows), ("summary.csv", summary)):
        with (directory / name).open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(values[0]))
            writer.writeheader()
            writer.writerows(values)
    queries = json.loads((directory / "queries.json").read_text())
    judgments = json.loads((directory / "judgments.json").read_text())
    runs = {p.stem: json.loads(p.read_text()) for p in (directory / "runs").glob("*.json")}
    coverage = []
    documents = json.loads((directory / "documents.json").read_text())
    for engine, run in sorted(runs.items()):
        coverage.append({"system": engine, "available_queries": sum(v["status"] == "ok" for v in run.values()),
                         "queries_with_five_results": sum(v["status"] == "ok" and len(v["results"]) >= 5 for v in run.values()),
                         "positions_in_top5": sum(min(5, len(v["results"])) for v in run.values()),
                         "positions_in_local_index": sum(
                             documents[canonical_url(hit["url"])].get("in_index", False)
                             for value in run.values() for hit in value["results"][:5]),
                         "role": "primary" if engine in PRIMARY else "limited_supplement"})
    with (directory / "coverage.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(coverage[0]))
        writer.writeheader()
        writer.writerows(coverage)
    labels = {"tfidf": "Мой TF-IDF", "wikipedia": "Wikipedia Search", "microsoft_learn": "Microsoft Learn Search",
              "bing": "Bing: ограниченная первая страница (дополнение)"}
    report = ["# Выдачи и оценки релевантности", "",
              "Первые пять позиций. Оценки выполнены языковой моделью по сохранённым текстовым фрагментам; "
              "это проверяемая первичная разметка, а не независимый человеческий эталон.", "",
              "Bing приведён отдельно: после фильтрации доменов его первая страница неполна. "
              "Не следует считать её метрики оценкой всей поисковой системы Bing.", ""]
    for query in queries:
        qid = query["id"]
        report.extend([f"## {qid}. {query['query']}", "", query["intent"], ""])
        for engine in (*PRIMARY, "bing"):
            item = runs[engine][qid]
            report.extend([f"### {labels[engine]}", ""])
            if item["status"] != "ok":
                report.extend([f"Выдача недоступна: {item.get('error', '')}", ""])
                continue
            report.extend(["| Позиция | Документ | Оценка | Обоснование |", "|---:|---|---:|---|"])
            for hit in item["results"][:5]:
                judgment = judgments[qid][canonical_url(hit["url"])]
                title = hit["title"].replace("|", "\\|")
                reason = judgment["reason"].replace("|", "\\|")
                report.append(f"| {hit['rank']} | [{title}](<{hit['url']}>) | {judgment['grade']} | {reason} |")
            if len(item["results"]) < 5:
                report.append(f"\nВ сохранённой выборке только {len(item['results'])} результатов.")
            report.append("")
    (directory / "results.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for axis, metric in zip(axes.flat, ("P", "DCG", "NDCG", "ERR")):
        for engine in PRIMARY:
            selected = sorted((r for r in summary if r["system"] == engine), key=lambda r: r["k"])
            axis.plot([r["k"] for r in selected], [r[metric] for r in selected], marker="o", label=labels[engine])
        axis.set_title(metric)
        axis.set_xlabel("Глубина выдачи k, позиций")
        axis.set_ylabel("Средняя оценка, безразмерная")
        axis.set_xticks([1, 3, 5])
        if metric != "DCG":
            axis.set_ylim(0, 1)
        axis.grid(alpha=0.25)
    axes[0, 0].legend(fontsize=8)
    fig.suptitle("Пилотная оценка поиска: 30 запросов, модельная разметка")
    fig.savefig(directory / "metrics.svg")
    fig.savefig(directory / "metrics.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Расчёт качества по сохранённым выдачам и разметке")
    parser.add_argument("--verify-index", action="store_true", help="сверить SHA-256 текущих бинарников со снимком")
    arguments = parser.parse_args()
    rows, summary = evaluate()
    write_outputs(rows, summary)
    if arguments.verify_index:
        snapshot = json.loads((HERE / "snapshot.json").read_text())
        for name, expected in snapshot["files"].items():
            path = HERE.parent / "results/boolean_index" / name
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
                    digest.update(block)
            if path.stat().st_size != expected["bytes"] or digest.hexdigest() != expected["sha256"]:
                raise ValueError(f"Индекс изменился после сбора выдач: {name}")
        print("SHA-256 всех бинарных файлов совпадают со снимком")
    for row in summary:
        print(f"{row['system']:10s} @{row['k']}  n={row['queries']:2d}  "
              f"P={row['P']:.4f}  DCG={row['DCG']:.4f}  "
              f"NDCG={row['NDCG']:.4f}  ERR={row['ERR']:.4f}")
