"""Self-contained, offline HTML report. No remote scripts or unsafe metadata HTML."""

import json


def render(report):
    payload = json.dumps(
        report, ensure_ascii=False, separators=(",", ":"), allow_nan=False
    )
    payload = (
        payload.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    )
    return TEMPLATE.replace("__REPORT_DATA__", payload)


TEMPLATE = r"""<!doctype html>
<html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Guitar++ — Estatísticas de Plus</title><link rel="icon" href="data:,">
<style>
body{font:15px system-ui,sans-serif;background:#141922;color:#e7edf4;margin:24px;max-width:1500px}h1,h2{font-weight:600}button,select,input{font:inherit;background:#263446;color:inherit;border:1px solid #697a90;padding:7px;border-radius:4px}button{cursor:pointer}label{display:inline-block;margin:6px}a{color:#8dcfff}summary{cursor:pointer;padding:8px}table{border-collapse:collapse;width:100%;font-size:14px}td,th{padding:7px;border-bottom:1px solid #3e4c60;text-align:left}th button{border:0;padding:2px}tr:hover{background:#253044}.scroll{overflow:auto;max-height:460px}canvas{display:block;background:#1d2633;width:100%}.plot{overflow:auto;max-height:700px}pre{white-space:pre-wrap;overflow-wrap:anywhere}.muted{color:#bbc9d7}.box{border:1px solid #44546a;padding:12px;margin:16px 0}.warning{color:#ffd18b}dialog{background:#141922;color:#e7edf4;border:1px solid #8dcfff;width:min(1200px,90vw);max-height:85vh;overflow:auto}dialog::backdrop{background:#0009}.close{float:right}#detailPlot{height:220px}
</style>
<h1>Guitar++ — Estatísticas de Plus</h1>
<p id="overview"></p>
<p>Frases reconhecidas pelo loader do Guitar++. A análise descreve escolhas dos charts; não deduz a intenção de seus autores nem simula energia ou pontuação.</p>
<details class="box"><summary>Como interpretar os resultados e os controles?</summary>
<p>Use os filtros para comparar o mesmo jogo, instrumento e dificuldade. Expert Guitar ([ExpertSingle]) é a seleção inicial. Cada música tem o mesmo peso nos resumos; dificuldades não são amostras independentes.</p>
<p>O tempo normalizado vai do primeiro ataque ao final do último sustain, considerando todas as partes suportadas do arquivo. Não é a duração do áudio. As faixas azuis representam Plus; a linha verde representa ataques por segundo.</p>
<p>O modelo aleatório conserva quantidade e durações em semínimas. Sorteia inícios entre ataques, sem sobreposição e dentro da parte jogável. A ordem de colocação é embaralhada; não é uma amostragem uniforme de todas as combinações possíveis. As barras mostram os percentis 2,5–97,5%, não intervalos de confiança sobre intenção autoral. Um ponto fora da barra não prova uma escolha intencional.</p>
<p>Limites musicais usam tolerância de max(1 tick, 1/48 de semínima). Compassos presumidos não entram nas métricas de alinhamento. Mudanças que interrompem um compasso iniciam outro e geram aviso. Sustains são caudas acima de 10 ms; sua duração é a usada pelo runtime, que pode combinar as caudas de um acorde.</p>
<p>Movimento é a distância entre centros das lanes de ataques consecutivos, sem converter open notes em uma sexta posição. Densidade, acordes e movimento são indicadores descritivos, não uma medida validada de dificuldade humana. Contexto anterior/posterior usa até 16 semínimas, limitado à parte jogável.</p>
<p>Seções são os rótulos encontrados nos arquivos: podem ser apenas marcações percentuais. Ausência de dados aparece como “—”. Charts sem Plus permanecem no resumo, mas não têm distribuição aleatória.</p>
<p>“Mostrar variantes” revela cópias e alternativas excluídas dos agregados. Para escolher outra representante, edite catalog.json (preferred, exclude, song_id, game) e execute novamente com --catalog. Não se escolhe uma variante por ter mais Plus.</p>
</details>
<div class="box"><label>Jogo <select id="game"></select></label><label>Parte <select id="part"></select></label><label>Buscar <input id="search" placeholder="Música, artista ou caminho"></label><label><input type="checkbox" id="variants"> Mostrar variantes</label><p id="count"></p></div>
<h2>Músicas e versões</h2><p>Clique no título para ver frases, contexto, avisos e resultados aleatórios. Clique nos cabeçalhos para ordenar.</p>
<div class="scroll"><table><thead><tr id="head"></tr></thead><tbody id="songs"></tbody></table></div>
<h2>Distribuição ao longo do chart</h2><p class="muted">0% → 100% da extensão jogável; uma linha por chart. Variantes são exibidas somente quando solicitadas.</p><div class="plot"><canvas id="overlay"></canvas></div>
<h2>Observado versus posições aleatórias</h2><label>Métrica <select id="metric"></select></label><p class="muted">Ponto azul: observado. Barra cinza: percentis 2,5–97,5% das amostras válidas. Cruz branca: mediana aleatória.</p><div class="plot"><canvas id="nullPlot"></canvas></div>
<h2>Resumos por origem e parte</h2><p class="muted">Medianas entre músicas, incluindo charts sem Plus. Correlações de Spearman são exploratórias; valores indefinidos não recebem zero.</p><div id="groups"></div>
<details class="box"><summary id="errorTitle">Diagnóstico da varredura</summary><pre id="errors"></pre></details>
<p><a href="report.json">JSON completo</a> · <a href="charts.csv">CSV de charts</a> · <a href="phrases.csv">CSV de frases</a> · <a href="catalog.json">Catálogo de seleção</a></p>
<dialog id="detail"><button class="close" id="close">Fechar</button><h2 id="detailTitle"></h2><p id="detailPath"></p><canvas id="detailPlot"></canvas><p id="detailSummary"></p><p class="warning" id="detailWarnings"></p><details><summary>Estatísticas completas e diagnóstico</summary><pre id="detailInfo"></pre></details><div class="scroll"><table><thead><tr><th>Frase</th><th>Início (s)</th><th>Duração (s)</th><th>Semínimas</th><th>Compassos</th><th>Ataques</th><th>Antes / durante / depois (ataques/s)</th><th>Seção</th></tr></thead><tbody id="phraseTable"></tbody></table></div></dialog>
<script type="application/json" id="data">__REPORT_DATA__</script>
<script>
'use strict';
const report = JSON.parse(document.getElementById('data').textContent),
      $ = id => document.getElementById(id);
const fmt = v => v === null || v === undefined ? '—' :
    typeof v === 'number' ? Number(v.toFixed(3)).toLocaleString('pt-BR') :
                            String(v);
const metrics = {
  mean_density: 'Densidade nas frases (ataques/s)',
  mean_chord_fraction: 'Proporção de acordes',
  mean_sustain_fraction: 'Proporção de sustains',
  mean_movement: 'Movimento médio entre lanes',
  mean_position: 'Posição média (0–1)',
  mean_bar_start_aligned: 'Inícios alinhados a compassos',
  mean_bar_end_aligned: 'Finais alinhados a compassos',
  mean_section_aligned: 'Inícios alinhados a seções',
  mean_before_density: 'Densidade anterior',
  mean_after_density: 'Densidade posterior',
  mean_gap_seconds: 'Espaço entre frases (s)',
  mean_solo_overlap_fraction: 'Fração temporal em solos marcados'
};
function options(id, values, initial) {
  const s = $(id);
  for (const [value, label] of values) {
    const o = document.createElement('option');
    o.value = value;
    o.textContent = label;
    s.append(o)
  }
  s.value = initial
}
options(
    'game',
    [
      ['', 'Todas as origens'],
      ...[...new Set(report.rows.map(r => r.game))].sort().map(x => [x, x])
    ],
    '');
options(
    'part',
    [
      ['', 'Todas as partes'],
      ...[...new Set(report.rows.map(r => r.instrument))].sort().map(
          x => [x, x])
    ],
    report.rows.some(r => r.instrument === '[ExpertSingle]') ?
        '[ExpertSingle]' :
        '');
options('metric', Object.entries(metrics), 'mean_density');
$('overview').textContent =
    `${report.inventory.length} arquivos encontrados · ${
        report.rows.length} partes · ${
        report.rows.filter(r => r.selected).length} representantes · seed ${
        report.settings.seed} · ${
        report.settings.samples} amostras solicitadas por parte`;
$('errorTitle').textContent =
    `Diagnóstico: ${report.errors.length} erros de entrada`;
$('errors').textContent =
    JSON.stringify({errors: report.errors, settings: report.settings}, null, 2);
let sortKey = 'title', ascending = true, visible = [];
const columns = [
  ['title', 'Música'], ['game', 'Origem'], ['instrument', 'Parte'],
  ['plus_count', 'Plus'], ['plus_per_minute', 'Plus/min'],
  ['density', 'Ataques/s'], ['coverage_fraction', 'Cobertura (0–1)'],
  ['selection_reason', 'Seleção']
];
for (const [key, label] of columns) {
  const th = document.createElement('th'), b = document.createElement('button');
  b.textContent = label;
  b.onclick = () => {
    ascending = key === sortKey ? !ascending : true;
    sortKey = key;
    update()
  };
  th.append(b);
  $('head').append(th)
}
function cell(row, value) {
  const td = document.createElement('td');
  td.textContent = fmt(value);
  row.append(td);
  return td
}
function canvas(id, height) {
  const c = $(id), w = Math.max(700, c.parentElement.clientWidth - 4);
  c.width = w;
  c.height = height;
  c.style.height = height + 'px';
  const x = c.getContext('2d');
  x.font = '12px system-ui';
  return [x, w, height]
}
function label(x, text, y) {
  x.fillStyle = '#e7edf4';
  x.fillText(text.length > 37 ? text.slice(0, 35) + '…' : text, 6, y)
}
function drawOverlay() {
  const [x, w] = canvas('overlay', Math.max(80, visible.length * 20 + 30)),
            left = 270, span = w - left - 15;
  x.fillStyle = '#bbc9d7';
  x.fillText('0%', left, 15);
  x.fillText('100%', w - 45, 15);
  visible.forEach((r, i) => {
    const y = 35 + i * 20;
    label(x, r.title, y);
    x.strokeStyle = '#39495d';
    x.beginPath();
    x.moveTo(left, y - 4);
    x.lineTo(w - 15, y - 4);
    x.stroke();
    x.fillStyle = r.selected ? '#6cb7ff' : '#bc9c75';
    for (const p of r.phrases) {
      const a = Math.max(0, Math.min(1, p.position)),
            b = Math.max(0, Math.min(1, p.end_position));
      x.fillRect(left + a * span, y - 10, Math.max(2, (b - a) * span), 11)
    }
  })
}
function drawNull() {
  const key = $('metric').value,
        rows = visible.filter(
            r => r.random?.metrics?.[key]?.median !== null &&
                r.random?.metrics?.[key]?.median !== undefined);
  const [x, w] = canvas('nullPlot', Math.max(80, rows.length * 22 + 30)),
            left = 270;
  let values = rows.flatMap(r => {
    const m = r.random.metrics[key];
    return [m.observed, m.p025, m.p975].filter(v => v !== null)
  }),
      lo = Math.min(0, ...values), hi = Math.max(1, ...values);
  const pos = v => left + (v - lo) / (hi - lo) * (w - left - 20);
  x.fillStyle = '#bbc9d7';
  x.fillText(fmt(lo), left, 15);
  x.fillText(fmt(hi), w - 50, 15);
  rows.forEach((r, i) => {
    const y = 35 + i * 22, m = r.random.metrics[key];
    label(x, r.title, y);
    x.strokeStyle = '#90a1b5';
    x.lineWidth = 3;
    x.beginPath();
    x.moveTo(pos(m.p025), y - 4);
    x.lineTo(pos(m.p975), y - 4);
    x.stroke();
    x.strokeStyle = '#fff';
    x.lineWidth = 1;
    x.beginPath();
    x.moveTo(pos(m.median), y - 9);
    x.lineTo(pos(m.median), y + 1);
    x.stroke();
    if (m.observed !== null) {
      x.fillStyle = '#66beff';
      x.beginPath();
      x.arc(pos(m.observed), y - 4, 4, 0, Math.PI * 2);
      x.fill()
    }
  })
}
function detail(row) {
  $('detailTitle').textContent = row.title + ' — ' + row.instrument;
  $('detailSummary').textContent = `${row.statistics.plus_count} frases · ${
      fmt(row.statistics.plus_per_minute)} Plus/min · ${
      fmt(row.statistics.coverage_fraction * 100)}% do tempo coberto · ${
      row.random?.completed ?? 0} amostras aleatórias válidas`;
  $('detailWarnings').textContent = row.warnings.join(' ');
  $('detailPath').textContent = row.path;
  $('detailInfo').textContent = JSON.stringify(
      {
        selection: row.selection_reason,
        warnings: row.warnings,
        statistics: row.statistics,
        random: row.random ??
            'Variante: comparação aleatória apenas para representantes'
      },
      null, 2);
  $('phraseTable').replaceChildren();
  for (const p of row.phrases) {
    const tr = document.createElement('tr');
    [p.index + 1, p.start_seconds, p.seconds, p.duration_beats, p.duration_bars,
     p.attacks,
     [p.before_density, p.density, p.after_density].map(fmt).join(' / '),
     p.section]
        .forEach(v => cell(tr, v));
    $('phraseTable').append(tr)
  }
  $('detail').showModal();
  const [x, w, h] = canvas('detailPlot', 220), left = 25, span = w - left - 15,
               max = Math.max(1, ...row.density_timeline);
  x.fillStyle = '#366592';
  for (const p of row.phrases) {
    const a = Math.max(0, Math.min(1, p.position)),
          b = Math.max(0, Math.min(1, p.end_position));
    x.fillRect(left + a * span, 20, Math.max(2, (b - a) * span), h - 45)
  }
  x.strokeStyle = '#8de1b3';
  x.beginPath();
  row.density_timeline.forEach((d, i) => {
    const a = left + (i + .5) / 100 * span, b = h - 25 - d / max * (h - 50);
    if (i === 0)
      x.moveTo(a, b);
    else
      x.lineTo(a, b)
  });
  x.stroke();
  x.fillStyle = '#fff';
  x.fillText(fmt(max) + ' ataques/s', left, 13);
  x.fillText('0%', left, h - 6);
  x.fillText('100%', w - 45, h - 6)
}
function update() {
  const q = $('search').value.toLowerCase();
  visible = report.rows.filter(
      r => (!$('game').value || r.game === $('game').value) &&
          (!$('part').value || r.instrument === $('part').value) &&
          ($('variants').checked || r.selected) &&
          (!q ||
           [r.title, r.artist, r.path].some(v => v.toLowerCase().includes(q))));
  const value = r => r[sortKey] ?? r.statistics[sortKey];
  visible.sort((a, b) => {
    const av = value(a), bv = value(b);
    return (typeof av === 'number' ? av - bv :
                                     String(av).localeCompare(String(bv))) *
        (ascending ? 1 : -1)
  });
  $('count').textContent = `${visible.length} partes exibidas. Zero Plus: ${
      visible.filter(r => r.statistics.plus_count === 0).length}.`;
  $('songs').replaceChildren();
  for (const r of visible) {
    const tr = document.createElement('tr');
    for (const [key] of columns) {
      const td = cell(tr, r[key] ?? r.statistics[key]);
      if (key === 'title') {
        td.replaceChildren();
        const b = document.createElement('button');
        b.textContent = r.title;
        b.onclick = () => detail(r);
        td.append(b)
      }
    }
    $('songs').append(tr)
  }
  drawOverlay();
  drawNull();
  $('groups').replaceChildren();
  for (const g of report.groups.filter(
           g => (!$('game').value || g.game === $('game').value) &&
               (!$('part').value || g.instrument === $('part').value))) {
    const d = document.createElement('details');
    d.className = 'box';
    const s = document.createElement('summary');
    s.textContent = `${g.game} ${g.instrument}: ${g.songs} músicas, ${
        g.zero_plus} sem Plus; mediana ${
        fmt(g.statistics.plus_per_minute.median)} Plus/min`;
    const p = document.createElement('pre');
    p.textContent = JSON.stringify(g, null, 2);
    d.append(s, p);
    $('groups').append(d)
  }
}
for (const id of ['game', 'part', 'search', 'variants'])
  $(id).addEventListener('input', update);
$('metric').onchange = drawNull;
$('close').onclick = () => $('detail').close();
window.addEventListener('resize', () => {
  drawOverlay();
  drawNull()
});
update();
</script></html>"""
