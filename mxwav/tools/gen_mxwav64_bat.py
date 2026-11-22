#!/usr/bin/env python3
# Generate mxwav/bat/mxwav64_*.bat from the X68Sound2025 presets (../start_*.bat)
# so that offline conversion uses exactly the same sound settings.
# Usage: python3 tools/gen_mxwav64_bat.py
import glob, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'bat')
SKIP = {'X68SOUND_PCM_BUFFER', 'X68SOUND_LATE_TIME', 'X68SOUND_BETW_TIME', 'X68SOUND_BUF_MULTIPLIER'}  # real-time only
NO_AI = {'standard'}   # "no processing" preset

TEMPLATE = r'''@echo off
chcp 65001 > nul
setlocal
REM =========================================================================
REM mxwav64 一括変換: {title}
REM =========================================================================
REM {desc}
REM 設定は ../start_{name}.bat と同じ音作り（リアルタイム用のバッファ設定は除く）。
REM {ai_note}
REM
REM 【使い方】
REM   このファイルを mxwav64.exe / X68Sound.dll と同じフォルダに置き、
REM   MDX ファイル（複数可）をこのファイルにドラッグ＆ドロップします。
REM   WAV は各 MDX と同じフォルダに「曲名.wav」で作られます。
REM   PDX は MDX と同じフォルダ、または環境変数 PDX のフォルダから探します。
REM
REM このファイルは mxwav/tools/gen_mxwav64_bat.py で生成しています。
REM =========================================================================

set "MXWAV=%~dp0mxwav64.exe"
if exist "%MXWAV%" goto check_args
echo [エラー] mxwav64.exe が見つかりません: "%MXWAV%"
echo このバッチを mxwav64.exe と同じフォルダに置いてください。
pause
exit /b 1

:check_args
if not "%~1"=="" goto settings
echo 使い方: MDX ファイルをこのバッチファイルにドラッグ＆ドロップしてください。
pause
exit /b 1

:settings
REM ---- X68Sound2025 の音質設定 ---------------------------------------------
{sets}

REM ---- mxwav のオプション -----------------------------------------------------
REM -A: AI による ADPCM 高音質化 / -L2: ループ曲は2回ループ後にフェードアウト
set "OPT={opt}"

set FAILED=0
:loop
if "%~1"=="" goto done
echo.
echo ==== 変換中: %~nx1
"%MXWAV%" %OPT% -O"%~dpn1.wav" "%~1"
REM （ファイル名に括弧を含んでも安全なように goto で分岐）
if errorlevel 1 goto failed_one
echo   → %~n1.wav
goto next
:failed_one
set FAILED=1
echo   [失敗] %~nx1
:next
shift
goto loop

:done
echo.
if "%FAILED%"=="1" echo 一部のファイルで変換に失敗しました。
if "%FAILED%"=="0" echo すべて変換しました。
pause
endlocal
'''

for src in sorted(glob.glob(os.path.join(ROOT, 'start_*.bat'))):
    name = os.path.basename(src)[len('start_'):-len('.bat')]
    text = open(src, encoding='utf-8').read().replace('\r\n', '\n')
    title = re.search(r'^REM (X68Sound2025 .+)$', text, re.M).group(1).replace('X68Sound2025 ', '')
    desc_lines = []
    m = re.search(r'^REM =+\nREM X68Sound2025 .+\nREM =+\n((?:REM .*\n)+?)REM\n', text, re.M)
    if m:
        desc_lines = [l[4:] for l in m.group(1).splitlines()]
    sets = []
    for line in text.splitlines():
        mm = re.match(r'^set (X68SOUND_\w+)=(\S*)', line)
        if mm and mm.group(1) not in SKIP:
            sets.append('set %s=%s' % (mm.group(1), mm.group(2)))
    ai = name not in NO_AI
    out = TEMPLATE.format(
        title=title, name=name,
        desc='\nREM '.join(desc_lines) if desc_lines else title,
        ai_note='AI による ADPCM 高音質化（-A）を有効にしています。' if ai else '加工なしの方針のため AI 補正（-A）は使いません。',
        sets='\n'.join(sets),
        opt='-A -L2' if ai else '-L2')
    dst = os.path.join(OUT, 'mxwav64_%s.bat' % name)
    with open(dst, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(out)
    print('generated', os.path.relpath(dst), '(%d settings, AI=%s)' % (len(sets), ai))
