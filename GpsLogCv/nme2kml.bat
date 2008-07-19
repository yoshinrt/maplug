@echo off

: .tab=4
set path=%path%;%~dp0\

: ***************************************************************************

if exist %1\*.dat (
	dir /a/b/s/ogn %* | find /i "gpslog.dat" >%temp%\gpslog.lst
) else (
	cd /d %1
	dir /a/b/s/ogn * | find /i "gpslog.dat" >%temp%\gpslog.lst
)

pushd %temp%
if exist gpslog.nme del gpslog.nme

: *** dat ‚ð nme ‚É•ÏŠ·¨cat ***
for /f "delims=" %%A in ( gpslog.lst ) do (
	
	type "%%A">>gpslog.tmp
)
gpslogcv -ITOKYO -INME -TNME -DW -SA -oR -F gpslog.tmp>gpslog.nme
gpsbabel -p "" -w -i nmea -f gpslog.nme -o kml,labels=0,line_color=FFFFFF00,line_width=1,units=m -F gpslog.kml

for /f "delims=" %%A in ( gpslog.lst ) do (
	set DstFile=%%~dpA
	goto move
)

:move
set DstFile=%DstFile:ULJS00128=%
set DstFile=%DstFile:~0,-1%.kml
move /y gpslog.kml "%DstFile%"

del gpslog.*

popd

:exit
