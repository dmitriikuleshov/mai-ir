"""Проверяет формулы на небольших примерах; MongoDB и интернет не требуются."""

import math

import pytest

from evaluate import metrics
from collect import canonical


# Лучший документ первым: DCG=7, NDCG=1, вероятность удовлетворения ERR=7/8.
def test_single_perfect_result():
    assert metrics([3], [3], 1) == {"P": 1, "DCG": 7, "NDCG": 1, "ERR": 0.875}


# Отсутствующие позиции не исчезают из знаменателя P@5.
def test_short_result_list_has_missing_positions():
    result = metrics([3], [3], 5)
    assert result["P"] == 0.2
    assert result["NDCG"] == 1


# Правильный порядок максимизирует NDCG, перестановка ухудшает его.
def test_ranking_order_changes_discounted_metrics():
    ideal = metrics([3, 2, 1], [1, 3, 2], 3)
    reversed_order = metrics([1, 2, 3], [1, 3, 2], 3)
    assert ideal["NDCG"] == 1
    assert reversed_order["NDCG"] < 1
    assert ideal["ERR"] > reversed_order["ERR"]
    assert ideal["P"] == reversed_order["P"] == pytest.approx(2 / 3)


# При отсутствии релевантных результатов метрики равны нулю, деления на ноль нет.
def test_no_relevant_documents():
    assert metrics([0, 0], [0, 0], 5) == {"P": 0, "DCG": 0, "NDCG": 0, "ERR": 0}


# DCG использует основание 2 у логарифма и экспоненциальную полезность оценки.
def test_dcg_formula():
    assert metrics([2, 1], [2, 1], 3)["DCG"] == pytest.approx(3 + 1 / math.log2(3))


# ERR учитывает вероятность, что пользователь ещё не нашёл удовлетворительного ответа.
def test_err_stopping_probability():
    expected = 3 / 8 + (1 - 3 / 8) * (7 / 8) / 2
    assert metrics([2, 3], [2, 3], 3)["ERR"] == pytest.approx(expected)


# Оценка 1 влияет на градуированные метрики, но не считается релевантной в P.
def test_partial_relevance_is_not_binary_relevance():
    value = metrics([1], [1], 1)
    assert value["P"] == 0
    assert value["DCG"] == 1
    assert value["ERR"] == 1 / 8


# Недопустимая шкала и нулевая глубина не принимаются.
@pytest.mark.parametrize("grades,k", [([4], 5), ([-1], 1), ([3], 0), ([1.5], 1), ([True], 1)])
def test_invalid_inputs(grades, k):
    with pytest.raises(ValueError):
        metrics(grades, grades, k)


# Разное URL-кодирование и порядок параметров не создают два документа в разметке.
def test_canonical_urls():
    assert canonical("http://en.wikipedia.org/wiki/Node.js#History") == canonical(
        "https://en.wikipedia.org/wiki/Node.js"
    )
    assert canonical("https://learn.microsoft.com/en-us/test?b=2&a=1&utm_source=x") == (
        "https://learn.microsoft.com/en-us/test?a=1&b=2"
    )


# Идеал берётся из общего пула, а не из собственной выдачи каждой системы.
def test_ideal_uses_shared_pool():
    value = metrics([2], [3, 2], 1)
    assert value["NDCG"] == pytest.approx(3 / 7)


# Пустая доступная выдача даёт нулевые метрики, даже если в общем пуле есть ответ.
def test_empty_available_run():
    assert metrics([], [3, 2], 5) == {"P": 0, "DCG": 0, "NDCG": 0, "ERR": 0}
