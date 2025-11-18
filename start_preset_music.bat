@echo off
REM =========================================
REM X68Sound2025 MUSIC プリセット
REM =========================================
REM
REM 音楽鑑賞用の最高音質設定です。
REM すべてのエフェクトをバランスよく有効化し、
REM 96kHzハイレゾ出力で最高の音質を実現します。
REM
REM 【動作環境】
REM - CPU: Intel Core i5以上 / AMD Ryzen 5以上
REM - メモリ: 8GB以上
REM - オーディオ: 96kHz対応インターフェース
REM
REM =========================================

echo X68Sound2025 MUSIC プリセットを適用中...
echo.

REM 【ハイレゾ出力】96kHz
set X68SOUND_OUTPUT_RATE=96000

REM 【音質向上機能】すべて有効化
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_ADPCM_MODE=1

REM 【オーディオエフェクト】音楽鑑賞最適化
set X68SOUND_EXCITER=2
set X68SOUND_SUB_BASS=2
set X68SOUND_FM_HARMONIC=2
set X68SOUND_FM_WARMTH=2
set X68SOUND_STEREO_WIDTH=150
set X68SOUND_REVERB=2
set X68SOUND_REVERB_DECAY=70
set X68SOUND_REVERB_MIX=20

REM 【バッファ設定】安定性重視
set X68SOUND_PCM_BUFFER=7
set X68SOUND_LATE_TIME=200
set X68SOUND_BETW_TIME=5
set X68SOUND_BUF_MULTIPLIER=1

echo 設定内容:
echo   プリセット: MUSIC（音楽鑑賞）
echo   出力サンプリングレート: 96kHz
echo   ADPCM高音質モード: ON (MSM6258)
echo.
echo 【有効なエフェクト】
echo   - ハーモニックエキサイター: Medium（高域補完）
echo   - サブハーモニックベース: Medium（低域補完）
echo   - FM倍音エンハンサー: Medium
echo   - FM温かみエフェクト: Medium
echo   - ステレオ幅: 150%% Wide
echo   - リバーブ: Medium Room
echo.
echo 【期待される効果】
echo   - CDクオリティを超える高音質
echo   - 豊かな低音と鮮明な高音
echo   - FM音源がアナログシンセのような温かさに
echo   - 自然な空間感と深みのある音
echo.

REM =========================================
REM アプリケーション起動
REM =========================================
REM 下の行を実際のアプリケーションパスに変更してください
REM 例: C:\Games\X68000\MDXPlayer.exe

echo アプリケーションを起動します...
echo （注意: your_application.exeを実際のパスに変更してください）
echo.

REM TODO: 下の行を編集してください
your_application.exe

pause
