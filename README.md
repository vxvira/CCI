CCI based strategy built & tested for somebody I know.  

Built on top of https://github.com/vxvira/AZBacktest.  

## Running

`./build.sh` runs `src/main.cpp`, `./build.sh src/optimizations/<stage>.cpp` runs a grid.  
Data is MNQ 5 min bars from Databento TBBO (2025-06-01 to 2026-05-14), costs 0.87 pts per round turn.  
The first run caches the bars to `build/bars_300.bin`, every run after loads in well under a second.

## Optimization

Every stage grids on the first 75% of the file (train) only and starts from the previous stage's frozen rules.  
A change is kept only if it adds more than 2% train P&L over the stage before.

| stage | file | grid | kept | train $ |
|---|---|---|---|---|
| baseline | | lengths 5,14,25,40, up 75, down 150, TP 170, SL 70, b/e at 50% | | 6,416 |
| 1 | signal.cpp | CCI lengths, long and short thresholds | 8,20,30,60, up 50, down 200 | 13,129 |
| 2 | exits.cpp | TP, SL, breakeven trigger, time stop | TP 180, SL 50, b/e at 75% | 18,247 |
| 3 | filters.cpp | ATR floor, EMA trend, UTC entry hours | nothing | 18,247 |
| 4 | orderflow.cpp | flow gate window and band, flow exit | skip if 6 bar flow is > 4% against | 19,013 |
| 5 | risk.cpp | daily loss limit, sides | nothing (140 pts, both) | 19,013 |

## Results (frozen rules, last 25% never seen)

| system | set | P&L $ | trades | win% | PF | max DD $ | sharpe |
|---|---|---|---|---|---|---|---|
| baseline | train | 6,416 | 554 | 25.5% | 1.16 | 3,029 | 1.48 |
| optimized | train | 19,013 | 602 | 27.6% | 1.47 | 2,493 | 4.26 |
| baseline | test | 3,672 | 177 | 28.2% | 1.28 | 1,742 | 2.45 |
| optimized | test | 2,774 | 199 | 23.6% | 1.20 | 2,545 | 2.10 |

The optimized rules triple train P&L but do worse than the baseline out of sample, so most of that gain is fit to the train period.  
Both stay profitable on the holdout, the test is only ~62 trading days though.
