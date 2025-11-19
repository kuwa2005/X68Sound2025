@echo off

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM 基本設定
REM システムの動作に関する基本的な設定です。
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM PCMバッファサイズ。大きいほど安定、小さいほど低遅延（2-20）
SET X68SOUND_PCM_BUFFER=5

REM Between時間（ms）。タイマー割り込み間隔（1-50）
SET X68SOUND_BETW_TIME=5

REM レイテンシ時間（ms）。大きいほど音途切れに強い（50-1000）
SET X68SOUND_LATE_TIME=500

REM サンプルレート補正係数（0.1-10.0）
SET X68SOUND_REV_MARGIN=1.0

REM バッファサイズ乗数。メモリに余裕がある場合に増加（1-8）
SET X68SOUND_BUF_MULTIPLIER=1

REM 出力サンプリングレート（Hz）（15625-192000）
SET X68SOUND_OUTPUT_RATE=96000

REM 低遅延重視: PCM_BUFFER=3, LATE_TIME=100
REM 安定性重視: PCM_BUFFER=8, LATE_TIME=300
REM 高音質: OUTPUT_RATE=96000 または 192000

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM 音声信号処理の品質を向上させる機能です。
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM PCM8/ADPCM線形補間（エイリアシングノイズを削減、滑らかな音質） 1=ON推奨 （0/1）
SET X68SOUND_LINEAR_INTERPOLATION=1

REM PCM8ボリュームスムージング（音量変化のクリックノイズを除去）。1=ON推奨（0/1）
SET X68SOUND_VOLUME_SMOOTHING=1

REM OPM正弦波線形補間。1=ON推奨（0/1）
SET X68SOUND_OPM_SINE_INTERP=1

REM ADPCMデコーダー（より正確なADPCMデコード、豊かな音色） 0=Legacy, 1=MSM6258高音質（0/1）
SET X68SOUND_ADPCM_MODE=1

REM OPM音質モード。0=標準, 1=高品質, 2=最高品質（0-2）
SET X68SOUND_OPM_QUALITY=2

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM ステレオ強化・立体音響
REM X68000のL/C/Rパンニングを立体的なステレオ音場に変換する機能です。
REM 効果: ヘッドホンの圧迫感を軽減、自然な定位感
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM ステレオ幅（%）。100=通常, 150=広い, 200=超広い（50-200）
SET X68SOUND_STEREO_WIDTH=125

REM クロスフィード（ヘッドホン最適化）
REM クロスフィードレベル。0=OFF, 30=自然, 50=強い（0-100）
SET X68SOUND_CROSSFEED=50

REM クロスフィード遅延（0.1ms単位）。2=0.2ms （1-10）
SET X68SOUND_CROSSFEED_DELAY=2

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM センターチャンネル幅拡張
REM L/C/Rを擬似的に広いステレオ音場に変換
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM センター定位の音を左右に広げる。0=OFF, 70=推奨（0-100）
SET X68SOUND_CENTER_WIDTH=70

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM ハース効果（先行音効果）
REM 音に奥行きと広がりを加え、立体的なサウンドステージを実現
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM ハース効果レベル。0=OFF, 50=推奨（0-100）
SET X68SOUND_HAAS_EFFECT=50

REM ハース効果遅延（0.1ms単位）。5=0.5ms（1-10）
SET X68SOUND_HAAS_DELAY=5

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM 初期反射音
REM 部屋の壁からの反射をシミュレート、空間の深みを演出
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM 初期反射音レベル。0=OFF, 30=推奨（0-100）
SET X68SOUND_EARLY_REFLECTIONS=50

REM 高音域補完（ハイジェネレーター）(0-4)
set X68SOUND_EXCITER=4

REM 低音域補完（ロージェネレーター）(0-4)
set X68SOUND_SUB_BASS=4


REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM set X68SOUND_DEBUG=0
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■

REM 普段使っているMDXプレイヤーを記述
"E:\Tools\mdx\mxv.exe"

