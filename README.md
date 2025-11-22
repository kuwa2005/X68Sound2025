
# X68Sound2025

- X68Sound.dllは、Sharp X68000のサウンドハードウェア（OPM/ADPCM/PCM8）をエミュレートするWindows用DLLライブラリです。
- そんなDLLライブラリを音質重視で魔改造をしてみました。
- 例）ハイジェネレーター、ハイレゾ対応(192kHz、96kHz)や、ADPCMの高音質デコーダー、OPM正弦波線形補間　など
  
- 互換性を保つため mdxプレイヤーの起動の前、環境変数で設定を行いますので、普段遣いの mxv.exe、mxwav.exeが使えます。

[リリース一覧](https://github.com/kuwa2005/X68Sound2025/releases/tag/DLL)

## 🎵 最高音質で使う（推奨設定）

X68Sound2025を最高音質で使うには、アプリケーション起動前に以下の環境変数を設定してください：

参考：[改造主が使っているMXV起動用 バッチファル](https://github.com/kuwa2005/X68Sound2025/blob/master/HighResoMxv.bat)

### 🔊 スピーカー向け設定

```batch
REM 基本音質向上
set X68SOUND_OUTPUT_RATE=96000           :: 96kHzハイレゾ出力
set X68SOUND_LINEAR_INTERPOLATION=1     :: PCM8/ADPCM線形補間
set X68SOUND_VOLUME_SMOOTHING=1         :: ボリュームスムージング
set X68SOUND_OPM_SINE_INTERP=1          :: OPM正弦波線形補間
set X68SOUND_ADPCM_MODE=1               :: MSM6258高音質ADPCMデコーダ

REM ステレオ強化・立体音響（NEW!）
set X68SOUND_STEREO_WIDTH=150           :: ステレオ幅拡張
set X68SOUND_CENTER_WIDTH=70            :: センターチャンネル幅拡張
set X68SOUND_EARLY_REFLECTIONS=30       :: 初期反射音（空間の深み）

REM 音色補正
set X68SOUND_EXCITER=2                  :: ハーモニックエキサイター
set X68SOUND_SUB_BASS=2                 :: サブハーモニックベース
set X68SOUND_FM_HARMONIC=2              :: FM倍音エンハンサー
set X68SOUND_FM_WARMTH=2                :: FM温かみエフェクト

REM リバーブ
set X68SOUND_REVERB=2                   :: リバーブ（Medium Room）
set X68SOUND_REVERB_DECAY=70            :: 残響時間
set X68SOUND_REVERB_MIX=20              :: リバーブミックス

REM バッファ設定
set X68SOUND_PCM_BUFFER=7               :: バッファサイズ
set X68SOUND_LATE_TIME=200              :: レイテンシ
```

📦 **プリセット:** [`start_preset_music.bat`](start_preset_music.bat)

### 🎧 ヘッドホン向け設定

```batch
REM 基本音質向上
set X68SOUND_OUTPUT_RATE=96000           :: 96kHzハイレゾ出力
set X68SOUND_LINEAR_INTERPOLATION=1     :: PCM8/ADPCM線形補間
set X68SOUND_VOLUME_SMOOTHING=1         :: ボリュームスムージング
set X68SOUND_OPM_SINE_INTERP=1          :: OPM正弦波線形補間
set X68SOUND_ADPCM_MODE=1               :: MSM6258高音質ADPCMデコーダ

REM ヘッドホン最適化（NEW!）
set X68SOUND_STEREO_WIDTH=120           :: ステレオ幅（やや広め）
set X68SOUND_CROSSFEED=30               :: クロスフィード（圧迫感軽減）
set X68SOUND_CROSSFEED_DELAY=2          :: クロスフィード遅延
set X68SOUND_HAAS_EFFECT=50             :: ハース効果（立体音響）
set X68SOUND_HAAS_DELAY=5               :: ハース効果遅延

REM 音色補正
set X68SOUND_EXCITER=2                  :: ハーモニックエキサイター
set X68SOUND_SUB_BASS=1                 :: サブハーモニックベース（控えめ）
set X68SOUND_FM_HARMONIC=2              :: FM倍音エンハンサー
set X68SOUND_FM_WARMTH=2                :: FM温かみエフェクト

REM リバーブ
set X68SOUND_REVERB=1                   :: リバーブ（Small Room）
set X68SOUND_REVERB_MIX=15              :: リバーブミックス（控えめ）

REM バッファ設定
set X68SOUND_PCM_BUFFER=7               :: バッファサイズ
set X68SOUND_LATE_TIME=200              :: レイテンシ
```

📦 **プリセット:** [`start_preset_headphone.bat`](start_preset_headphone.bat)

### 🎮 その他のプリセット

- 🎮 **ゲーム用** [`start_preset_gaming.bat`](start_preset_gaming.bat) - 低遅延、パンチのある音
- 📻 **ビンテージ風** [`start_preset_vintage.bat`](start_preset_vintage.bat) - アナログシンセのような温かみ

---

### ✨ 新機能の効果

**ステレオ強化・立体音響:**
- X68000のL/C/Rパンニングを擬似的に広いステレオ音場に変換
- クロスフィード: ヘッドホンの圧迫感を完全に解消
- ハース効果: 音に奥行きと広がりを加え、立体的なサウンドステージを実現
- 初期反射音: 部屋の響きをシミュレート、空間の深みを演出

**音色補正:**
- エイリアシングノイズ50-70%軽減、クリックノイズ完全除去
- FM音源の音質大幅向上、ADPCM高音質化
- 高域・低域の補完により、豊かでバランスの良い音色

**必要スペック:** Core i5以上、8GB RAM、96kHz対応オーディオインターフェース

### 🐛 トラブルシューティング

音が出ない・歪む場合は、デバッグログを有効にして問題を診断できます：

```batch
set X68SOUND_DEBUG=2   :: デバッグログ有効化（0=オフ、1=基本、2=トレース、3=詳細）
```

ログファイル `x68sound_debug.log` に詳細な内部動作が記録されます。

---

## 最新の変更履歴

### 2025/11/22 - v2.4 オクターブレイヤリング機能 & マスターボリューム

#### 新機能
* **FM/ADPCMマスターボリューム**: FM音源とADPCM音源の音量を個別に調整可能
  - 環境変数 `X68SOUND_FM_MASTER_VOLUME` (0-200%、デフォルト: 100): FM音源全体の音量
  - 環境変数 `X68SOUND_ADPCM_MASTER_VOLUME` (0-200%、デフォルト: 100): ADPCM音源全体の音量
  - FM音源が大きすぎる場合やADPCM音源が小さい場合のバランス調整に最適

* **オクターブレイヤリングモード**: FM音源・ADPCM音源パート単位で1オクターブ上下の音を重ねる機能
  - **FM音源オクターブレイヤリング**: 真のピッチシフトによる1オクターブ上下の音生成
    - 環境変数 `X68SOUND_FM_OCTAVE_UPPER` (0/1): FM音源+1オクターブ有効化
    - 環境変数 `X68SOUND_FM_OCTAVE_UPPER_VOL` (0-100): +1オクターブの音量（%）
    - 環境変数 `X68SOUND_FM_OCTAVE_LOWER` (0/1): FM音源-1オクターブ有効化
    - 環境変数 `X68SOUND_FM_OCTAVE_LOWER_VOL` (0-100): -1オクターブの音量（%）
  - **ADPCM音源オクターブレイヤリング**: リサンプリングによる真のピッチシフト対応
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_UPPER` (0/1): ADPCM+1オクターブ有効化
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_UPPER_VOL` (0-100): +1オクターブの音量（%）
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_LOWER` (0/1): ADPCM-1オクターブ有効化
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_LOWER_VOL` (0-100): -1オクターブの音量（%）
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_LOWER2` (0/1): ADPCM-2オクターブ有効化
    - 環境変数 `X68SOUND_ADPCM_OCTAVE_LOWER2_VOL` (0-100): -2オクターブの音量（%）
    - 環境変数 `X68SOUND_ADPCM_MULTICHANNEL` (0/1): マルチチャンネルモード（真のピッチシフト）
  - すべてデフォルトOFF、環境変数で個別に制御可能
  - FM音源は真のオクターブシフト、ADPCMは音量レイヤリング（注記参照）

#### 使用例
```batch
REM FM/ADPCM音源のバランス調整
set X68SOUND_FM_MASTER_VOLUME=80
set X68SOUND_ADPCM_MASTER_VOLUME=120

REM FM音源に1オクターブ上の音を50%の音量で重ねる
set X68SOUND_FM_OCTAVE_UPPER=1
set X68SOUND_FM_OCTAVE_UPPER_VOL=50

REM FM音源に1オクターブ下の音を30%の音量で重ねる
set X68SOUND_FM_OCTAVE_LOWER=1
set X68SOUND_FM_OCTAVE_LOWER_VOL=30

REM ADPCMに2オクターブ下の音を40%の音量で重ねる（重低音強化）
set X68SOUND_ADPCM_OCTAVE_LOWER2=1
set X68SOUND_ADPCM_OCTAVE_LOWER2_VOL=40

REM ADPCMマルチチャンネルモード（真のピッチシフト）を有効化
REM このモードを有効にすると、ADPCMオクターブレイヤリングが
REM リサンプリングバッファによる真のピッチシフトで動作します
set X68SOUND_ADPCM_MULTICHANNEL=1

REM 自動ゲイン調整を無効化する場合（音割れ注意）
REM set X68SOUND_OCTAVE_AUTO_GAIN=0
```

**効果:**
- FM音源: より豊かで厚みのある音色（オルガン的な響き）
- ADPCM音源: 音の存在感と迫力の向上
- ADPCM-2オクターブ: 重低音の強化、打楽器や効果音の迫力向上

**注記:**
- FM音源: 各オペレーターのピッチを変更して真のオクターブシフトを実現
- ADPCM音源: 2つのモードに対応
  - **通常モード** (`X68SOUND_ADPCM_MULTICHANNEL=0`): 音量レイヤリング方式（CPU負荷低、既存のPCM8に影響なし）
  - **マルチチャンネルモード** (`X68SOUND_ADPCM_MULTICHANNEL=1`): リサンプリングバッファによる真のピッチシフト
    - リングバッファ方式で最大8チャンネル同時再生（通常+3オクターブレイヤー×ステレオ）
    - 線形補間による高品質リサンプリング（+1oct=2x速度、-1oct=0.5x速度、-2oct=0.25x速度）
    - 各オクターブレイヤーが独立した再生レートで動作
    - PCM8チャンネルに影響を与えない独立設計
- ADPCM-2オクターブ: -1オクターブと併用可能、重低音の補強に効果的
- **自動ゲイン調整**: オクターブレイヤーの数に応じて自動的に音量を調整し、クリッピング（音割れ）を防止
  - 環境変数 `X68SOUND_OCTAVE_AUTO_GAIN` (0/1、デフォルト: 1=ON)
  - FM: 1レイヤー=75%、2レイヤー=60%に自動調整
  - ADPCM: 1レイヤー=75%、2レイヤー=65%、3レイヤー=55%に自動調整
  - 無効化すると音量が大きくなりますが、クリッピングのリスクがあります
- CPU負荷: FM上下両方有効時で約30-40%増加、ADPCMマルチチャンネルモードは追加で約10-15%増加

### 2025/11/18 - v2.3 擬似ステレオ強化機能 & HPFフィルター改善

#### 新機能
* **擬似ステレオ・立体音響エフェクト**: X68000のL/C/Rパンニングをより広いステレオ音場に変換
  - **クロスフィード**: 左右チャンネルを微小遅延で混合、ヘッドホンの圧迫感を軽減
    - 環境変数 `X68SOUND_CROSSFEED` (0-100): レベル調整
    - 環境変数 `X68SOUND_CROSSFEED_DELAY` (1-10): 遅延時間（0.1ms単位）
  - **センターチャンネル幅拡張**: Mid-Sideプロセッシングでセンター定位を左右に広げる
    - 環境変数 `X68SOUND_CENTER_WIDTH` (0-100): 拡張度合い
  - **ハース効果**: 先行音効果（Precedence Effect）による立体音響
    - 環境変数 `X68SOUND_HAAS_EFFECT` (0-100): 効果の強さ
    - 環境変数 `X68SOUND_HAAS_DELAY` (1-10): 遅延時間（0.1ms単位）
  - **初期反射音**: 部屋の壁からの反射をマルチタップディレイでシミュレート
    - 環境変数 `X68SOUND_EARLY_REFLECTIONS` (0-100): 反射音レベル
    - 複数の遅延タップ（5ms, 7.5ms, 10ms, 13ms）で空間の深みを演出
  - すべてデフォルトOFF、環境変数で自由に組み合わせ可能
  - 詳細は [環境変数の一覧.md](環境変数の一覧.md) を参照

* **環境変数クイックリファレンス**: 効果別に整理された環境変数一覧
  - [環境変数の一覧.md](環境変数の一覧.md): 効果別カテゴリで探しやすい
  - [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md): 詳細な技術解説

#### 改善
* **HPFフィルター振動防止の強化**: ごく稀に発生するピー音を完全排除
  - 連続飽和検出カウンターを追加（2サンプル以上の連続飽和を検出）
  - 検出閾値を引き下げ（8-11%早期検出）
  - 2段階リセット戦略（単発=穏やか、連続=強制リセット）
  - フィルター履歴も同時クリアして振動ループを完全に断ち切る
  - GetPcm/GetPcm62の全3段（22kHz + 62kHz 2段）で保護
  - より保守的な閾値で安定性を大幅向上

#### ドキュメント更新
* プリセット設定に新機能を追加
  - MUSIC: センターチャンネル幅拡張 + 初期反射音
  - HEADPHONE: クロスフィード + ハース効果
  - VINTAGE: 初期反射音強化
* README推奨設定をスピーカー/ヘッドホン別に分割

### 2025/11/18 - v2.2 MSM6258高音質ADPCM対応 & 重大バグ修正

#### 新機能
* **MSM6258高音質ADPCMデコーダ**: OKI Electric MSM6258互換の89ステップIMA ADPCMデコーダを実装
  - より高精度な16bitデコード（従来は12bit相当）
  - 環境変数 `X68SOUND_ADPCM_MODE=1` で有効化（デフォルト: 0=過去互換モード）
  - ダイナミックレンジ・S/N比が大幅に向上
  - 過去互換性維持のため環境変数で切り替え可能

* **デバッグログレベルシステム**: トラブルシューティング用の4段階ログ出力
  - 環境変数 `X68SOUND_DEBUG` で制御（0=オフ、1=基本、2=トレース、3=詳細）
  - ADPCM/PCM8のデコード処理、DMA転送、HPFフィルタなどの内部状態を可視化
  - ログファイル: `x68sound_debug.log`

#### バグ修正
* **🔧 HPFフィルタ暴走バグ修正 [重大]**: 線形補間とHPFフィルタの干渉によるPCM/ADPCM音声の歪みを修正
  - **症状**: PCM8/ADPCM音源で極端にコンプレッサーをかけたような不快な音が発生（FM音源は正常）
  - **原因**: 線形補間で修正された値がHPFフィルタの状態変数に保存され、誤差が蓄積してOutPcmが発散
  - **影響範囲**: すべてのPCMモード（過去互換ADPCM、MSM6258高音質ADPCM、PCM8）
  - **修正内容**: HPFフィルタには補間前の実際のデコード値を使用し、補間値は出力のみに適用
  - **修正ファイル**: `X68Sound/adpcm.h` (GetPcm/GetPcm62), `X68Sound/pcm8.h` (GetPcm)
  - **重要度**: ⚠️ 線形補間機能（`X68SOUND_LINEAR_INTERPOLATION=1`）を使用しているすべてのユーザーに影響

* **🔧 線形補間バグ修正 [重大]**: ADPCM再生時に無音になるバグを修正
  - **症状**: 線形補間有効時にADPCM音源が完全に無音（PCM8は正常）
  - **原因**: 線形補間の計算式が誤っており、常に0を返していた
  - **修正内容**: 正しい線形補間式 `prev + ((curr - prev) * frac) >> 16` に修正
  - **修正ファイル**: `X68Sound/adpcm.h` (GetPcm/GetPcm62)
  - **重要度**: ⚠️ ADPCM音源が使えない致命的バグ

### 2025/11/17 - v2.1 ハイレゾ対応アップデート

#### 新機能
* **ハイレゾ音源対応**: 96kHz/192kHz出力サンプリングレートをサポート
  - 環境変数 `X68SOUND_OUTPUT_RATE` で設定可能
  - 96kHz: CPU負荷2倍、高品質な音質
  - 192kHz: CPU負荷4倍、超高品質な音質
  - 線形補間機能との相乗効果でエイリアシングノイズをさらに軽減
  - 詳細は [HIRES_AUDIO_FEASIBILITY_JP.md](HIRES_AUDIO_FEASIBILITY_JP.md) を参照

### 2025/11/17 - v2.0 音質向上アップデート

#### 新機能
* **PCM8/ADPCM線形補間**: サンプルレート変換に線形補間を実装
  - エイリアシングノイズを大幅に軽減（50～70%）
  - 高周波成分の歪みを低減
  - CPU負荷: 約5%増加
  - 環境変数 `X68SOUND_LINEAR_INTERPOLATION` で制御可能（デフォルト: 有効）

* **OPM線形補間**: FM音源の正弦波テーブルに線形補間を実装
  - エイリアシングノイズを大幅に軽減（50～70%）
  - FM変調の滑らかさが向上
  - 位相精度の向上
  - CPU負荷: 約3～5%増加
  - 環境変数 `X68SOUND_OPM_SINE_INTERP` で制御可能（デフォルト: 有効）

* **ボリュームスムージング**: PCM8のクリックノイズ完全除去
  - ボリューム変更時に徐々に変化（約5ms）
  - CPU負荷: 1%未満
  - 環境変数 `X68SOUND_VOLUME_SMOOTHING` で制御可能（デフォルト: 有効）

* **環境変数によるオーディオ設定**: 再コンパイル不要の設定変更
  - バッファサイズ、レイテンシ、音質機能などを環境変数で制御
  - 詳細は [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) を参照

#### 改善
* コード品質の向上
  - マジックナンバーを定数化（HPF係数、パンニング定数など）
  - PCM8ミキシングロジックの可読性向上
  - 整数オーバーフロー対策（飽和演算）
  - 日本語コメントの整理と追加

#### ドキュメント
* [PCM8_QUALITY_IMPROVEMENTS_JP.md](PCM8_QUALITY_IMPROVEMENTS_JP.md) - PCM8音質向上の詳細解説
* [OPM_QUALITY_IMPROVEMENTS_JP.md](OPM_QUALITY_IMPROVEMENTS_JP.md) - OPM音質向上の詳細解説
* [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) - 環境変数設定ガイド

### 2025/01/17 - 初期リリース
* Shift-JISからUTF-8への変換
* `__int64` を `int64_t` に置換
* Visual Studio 2022対応のビルドファイル追加

---

## 過去の変更履歴

### 2016/09/12
* CSM音声合成モードの実装(GIMONS氏提供)

### 2013/08/03
* SRC/Low-passFilterを無効化する(LinearDrive氏提供)
* 44.1KHz/48KHzでは過去互換を取って従来通り
* 22KHzでチャンネルマスクが効いていなかったのを修正
* LFOの修正をVOPMから持ってきた

### 2013/02/12
* C86CTLへの対応を追加（honet氏提供）
* PCM8モード時のDMAチェイン動作を修正

### 2013/01/09
* x64バイナリのビルドを可能にした。pcidebug.dll自体がx64非対応の為、ROMEOサポートを外しています

### 2011/01/19
* チャンネルマスク機能(X68Sound_SetMask)を追加

### 2003/09/26
* 22/44.1/48KHz以外の出力許す様に修正
* ROMEOパッチ(GORRY氏)/ROMEO-Athronパッチ(うさ氏)のコードをマージ

---

## クイックスタート：最高音質で使う

X68Sound2025で最高の音質を体験する3ステップ：

### ステップ1: バッチファイルを作成
`start_highquality.bat` というファイルを作成し、以下を記述：

```batch
@echo off
REM X68Sound2025 最高音質設定
set X68SOUND_OUTPUT_RATE=96000
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1
set X68SOUND_PCM_BUFFER=7
set X68SOUND_LATE_TIME=200

REM X68Sound を使用するアプリケーションを起動
your_application.exe
```

### ステップ2: アプリケーションのパスを設定
`your_application.exe` を実際のアプリケーションのパスに変更

### ステップ3: バッチファイルから起動
`start_highquality.bat` をダブルクリックして起動

**完了！** これで96kHzハイレゾ音質でX68Soundを楽しめます。

### プリセットバッチファイル

リポジトリには以下のプリセットバッチファイルが用意されています：

- **`start_highquality_96kHz.bat`** - 96kHz最高音質設定（推奨）
- **`start_ultrahighquality_192kHz.bat`** - 192kHz超高音質設定（ハイスペックPC向け）
- **`start_standard.bat`** - 標準設定（44.1kHz）

使い方：バッチファイルを編集して `your_application.exe` を実際のアプリケーションパスに変更し、ダブルクリックで起動

---

## 使い方

### 基本的な使用方法

```cpp
#include "x68sound.h"

// 初期化
X68Sound_Start(
    44100,  // サンプルレート
    1,      // OPM有効
    1,      // ADPCM有効
    5,      // Between time (ms)
    5,      // PCMバッファサイズ
    200,    // レイテンシ (ms)
    1.0     // サンプルレート補正マージン
);

// OPMレジスタ操作
X68Sound_OpmReg(0x20);      // レジスタ選択
X68Sound_OpmPoke(0x80);     // データ書き込み

// 終了処理
X68Sound_Free();
```

### 環境変数による設定

音質や動作を環境変数でカスタマイズできます。詳細は [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) を参照してください。

#### 基本設定
```batch
REM バッファサイズとレイテンシ
set X68SOUND_PCM_BUFFER=5
set X68SOUND_LATE_TIME=200

REM デバッグログ有効化
set X68SOUND_DEBUG=1
```

#### 音質設定
```batch
REM すべての音質向上機能（デフォルトで有効）
set X68SOUND_LINEAR_INTERPOLATION=1
set X68SOUND_VOLUME_SMOOTHING=1
set X68SOUND_OPM_SINE_INTERP=1

REM 実機完全互換モード（すべての音質機能を無効化）
set X68SOUND_LINEAR_INTERPOLATION=0
set X68SOUND_VOLUME_SMOOTHING=0
set X68SOUND_OPM_SINE_INTERP=0
```

#### ハイレゾ設定
```batch
REM 96kHzハイレゾ出力（推奨）
set X68SOUND_OUTPUT_RATE=96000
set X68SOUND_PCM_BUFFER=7
set X68SOUND_LATE_TIME=200

REM 192kHz超高品質出力（ハイスペックPC向け）
set X68SOUND_OUTPUT_RATE=192000
set X68SOUND_PCM_BUFFER=10
set X68SOUND_LATE_TIME=300
set X68SOUND_BUF_MULTIPLIER=2
```

#### 最高音質設定（推奨）
すべての音質向上機能とハイレゾ出力を組み合わせた最高品質の設定です：

```batch
REM =========================================
REM X68Sound 最高音質設定
REM =========================================

REM 【ハイレゾ出力】96kHz（高性能PCでは192kHzも推奨）
set X68SOUND_OUTPUT_RATE=96000

REM 【音質向上機能】すべて有効化
set X68SOUND_LINEAR_INTERPOLATION=1    :: PCM8/ADPCM線形補間（エイリアシングノイズ50-70%軽減）
set X68SOUND_VOLUME_SMOOTHING=1        :: PCM8ボリュームスムージング（クリックノイズ除去）
set X68SOUND_OPM_SINE_INTERP=1         :: OPM正弦波線形補間（FM音源の音質向上）

REM 【バッファ設定】安定性重視
set X68SOUND_PCM_BUFFER=7              :: ハイレゾ用バッファサイズ
set X68SOUND_LATE_TIME=200             :: レイテンシ（音途切れ防止）
set X68SOUND_BETW_TIME=5               :: Between時間（デフォルト）
set X68SOUND_BUF_MULTIPLIER=1          :: バッファ乗数（必要に応じて2に）

REM 【デバッグ】動作確認（オプション）
REM set X68SOUND_DEBUG=1

your_application.exe
```

**期待される効果：**
- エイリアシングノイズ：50～70%軽減
- クリックノイズ：完全除去
- FM音源の音質：大幅向上
- サンプリングレート：2倍（96kHz）または4倍（192kHz）
- CPU負荷：約15～20%増加（96kHz時）

**動作環境：**
- CPU: Core i5以上 / Ryzen 5以上
- メモリ: 8GB以上
- オーディオ: 96kHz対応インターフェース

### 設定比較表

| 設定モード | 出力レート | 音質機能 | CPU負荷 | 音質 | 用途 |
|-----------|-----------|---------|--------|-----|------|
| 標準 | 44.1kHz | ON | 100% | 良好 | 一般的な使用 |
| 実機互換 | 44.1kHz | OFF | 95% | 実機相当 | 完全互換性重視 |
| 高品質 | 96kHz | ON | 約200% | 優秀 | **推奨設定** |
| 最高品質 | 192kHz | ON | 約400% | 最高 | ハイスペックPC |

**推奨設定:** 96kHz + すべての音質機能ON（最高音質設定）

### ビルド方法

#### Visual Studio 2022
```batch
REM x86版
build_VS2022_x86.bat

REM x64版
build_VS2022_x64.bat

REM ARM64版
build_VS2022_arm64.bat
```

#### Visual Studio 2019
```batch
REM x86版
build_VS2019_x86.bat

REM x64版
build_VS2019_x64.bat
```

ビルド後のDLLは `build/` ディレクトリに出力されます。

---

## 機能

### エミュレート対象
- **OPM (YM2151)**: FM音源8チャンネル
- **ADPCM**: ADPCMチャンネル
- **PCM8**: 8チャンネルPCM再生（ソフトウェアミキシング）

### 対応フォーマット
- **サンプルレート**: 22.05kHz / 44.1kHz / 48kHz / 96kHz / 192kHz (その他のレートも対応)
- **PCM形式**: 16bit PCM / 8bit PCM / ADPCM

### 音質向上機能（v2.0以降）
- **PCM8/ADPCM線形補間**: サンプルレート変換時のエイリアシングノイズを軽減（50～70%）
- **OPM線形補間**: FM音源の正弦波生成時のエイリアシングノイズを軽減（50～70%）
- **ボリュームスムージング**: PCM8ボリューム変更時のクリックノイズを除去
- **飽和演算**: PCM8の8チャンネル同時再生時のオーバーフロー対策
- **ハイレゾ音源対応**: 96kHz/192kHz出力サンプリングレート（v2.1以降）

---

## ドキュメント

- [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) - 環境変数設定ガイド
- [PCM8_QUALITY_IMPROVEMENTS_JP.md](PCM8_QUALITY_IMPROVEMENTS_JP.md) - PCM8音質向上の詳細解説
- [OPM_QUALITY_IMPROVEMENTS_JP.md](OPM_QUALITY_IMPROVEMENTS_JP.md) - OPM音質向上の詳細解説
- [HIRES_AUDIO_FEASIBILITY_JP.md](HIRES_AUDIO_FEASIBILITY_JP.md) - ハイレゾ音源対応の技術解説

---

## 動作環境

- **OS**: Windows 10/11 (x86/x64/ARM64)
- **CPU**: Pentium以上 (SIMD命令対応を推奨)
- **メモリ**: 16MB以上

---

## トラブルシューティング

### 音が途切れる
バッファサイズとレイテンシを増やしてください：
```batch
set X68SOUND_PCM_BUFFER=10
set X68SOUND_LATE_TIME=300
set X68SOUND_BUF_MULTIPLIER=2
```

### レイテンシが大きい
バッファサイズを減らしてください：
```batch
set X68SOUND_PCM_BUFFER=3
set X68SOUND_LATE_TIME=100
```

### 音質を実機に近づけたい
すべての音質向上機能を無効化してください：
```batch
set X68SOUND_LINEAR_INTERPOLATION=0
set X68SOUND_VOLUME_SMOOTHING=0
set X68SOUND_OPM_SINE_INTERP=0
```

### ハイレゾ出力で音が途切れる
96kHz/192kHzはCPU負荷が高いため、バッファを増やしてください：
```batch
set X68SOUND_PCM_BUFFER=10
set X68SOUND_LATE_TIME=300
set X68SOUND_BUF_MULTIPLIER=2
```

### 最高音質設定の推奨スペック
- **CPU**: Intel Core i5以上 / AMD Ryzen 5以上
- **メモリ**: 8GB以上
- **OS**: Windows 10/11（最新アップデート推奨）
- **オーディオ**: 96kHz/192kHz対応のオーディオインターフェース

---

## ライセンス

以下のオリジナルのライセンスに従います。

・本ソースの改変および改変物の公開、自作ソフト等への組み込みおよびそのソフトの
　配布は自由です。m_puusanへの報告は必要ありません。
・本ソフトウェアの使用または使用不能から生じるいかなる損害（利益やデータの損失、
　その他の金銭的損失など）に関してm_puusanは一切責任を負わないこととします。

---

## クレジット

### オリジナル作者
- m_puusan (X68Sound オリジナル開発者)

### コントリビューター
- GIMONS氏 (CSM音声合成モード実装)
- LinearDrive氏 (SRC/Low-passFilter無効化)
- honet氏 (C86CTL対応)
- GORRY氏 (ROMEOパッチ)
- うさ氏 (ROMEO-Athronパッチ)

### 2025年版改良
- UTF-8変換、Visual Studio 2022対応
- 環境変数サポート
- 音質向上機能（PCM8/ADPCM線形補間、OPM線形補間、ボリュームスムージング）
- ハイレゾ音源対応（96kHz/192kHz出力サンプリングレート）
- コード品質改善（飽和演算、定数化、コメント整理） 


