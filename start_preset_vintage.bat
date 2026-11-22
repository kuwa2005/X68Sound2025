@echo off
chcp 65001 > nul
setlocal
REM =========================================================================
REM X68Sound2025 VINTAGE プリセット（アナログ風, 48kHz）
REM =========================================================================
REM FM 音源に温かみとコーラスを加え、高域を少し丸めて、アナログシンセ風の音にします。
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
set X68SOUND_OUTPUT_RATE=48000

REM ---- アナログ風の音色 ---------------------------------------------------
set X68SOUND_FM_WARMTH=3
set X68SOUND_FM_HARMONIC=1
REM 3声ステレオコーラス（FM_CHORUS=深さ 1-3）
set X68SOUND_FM_CHORUS=2
set X68SOUND_CHORUS_ALGO=1
set X68SOUND_SUB_BASS=1
REM EQ: 低域を少し太く、高域を丸める
set X68SOUND_EQ_LOW_GAIN=2
set X68SOUND_EQ_LOW_FREQ=150
set X68SOUND_EQ_HIGH_GAIN=-3
set X68SOUND_EQ_HIGH_FREQ=6000

REM ---- 空間（広めの部屋） ---------------------------------------------------
set X68SOUND_EARLY_REFLECTIONS=30
set X68SOUND_REVERB=3
set X68SOUND_REVERB_ALGO=1
set X68SOUND_REVERB_DECAY=75
set X68SOUND_REVERB_MIX=20
set X68SOUND_REVERB_PREDELAY=20

REM ---- バッファ（音が途切れる場合は PCM_BUFFER / LATE_TIME を増やす） ------
set X68SOUND_PCM_BUFFER=5
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
echo VINTAGE プリセット を適用して起動します: %APP%
"%APP%" %*
endlocal
