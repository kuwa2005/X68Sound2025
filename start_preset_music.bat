@echo off
chcp 65001 > nul
setlocal
REM =========================================================================
REM X68Sound2025 MUSIC プリセット（音楽鑑賞・スピーカー向け, 96kHz）
REM =========================================================================
REM ハイレゾ出力に、控えめな音色補正・自然な広がり・上質なリバーブを加えます。
REM
REM 【使い方】
REM   1. 下の APP= を、普段使っている MDX プレイヤー等のパスに書き換える
REM   2. このファイルをダブルクリック（MDX ファイルをこのファイルに
REM      ドラッグ＆ドロップすると、そのファイルを引数として渡します）
REM   ※ 設定はこのバッチ内だけで有効です（setlocal）。
REM =========================================================================

set "APP=your_application.exe"

REM ---- 基本の高音質化（リアルタイム再生でも負荷は小さい） ----------------
REM OPM 正弦波補間（1=線形, HQ_SINE=1 で放物線補間）
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_HQ_SINE=1
REM PCM8 の音量変化時のクリックノイズ除去
set X68SOUND_VOLUME_SMOOTHING=1
REM ADPCM/PCM8 の高品位リサンプラー（折り返しノイズを -94dB まで低減）
REM   ※ X68SOUND_LINEAR_INTERPOLATION は現在の出力経路では効果がありません
set X68SOUND_PCM_RESAMPLE=1
REM ADPCM デコーダ: 0=標準（実機と同じ OKI 方式。推奨）
REM   ※ 1 は別方式のテーブルで、音量が約 -6.6dB 下がるだけのため非推奨
set X68SOUND_ADPCM_MODE=0

REM ---- 出力 -------------------------------------------------------------
set X68SOUND_OUTPUT_RATE=96000

REM ---- 音色補正（控えめ） ------------------------------------------------
set X68SOUND_EXCITER=1
set X68SOUND_SUB_BASS=1
set X68SOUND_FM_WARMTH=1
REM 3バンドEQ（dB）: 低域を少しだけ持ち上げる
set X68SOUND_EQ_LOW_GAIN=2
set X68SOUND_EQ_LOW_FREQ=100

REM ---- 広がり（STEREO_WIDTH: 100=原音, 120=1.2倍） -------------------------
set X68SOUND_STEREO_WIDTH=120
set X68SOUND_CENTER_WIDTH=40
set X68SOUND_EARLY_REFLECTIONS=20

REM ---- リバーブ（FDN 方式: 自然で金属的な響きが出にくい） ------------------
set X68SOUND_REVERB=2
set X68SOUND_REVERB_ALGO=1
set X68SOUND_REVERB_DECAY=60
set X68SOUND_REVERB_MIX=15
set X68SOUND_REVERB_PREDELAY=15

REM ---- バッファ（音が途切れる場合は PCM_BUFFER / LATE_TIME を増やす） ------
set X68SOUND_PCM_BUFFER=7
set X68SOUND_LATE_TIME=200
set X68SOUND_BETW_TIME=5
set X68SOUND_BUF_MULTIPLIER=1

REM ---- 先読みリミッター（音割れ防止。上限 -1.0dBFS、遅延 1.5ms） ----------
set X68SOUND_LIMITER=1
set X68SOUND_LIMITER_CEILING=-10

REM =========================================================================
REM 起動
REM =========================================================================
REM （パスに括弧を含む場合でも安全なように goto で分岐）
if exist "%APP%" goto run
echo [エラー] アプリケーションが見つかりません: "%APP%"
echo このファイルを編集して APP= の行を実際のパスに変更してください。
pause
exit /b 1
:run
echo MUSIC プリセット を適用して起動します: %APP%
"%APP%" %*
endlocal
