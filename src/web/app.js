// Переключает виды поиска, отправляет запросы и безопасно отображает найденные документы.
const modes = {
  tfidf: {
    help: "Обычный текстовый запрос. Самые близкие по термам документы будут первыми.",
    placeholder: "Что вы хотите найти?",
    examples: ["retrieval augmented generation", "distributed databases", "machine learning"],
  },
  boolean: {
    help: "Используйте AND, OR, NOT и скобки: документы должны удовлетворять выражению.",
    placeholder: "cloud AND (security OR database)",
    examples: ["cloud AND security", "database AND (replication OR sharding)", "python AND NOT snake"],
  },
  phrase: {
    help: "Слова должны идти подряд в указанном порядке. Кавычки необязательны; словоформы нормализуются.",
    placeholder: "Например, cloud security",
    examples: ["cloud security", "machine learning", "distributed database"],
  },
};
const sources = {
  wikipedia: { name: "Wikipedia", mark: "W", style: "wiki" },
  arxiv: { name: "arXiv", mark: "aX", style: "arxiv" },
  microsoft_learn: { name: "Microsoft Learn", mark: "ML", style: "learn" },
};
const $ = (id) => document.getElementById(id);
const number = new Intl.NumberFormat("ru-RU");
let mode = "tfidf";
let activeRequest = null;
let requestNumber = 0;

// Создаёт элементы с textContent: текст корпуса не интерпретируется как HTML.
function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

// Обновляет подсказку, примеры и доступное состояние кнопок выбранного режима.
function selectMode(nextMode) {
  mode = nextMode;
  document.querySelectorAll(".mode").forEach((button) => {
    const selected = button.dataset.mode === mode;
    button.classList.toggle("active", selected);
    button.setAttribute("aria-pressed", String(selected));
  });
  $("mode-help").textContent = modes[mode].help;
  $("query").placeholder = modes[mode].placeholder;
  $("example-buttons").replaceChildren(...modes[mode].examples.map((query) => {
    const button = element("button", "example", query);
    button.type = "button";
    button.addEventListener("click", () => {
      $("query").value = query;
      search();
    });
    return button;
  }));
}

// Разрешает ссылкам из корпуса только обычные веб-адреса.
function safeUrl(value) {
  try {
    const url = new URL(value);
    return ["http:", "https:"].includes(url.protocol) ? url : null;
  } catch {
    return null;
  }
}

// Показывает документ в едином формате; специальные поля зависят от вида поиска.
function renderResult(document, resultMode) {
  const item = element("li", "result");
  const source = sources[document.source] || { name: "Документ", mark: "D", style: "wiki" };
  const url = safeUrl(document.url);
  const sourceLine = element("div", "result-source");
  sourceLine.append(element("span", `source-mark ${source.style}`, source.mark));
  const site = element("span", "site-name", source.name);
  if (url) site.append(element("span", "hostname", url.hostname));
  sourceLine.append(site);
  const title = element("h2");
  const link = element(url ? "a" : "span", "", document.title || `Документ ${document.doc_id}`);
  if (url) {
    link.href = url.href;
    link.target = "_blank";
    link.rel = "noopener noreferrer";
  }
  title.append(link);
  item.append(sourceLine, title);
  if (document.preview) item.append(element("p", "result-preview", document.preview));
  if (document.metadata_missing) {
    item.append(element("p", "result-preview", "Документ найден в индексе, но его метаданные отсутствуют в MongoDB."));
  }
  const details = element("div", "result-details");
  if (resultMode === "tfidf") {
    details.append(element("span", "", `Оценка TF-IDF: ${document.score.toFixed(6)}`));
  } else if (resultMode === "phrase") {
    details.append(element("span", "", `Вхождений: ${number.format(document.occurrences)}`));
    const positions = document.positions.join(", ") + (document.occurrences > document.positions.length ? ", …" : "");
    details.append(element("span", "", `Начальные позиции: ${positions}`));
  }
  details.append(element("span", "", `docID: ${document.doc_id}`));
  item.append(details);
  return item;
}

// Отображает пустую выдачу или понятную ошибку рядом со строкой поиска.
function showMessage(title, text, isError = false) {
  $("message").className = isError ? "message error" : "message";
  $("message").replaceChildren(element("strong", "", title), element("p", "", text));
  $("message").hidden = false;
}

// Выполняет запрос; устаревший ответ не может заменить результат нового запроса.
async function search(updateHistory = true) {
  const query = $("query").value.trim();
  if (!query) {
    $("query").focus();
    return;
  }
  activeRequest?.abort();
  const controller = new AbortController();
  activeRequest = controller;
  const currentRequest = ++requestNumber;
  const requestMode = mode;
  document.title = `${query} · Технический корпус`;
  $("page").classList.add("has-results");
  $("results-section").hidden = false;
  $("results-section").setAttribute("aria-busy", "true");
  $("results").replaceChildren();
  $("message").hidden = true;
  $("loading").hidden = false;
  $("result-summary").textContent = "Выполняется поиск";
  $("search-time").textContent = "";
  $("result-limit").textContent = "";
  if (updateHistory) {
    const url = new URL(location.href);
    url.search = new URLSearchParams({ mode: requestMode, q: query });
    history.pushState(null, "", url);
  }
  try {
    const response = await fetch("/api/search", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mode: requestMode, query }), signal: controller.signal,
    });
    const data = await response.json();
    if (currentRequest !== requestNumber) return;
    if (!response.ok || !data.ok) throw new Error(data.error || "Поиск недоступен.");
    $("result-summary").textContent = `Найдено документов: ${number.format(data.total)}` +
      (requestMode === "phrase" ? ` · вхождений: ${number.format(data.occurrences)}` : "");
    $("search-time").textContent = `${number.format(Number(data.search_ms.toFixed(1)))} мс`;
    $("results").replaceChildren(...data.results.map((hit) => renderResult(hit, requestMode)));
    if (!data.total) {
      showMessage("Совпадений не найдено", requestMode === "phrase"
        ? "Попробуйте более короткую фразу: слова должны стоять рядом и в том же порядке."
        : "Попробуйте другие термины или менее ограничивающий запрос.");
    } else if (data.total > data.results.length) {
      $("result-limit").textContent = `Показаны первые ${data.results.length} из ${number.format(data.total)} документов. Уточните запрос, чтобы сузить выдачу.`;
    }
  } catch (error) {
    if (error.name === "AbortError" || currentRequest !== requestNumber) return;
    $("result-summary").textContent = "Поиск не выполнен";
    showMessage("Не удалось выполнить запрос", error.message, true);
  } finally {
    if (currentRequest === requestNumber) {
      $("loading").hidden = true;
      $("results-section").setAttribute("aria-busy", "false");
    }
  }
}

// Восстанавливает запрос из адреса, в том числе при переходе кнопкой «Назад».
function restoreLocation() {
  const params = new URLSearchParams(location.search);
  selectMode(Object.hasOwn(modes, params.get("mode")) ? params.get("mode") : "tfidf");
  $("query").value = params.get("q") || "";
  if ($("query").value.trim()) search(false);
  else {
    activeRequest?.abort();
    ++requestNumber;
    $("page").classList.remove("has-results");
    $("results-section").hidden = true;
    document.title = "Поиск · Технический корпус";
  }
}

$("search-form").addEventListener("submit", (event) => { event.preventDefault(); search(); });
document.querySelectorAll(".mode").forEach((button) => button.addEventListener("click", () => {
  selectMode(button.dataset.mode);
  if ($("page").classList.contains("has-results")) search();
}));
window.addEventListener("popstate", restoreLocation);
restoreLocation();
fetch("/api/info").then((response) => response.json()).then((data) => {
  $("index-info").textContent = data.ok ? `${number.format(data.documents)} документов в индексе` : "Индекс недоступен";
}).catch(() => { $("index-info").textContent = "Нет соединения с сервером"; });
