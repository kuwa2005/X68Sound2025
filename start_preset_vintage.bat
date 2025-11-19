@echo off
REM =========================================
REM X68Sound2025 VINTAGE プリセット
REM =========================================
REM
REM アナログ風の温かみのあるサウンド設定です。
REM FM音源の倍音を豊かにし、コーラスとリバーブで
REM ビンテージシンセのような雰囲気を再現します。
REM
REM 【動作環境】
REM - CPU: Intel Core i3以上 / AMD Ryzen 3以上
REM - メモリ: 4GB以上
REM - オーディオ: 48kHz対応
REM
REM =========================================

echo X68Sound2025 VINTAGE プリセットを適用中...
echo.

REM 【標準出力】48kHz
set X68SOUND_OUTPUT_RATE=48000

REM 【音質向上機能】すべて有効化
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_ADPCM_MODE=1

REM 【オーディオエフェクト】ビンテージサウンド
set X68SOUND_EXCITER=1
set X68SOUND_SUB_BASS=2
set X68SOUND_FM_HARMONIC=3
set X68SOUND_FM_WARMTH=3
set X68SOUND_FM_CHORUS=2
set X68SOUND_EARLY_REFLECTIONS=40
set X68SOUND_REVERB=3
set X68SOUND_REVERB_DECAY=80
set X68SOUND_REVERB_MIX=25

REM 【バッファ設定】標準
set X68SOUND_PCM_BUFFER=5
set X68SOUND_LATE_TIME=200
set X68SOUND_BETW_TIME=5
set X68SOUND_BUF_MULTIPLIER=1

echo 設定内容:
echo   プリセット: VINTAGE（アナログ風）
echo   出力サンプリングレート: 48kHz
echo   ADPCM高音質モード: ON (MSM6258)
echo.
echo 【有効なエフェクト】
echo   - ハーモニックエキサイター: Gentle（穏やかな高域）
echo   - サブハーモニックベース: Medium（重厚な低音）
echo   - FM倍音エンハンサー: Rich（豊かな倍音）
echo   - FM温かみエフェクト: Warm（温かく柔らかい音）
echo   - FMコーラス: Rich（アナログシンセ風）
echo   - 初期反射音: Rich（アナログ機材の空間感）
echo   - リバーブ: Large Room（広い空間感）
echo.
echo 【期待される効果】
echo   - アナログシンセのような温かみ
echo   - 豊かな倍音と深みのある音色
echo   - ビンテージ機材のような雰囲気
echo   - ノスタルジックで心地よいサウンド
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
