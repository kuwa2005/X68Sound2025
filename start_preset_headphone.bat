@echo off
REM =========================================
REM X68Sound2025 HEADPHONE プリセット
REM =========================================
REM
REM ヘッドホンリスニング用の最適化設定です。
REM クロスフィードで圧迫感を軽減し、
REM 適度なステレオ幅で自然な定位感を実現します。
REM
REM 【動作環境】
REM - CPU: Intel Core i5以上 / AMD Ryzen 5以上
REM - メモリ: 8GB以上
REM - オーディオ: ヘッドホン、96kHz対応DAC推奨
REM
REM =========================================

echo X68Sound2025 HEADPHONE プリセットを適用中...
echo.

REM 【ハイレゾ出力】96kHz
set X68SOUND_OUTPUT_RATE=96000

REM 【音質向上機能】すべて有効化
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_ADPCM_MODE=1

REM 【オーディオエフェクト】ヘッドホン最適化
set X68SOUND_EXCITER=2
set X68SOUND_SUB_BASS=1
set X68SOUND_FM_HARMONIC=2
set X68SOUND_FM_WARMTH=2
set X68SOUND_STEREO_WIDTH=120
set X68SOUND_CROSSFEED=30
set X68SOUND_CROSSFEED_DELAY=2
set X68SOUND_HAAS_EFFECT=50
set X68SOUND_HAAS_DELAY=5
set X68SOUND_REVERB=1
set X68SOUND_REVERB_MIX=15

REM 【バッファ設定】安定性重視
set X68SOUND_PCM_BUFFER=7
set X68SOUND_LATE_TIME=200
set X68SOUND_BETW_TIME=5
set X68SOUND_BUF_MULTIPLIER=1

echo 設定内容:
echo   プリセット: HEADPHONE（ヘッドホン用）
echo   出力サンプリングレート: 96kHz
echo   ADPCM高音質モード: ON (MSM6258)
echo.
echo 【有効なエフェクト】
echo   - ハーモニックエキサイター: Medium（高域補完）
echo   - サブハーモニックベース: Subtle（控えめな低音）
echo   - FM倍音エンハンサー: Medium
echo   - FM温かみエフェクト: Medium
echo   - ステレオ幅: 120%%（やや広め）
echo   - クロスフィード: Natural（圧迫感軽減）
echo   - ハース効果: Medium（立体音響）
echo   - リバーブ: Small Room（控えめな空間感）
echo.
echo 【期待される効果】
echo   - ヘッドホンの圧迫感が完全に解消
echo   - 自然な楽器の定位感
echo   - 長時間リスニングでも疲れにくい
echo   - 適度な空間感で没入感向上
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
