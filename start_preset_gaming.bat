@echo off
REM =========================================
REM X68Sound2025 GAMING プリセット
REM =========================================
REM
REM ゲームプレイ用の設定です。
REM 低レイテンシとパンチのある音を重視し、
REM リバーブを無効化して明瞭度を最大限に引き出します。
REM
REM 【動作環境】
REM - CPU: Intel Core i3以上 / AMD Ryzen 3以上
REM - メモリ: 4GB以上
REM - オーディオ: 48kHz対応
REM
REM =========================================

echo X68Sound2025 GAMING プリセットを適用中...
echo.

REM 【標準出力】48kHz（低レイテンシ）
set X68SOUND_OUTPUT_RATE=48000

REM 【音質向上機能】すべて有効化
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_ADPCM_MODE=1

REM 【オーディオエフェクト】ゲーム最適化
set X68SOUND_EXCITER=3
set X68SOUND_SUB_BASS=3
set X68SOUND_FM_HARMONIC=1
set X68SOUND_COMPRESSOR=2
set X68SOUND_REVERB=0

REM 【バッファ設定】低レイテンシ重視
set X68SOUND_PCM_BUFFER=5
set X68SOUND_LATE_TIME=100
set X68SOUND_BETW_TIME=5
set X68SOUND_BUF_MULTIPLIER=1

echo 設定内容:
echo   プリセット: GAMING（ゲーム用）
echo   出力サンプリングレート: 48kHz（低レイテンシ）
echo   ADPCM高音質モード: ON (MSM6258)
echo.
echo 【有効なエフェクト】
echo   - ハーモニックエキサイター: Bright（明瞭な高音）
echo   - サブハーモニックベース: Deep（迫力ある低音）
echo   - FM倍音エンハンサー: Subtle
echo   - コンプレッサー: Medium（ダイナミクス最適化）
echo   - リバーブ: OFF（明瞭度重視）
echo.
echo 【期待される効果】
echo   - 低レイテンシで反応の良い音
echo   - パンチの効いた明瞭なサウンド
echo   - 効果音がクリアに聞こえる
echo   - 爽快感のあるゲームプレイ
echo.

REM =========================================
REM アプリケーション起動
REM =========================================

echo アプリケーションを起動します...
echo （注意: your_application.exeを実際のパスに変更してください）
echo.

REM TODO: 下の行を編集してください
your_application.exe

pause
