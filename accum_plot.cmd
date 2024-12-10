set terminal postscript color
set output 'plot.ps'
set data style lines
plot 'radar.plot','gauge.plot', 'radar-gauge.plot'
