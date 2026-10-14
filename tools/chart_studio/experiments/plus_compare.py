#!/usr/bin/env python3
"""Compare Plus profiles on frozen studio projects, never remapping their lanes.

Only standard-library Python is required. Statistical measurements come from
plus_stats.py; this driver adds the shared C++ economy simulations and provenance.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('projects', nargs='+', type=Path)
    parser.add_argument('--cli', required=True, type=Path)
    parser.add_argument('--dumper', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--reference', action='append', default=[], type=Path)
    parser.add_argument('--trials', type=int, default=256)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    root = args.output_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if (root / 'comparison.json').exists() and not args.replace:
        parser.error('Comparison exists; choose another directory or --replace')
    cli, dumper = args.cli.resolve(), args.dumper.resolve()
    projects = [p.resolve() for p in args.projects]
    references = [p.resolve() for p in args.reference]
    inputs = {str(p): digest(p) for p in projects + references}
    rows, charts, catalog = [], [], {'schema': 1, 'files': {}}
    for index, project in enumerate(projects):
        previous_notes = None
        for profile in ('balanced', 'strategic', 'generous', 'sparse'):
            chart = root / f'{index:02d}-{profile}.chart'
            report = chart.with_suffix('.json')
            command = [str(cli), 'plus-generate', str(project), '--plus-profile', profile,
                       '--plus-trials', str(args.trials), '--plus-seed', str(args.seed),
                       '--output', str(chart), '--report', str(report)]
            if args.replace:
                command.append('--replace')
            result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=300)
            if result.returncode:
                raise RuntimeError(f'{project.name}/{profile}: {result.stderr}')
            notes = [line for line in chart.read_text().splitlines() if ' = N ' in line or ' = B ' in line]
            if previous_notes is not None and notes != previous_notes:
                raise RuntimeError('Profile changed notes or tempo')
            previous_notes = notes
            if profile == 'strategic':
                before = digest(chart)
                subprocess.run(command + ([] if '--replace' in command else ['--replace']),
                               cwd=root, check=True, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=300)
                if digest(chart) != before:
                    raise RuntimeError('Non-deterministic second-process export')
            data = json.loads(report.read_text())
            rows.append({'song': project.stem, 'profile': profile, 'chart': chart.name,
                         'report': report.name, 'sha256': digest(chart), 'analysis': data,
                         'diagnostics': result.stderr.splitlines()})
            charts.append(chart)
            catalog['files'][str(chart)] = {'game': 'guitarpp-' + profile,
                                            'song_id': project.stem, 'preferred': True}
            print(project.stem, profile, len(data['phrases']), 'Plus', flush=True)
    for index, reference in enumerate(references):
        report = root / f'reference-{index:02d}.json'
        command = [str(cli), 'plus-simulate', str(reference), '--plus-trials', str(args.trials),
                   '--plus-seed', str(args.seed), '--output', str(report)]
        if args.replace:
            command.append('--replace')
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=300)
        if result.returncode:
            raise RuntimeError(result.stderr)
        rows.append({'song': reference.parent.name, 'profile': 'reference',
                     'report': report.name, 'analysis': json.loads(report.read_text())})
    catalog_path = root / 'catalog-input.json'
    catalog_path.write_text(json.dumps(catalog, indent=2))
    stats = Path(__file__).resolve().parents[1] / 'plus_stats.py'
    command = [sys.executable, str(stats), *map(str, charts + references),
               '--dumper', str(dumper), '--output-dir', str(root / 'statistics'),
               '--samples', '0', '--catalog', str(catalog_path)]
    if args.replace:
        command.append('--replace')
    subprocess.run(command, cwd=root, check=True)
    for path, expected in inputs.items():
        if digest(Path(path)) != expected:
            raise RuntimeError(f'Input changed: {path}')
    result = {'schema': 1, 'inputs_sha256': inputs, 'seed': args.seed,
              'trials': args.trials, 'rows': rows}
    (root / 'comparison.json').write_text(json.dumps(result, indent=2))
    table = []
    for row in rows:
        for simulation in row['analysis']['simulations']:
            if simulation['strategy'] != 'dense' or simulation['completion'] != 'sequence':
                continue
            n = len(row['analysis']['regions'])
            reserve = sum(e >= .4 - 1e-9 for e in simulation['region_energy'])
            values = [row['song'], row['profile'], f"{simulation['success_per_attack']:.0%}",
                      len(row['analysis']['phrases']), f'{reserve}/{n}',
                      f"{simulation['completed']:.2f}", f"{simulation['active_seconds']:.1f}",
                      f"{simulation['wasted_energy']:.3f}", f"{simulation['normalized_benefit']:.0f}"]
            cells = ''.join('<td>' + html.escape(str(v)) + '</td>' for v in values)
            cells += '<td><a href="' + html.escape(row['report'], quote=True) + '">JSON</a></td>'
            table.append('<tr>' + cells + '</tr>')
    headers = ['Música', 'Perfil', 'Acerto/ataque', 'Frases', 'Reserva média ≥40%',
               'Concluídas', 'Ativação (s)', 'Energia perdida', 'Benefício 1x', 'Detalhes']
    page = '''<!doctype html><html lang="pt-BR"><meta charset="utf-8">
<title>Guitar++ — avaliação estratégica de Plus</title>
<style>body{background:#141a23;color:#e2e8f0;font:15px system-ui;padding:24px}a{color:#81d4fa}table{border-collapse:collapse}th,td{padding:8px;border-bottom:1px solid #445;text-align:right}td:first-child{text-align:left}th{position:sticky;top:0;background:#202c3c}input{padding:8px;width:420px}</style>
<h1>Guitar++ — avaliação estratégica de Plus</h1>
<p>Mesmas lanes e parâmetros de tempo entre perfis; Strategic repetido em outro processo.
Arquivos de origem verificados por SHA-256. Referências são outras representações da música,
não alvos de otimização.</p>
<p><a href="statistics/report.html">Análise estatística existente: duração, compassos, densidade e cobertura</a> · <a href="comparison.json">Resultados completos</a></p>
<p>Tabela: estratégia de guardar para regiões valiosas, conclusão da sequência inteira.
Reserva média não é probabilidade de atingir a reserva. Os JSON incluem os cinco modos de ativação
e a comparação com a verificação atual do último ataque. Benefício é potencial normalizado, não pontuação humana prevista.
Os sorteios são independentes por ataque: frases longas podem ter baixa chance de conclusão.
Não houve avaliação perceptual ou otimização para imitar GH3/RB4.</p>
<input id="filter" placeholder="Filtrar música ou perfil"><table><thead><tr>'''
    page += ''.join('<th>' + title + '</th>' for title in headers)
    page += '</tr></thead><tbody>' + ''.join(table) + '</tbody></table>'
    page += '''<script>document.querySelector('#filter').oninput=function(){for(const row of document.querySelectorAll('tbody tr'))row.hidden=!row.textContent.toLowerCase().includes(this.value.toLowerCase())}</script></html>'''
    (root / 'comparison.html').write_text(page)


if __name__ == '__main__':
    main()
