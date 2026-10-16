# Medições A/B

Média das repetições; desvio padrão entre execuções entre parênteses. Durações internas apenas na série instrumentada. Cycles/update abaixo divide **todos os ciclos do processo e threads** pelo número de updates; não isola o custo CPU do update.

| Série/caso | Métrica | Antes (DP) | Depois (DP) | Variação |
|---|---|---:|---:|---:|
| final-perf/1-0-1-0-1 | updates/s | 68.65 (1.37) | 67.02 (5.61) | -2.38% |
| final-perf/1-0-1-0-1 | frames/s | 68.65 (1.37) | 67.02 (5.61) | -2.38% |
| final-perf/1-0-1-0-1 | total cycles/update | 6.115e+07 (7.74e+05) | 6.34e+07 (4.5e+06) | +3.68% |
| final-perf/1-0-1-0-1 | total cycles/frame | 6.115e+07 (7.74e+05) | 6.34e+07 (4.5e+06) | +3.68% |
| final-perf/1-0-1-0-1 | IPC | 2.412 (0.0337) | 2.341 (0.152) | -2.93% |
| final-perf/1-0-1-0-1 | cache-misses:u/frame | 4.057e+05 (1.04e+04) | 4.168e+05 (2.25e+04) | +2.73% |
| final-perf/1-0-1-0-1 | L1-dcache-load-misses/frame | 1.678e+06 (1.11e+04) | 1.717e+06 (6.39e+04) | +2.34% |
| final-perf/1-0-1-0-1 | context-switches/frame | 6.737 (0.105) | 6.692 (0.176) | -0.67% |
| final-perf/1-0-1-0-1 | cpu-migrations/frame | 2.142 (0.117) | 2.113 (0.187) | -1.35% |
| final-perf/1-1-0-0-1 | updates/s | 3712 (83.5) | 3730 (46) | +0.49% |
| final-perf/1-1-0-0-1 | frames/s | 70.51 (3.01) | 71.12 (2.7) | +0.86% |
| final-perf/1-1-0-0-1 | total cycles/update | 1.161e+06 (2.4e+04) | 1.156e+06 (1.39e+04) | -0.39% |
| final-perf/1-1-0-0-1 | total cycles/frame | 6.118e+07 (2.5e+06) | 6.071e+07 (2.21e+06) | -0.77% |
| final-perf/1-1-0-0-1 | IPC | 2.385 (0.0966) | 2.402 (0.0898) | +0.72% |
| final-perf/1-1-0-0-1 | cache-misses:u/frame | 4.077e+05 (1.6e+04) | 4.035e+05 (1.94e+04) | -1.02% |
| final-perf/1-1-0-0-1 | L1-dcache-load-misses/frame | 1.6e+06 (1.01e+04) | 1.598e+06 (3.97e+03) | -0.17% |
| final-perf/1-1-0-0-1 | context-switches/frame | 59.65 (1.49) | 59.37 (1.45) | -0.47% |
| final-perf/1-1-0-0-1 | cpu-migrations/frame | 5.205 (0.28) | 5.086 (0.258) | -2.30% |
| final-perf/2-1-1-0-1 | updates/s | 3693 (59.1) | 3717 (26.8) | +0.64% |
| final-perf/2-1-1-0-1 | frames/s | 37.52 (1.13) | 37.9 (0.607) | +1.01% |
| final-perf/2-1-1-0-1 | total cycles/update | 1.19e+06 (2.24e+04) | 1.184e+06 (1.39e+04) | -0.55% |
| final-perf/2-1-1-0-1 | total cycles/frame | 1.172e+08 (3.2e+06) | 1.161e+08 (1.82e+06) | -0.94% |
| final-perf/2-1-1-0-1 | IPC | 2.395 (0.0665) | 2.417 (0.037) | +0.94% |
| final-perf/2-1-1-0-1 | cache-misses:u/frame | 6.915e+05 (2.08e+04) | 6.66e+05 (2.96e+03) | -3.68% |
| final-perf/2-1-1-0-1 | L1-dcache-load-misses/frame | 3.079e+06 (9.76e+03) | 3.103e+06 (5.62e+04) | +0.78% |
| final-perf/2-1-1-0-1 | context-switches/frame | 107.6 (1.91) | 106.9 (1.2) | -0.63% |
| final-perf/2-1-1-0-1 | cpu-migrations/frame | 6.925 (0.276) | 6.826 (0.432) | -1.43% |
| final-perf/2-1-1-50-1 | updates/s | 3727 (14.2) | 3761 (26.9) | +0.91% |
| final-perf/2-1-1-50-1 | frames/s | 12.99 (0.151) | 12.98 (0.259) | -0.01% |
| final-perf/2-1-1-50-1 | total cycles/update | 4.168e+05 (5.19e+03) | 4.093e+05 (1.33e+04) | -1.79% |
| final-perf/2-1-1-50-1 | total cycles/frame | 1.197e+08 (2.46e+06) | 1.186e+08 (5.42e+06) | -0.85% |
| final-perf/2-1-1-50-1 | IPC | 2.393 (0.0468) | 2.418 (0.109) | +1.04% |
| final-perf/2-1-1-50-1 | cache-misses:u/frame | 7.101e+05 (3.93e+04) | 6.99e+05 (4.52e+04) | -1.56% |
| final-perf/2-1-1-50-1 | L1-dcache-load-misses/frame | 3.137e+06 (2.63e+04) | 3.158e+06 (4.89e+04) | +0.67% |
| final-perf/2-1-1-50-1 | context-switches/frame | 298.4 (3.9) | 300.9 (6.13) | +0.85% |
| final-perf/2-1-1-50-1 | cpu-migrations/frame | 11.12 (0.721) | 10.37 (2.6) | -6.74% |
| final-timing/1-0-1-0-1 | update mean us | 4.395 (0.794) | 4.221 (0.818) | -3.94% |
| final-timing/1-0-1-0-1 | update p99 us | 9.156 (3.08) | 7.654 (2.51) | -16.41% |
| final-timing/1-0-1-0-1 | updatePlayer mean us | 3.78 (0.581) | 3.615 (0.564) | -4.37% |
| final-timing/1-0-1-0-1 | updatePlayer p99 us | 8.09 (2.95) | 6.458 (1.71) | -20.17% |
| final-timing/1-0-1-0-1 | render mean us | 5899 (193) | 5911 (231) | +0.20% |
| final-timing/1-0-1-0-1 | render p99 us | 7324 (667) | 6974 (757) | -4.78% |
| final-timing/1-0-1-0-1 | renderPlayer mean us | 5899 (193) | 5911 (231) | +0.20% |
| final-timing/1-0-1-0-1 | renderPlayer p99 us | 7324 (667) | 6974 (757) | -4.78% |
| final-timing/1-0-1-0-1 | interval mean us | 1.342e+04 (664) | 1.339e+04 (716) | -0.24% |
| final-timing/1-0-1-0-1 | interval p99 us | 1.55e+04 (1.44e+03) | 1.495e+04 (1.39e+03) | -3.58% |
| final-timing/1-0-1-0-1 | mutex mean us | 0.09204 (0.0238) | 0.09065 (0.0322) | -1.51% |
| final-timing/1-0-1-0-1 | mutex p99 us | 0.548 (0.118) | 0.378 (0.21) | -31.02% |
| final-timing/1-1-0-0-1 | update mean us | 2.233 (0.0375) | 2.253 (0.093) | +0.92% |
| final-timing/1-1-0-0-1 | update p99 us | 4.35 (0.358) | 4.69 (0.433) | +7.81% |
| final-timing/1-1-0-0-1 | updatePlayer mean us | 1.983 (0.0356) | 1.995 (0.083) | +0.57% |
| final-timing/1-1-0-0-1 | updatePlayer p99 us | 3.918 (0.311) | 4.204 (0.348) | +7.30% |
| final-timing/1-1-0-0-1 | render mean us | 5550 (93.7) | 5605 (80.9) | +0.99% |
| final-timing/1-1-0-0-1 | render p99 us | 6539 (228) | 6731 (504) | +2.94% |
| final-timing/1-1-0-0-1 | renderPlayer mean us | 5549 (93.7) | 5605 (80.9) | +0.99% |
| final-timing/1-1-0-0-1 | renderPlayer p99 us | 6538 (228) | 6730 (504) | +2.93% |
| final-timing/1-1-0-0-1 | interval mean us | 253.9 (0.611) | 253.5 (2.21) | -0.16% |
| final-timing/1-1-0-0-1 | interval p99 us | 263.3 (1.6) | 268.4 (12.8) | +1.93% |
| final-timing/1-1-0-0-1 | mutex mean us | 0.03364 (0.000407) | 0.03406 (0.0019) | +1.26% |
| final-timing/1-1-0-0-1 | mutex p99 us | 0.088 (0.00447) | 0.088 (0.013) | +0.00% |
| final-timing/2-1-1-0-1 | update mean us | 4.097 (0.0623) | 4.046 (0.0463) | -1.24% |
| final-timing/2-1-1-0-1 | update p99 us | 8.078 (0.552) | 8.35 (0.86) | +3.37% |
| final-timing/2-1-1-0-1 | updatePlayer mean us | 1.902 (0.0307) | 1.877 (0.0217) | -1.32% |
| final-timing/2-1-1-0-1 | updatePlayer p99 us | 3.874 (0.231) | 3.974 (0.313) | +2.58% |
| final-timing/2-1-1-0-1 | render mean us | 1.087e+04 (124) | 1.114e+04 (352) | +2.57% |
| final-timing/2-1-1-0-1 | render p99 us | 1.251e+04 (319) | 1.336e+04 (807) | +6.77% |
| final-timing/2-1-1-0-1 | renderPlayer mean us | 5432 (61.8) | 5572 (176) | +2.57% |
| final-timing/2-1-1-0-1 | renderPlayer p99 us | 6543 (248) | 6866 (373) | +4.93% |
| final-timing/2-1-1-0-1 | interval mean us | 253.5 (0.561) | 253.4 (0.311) | -0.06% |
| final-timing/2-1-1-0-1 | interval p99 us | 266.5 (1.3) | 266.9 (1.31) | +0.17% |
| final-timing/2-1-1-0-1 | mutex mean us | 0.03461 (0.00064) | 0.03405 (0.00108) | -1.62% |
| final-timing/2-1-1-0-1 | mutex p99 us | 0.092 (0.00837) | 0.084 (0.00548) | -8.70% |
| final-timing/2-1-1-50-1 | update mean us | 4.326 (0.407) | 4.187 (0.214) | -3.23% |
| final-timing/2-1-1-50-1 | update p99 us | 8.306 (1.9) | 8.466 (1.74) | +1.93% |
| final-timing/2-1-1-50-1 | updatePlayer mean us | 2.01 (0.184) | 1.942 (0.0955) | -3.37% |
| final-timing/2-1-1-50-1 | updatePlayer p99 us | 3.916 (0.917) | 4.016 (0.815) | +2.55% |
| final-timing/2-1-1-50-1 | render mean us | 1.181e+04 (1.08e+03) | 1.174e+04 (560) | -0.56% |
| final-timing/2-1-1-50-1 | render p99 us | 1.741e+04 (5.42e+03) | 1.69e+04 (2.17e+03) | -2.93% |
| final-timing/2-1-1-50-1 | renderPlayer mean us | 5903 (542) | 5870 (280) | -0.56% |
| final-timing/2-1-1-50-1 | renderPlayer p99 us | 1.019e+04 (4.09e+03) | 1.017e+04 (1.95e+03) | -0.19% |
| final-timing/2-1-1-50-1 | interval mean us | 255.2 (5.08) | 254.4 (2.75) | -0.32% |
| final-timing/2-1-1-50-1 | interval p99 us | 289.8 (53.1) | 278.8 (27.8) | -3.78% |
| final-timing/2-1-1-50-1 | mutex mean us | 0.03742 (0.00594) | 0.0367 (0.00406) | -1.91% |
| final-timing/2-1-1-50-1 | mutex p99 us | 0.102 (0.0228) | 0.1 (0.0187) | -1.96% |
