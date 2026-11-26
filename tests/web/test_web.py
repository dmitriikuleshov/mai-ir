"""Проверки HTTP-интерфейса без пользовательской MongoDB и больших индексов."""

import asyncio

import pytest
from aiohttp.test_utils import TestClient, TestServer

from web.main import create_app


class FakeWorker:
    """Заменяет только поисковый процесс: HTTP-маршруты работают по-настоящему."""

    def __init__(self, response=None, error=None):
        self.response = response if response is not None else {"ok": True, "results": [], "total": 0}
        self.error = error
        self.requests = []
        self.closed = False

    async def request(self, payload):
        self.requests.append(payload)
        if self.error:
            raise self.error
        return self.response

    async def close(self):
        self.closed = True


@pytest.mark.parametrize("mode", ["tfidf", "boolean", "phrase"])
def test_search_passes_normalized_request_to_worker(mode):
    # Каждый режим передаётся одному и тому же адаптеру; крайние пробелы удаляются.
    async def scenario():
        worker = FakeWorker()
        async with TestClient(TestServer(create_app(worker))) as client:
            response = await client.post("/api/search", json={"mode": mode, "query": "  cloud security  "})
            assert response.status == 200
            assert (await response.json())["ok"]
            assert worker.requests == [{"mode": mode, "query": "cloud security"}]
        assert worker.closed
    asyncio.run(scenario())


@pytest.mark.parametrize("payload", [
    [], {"query": ""}, {"query": "  "}, {"query": "a\nb"}, {"query": "a\0b"},
    {"query": "a" * 1001}, {"query": 12}, {"query": "cloud", "mode": "shell"},
    {"query": "cloud", "mode": []},
])
def test_invalid_queries_never_reach_worker(payload):
    # Пустые, многострочные и неверно типизированные запросы отбрасываются до C++.
    async def scenario():
        worker = FakeWorker()
        async with TestClient(TestServer(create_app(worker))) as client:
            response = await client.post("/api/search", json=payload)
            assert response.status == 400
            assert not (await response.json())["ok"]
            assert not worker.requests
    asyncio.run(scenario())


@pytest.mark.parametrize("worker_response,status", [
    ({"ok": False, "error": "Ожидался терм", "kind": "query"}, 400),
    ({"ok": False, "error": "Индекс недоступен", "kind": "unavailable"}, 503),
])
def test_query_and_resource_errors_are_distinguished(worker_response, status):
    # Ошибка запроса отличается от недоступности индекса или MongoDB.
    async def scenario():
        async with TestClient(TestServer(create_app(FakeWorker(worker_response)))) as client:
            response = await client.post("/api/search", json={"query": "cloud"})
            assert response.status == status
            assert (await response.json())["error"] == worker_response["error"]
    asyncio.run(scenario())


@pytest.mark.parametrize("error,status", [(RuntimeError("Процесс остановлен"), 503), (TimeoutError(), 504)])
def test_worker_failure_is_returned_as_json(error, status):
    # Сбой или тайм-аут процесса превращается в понятный JSON-ответ вместо HTML traceback.
    async def scenario():
        async with TestClient(TestServer(create_app(FakeWorker(error=error)))) as client:
            response = await client.post("/api/search", json={"query": "cloud"})
            assert response.status == status
            assert not (await response.json())["ok"]
    asyncio.run(scenario())


def test_status_and_page_assets():
    # Страница и её ресурсы доступны; статус отражает ответ настоящего адаптера.
    async def scenario():
        worker = FakeWorker({"ok": True, "documents": 42})
        async with TestClient(TestServer(create_app(worker))) as client:
            response = await client.get("/api/info")
            assert (await response.json())["documents"] == 42
            assert worker.requests == [{"mode": "status"}]
            for path in ["/", "/app.js", "/styles.css"]:
                assert (await client.get(path)).status == 200
            assert (await client.get("/main.py")).status == 404
    asyncio.run(scenario())


def test_malformed_json():
    # Повреждённый JSON возвращает 400 и не запускает поиск.
    async def scenario():
        worker = FakeWorker()
        async with TestClient(TestServer(create_app(worker))) as client:
            response = await client.post("/api/search", data="{broken", headers={"Content-Type": "application/json"})
            assert response.status == 400
            assert not worker.requests
    asyncio.run(scenario())
