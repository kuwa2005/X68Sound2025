# mxwav for Windows / Linux（X68Sound2025 版）

MDX ファイル（X68000 の音楽データ）を WAV ファイルに変換するコマンドラインツールです。
GORRY 氏の **mxwav 2.02a** と **MXDRVg 2.00b**（mxdrv.dll）を移植し、音源エミュレーションに本リポジトリの **X68Sound2025** を静的リンクしています。
DLL は不要で、実行ファイル1つで動作します。

- Windows（32bit / 64bit）と Linux（64bit）に対応しています。
- X68Sound2025 の音質設定（環境変数 `X68SOUND_*`）がそのまま使えます。

## 使い方

```sh
mxwav [オプション] ファイル.mdx
```

| オプション | 内容 |
|---|---|
| `-Ln` | 無限ループ曲を n 回ループしたらフェードアウトします（1〜255、既定 2） |
| `-Sn` | サンプリングレートを指定します（22050 / 44100 / 48000 / **96000 / 192000**、既定 44100） |
| `-Pn` | PDX バッファの大きさを KB 単位で指定します（既定 1024） |
| `-Wn` | 演奏終了後、録音を終えるまでの待ち時間を ms 単位で指定します（既定 2000） |
| `-U` | PCM8 を使わずに演奏します |
| `-A` | AI による ADPCM 高音質化を行います（`X68SOUND_AI_ENHANCE=1` と同じ） |
| `-B` | ベンチマークモードで起動します（WAV ファイルは作りません） |
| `-Ofile` | 出力ファイル名を指定します（既定はカレントフォルダの「MDX 名.wav」） |
| `-Mch` | 指定したチャンネルをミュートします（`12345678` = FM、`PQRSTUVW` = ADPCM/PCM8） |

PDX の検索フォルダは、環境変数 `mxp`（オリジナルと同じ）または `PDX` で指定します。
まず MDX と同じフォルダを探し、次に指定フォルダを探します。

```sh
# Linux
export PDX=~/mdx/pdx
X68SOUND_OUTPUT_RATE=96000 X68SOUND_REVERB=2 ./mxwav -L2 song.mdx

# Windows
set PDX=C:\mdx\pdx
mxwav.exe -S48000 song.mdx
```

### オリジナルとの違い
- **`X68SOUND_*` の音質設定**：X68Sound2025 の各種設定がそのまま有効です。`X68SOUND_OUTPUT_RATE` を指定した場合は、WAV ヘッダにも実際の出力レートが書き込まれます。
- **`-S` の追加値**：96000 と 192000 を指定できます。
- **`-A`（新規）**：AI による ADPCM 高音質化を行います。`X68SOUND_PCM_RESAMPLE=1` と組み合わせると効果的です。
- **曲名の表示**：MDX の曲名（Shift_JIS）を UTF-8 に変換して表示します。
- **PDX の検索（Linux）**：大文字小文字を区別せずに探します（例：`TEST.PDX` と `test.pdx` を同一視）。Shift_JIS のファイル名は UTF-8 に変換した名前でも探します。
- **進捗表示**：経過時間を実際のサンプリングレートで計算します（オリジナルは常に 44.1kHz として計算していました）。

## ビルド

| 対象 | コマンド | 出力 |
|---|---|---|
| Linux | `./build.sh` または `make` | `build/linux/mxwav` |
| Windows 32bit（MinGW） | `./build.sh windows` | `build/win32/mxwav.exe` |
| Windows 64bit（MinGW） | `./build.sh windows64` | `build/win64/mxwav.exe` |
| Windows（Visual Studio 2022） | `build_vc2022.bat`（x86）／`build_vc2022.bat x64` | `build\vc_x86\mxwav.exe` など |

Linux 版は**完全な静的リンクの単一ファイル**（約1.2MB）です。`build/linux/mxwav` の1ファイルをコピーするだけで、共有ライブラリやロケール、iconv なしで、どの x86_64 Linux でも動きます。Shift_JIS の変換表は実行ファイルに埋め込んでいます（`src/cp932_table.h`、生成スクリプトは `tools/gen_cp932_table.py`）。ビルドには g++ と静的版の C/C++ ライブラリ（通常は g++ に同梱）が必要です。

```sh
# Debian / Ubuntu の場合（静的ライブラリも含まれます）
sudo apt install g++
```

### 64bit 対応について
MXDRVg は 68000 のアセンブラをほぼそのまま C に変換したもので、アドレスを 32bit のデータレジスタ（D0〜D7）や DMA レジスタに入れて扱います。
64bit 環境で正しく動かすため、次のようにしています。

- データレジスタの型を、68000 と同じ 32bit（`uint32_t`）に固定しました（`depend.h`）。
- MXDRVg が扱うメモリは、すべて 4GB 未満のアドレスに置きます（`src/lowmem.*`）。
  - Linux：`mmap(MAP_32BIT)` で確保し、非 PIE でリンクします。
  - Windows x64：`VirtualAlloc` で低位アドレスから確保し、イメージベースを固定して ASLR を無効にします。
- 万一 32bit を超えるアドレスが渡された場合は、音を壊さずにその場で異常終了します（`PtrToU32`）。
- X68Sound2025 の DMA アドレス処理を、ポインタ幅に依存しない 32bit ビッグエンディアンの読み書きに修正しました。

## テスト

```sh
./build.sh && tests/run_mxwav_tests.sh
```

テスト用の MDX/PDX は `tests/gen_test_data.py` で生成します（著作物は使いません）。
変換した WAV を解析し、次の項目を確認します。

- フォーマット、再生時間、音程（o4a = 440Hz、オクターブ）
- ループ回数とフェードアウト
- ADPCM：MSM6258（DMA）経由と PCM8 経由の両方で、音程・無音区間・歪み率を確認
- チャンネルマスク、各サンプリングレート、`X68SOUND_*` の設定
- エラー処理

## ファイル構成

```
mxwav/
  src/mxwav.cpp          mxwav 本体（移植版）
  src/lowmem.*           32bit アドレス用のメモリ確保
  src/mxdrv/             MXDRVg（移植版）
  original/              オリジナルのソースと説明書（Shift_JIS のまま。比較用）
  tests/                 テスト
```

移植で変更した箇所には、ソース中に `[port]` と注記しています。

## 権利表記・ライセンス

- **mxwav**：Copyright (C) 2000-2013 GORRY.
  「mxwav.exeと、mxwav.exe生成のためのソースファイル群は、自由に使用・配布・改変を行うことができます。これらは無保証であり、使用した、あるいはしなかったことによる一切の責任は所有者にあるものとします。」（`original/mxwav202a/mxwav.txt` より）
- **MXDRVg**：X68k MXDRV music driver version 2.06+17 Rel.X5-S (c)1988-92 milk., K.MAEKAWA, Missy.M, Yatsube / Converted for Win32 [MXDRVg] Copyright (C) 2000-2002 GORRY.
  「mxdrv.dllは、原作であるMXDRV.Xの配布規定に従って配布・使用が可能です。」とされています。ただし、原作 MXDRV.X の配布規定文書は作者も発見できておらず、「X68 的に慣例のフリーソフトウェア扱い」と推定されています。また、ソースには MXDRV.X をディスアセンブルしたものが含まれます（`original/mxdrv200b/mxp.txt`）。
- **X68Sound**：m_puusan 氏の使用許諾に従います（リポジトリ直下の `readme.txt`）。

入手元：GORRY 氏のページ（http://gorry.haun.org/mx/）で配布されている mxwav202a.lzh と mxdrv200b.lzh。
