@echo off
chcp 65001 > nul
setlocal
REM =========================================================================
REM X68Sound2025 GAMING プリセット（ゲーム向け・低遅延, 48kHz）
REM =========================================================================
REM 低遅延を優先し、響きを付けずに輪郭のはっきりしたパンチのある音にします。
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
set X68SOUND_HQ_SINE=0
REM PCM8 の音量変化時のクリックノイズ除去
set X68SOUND_VOLUME_SMOOTHING=1
REM ADPCM/PCM8 の高品位リサンプラー（折り返しノイズを -94dB まで低減）
REM   ※ X68SOUND_LINEAR_INTERPOLATION は現在の出力経路では効果がありません
set X68SOUND_PCM_RESAMPLE=1
REM ADPCM デコーダ: 0=標準（実機と同じ OKI 方式。推奨）
REM   ※ 1 は別方式のテーブルで、音量が約 -6.6dB 下がるだけのため非推奨
set X68SOUND_ADPCM_MODE=0

REM ---- 出力（48kHz: 低遅延・低負荷） ----------------------------------------
set X68SOUND_OUTPUT_RATE=48000

REM ---- 音色（明瞭さとパンチ） -----------------------------------------------
set X68SOUND_EXCITER=2
set X68SOUND_SUB_BASS=2
set X68SOUND_COMPRESSOR=1
REM リバーブは OFF（効果音の明瞭度優先）
set X68SOUND_REVERB=0

REM ---- バッファ（音が途切れる場合は PCM_BUFFER / LATE_TIME を増やす） ------
set X68SOUND_PCM_BUFFER=4
set X68SOUND_LATE_TIME=100
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
echo GAMING プリセット を適用して起動します: %APP%
"%APP%" %*
endlocal
