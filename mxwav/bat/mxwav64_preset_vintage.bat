@echo off
chcp 65001 > nul
setlocal
REM =========================================================================
REM mxwav64 一括変換: VINTAGE プリセット（アナログ風, 48kHz）
REM =========================================================================
REM FM 音源に温かみとコーラスを加え、高域を少し丸めて、アナログシンセ風の音にします。
REM 設定は ../start_preset_vintage.bat と同じ音作り（リアルタイム用のバッファ設定は除く）。
REM AI による ADPCM 高音質化（-A）を有効にしています。
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
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_HQ_SINE=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_PCM_RESAMPLE=1
set X68SOUND_ADPCM_MODE=0
set X68SOUND_OUTPUT_RATE=48000
set X68SOUND_FM_WARMTH=3
set X68SOUND_FM_HARMONIC=1
set X68SOUND_FM_CHORUS=2
set X68SOUND_CHORUS_ALGO=1
set X68SOUND_SUB_BASS=1
set X68SOUND_EQ_LOW_GAIN=2
set X68SOUND_EQ_LOW_FREQ=150
set X68SOUND_EQ_HIGH_GAIN=-3
set X68SOUND_EQ_HIGH_FREQ=6000
set X68SOUND_EARLY_REFLECTIONS=30
set X68SOUND_REVERB=3
set X68SOUND_REVERB_ALGO=1
set X68SOUND_REVERB_DECAY=75
set X68SOUND_REVERB_MIX=20
set X68SOUND_REVERB_PREDELAY=20
set X68SOUND_LIMITER=1
set X68SOUND_LIMITER_CEILING=-10

REM ---- mxwav のオプション -----------------------------------------------------
REM -A: AI による ADPCM 高音質化 / -L2: ループ曲は2回ループ後にフェードアウト
set "OPT=-A -L2"

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
